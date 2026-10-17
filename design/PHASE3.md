# PHASE 3: Cross-Platform Hermetic Qt 6.10.2 Build (`linux`, `macos`, `windows` × `amd64`, `arm64`)

## 1. Executive Summary & Goals

Phase 1 and Phase 2 brought up Qt 6.10.2 from source in Bazel targeting `linux_amd64`. **Phase 3** makes the entire Qt Bazel workspace seamlessly cross-platform across all 6 primary desktop target platforms using the hermetic `@llvm` toolchain (`0.8.24`):

| OS | `amd64` (`x86_64`) | `arm64` (`aarch64`) | QPA Platform Plugin | Native Graphics / Font Backend |
| :--- | :--- | :--- | :--- | :--- |
| **Linux** | `@llvm//platforms:linux_amd64_gnu.2.39` (or `:linux_amd64`) | `@llvm//platforms:linux_arm64_gnu.2.39` (or `:linux_arm64`) | `@qtbase//:xcb` + `:offscreen` | OpenGL/Software + FreeType + Fontconfig + HarfBuzz |
| **macOS / Darwin** | `@llvm//platforms:macos_amd64` | `@llvm//platforms:macos_arm64` | `@qtbase//:cocoa` + `:offscreen` | Metal (`qrhimetal`) + CoreText + FreeType + HarfBuzz |
| **Windows** | `@llvm//platforms:windows_amd64` (MinGW-UCRT) / `:windows_x86_64_msvc` | `@llvm//platforms:windows_arm64` (MinGW-UCRT) / `:windows_aarch64_msvc` | `@qtbase//:windows` + `:offscreen` | D3D11/D3D12 + Direct2D/DirectWrite + GDI + FreeType + HarfBuzz |

### Core Design Principles
1. **Standard `@llvm//platforms/...` labels only**: Never define custom `platform()` rules in root `BUILD.bazel`.
2. **Zero patches to upstream Qt sources**: All OS/arch branching happens in Bazel `select()`s and `*_config.bzl` header templates.
3. **Seamless `qt_cc_{binary,library,test}` user experience**: Consumer targets do not specify platform plugins, Objective-C++ flags, Windows subsystem/entry-point glue, or OS-specific system libraries—the `@qt//:qt.bzl` macros wire them automatically via `select()`.
4. **Full $N \times N$ Cross-Compilation (Linux `x86_64` $\to$ all targets first, any-to-any across all 6 supported platforms)**:
   - **Primary workflow**: Cross-compiling from `linux_amd64` (`--host_platform` / exec platform) to all 5 other target platforms (`linux_arm64`, `macos_amd64`, `macos_arm64`, `windows_amd64`, `windows_arm64`) must work out-of-the-box (`bazel build --config=<target> //...`).
   - **General any-to-any cross-compilation**: Because `@llvm` registers `SUPPORTED_EXECS × SUPPORTED_TARGETS` (`{linux,macos,windows}_{x86_64,aarch64}` $\to$ `{linux,macos,windows}_{x86_64,aarch64}`) with hermetic sysroots (`glibc`, `MacOSX26.5.sdk`, `mingw-w64`), cross-compiling from **any** of the 6 supported host platforms to **any** of the 6 target platforms works automatically so long as:
     1. Host tools (`bootstrap`, `moc`, `rcc`, `uic`, `qlalr`, `syncqt`, `qsb`, `qmltyperegistrar`, `qmlcachegen`) build cleanly when evaluated in `cfg = "exec"` on Linux, macOS, and Windows hosts.
     2. Build actions in `@qt//:qt_tools.bzl` avoid host-OS-specific shell assumptions where trivial (or use `ctx.actions.run` / portable Python/Starlark actions instead of GNU-specific `realpath` shell idioms).

---

## 2. Hermetic `@llvm` Toolchain Capabilities & Configuration

### 2.1 Target Platform Matrix in `@llvm` (`0.8.24`)
`@llvm//platforms:BUILD.bazel` declares the following standard platforms (all defaulting to `@llvm//constraints/cxxstdlib:libcxx`):
- **Linux**: `@llvm//platforms:linux_{x86_64,amd64,aarch64,arm64}` (default `gnu.2.28`) and explicit glibc variants `linux_{amd64,arm64}_gnu.2.28` .. `gnu.2.44` plus `_musl`.
- **macOS**: `@llvm//platforms:macos_{x86_64,amd64,aarch64,arm64}`.
  - Uses Apple's `MacOSX26.5.sdk` unpacked hermetically on any host via Starlark `http_pkg_archive` (`osx.from_archive(...)`).
  - Default deployment target is `--macos_minimum_os=14.0` (`-mmacosx-version-min=14.0`). Note: Qt 6.10.2's `cocoa` plugin explicitly rejects `CMAKE_OSX_DEPLOYMENT_TARGET >= 15.0`, so `14.0` (or `13.0`) is required and must be pinned in `.bazelrc`.
  - Frameworks in `@macos_sdk` are controlled by `osx.frameworks(names = [...])` in root `MODULE.bazel` and linked via `-framework <Name>`.
- **Windows (Two ABI Variants)**:
  1. **MinGW-w64 / UCRT (`@llvm//platforms:windows_{amd64,arm64}`)**:
     - Triple: `{x86_64,aarch64}-w64-windows-gnu`, driver `clang`/`clang++` + `ld.lld`.
     - Built from source against `mingw-w64` v14.0.0 headers/import-libs with UCRT (`-lucrt`) and static `libc++`.
     - **Requires no EULA environment variables**, accepts standard GNU/Clang flags (`-std=gnu++20`, `-Wa,-mbig-obj`), and matches Qt's official `win32-clang-g++` mkspec (`QT_FEATURE_stdlib_libcpp = 1`).
  2. **MSVC / UCRT (`@llvm//platforms:windows_{x86_64,aarch64}_msvc`)**:
     - Triple: `{x86_64,aarch64}-pc-windows-msvc`, driver `clang-cl` + `lld-link`.
     - Lazily downloads Microsoft MSVC 14.50 + Windows SDK 10.0.26100; requires `--repo_env=BAZEL_MSVC_RUNTIME_VISUAL_STUDIO_EULA=1 --repo_env=BAZEL_WINDOWS_SDK_EULA=1`.
     - Note: `@llvm` links **static `libc++`** even on the MSVC ABI (`clang_cl_static_libcxx_default_lib`), whereas upstream Qt's `configure.cmake` only models `stdlib_libcpp` on `MINGW OR LINUX` and expects MSVC STL under `_MSC_VER`. Furthermore, `clang-cl` rejects unprefixed GNU flags (`-fPIC`, `-fvisibility=hidden`, `-std=gnu++20`).
  - **Decision**: Target **MinGW-UCRT (`@llvm//platforms:windows_{amd64,arm64}`)** as our standard Windows platform in Phase 3 (zero EULA friction, GNU/Clang driver compatibility, first-class `libc++` support in Qt).

### 2.2 `.bazelrc` and Root `MODULE.bazel` Adjustments
1. **`.bazelrc` platform config aliases**:
   Keep a default host/target configuration while providing explicit `--config=<platform>` presets and `--experimental_platform_in_output_dir` (so switching `--platforms` or cross-compiling multiple targets in sequence preserves per-platform `bazel-out/<platform>-fastbuild` caches without thrashing analysis or exec tools):
   ```ini
   common --experimental_platform_in_output_dir

   # Default remains linux_amd64_gnu.2.39 (can be overridden by --config or --platforms)
   build --platforms=@llvm//platforms:linux_amd64_gnu.2.39
   build --macos_minimum_os=14.0

   build:linux_amd64 --platforms=@llvm//platforms:linux_amd64_gnu.2.39
   build:linux_arm64 --platforms=@llvm//platforms:linux_arm64_gnu.2.39
   build:macos_amd64 --platforms=@llvm//platforms:macos_amd64
   build:macos_arm64 --platforms=@llvm//platforms:macos_arm64
   build:windows_amd64 --platforms=@llvm//platforms:windows_amd64
   build:windows_arm64 --platforms=@llvm//platforms:windows_arm64
   ```
2. **`osx.frameworks` in root `MODULE.bazel`**:
   Add `"Network"` (required by `src/network/kernel/qnetconmonitor_darwin.mm` in `@qtbase//:network`) to the `osx.frameworks(names = [...])` list in `MODULE.bazel`.

### 2.3 Any-to-Any Cross-Compilation Architecture ($6 \text{ Exec Hosts} \times 6 \text{ Targets}$)
In Bazel, every Qt build involves two distinct platform configurations:
1. **`exec` configuration (`cfg = "exec"`)**: Compiles and runs code generators (`syncqt`, `bootstrap`, `moc`, `rcc`, `uic`, `qlalr`, `qsb`, `qmltyperegistrar`, `qmlcachegen`).
2. **`target` configuration**: Compiles Qt libraries (`core`, `gui`, `widgets`, `qml`, `quick`, etc.), static QPA/image/SQL/TLS plugins, and user `qt_cc_{binary,library,test}` targets.

Because `@llvm` provides prebuilt stage0 toolchains for `linux-{amd64,arm64}`, `darwin-{amd64,arm64}`, and `windows-{amd64,arm64}`, and declares cross-toolchains for every `(exec_os, exec_cpu) -> (target_os, target_cpu)` pair:
- **No special cross-toolchain wiring is needed**: Once `@qtbase//:bootstrap`, `@qtbase//:core`, `@qtdeclarative//:qml`, etc. have complete `select()` arms for `linux`, `macos`, and `windows` on `amd64` and `arm64`, the **exact same target definitions** serve both as `cfg = "exec"` host tools on any of the 6 host platforms and as cross-compiled target libraries for any of the 6 target platforms.
- **Exec vs. Target separation in `moc_gen`**:
  - `moc` itself is compiled for the `exec` platform (`cfg = "exec"`), while `_MOC_PLATFORM_DEFINES` is `select()`ed on the **target** platform (`@platforms//os:{linux,macos,windows}` and `@platforms//cpu:{x86_64,aarch64}`). Thus, a `linux_x86_64` `moc` binary cross-generating meta-objects for `macos_arm64` or `windows_amd64` automatically receives the target's predefined macros (`__APPLE__`, `__aarch64__` or `_WIN32`, `_WIN64`, `__x86_64__`) and evaluates `#if QT_CONFIG(...)` identically to the target cross-compiler.

---

## 3. Audit of Required Changes Across the Codebase

### 3.1 `@qt` Rules & Consumer Hooks (`repos/qt/`)

#### A. `repos/qt/BUILD.bazel` — Platform & Architecture `config_setting`s
Already defines `linux_x86_64`, `linux_arm64`, `osx_x86_64`, `osx_arm64`, `windows_x86_64`, `windows_arm64`. We will add:
- `@qt//:unix` (`config_setting_group` matching `@platforms//os:linux` or `@platforms//os:macos`) so POSIX/UNIX sources (`qcore_unix.cpp`, `qfilesystemengine_unix.cpp`, `qprocess_unix.cpp`, `qthread_unix.cpp`, `qlocalserver_unix.cpp`, etc.) are shared cleanly between Linux and macOS instead of duplicated or trapped in `os:linux`.
- `@qt//:x86_64` and `@qt//:arm64` aliases (or direct `@platforms//cpu:{x86_64,aarch64}`) so CPU-specific flags (`-mshstk`, `-march=core-avx2`, `ENABLE_PIXMAN_DRAWHELPERS`) are keyed on **CPU architecture** rather than `@qt//:linux_x86_64`.
- Shared `QT_COMMON_COPTS` and `QT_COMMON_DEFINES` Starlark constants in `@qt//:qt_flags.bzl` (or `qt_internal_library.bzl`) that select per OS/ABI instead of hardcoding Linux/ELF flags in every `repos/*/BUILD.bazel`.

#### B. `repos/qt/qt.bzl` — `qt_cc_binary`, `qt_cc_library`, `qt_cc_test`
1. **`_QPA_PLATFORM_CONFIG` & `_platform_plugin_selects`**:
   Update `_QPA_PLATFORM_CONFIG` so GUI binaries/tests automatically link the native QPA plugin and set `QT_QPA_PLATFORM` fallback:
   ```python
   _QPA_PLATFORM_CONFIG = {
       "@platforms//os:linux": {
           "plugins": ["@qtbase//:xcb"],
           "env": {"QT_QPA_PLATFORM": "xcb;offscreen"},
       },
       "@platforms//os:macos": {
           "plugins": ["@qtbase//:cocoa"],
           "env": {"QT_QPA_PLATFORM": "cocoa;offscreen"},
       },
       "@platforms//os:windows": {
           "plugins": ["@qtbase//:windows"],
           "env": {"QT_QPA_PLATFORM": "windows;offscreen"},
       },
       "//conditions:default": {
           "plugins": [],
           "env": {},
       },
   }
   ```
   And register `"@qtbase//:windows": ["@qtbase//:windows_qpa_shared"]` in `_QTBASE_SHARED_MAP`.
2. **Conditional `dbus_shared` in `_QTBASE_SHARED_MAP`**:
   Currently `_QTBASE_SHARED_MAP` unconditionally attaches `@qtbase//:dbus_shared` whenever a target depends on `@qtbase//:gui` or `@qtbase//:widgets`. On macOS and Windows, `QtGui` does not link `QtDBus` (unless XCB is enabled on macOS). Either:
   - Build `@qtbase//:dbus` on all 3 OSes (with `QT_FEATURE_dbus_linked = -1` on macOS/Windows so it compiles against Qt's bundled `src/dbus/dbus_minimal_p.h` without needing `@dbus`), **or**
   - Make `dbus_shared` attachment OS-conditional. Building `:dbus` everywhere with `dbus_linked = -1` on non-Linux matches upstream Qt (`QT_FEATURE_dbus` is ON on all desktop OSes) and keeps `_QTBASE_SHARED_MAP` uniform!
3. **Windows `QtEntryPoint` & Subsystem Handling in `qt_cc_binary`**:
   - Console binaries/tests (`main(int, char**)`) work out-of-the-box on Windows with the default console subsystem.
   - Add an optional `win_subsystem = "console" | "windows"` attribute (default `"console"`) on `qt_cc_binary`, plus `@qtbase//:entrypoint` (`src/entrypoint/qtentrypoint_win.cpp`, linking `-lmingw32` before `entrypoint` + `-lshell32` and defining `QT_NEEDS_QMAIN`) when `win_subsystem == "windows"`.
4. **Windows DLL Runtime & Import Libraries (`cc_shared_library`)**:
   - On ELF (Linux) and Mach-O (macOS), symbols in static dependencies listed in `exports_filter` are automatically exported from `lib*_shared.{so,dylib}` unless hidden.
   - On PE/COFF (Windows), a `.dll` only exports symbols marked `__declspec(dllexport)` **or** listed in a module definition (`.def`) file. Qt's own classes have `Q_CORE_EXPORT` (`__declspec(dllexport)` when `QT_BUILD_*_LIB` is defined), **except** when `QT_STATIC` is defined!
   - **Critical discovery**: Currently `qt_internal_library` compiles a single `cc_library` and feeds it to `cc_shared_library`. Let's verify how `Q_CORE_EXPORT` expands on Windows: in `qcompilerdetection.h`, if neither `QT_STATIC` nor `QT_SHARED` is defined, `QT_SHARED` is assumed (`Q_DECL_EXPORT = __declspec(dllexport)` when `QT_BUILD_CORE_LIB` is defined, and `Q_DECL_IMPORT = __declspec(dllimport)` for consumers).
   - For bundled 3rdparty static libraries re-exported via `exports_filter` (like `zlib`, `pcre2`, `double-conversion`, `freetype`, `harfbuzz`, `libpng`, `openssl`), their headers do not use `dllexport`. On Windows we must either:
     1. Enable `features = ["windows_export_all_symbols"]` on `cc_shared_library` / `cc_library` targets (supported by `@llvm` toolchain via `@rules_cc//cc/toolchains/args/def_file:def_file`), or
     2. Link 3rdparty static libs privately into each DLL (or export them via `windows_export_all_symbols`).

#### C. `repos/qt/qt_tools.bzl` — Host Tools (`moc`, `rcc`, `uic`, `qlalr`, `qsb`, `qmltyperegistrar`, `qt_sync_headers`) Across Any Exec Platform
- All code-generation rules execute with `cfg = "exec"`, so when cross-compiling from any host to any target, the host tools (`bootstrap`, `moc`, `rcc`, `uic`, `qlalr`, `qsb`, `qmltyperegistrar`, `qmlcachegen`) compile for the `exec` platform and generate code for the `target` platform.
- **Making `bootstrap` work on all 6 exec platforms (`repos/qtbase/BUILD.bazel`)**:
  `bootstrap` is linked by `moc`, `rcc`, `uic`, and `qlalr` (whereas `qsb`, `qmltyperegistrar`, and `qmlcachegen` link `@qtbase//:core`). Adding `@qt//:unix`, `@platforms//os:macos`, and `@platforms//os:windows` arms to `bootstrap` (matching `src/tools/bootstrap/CMakeLists.txt`) ensures host tools build natively whether the exec host is Linux, macOS, or Windows (`amd64` or `arm64`).
- **Keeping `qt_tools.bzl` actions host-agnostic**:
  - `uic`, `rcc`, `qsb`, `qmltyperegistrar`, and `moc --collect-json` already use `ctx.actions.run` (no shell required, works identically on Linux, macOS, and Windows exec hosts).
  - `moc_gen` (which moves `<out>.tmp` $\to$ `<out>`), `qt_sync_headers` (which resolves the real source directory and invokes `syncqt`), and `qt_lalr` (which runs `qlalr` with `cwd = out_dir`) use POSIX `sh` commands (`mv`, `cp`, `mkdir -p`, `realpath`). These work natively on both **Linux and macOS exec hosts**, and on **Windows exec hosts** when MSYS2/Git-Bash is registered as Bazel's `SH_EXECUTABLE` (standard Bazel-on-Windows requirement).
- **`_MOC_PLATFORM_DEFINES` in `qt_tools.bzl`**:
  Update `_MOC_PLATFORM_DEFINES` to include both target OS defines (`__linux__`, `__APPLE__`, `_WIN32`/`_WIN64`) and target CPU architecture defines (`__x86_64__` / `__aarch64__`) so `qconfig.h` / `qconfig_p.h` OS and architecture blocks evaluate identically during `moc` and C++ cross-compilation.

---

### 3.2 `@qtbase` Cross-Platform Overhaul (`repos/qtbase/`)

#### A. Config Headers (`repos/qtbase/qtbase_config.bzl`)
Currently `qtbase_config.bzl` uses partial `#ifdef __APPLE__` guards and has no `_WIN32` guards, plus a bug where `QT_FEATURE_freetype` is `-1` on `__APPLE__` (which breaks `@qtbase//:offscreen` on macOS).
We will update all config templates in `qtbase_config.bzl` to use clean 3-way preprocessor branching (`#if defined(_WIN32) ... #elif defined(__APPLE__) ... #else ... #endif`) and 2-way CPU branching (`#if defined(__x86_64__) || defined(_M_X64) ... #elif defined(__aarch64__) || defined(_M_ARM64) ... #endif`):
- **`qconfig.h` & `qconfig_p.h`**:
  - `QT_FEATURE_stdlib_libcpp`: `1` on all platforms (we use `@llvm`'s `libc++` everywhere).
  - `QT_FEATURE_libstdcpp_assertions`, `glibc_fortify_source`, `relro_now_linker`, `stack_clash_protection`, `elf`: `1` on Linux only, `-1` on macOS/Windows.
  - `QT_FEATURE_dlopen`: `1` on Linux/macOS, `-1` on Windows.
  - `QT_FEATURE_posix_fallocate`: `1` on Linux, `-1` on macOS/Windows.
  - `QT_FEATURE_dbus`: `1` on all, `QT_FEATURE_dbus_linked`: `1` on Linux, `-1` on macOS/Windows.
- **`qtcore-config.h` & `qtcore-config_p.h`**:
  - Linux-only (`1` on Linux, `-1` elsewhere): `glib`, `icu`, `inotify`, `getauxval`, `linkat`, `renameat2`, `forkfd_pidfd`, ` copy_file_range`, `dup3`, `accept4`, `memrchr`, `pthread_condattr_setclock`, `pthread_timedjoin`, `poll_ppoll`.
  - UNIX (`1` on Linux & macOS, `-1` on Windows): `clock_gettime`, `dladdr`, `futimens`, `getentropy`, `memmem`, `poll_poll`, `posix_sem`, `posix_shm`, `sysv_sem`, `sysv_shm`, `backtrace`, `QT_THREADSAFE_CLOEXEC`.
  - Windows-only (`1` on Windows, `-1` elsewhere): `broken_threadlocal_dtors` (`1`), `poll_select` (`-1` on Win, `1` on macOS). Keep `cpp_winrt -1`, `winsdkicu -1`, `etw -1` so Windows uses `qlocale_win.cpp`, `qcollator_win.cpp`, `qtimezoneprivate_win.cpp` without extra SDK deps.
- **`qtgui-config.h` & `qtgui-config_p.h`**:
  - All platforms: `freetype 1`, `system_freetype 1`, `harfbuzz 1`, `system_harfbuzz -1`, `png 1`, `system_png 1`, `jpeg 1`, `system_jpeg 1`.
  - Linux-only: `fontconfig 1`, `xcb 1`, `xcb_xlib 1`, `xcb_sm 1`, `xkbcommon 1`, `xkbcommon_x11 1`, `xlib 1`, `evdev 1`, `linuxfb 1`, `vnc 1`, `multiprocess 1`.
  - macOS-only: `metal 1`, `graphicsframecapture -1`.
  - Windows-only: `direct2d 1`, `direct2d1_1 1`, `directwrite 1`, `directwrite3 1`, `directwritecolrv1 1`.
- **`qtnetwork-config.h` & `qtnetwork-config_p.h`**:
  - All platforms: `openssl 1`, `openssl_linked 1`, `opensslv30 1`, `publicsuffix_qt 1`, `publicsuffix_system -1`, `system_proxies 1`.
  - Linux-only: `linux_netlink 1`, `libresolv 1`.
  - macOS-only: `getifaddrs 1`, `ipv6ifname 1`, `libresolv 1`.
  - Windows-only: `networklistmanager -1` (uses `qnetconmonitor_stub.cpp`), `schannel -1`, `sspi -1` (uses our `qopensslbackend` plugin across all 3 OSes!).
- **`qtwidgets-config.h` & `qtwidgets-config_p.h`**:
  - `style_windows 1`, `style_fusion 1` on all platforms; `style_mac -1`, `style_windowsvista -1`, `style_windows11 -1` in `libQt6Widgets` core (or enabled once style plugins are added).

#### B. Per-Target Source, Flag, Mkspec & Dependency `select()`s in `repos/qtbase/BUILD.bazel`
1. **Mkspecs (`qplatformdefs.h`)**:
   - Linux: `mkspecs/linux-g++` + `mkspecs/common/posix/qplatformdefs.h`
   - macOS: `mkspecs/macx-clang` + `mkspecs/common/posix/qplatformdefs.h` + `mkspecs/common/mac/qplatformdefs.h`
   - Windows (MinGW): `mkspecs/win32-clang-g++` + `mkspecs/win32-g++`
2. **Common Copts & Defines**:
   - Linux/macOS: `["-w", "-fPIC", "-fvisibility=hidden", "-fvisibility-inlines-hidden", "-std=gnu++20"]`
   - Windows (MinGW): `["-w", "-std=gnu++20", "-Wa,-mbig-obj", "-Wno-ignored-attributes"]` + `["-mcx16"]` on `windows_x86_64`.
   - Windows common defines: `["UNICODE", "_UNICODE", "WIN32", "_WIN64", "NOMINMAX", "_USE_MATH_DEFINES", "_WIN32_WINNT=0x0A00", "WINVER=0x0A00", "MINGW_HAS_SECURE_API=1"]`.
   - Drop `_GLIBCXX_ASSERTIONS` (no-op with `libc++`) and gate `_LARGEFILE64_SOURCE`, `_LARGEFILE_SOURCE`, `_REENTRANT` on `@qt//:unix`.
3. **`@qtbase//:core`**:
   - Split current `os:linux` sources into `@qt//:unix` (shared POSIX files) and `@platforms//os:linux` (Linux-only `inotify`, `glib`, `icu`, `qstorageinfo_linux`, `qelfparser_p`, `qlocale_unix`, `qtimezoneprivate_tz`).
   - Add `@platforms//os:macos` sources (`.mm` and `.cpp` files from §3.1 of `phase3_research.md`, plus `qcore_mac.mm` compiled in a small helper `cc_library` with `copts = ["-fobjc-weak"]`, and `qfilesystemengine_unix.cpp` compiled as Objective-C++ on macOS via `-x objective-c++` or helper library) and frameworks (`CoreFoundation`, `Foundation`, `IOKit`, `AppKit`, `ApplicationServices`, `CoreServices`, `Security`, `DiskArbitration`, `UniformTypeIdentifiers`).
   - Add `@platforms//os:windows` sources (`*_win.cpp`, `qcoffpeparser.cpp`, `qsystemlibrary.cpp`, `qwinregistry.cpp`, `qwineventnotifier.cpp`, `qwindowspipereader.cpp`, `qwindowspipewriter.cpp`) and `linkopts` (`-ladvapi32 -lauthz -lkernel32 -lnetapi32 -lntdll -lole32 -lruntimeobject -lshell32 -lsynchronization -luser32 -luuid -lversion -lwinmm -lws2_32 -lmpr -luserenv`).
4. **`@qtbase//:gui`**:
   - Move `@freetype` and `@harfbuzz` to unconditional `deps` (used on all 3 OSes). Keep `@fontconfig`, `@glib//glib`, `@libxcb`, `@xkbcommon`, `:dbus`, `:x11_shared` under `@platforms//os:linux`.
   - Gate `ENABLE_PIXMAN_DRAWHELPERS` on `@qt//:linux_x86_64` and `@qt//:osx_x86_64` (per CMake `UNIX AND NOT arm64`), and `-march=core-avx2` on `@platforms//cpu:x86_64`.
   - Add `@platforms//os:macos` sources (`qcocoanativeinterface.mm`, `qimage_darwin.mm`, `qcoregraphics.mm`, `qrasterbackingstore.cpp`, `qrhibackingstore.cpp`, `qmacmimeregistry.mm`, `qutimimeconverter.mm`, `qapplekeymapper.mm`, `qappleiconengine.mm`, `qcoretextfontdatabase.mm`, `qfontengine_coretext.mm`, `qmetallayer.mm`, `qrhimetal.mm`, `qaccessiblecache_mac.mm`) and frameworks (`AppKit`, `Carbon`, `CoreFoundation`, `CoreGraphics`, `CoreText`, `Foundation`, `ImageIO`, `QuartzCore`, `Metal`).
   - Add `@platforms//os:windows` sources (`qpixmap_win.cpp`, `qwindowsguieventdispatcher.cpp`, `qwindowsmimeconverter.cpp`, `qwindowsnativeinterface.cpp`, `qwindowsthemecache.cpp`, `qrhid3d11.cpp`, `qrhid3d12.cpp`, `qrhid3dhelpers.cpp`, `src/3rdparty/D3D12MemoryAllocator/D3D12MemAlloc.cpp`, `qdxgivsyncservice.cpp`, `qdxgihdrinfo.cpp`, `qwindowsfontdatabase*.cpp`, `qwindowsfontengine*.cpp`, `qwindowsnativeimage.cpp`, `qedidparser.cpp`) and `linkopts` (`-ladvapi32 -lgdi32 -lole32 -lshell32 -luser32 -luxtheme -ld3d11 -ldxgi -ldxguid -ld3d12 -ld2d1 -ldwrite -luuid`).
5. **`@qtbase//:network`**:
   - Split UNIX socket/host/dns sources into `@qt//:unix` vs `@platforms//os:linux` (`qnetworkinterface_linux.cpp`) vs `@platforms//os:macos` (`qnetworkinterface_unix.cpp`, `qnetconmonitor_darwin.mm`, `qnetworkproxy_darwin.cpp`, frameworks `CoreFoundation`, `Security`, `CFNetwork`, `Network`, `CoreServices`, `SystemConfiguration`).
   - Add `@platforms//os:windows` sources (`qhostinfo_win.cpp`, `qnetworkinterface_win.cpp`, `qnetworkproxy_win.cpp`, `qnativesocketengine_win.cpp`, `qdnslookup_win.cpp`, `qlocalserver_win.cpp`, `qlocalsocket_win.cpp`) and `linkopts` (`-ladvapi32 -ldnsapi -liphlpapi -lsecur32 -lwinhttp -lws2_32`).
6. **`@qtbase//:widgets`, `:dbus`, `:sql`, `:xml`, `:concurrent`, `:test`**:
   - `widgets`: Add macOS (`qmacgesturerecognizer.cpp`, `qmenu_mac.mm`, `-framework AppKit -lz`) and Windows (`qwizard_win.cpp`, `-ldwmapi -lshell32 -luxtheme`) arms.
   - `dbus`: Make `@dbus` dependency Linux-only; on macOS/Windows compile against bundled `src/dbus/dbus_minimal_p.h` (`QT_FEATURE_dbus_linked -1`) and link `-ladvapi32 -lnetapi32 -luser32 -lws2_32` on Windows.
   - `sql`: Make `-ldl -lpthread` on `:sqlite3_bundled` Linux/macOS-only.
   - `test`: Add macOS (`qappletestlogger.cpp`, `qtestutil_macos.mm`, frameworks `Security`, `AppKit`, `ApplicationServices`, `Foundation`) and Windows (`-ladvapi32 -luser32`) arms.
7. **New Native QPA Platform Plugins (`@qtbase//:cocoa` and `@qtbase//:windows`)**:
   - `@qtbase//:cocoa` (`src/plugins/platforms/cocoa/**`): `qt_static_plugin` (`QCocoaIntegrationPlugin`), 39 compiled `.mm` + 1 `.cpp`, 10 textual `qnsview_*.mm` in `hdrs`, cursor resources via `qt_rcc`, frameworks `Foundation`, `AppKit`, `Carbon`, `CoreServices`, `CoreVideo`, `IOKit`, `IOSurface`, `Metal`, `QuartzCore`, `UniformTypeIdentifiers`. (`target_compatible_with = ["@platforms//os:macos"]`).
   - `@qtbase//:windows` (`src/plugins/platforms/windows/**`): `qt_static_plugin` (`QWindowsIntegrationPlugin`), 32 `.cpp` + 22 `uiautomation/*.cpp`, cursor resources via `qt_rcc`, Windows system `linkopts`. (`target_compatible_with = ["@platforms//os:windows"]`).
   - `@qtbase//:xcb` and `:x11_shared`: Add `target_compatible_with = ["@platforms//os:linux"]` so `bazel build //...` on macOS/Windows automatically skips Linux X11 targets.

---

### 3.3 Other Phase 2 Modules & BCR Dependencies

1. **`@qtsvg`, `@qtimageformats`, `@qtlanguageserver`, `@qtshadertools`, `@qtdeclarative`**:
   - Replace hardcoded `common_copts` (`-fPIC`, `_GLIBCXX_ASSERTIONS`, `-lpthread`) with shared cross-platform flags from `@qt`.
   - In `@qtdeclarative`:
     - `masm` / Yarr JIT (`QT_FEATURE_qml_jit`): enabled on `x86_64` and `arm64` for Linux and macOS; on Windows, check Qt's `src/qml/configure.cmake` (`qml-jit` condition is `(ARCH_x86_64 AND NOT WIN32) OR ARCH_arm64 ...` — Qt disables JIT on 64-bit Windows due to Windows x64 ABI calling convention in `masm`!). Our `QT_QML_JIT_SUPPORTED_IMPL` in `qtdeclarative_config.bzl` already disables `QT_FEATURE_qml_jit` on `_WIN32` (`#elif defined(_WIN32)` is omitted from the supported OS list in `qtdeclarative_config.bzl`!), so we must also `select()` the `masm` sources out on `@platforms//os:windows`.
     - Platform-specific sources in `Quick` / `QuickDialogs2QuickImpl` (e.g., macOS native menu/dialog `.mm` files if enabled, or keep `quickdialogs2` cross-platform QML implementation).
2. **BCR Dependencies Portability**:
   - **HarfBuzz (`14.1.0.bcr.2`)**: On Linux, BCR `@harfbuzz` selects `hb-glib.cc`; on macOS/Windows it selects `hb-icu.cc` and pulls `@icu`! Verify that `@harfbuzz` compiles cleanly for all 6 platforms or override its flags if needed.
   - **OpenSSL (`3.5.8.bcr.0`)**: Uses generated perlasm for Linux/macOS/Windows (`x86_64` and `arm64`). Verify `@openssl//:ssl` + `@openssl//:crypto` cross-compiles cleanly on all 6 platforms (note: on Windows `mingw`, check whether BCR `openssl` selects `.S` GAS assembly or `.asm` NASM assembly).
   - **MinGW `aarch64` import libraries in `@llvm`**: `@llvm`'s `mingw` runtime builds `lib-common` for `aarch64`. Verify all Windows system libs linked by `QtCore`, `QtGui`, `QtNetwork`, `QtWidgets`, and `qwindows` (`d3d11`, `d3d12`, `d2d1`, `dwrite`, `dxgi`, `uxtheme`, `dwmapi`, `imm32`, `wtsapi32`, `shcore`, `runtimeobject`, `synchronization`, `authz`, `winhttp`, `dnsapi`, `iphlpapi`) have `.def` files in `mingw-w64/mingw-w64-crt/lib-common`.

---

## 4. Phased Execution Milestones for Phase 3

### Milestone 3.1: `linux_arm64` Cross-Compilation (Architecture Decoupling)
- Add `.bazelrc` `--config=linux_arm64` (`@llvm//platforms:linux_arm64_gnu.2.39`) and `--experimental_platform_in_output_dir`.
- Fix CPU-specific `select()`s in `@qtbase` (`-mshstk`, `-march=core-avx2`, `ENABLE_PIXMAN_DRAWHELPERS`) and add CPU defines (`-D__x86_64__` / `-D__aarch64__`) to `_MOC_PLATFORM_DEFINES` in `repos/qt/qt_tools.bzl`.
- Verify `bazel build --config=linux_arm64 //tests/...` builds all targets cleanly from the Linux `x86_64` host.

### Milestone 3.2: `macos_arm64` & `macos_amd64` Cross-Compilation + `@qtbase//:cocoa`
- Add `"Network"` to `osx.frameworks` in root `MODULE.bazel` and pin `--macos_minimum_os=14.0` in `.bazelrc`.
- Update `repos/qtbase/qtbase_config.bzl` (`QT_FEATURE_freetype 1`, `system_freetype 1`, `dbus_linked -1`, `qtnetwork-config.h` macOS features).
- Refactor `repos/qtbase/BUILD.bazel` to split `@qt//:unix` from `@platforms//os:linux`, add `@platforms//os:macos` `.mm`/`.cpp` sources, frameworks, and `mkspecs/macx-clang`, and mark `:xcb`/`:x11_shared` `target_compatible_with = ["@platforms//os:linux"]`.
- Define `@qtbase//:cocoa` (`qt_static_plugin`) and wire it into `repos/qt/qt.bzl` `_QPA_PLATFORM_CONFIG` and `_QTBASE_SHARED_MAP`.
- Verify `bazel build --config=macos_arm64 //tests/...` and `bazel build --config=macos_amd64 //tests/...` succeed.

### Milestone 3.3: `windows_amd64` & `windows_arm64` Cross-Compilation + `@qtbase//:windows`
- Update `repos/qtbase/qtbase_config.bzl` with `#if defined(_WIN32)` branches across all config headers.
- Update `repos/qt/qt_internal_library.bzl` / `repos/qtbase/BUILD.bazel` for Windows DLL export semantics (`windows_export_all_symbols` for re-exported 3rdparty static libs) and Windows compiler/linker flags.
- Add `@platforms//os:windows` sources, system `linkopts`, and `mkspecs/win32-clang-g++` to `core`, `gui`, `network`, `widgets`, `dbus`, `sql`, `xml`, `test`, and `bootstrap`.
- Define `@qtbase//:windows` (`QWindowsIntegrationPlugin`) and `@qtbase//:entrypoint` (`Qt6EntryPoint`), and wire them into `repos/qt/qt.bzl`.
- Verify `bazel build --config=windows_amd64 //tests/...` and `bazel build --config=windows_arm64 //tests/...` succeed.

### Milestone 3.4: Phase 2 Submodules Cross-Platform Verification
- Audit and fix any remaining OS/arch assumptions in `@qtsvg`, `@qtimageformats`, `@qtlanguageserver`, `@qtshadertools`, `@qtdeclarative`, and subsequent Phase 2 modules so the full test suite cross-builds across all 6 platform configs.

---

## 5. Resolved Design Decisions

1. **Windows Target ABI (`@llvm//platforms:windows_{amd64,arm64}`)**:
   - Standardized on `@llvm//platforms:windows_{amd64,arm64}` (MinGW-w64 v14 + UCRT + `libc++`), avoiding Microsoft EULA environment variable requirements and keeping GNU/Clang flag syntax uniform across Linux, macOS, and Windows.
2. **Linux glibc Baseline (`@llvm//platforms:linux_{amd64,arm64}_gnu.2.39`)**:
   - Standardized on `gnu.2.39` for both `amd64` and `arm64` (`@llvm//platforms:linux_{amd64,arm64}_gnu.2.39`).
3. **macOS Minimum Deployment Target (`--macos_minimum_os=14.0`)**:
   - Pinned `--macos_minimum_os=14.0` in `.bazelrc` for `@llvm//platforms:macos_{amd64,arm64}`, matching `@llvm`'s default and satisfying Qt 6.10 `cocoa`'s `< 15.0` requirement.
4. **Windows Subsystem & `Qt6EntryPoint` (`win_subsystem = "console"` default)**:
   - `qt_cc_binary` and `qt_cc_test` default to the Windows console subsystem (standard `main()`, preserving stdout/stderr for logs and test runners), with an opt-in `win_subsystem = "windows"` attribute on `qt_cc_binary` that links `@qtbase//:entrypoint` (`Qt6EntryPoint` + `-lmingw32` + `QT_NEEDS_QMAIN`).

