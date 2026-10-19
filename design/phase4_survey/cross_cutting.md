# Cross-cutting survey: Bazel Qt 6.10.2 vs Qt's own CMake build

This was read-only research. Bazel was never run, so every point comes from reading the source. Points marked **(?)** are inferences I have not checked. Paths are shortened as follows:

- `QB` = `/Users/simmers/development/qt_bazel`
- `WS` = `/Users/simmers/development/qt_bazel_workspace`
- `UP` = `.../external/<module>+` (the upstream sources)
- `LLVM` = `.../external/llvm+` (the toolchain)

---

## 1. Compile flags and defines

### 1a. Flags on Qt's own code

CMake applies the same flags to every module through `PlatformCommonInternal`, `PlatformModuleInternal` and `PlatformPluginInternal` (`UP/qtbase/cmake/QtInternalTargets.cmake`). Bazel has no such central layer. Each overlay copy-pastes its own `copts`/`local_defines` list, so modules differ from each other.

There are four separate macro paths in Bazel:
- `qt_internal_library` (`QB/qt_internal_library.bzl`): adds only `QT_PLATFORM_COPTS`/`QT_PLATFORM_DEFINES`, and only on Windows (L7-31).
- `qt_cc_library` rule (`QB/qt_tools.bzl:1678`).
- `qt_qml_module` (`QB/qt.bzl:2247`): adds only `-w` and `QT_BUILDING_QT`.
- `common_copts` (`QB/qt.bzl:2344`): `-w -fPIC -O2 -DNDEBUG -DQT_NO_CAST_TO_ASCII -DQT_BUILDING_QT`.

| Aspect | CMake (default release build) | Bazel | Impact |
|---|---|---|---|
| Global internal defines: `QT_NO_JAVA_STYLE_ITERATORS`, `QT_NO_QASCONST`, `QT_NO_QEXCHANGE`, `QT_NO_QSNPRINTF`, `QT_NO_NARROWING_CONVERSIONS_IN_CONNECT`, `QT_EXPLICIT_QFILE_CONSTRUCTION_FROM_PATH`, `QT_NO_FOREACH`; plus `QT_USE_QSTRINGBUILDER` and `QT_NO_STD_FORMAT_SUPPORT` for modules/plugins/tools (QtInternalTargets.cmake ~L190-205) | Hand-copied into qtbase core/gui (`WS/modules/qtbase/6.10.2/overlay/BUILD.bazel:919-944`, `1374-1400`) and many overlays. **Missing** on every `qt_qml_module` target (qtlocation, qtpositioning, qtlottie, qtquicktimeline, qtremoteobjects, qtscxml, qtvirtualkeyboard, qtconnectivity). `QT_NO_STD_FORMAT_SUPPORT` is missing everywhere except qtquick3d | Low. These mostly affect source hygiene and inlining |
| `QT_NO_DEBUG` (internal) | Non-Debug configs only (`$<NOT:$<CONFIG:Debug>>`, L221) | Always on, in the public `common_defines` (`qtbase/BUILD.bazel:51`) and in most overlays | Medium (see §8) |
| Optimisation | Comes from `CMAKE_BUILD_TYPE` | Hardcoded `-O2 -DNDEBUG` in `common_copts` and in core/gui copts (`qtbase/BUILD.bazel:1277`, `2794`). Target copts come after toolchain/`--copt` flags, so they override `-c dbg` and `--config=asan`'s `--copt -O0` | Medium. Debug and sanitizer builds of Qt stay optimised, and NDEBUG is always set |
| C++ standard | C++17 unless the `cxx20` feature is on (it is off by default) (`QtFlagHandlingHelpers.cmake:356-371`); `CXX_EXTENSIONS` off; consumers are required to use C++17 | Toolchain default is `-std=c++17` (`LLVM/toolchain/features/legacy/BUILD.bazel:27`). qtbase/qt3d/qttools/qtwayland and others add `-std=gnu++20`. `qt_qml_module` and plain `qt_internal_library` targets build as C++17. `qconfig.h` says `QT_FEATURE_cxx20 -1` (`qtbase_config.bzl:33`) | Low-Medium. Mixed dialect, so inline code that depends on `__cplusplus` is compiled differently across TUs, an ODR risk **(?)** |
| Visibility | All modules get `-fvisibility=hidden -fvisibility-inlines-hidden` (`reduce_exports`) | Only some targets (qtbase core/gui/…, qt3d, qtdatavis3d, qtgraphs, qtmultimedia, qtquick3d, qtquick3dphysics, qtshadertools). Every `qt_qml_module` and most plain `qt_internal_library` targets use default visibility. On Windows everything is forced to `-fvisibility=default` and `-Wl,--export-all-symbols` (`qt_internal_library.bzl:7-39`) | Medium. Private symbols are exported, so binaries are larger and loading is slower. Windows DLLs may approach the 65 535-export limit **(?)** |
| Exceptions | `-fno-exceptions` plus `QT_NO_EXCEPTIONS` unless a module opts in with `EXCEPTIONS` (`qt_internal_add_exceptions_flags`) | Toolchain default is exceptions **on**. Only gui and a few others pass `-fno-exceptions`. About 19 overlays define `QT_NO_EXCEPTIONS` while still compiling with exceptions | Low (code size) |
| RTTI | On | On | Same |
| SIMD | Baseline ISA (x86-64/SSE2). Per-file `-mavx2` etc. via `qt_internal_add_simd_part`, with runtime dispatch through `qCpuHasFeature` | `qconfig.h` claims compiler support for SSE4.2…AVX-512 and SHA (`qtbase_config.bzl:61-91`). That is fine with target attributes. **But all of QtGui is compiled with `-march=core-avx2` on every x86_64 target** (`qtbase/BUILD.bazel:2800-2805`), including macOS x86_64 and Windows x86_64. qtmultimedia does it correctly per file (`qtmultimedia/BUILD.bazel:161,180`) | **High.** libgui requires Haswell or newer. Expect SIGILL on older CPUs, some VMs and CI runners, and **Rosetta 2**. `qCpuHasFeature` checks can be compiled away |
| `ENABLE_PIXMAN_DRAWHELPERS` | ARM NEON only | Defined on `@qt//:unix_x86_64` (gui `local_defines`) | Low **(?)** |
| Hardening | `-fstack-protector-strong`, `-fstack-clash-protection`, `-D_FORTIFY_SOURCE=3`, `-ftrivial-auto-var-init=pattern` (feature), relro/now, `_GLIBCXX_ASSERTIONS` or `_LIBCPP_HARDENING_MODE` (QtInternalTargets.cmake:400-460); `-mshstk` if `intelcet` | Toolchain: `-fstack-protector`, `-D_FORTIFY_SOURCE=1` (opt), relro/now (`LLVM/toolchain/args/linux/BUILD.bazel:14`). qtbase core/gui/etc. add `-U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=3`. `_FORTIFY_SOURCE=3` is a plain define on all OSes in core (`qtbase/BUILD.bazel:1397`). `-mshstk` is set only on the bootstrap tool lib (`:635`). `_GLIBCXX_ASSERTIONS` is a no-op with libc++. `_LIBCPP_HARDENING_MODE` and `-fstack-clash-protection` are never set, even though `qconfig_p.h` claims them (`qtbase_config.bzl:207-211`) | Low-Medium |
| libc++ | `_LIBCPP_REMOVE_TRANSITIVE_INCLUDES` | Not defined (even though `stdlib_libcpp 1`) | Low |
| macOS | `GL_SILENCE_DEPRECATION` | Not defined | Low |
| Windows (clang/MinGW) | `-Wa,-mbig-obj`, `-Wno-ignored-attributes`, `NOMINMAX`, `_USE_MATH_DEFINES`, **`-mcx16`** | All present except **`-mcx16`** (`qt_internal_library.bzl:7-31`) | Low |
| Warnings | `-Wall -Wextra` (`-Werror` in developer builds) | `-w` everywhere | Low (consumer side in §8) |
| LTO | Off by default | Off | Same |
| Sanitizers | `FEATURE_sanitize_*` | `.bazelrc` configs via `@llvm//config`. Target `-O2` overrides the config's `--copt -O0` | Low |

### 1b. Flags and defines exported to consumers

| Aspect | CMake | Bazel | Impact |
|---|---|---|---|
| `QT_<MODULE>_LIB` | Every module exports it | Exported by qtbase/qtdeclarative/qtquick3d and others. **Not** exported by `qt_qml_module`-based modules (qtlocation, qtpositioning, qtlottie, qtquicktimeline, qtremoteobjects, qtscxml, qtvirtualkeyboard, qtconnectivity) | Low |
| `QT_NO_DEBUG` to consumers | Non-Debug configs only (`UP/qtbase/src/corelib/Qt6CoreConfigExtras.cmake.in:15`) | Always (public `defines = common_defines`, `qtbase/BUILD.bazel:51`) | Medium. User `Q_ASSERT` is off even with `-c dbg` |
| `-fPIC` requirement | INTERFACE `-fPIC` only together with `reduce_relocations` + `-Bsymbolic-functions` (`QtModuleHelpers.cmake:311-325`) | `qconfig.h` defines `QT_REDUCE_RELOCATIONS` on Linux (`qtbase_config.bzl:15-19`), but **no** Qt .so is linked with `-Bsymbolic-functions`, and consumers get no `-fPIC` | Medium. PIE or non-PIC consumer TUs hit the `#error` in `qcompilerdetection.h:1339-1349` **(?)** depending on Bazel's PIC mode. The define buys nothing |
| `QT_SHARED`/`QT_STATIC` | `QT_STATIC` in `qconfig.h` for static builds | Shared implied. Targets with `create_shared=False` (including all `qt_qml_module` libs) export `QT_STATIC_<X>_LIB` (`qt_internal_library.bzl:105-111`), so `Q_X_EXPORT` expands to nothing even when wrapped by a hand-written `cc_shared_library` (e.g. `qtlocation/BUILD.bazel:313`) | Low-Medium. Works through default visibility or export-all, but fragile and loses dllimport |
| `QT_NAMESPACE` | Supported | Not supported | Low |
| Platform defines | `Qt6::Platform` exports Windows UNICODE etc. | Same, plus `_LARGEFILE64_SOURCE`, `_REENTRANT`, `QT_NO_VERSION_TAGGING` and `QT_VERSION_*` leaked to all consumers | Low |

---

## 2. Linking

| Aspect | CMake | Bazel | Impact |
|---|---|---|---|
| Library kind | Shared by default; macOS **frameworks**; plugins are separate shared modules | Each module is a `cc_library` plus a `cc_shared_library` named `<name>_shared` (`qt_internal_library.bzl:257-280`). `qt_cc_binary` maps deps to `*_shared` (`qt.bzl:2008-2022`). **All plugins are static** | See §4 |
| File names / SONAME | `libQt6Core.so.6` SONAME; `Qt6Core.dll`; `QtCore.framework`; install_name `@rpath/...` | `libcore_shared.so`; Windows `lib<name>_shared.dll` (`qt_internal_library.bzl:269-272`). No version suffix and no explicit soname/install_name **(?)** (rules_cc defaults) | Medium. Generic names, not ABI-compatible with system Qt, runfiles-only deployment |
| ELF symbol versioning | `Qt_6` / `Qt_6_PRIVATE_API` version script (`QtFlagHandlingHelpers.cmake:60-143`) | None | Medium. Can't share a process with a system Qt, and private-API use goes unmarked |
| `-Bsymbolic-functions` | Yes on Linux | No | Low-Medium |
| Windows exports | `Q_DECL_EXPORT` plus hidden visibility | `--export-all-symbols`, `--exclude-symbols,__dynamic_cast`, `-fvisibility=default` (`qt_internal_library.bzl:33-39`); whole-archive for Windows tools (`qt_tools.bzl:1803-1813`) | Medium. No MSVC-ABI consumers |
| rpath | `$ORIGIN/../lib` or `@loader_path/../Frameworks` | Bazel `_solib` runfiles rpath (`LLVM/toolchain/features/runtime_library_search_directories/BUILD.bazel:42-43`); Windows copies DLLs next to the exe | Medium. No relocatable install tree |
| Third-party code inside Qt .so | Linux uses system shared libs | BCR libs linked **statically into Qt .so files and re-exported**: core exports glib, ICU, pcre2 and zlib (`qtbase/BUILD.bazel:1294-1303`); gui exports expat, fontconfig, freetype, harfbuzz and libpng (`:2817-2825`); `x11_shared` bundles libxcb, libX11, xau and xkbcommon (`:2427-2436`); qwayland exports wayland_client, wayland_cursor and libffi (`:4795-4798`) | **High (?)** System GL/EGL drivers load the system libxcb, libX11, libwayland, libexpat and libz into the same process. Two copies plus symbol interposition makes crashes in GLX/EGL/Wayland interop likely (FEEDBACK1 §5) |

---

## 3. Platform and architecture coverage

| Aspect | CMake default | Bazel | Impact |
|---|---|---|---|
| Windows compiler | MSVC (primary), MinGW, clang-cl, llvm-mingw | MinGW-style clang/lld only, x86_64 and arm64 | Medium (no MSVC ABI) |
| Linux libc | any | glibc 2.34 pinned (`WS/platforms/BUILD.bazel`) | Low |
| macOS min | 13+ | 14.0 (`.bazelrc`) | Low |
| Linux QPA plugins | xcb, wayland, eglfs(+kms/gbm/x11/emu), linuxfb, vnc, minimal, minimalegl, offscreen | xcb, wayland (+ shells/decorations), offscreen. **No** eglfs, linuxfb, vnc, minimal or minimalegl, despite `linuxfb 1`, `vnc 1` (`qtbase_config.bzl:645,668`) | Medium |
| XCB GL integration | `xcb_glx` and `xcb_egl` plugins | Disabled; GL stub via dlopen `RTLD_LOCAL` (`qtbase_config.bzl:1239-1291`; FEEDBACK1 §5) | High (known) |
| Default QPA name | Linux `xcb` (Wayland preferred in Wayland sessions) | Linux compiled-in default is **`offscreen`** (`qtbase/BUILD.bazel:2909`). `xcb;offscreen` is set only in the `qt_cc_binary` `bazel run` env (`qt.bzl:2072-2089`). Wayland is never auto-linked | **High.** Deployed Linux GUI binaries show no window |
| RHI | GL everywhere; Vulkan if found; Metal; D3D11/12 | GL, Metal, D3D11/12. **Vulkan off everywhere** (`qtbase_config.bzl:516`) | Medium |
| Linux input | libinput/evdev/tslib/mtdev/tuiotouch plugins; compose and ibus input contexts | None built; `libinput/mtdev/libudev -1` (`qtbase_config.bzl:226,678,683`). **No compose or ibus** | **High.** No IME and no dead keys |
| Accessibility | AT-SPI on Linux | `accessibility_atspi_bridge -1` | Medium |
| Platform themes | xdgdesktopportal, gtk3 | None | Medium |
| Widget styles | **macOS (`qmacstyle`) and Windows Vista/11 (`qmodernwindowsstyle`) styles are plugins** | None built | **High** for Widgets look on macOS/Windows |

---

## 4. Plugin system and runtime layout

| Aspect | CMake | Bazel | Impact |
|---|---|---|---|
| Plugin linkage | Dynamic plugins under `<prefix>/plugins`, found via `QT_PLUGIN_PATH`/QLibraryInfo; static builds auto-import defaults | All static `Q_IMPORT_PLUGIN` (`qt_internal_library.bzl:282-410`). Only `offscreen` plus the OS QPA are auto-added (`qt.bzl:2072-2089, 2731-2741`) | Medium-High. Image formats, TLS, SQL, QML, styles and input contexts must be listed by hand, and missing ones fail silently |
| Plugin metadata | `-Muri=<uri>` for QML plugins (`Qt6QmlMacros.cmake:596,2716`) | JSON is auto-globbed. `-Muri` is passed only in qtdeclarative (`qtdeclarative/BUILD.bazel:1803…3382`). Missing in the generic `qt_qml_module` path (`qt.bzl:2323-2342`) and in other modules' QML plugins | Medium **(?)**. Static QML plugins can't be matched by URI |
| qtbase imageformats | `qgif`, `qico`, `qjpeg` | **Not built**, and no libjpeg dependency, despite `gif/jpeg 1` (`qtbase_config.bzl:698-703`) | **High.** No JPEG, GIF or ICO |
| TLS | openssl (runtime), schannel, securetransport, certonly | Only openssl, statically linked (`qtbase/BUILD.bazel:2186-2263`); `openssl_linked -1` in qconfig; `schannel/securetransport -1` (`qtbase_config.bzl:887-889`) | Medium |
| SQL | sqlite plus odbc/psql/mysql when found | sqlite only | Low |
| networkinformation / cups | Built when available | None | Low-Medium |
| QLibraryInfo / qt.conf | Prefix relocatable from libQt6Core | Empty `qt_prfxpath`, `relocatable 1`, `LIBLOCATION_TO_PREFIX "../"` (`qtbase_config.bzl:236-259`), so the prefix lands inside `_solib_*`. Plugins/Qml/Translations paths don't exist. `qt.conf` lookup still works | Medium |
| QML imports | `<prefix>/qml`, qmlimportscanner | Resources at `:/qt-project.org/imports/<uri>` (`qt_tools.bzl:957-997`). No importscanner | Medium |

---

## 5. Third-party dependencies

| Library | CMake default (Linux / macOS / Windows) | Bazel | Impact |
|---|---|---|---|
| zlib | system / SDK / bundled | BCR static, exported from core | Interposition (§2) |
| pcre2 | system or bundled (JIT) | bundled `qt_pcre2` with **`PCRE2_DISABLE_JIT`** (`qtbase/BUILD.bazel:432-481`); qconfig says `system_pcre2 1` | Low-Medium (slower regex) |
| double-conversion, md4c, tinycbor, … | bundled | bundled | Same |
| harfbuzz | bundled | BCR 14.1, exported from gui | Version differs **(?)** |
| freetype / fontconfig / libpng | system (Linux) / bundled | BCR static on all OSes (fontconfig Linux only, `/etc/fonts`) | Interposition |
| libjpeg | system / bundled | **absent** | No JPEG |
| ICU | system on Linux; off on macOS/Windows | Linux: BCR ICU 78 static with **stubdata plus `icudt78l.dat` from runfiles** (`external/icu+/icu4c/source/common/BUILD.bazel:49-80`). Off elsewhere | Medium. Fragile outside runfiles **(?)** |
| glib | Linux if found | BCR 2.82 static, exported | Possible clash with a system glib **(?)** |
| dbus | runtime or linked; all OSes | BCR static, linked; removed on Windows (`qt_internal_library.bzl:262-266`) | Low |
| openssl | system, runtime-loaded | BCR 3.5.8 static | §4 |
| xkbcommon, xcb*, X11, ICE/SM, wayland | system | BCR static, exported | Interposition (§2) |
| libinput, mtdev, tslib, udev, drm, gbm, Vulkan | system when found | absent | §3 |
| EGL/GL | system libs linked | Registry headers plus dlopen stubs | High (FEEDBACK1 §5b) |
| zstd | when found | `zstd -1` | Low |

---

## 6. Tooling parity

| Tool | CMake | Bazel | Impact |
|---|---|---|---|
| moc predefs | Compiler-generated `moc_predefs.h` | Hand list `_MOC_PLATFORM_DEFINES` (`qt_tools.bzl:326-383`): fake `__GNUC__=10`, no `__clang__` or arch macros | Low-Medium **(?)** |
| moc flags | `--output-json`, dep-file, predefs include, warnings shown | `--no-warnings --output-json -f`, `--collect-json` (`qt_tools.bzl:116-248`) | Low |
| syncqt | Also writes the version-script file and runs header checks | `-all -copy` (`qt_tools.bzl:390-506`) | Low |
| Export headers | `modulecppexports.h.in` with `BUILD_REMOVED_API`/`REMOVED_SINCE` | Bazel template always sets `inline` and `REMOVED_SINCE 0` (`qt_tools.bzl:611-637`) | Medium. `removed_api.cpp` (`qtbase/BUILD.bazel:971`) is empty, so there is no BC with official Qt binaries |
| rcc | zstd or zlib | zlib, with `QT_RCC_SOURCE_DATE_OVERRIDE` (`qt_tools.bzl:758`) | Low |
| uic | AUTOUIC | genrule (`qt.bzl:2426-2465`) | Same |
| qmltyperegistrar | Includes `--namespace` | Lacks `--namespace`; minor version defaults to 10 (`qt_tools.bzl:833-897`) | Low |
| qmlcachegen / qmlsc | `-I <imports> -i qmldir --resource <qrc> [--direct-calls]` with dep qmltypes (`Qt6QmlMacros.cmake:3395-3410, 3720-3745`) | Only `--bare --resource-path -i -o`. **No `-I`, no `--resource`, no qmltypes** (`qt_tools.bzl:1269-1379`; `qt_internal_library.bzl:218-230`) | Medium. Effectively no AOT |
| qmldir from `qt_qml_module` | Lists QML-file types plus depends/imports | Only `module`, `optional plugin`, `classname`, `typeinfo`, `prefer` (`qt.bzl:2288-2296`) | **High (?)** `MapView` (`qtlocation/BUILD.bazel:297`) and `InputPanel`/`EnterKey`/`HandwritingInputPanel` (`qtvirtualkeyboard/BUILD.bazel:660`) are not registered |
| qmllint / qmlls | Targets plus `.qmlls.ini` | None | Low-Medium |
| qsb / lrelease | `qt_add_shaders` / `qt_add_translations` | `qt_qsb` / `qt_translation` | Same |

---

## 7. Version metadata

| Aspect | CMake | Bazel | Impact |
|---|---|---|---|
| `QT_VERSION*` | `qconfig.h` | `qconfig.h` **and** public `-D`s (`qtbase/BUILD.bazel:53-56`) | Low |
| `QLibraryInfo` build info | Accurate | Always shared, release and relocatable; plugins are actually static | Low |
| Config files | `Qt6Config.cmake`, `.pc`, qmake/mkspecs, qtpaths | None | Medium. Non-Bazel consumers can't use this Qt |
| Feature macros | Configure-tested | Hand-written. Mismatches: cxx20, system_pcre2, jpeg/gif, linuxfb/vnc/evdev, openssl_linked, stack_clash_protection, reduce_relocations | Medium |

---

## 8. Other systematic differences

| Aspect | CMake | Bazel | Impact |
|---|---|---|---|
| `QT_BUILDING_QT` leaks to consumers | Internal only | The public `qt_cc_library` adds `QT_BUILDING_QT` and `-w` to user code (`qt.bzl:2564, 2630`) | Medium. Changes `Q_DECLARE_LOGGING_CATEGORY`/`Q_LOGGING_CATEGORY` (`qloggingcategory.h:112-135`) and `Q_AUTOTEST_EXPORT`, and hides all user warnings |
| Debug builds | Real debug config | Impossible: hardcoded `-O2`, `NDEBUG` and public `QT_NO_DEBUG` | Medium |
| Translations | `.qm` installed in `TranslationsPath` | `@qttranslations//:qm_files` only | Low-Medium |
| Deployment tools | win/mac/androiddeployqt, deploy scripts | None | Medium |
| App packaging | `WIN32_EXECUTABLE` + Qt6EntryPoint; `MACOSX_BUNDLE` + Info.plist | `@qtbase//:entrypoint` exists (`qtbase/BUILD.bazel:4327-4329`) but is not auto-linked. No `-mwindows`, no .app or Info.plist | Medium. Windows console window; macOS `QPermissions` and activation problems |
| QML debug/profiler | On | `qml_profiler -1`, `qml_preview -1` | Low |

---

## Ranked findings

### High
1. **QtGui is built with `-march=core-avx2` on all x86_64** (`qtbase/BUILD.bazel:2800-2805`). Expect SIGILL without AVX2, including under Rosetta 2. Fix: use per-file SIMD flags, as qtmultimedia does.
2. **Linux default QPA is `offscreen`** (`qtbase/BUILD.bazel:2909`); xcb is set only in the `bazel run` env (`QB/qt.bzl:2072-2089`). Wayland is never auto-linked.
3. **No qgif/qico/qjpeg plugins and no libjpeg**, despite `QT_FEATURE_jpeg/gif 1` (`qtbase_config.bzl:698-703`).
4. **Missing platform plugins:** compose/ibus input contexts (no IME), mac and Windows style plugins, platform themes, AT-SPI.
5. **Static third-party libs re-exported from Qt .so files** clash with the system copies loaded by GL/EGL drivers **(?)** (`qtbase/BUILD.bazel:1294-1303, 2817-2825, 2427-2436, 4795-4798`). Related: XCB GLX/EGL integration missing and `RTLD_LOCAL` GL stubs (FEEDBACK1 §5).
6. **`qt_qml_module` qmldir lacks QML-file type entries** (`QB/qt.bzl:2288-2296`); confirm at runtime **(?)**.

### Medium
7. Public `QT_NO_DEBUG` plus hardcoded `-O2 -DNDEBUG` (`qtbase/BUILD.bazel:51`; `QB/qt.bzl:2344-2352`).
8. `qt_cc_library` leaks `QT_BUILDING_QT` and `-w` to users (`QB/qt.bzl:2564, 2630`).
9. `QT_REDUCE_RELOCATIONS` without `-Bsymbolic-functions` or `-fPIC` export (`qtbase_config.bzl:15-19`).
10. No ELF versioning, generic unversioned sonames, no frameworks (`QB/qt_internal_library.bzl:267-280`).
11. Inconsistent visibility; Windows `--export-all-symbols` (`QB/qt_internal_library.bzl:7-39`).
12. qmlcachegen without import paths, resources or qmltypes (`QB/qt_tools.bzl:1304-1310`); no `-Muri` on generic QML plugins **(?)**.
13. Export-header template drops `REMOVED_SINCE` handling, so no BC with official binaries (`QB/qt_tools.bzl:611-637`).
14. Vulkan off; no eglfs/linuxfb/vnc/minimal QPA; no libinput/evdev/tslib.
15. Static-only plugins with no auto-import; QLibraryInfo paths inside `_solib`; no translations wiring, deployment tools or config files.
16. ICU via stubdata plus a runfiles `.dat` (Linux) **(?)**.
17. MinGW-clang only on Windows; console subsystem, no entrypoint; no macOS bundle.
18. Hand-written feature macros that contradict the build.

### Low
19. Mixed C++17/gnu++20 across Qt modules **(?)**.
20. Weaker or inconsistent hardening; `-mcx16` missing on Windows.
21. Exceptions on where CMake turns them off.
22. Hand-rolled moc predefs and `--no-warnings`.
23. pcre2 JIT off; no zstd.
24. Missing `QT_<X>_LIB` and global defines on `qt_qml_module` targets; pixman define on x86_64.
25. No networkinformation, cups, schannel or securetransport; sqlite is the only SQL driver; no qml profiler.
