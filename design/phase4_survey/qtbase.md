# qtbase 6.10.2: Bazel build vs. Qt's CMake build

Paths used below:
- `UP` = `/Users/simmers/Library/Caches/bazel/_bazel_simmers/223ccc89642a35e03547a0f3440b8160/external/qtbase+`
- `OV` = `/Users/simmers/development/qt_bazel_workspace/modules/qtbase/6.10.2/overlay`
  - `OV/BUILD.bazel` (5389 lines)
  - `OV/qtbase_config.bzl` (1597 lines)
- `QB` = `/Users/simmers/development/qt_bazel` (`qt.bzl`, `qt_internal_library.bzl`)

**How this was checked:**
- Read-only; Bazel was not run.
- Source coverage comes from a script. It parsed every non-test `CMakeLists.txt` under `UP/src`.
- It collected `SOURCES` for every `qt_internal_add_*` call and every `qt_internal_extend_target(... CONDITION ...)` block.
- It then compared them with every `.c`/`.cc`/`.cpp`/`.mm` path string in `OV/BUILD.bazel`.
- Script and output are in `/Users/simmers/.gemini/jetski/brain/5328e8b3-05ea-4d49-b79c-6cffd4edff75/scratch/`, files `cmp.py` and `cmp.txt`.

## qtbase

**Coverage**: 33 of 72 in-scope CMake targets are present (2 of those are partial).
- Present: 29 targets fully present, 2 partial (Gui, offscreen), and 2 replaced by external BCR dependencies.
- Missing: 39 targets.

"In-scope" excludes two kinds of target, both listed as N/A in the tables:
- targets for out-of-scope platforms;
- targets whose feature is OFF by default in CMake on all of Linux, macOS and Windows.

### Target inventory

**Modules / libraries**

| CMake target | kind | gating | Bazel target | status | notes |
|---|---|---|---|---|---|
| Core | module | always | `:core` / `:core_shared` (BUILD 953) | present | see the sources and features sections |
| Concurrent | module | `QT_FEATURE_concurrent` | `:concurrent` (1558) | present | |
| Sql | module | `QT_FEATURE_sql` | `:sql` (1755) | present | |
| Network | module | `QT_FEATURE_network` | `:network` (1917) | present | some features are off; see below |
| Xml | module | `QT_FEATURE_xml` | `:xml` (1640) | present | |
| DBus | module | `QT_FEATURE_dbus` | `:dbus` (2271) | present | libdbus is linked on Linux and loaded at runtime (dlopen) on macOS/Windows |
| Gui | module | `QT_FEATURE_gui` | `:gui` (2468) | **partial** | missing: Vulkan, AT-SPI, GLX convenience code, frame capture |
| OpenGL | module | `QT_FEATURE_opengl` | `:opengl` (3071) | present | sources are globbed; ES2 and compositor files excluded |
| Widgets | module | `QT_FEATURE_widgets` | `:widgets` (3306) | present | no native styles (those are plugins) |
| OpenGLWidgets | module | opengl AND widgets | `:openglwidgets` (3763) | present | a separate module, as in CMake |
| Test | module | testlib | `:test` (3851), alias `:testlib` | present | |
| PrintSupport | module | printsupport (default ON on Linux/macOS/Windows) | — | **missing** | no QPrinter, QPrintDialog or QPrintPreview*. Yet `qconfig_p.h` sets `QT_FEATURE_printsupport 1` (qtbase_config.bzl:221) |
| EntryPointPrivate (+ EntryPointImplementation) | module | WIN32 | `:entrypoint` (4327) | present | Windows-only cc_library |
| TestInternalsPrivate | module | testlib | — | missing | only used by Qt's own tests |
| XcbQpaPrivate | module | xcb | `:xcb_qpa` (4343) | present | |
| WaylandGlobalPrivate | module | wayland | `:wayland_global_private` (4626) | present | |
| WaylandClient | module | wayland_client | `:wayland_client` (4708) | present | no Vulkan sources |
| WlShellIntegrationPrivate | module | wl_shell | `:wl_shell_integration_private` (4901) | present | |
| InputSupportPrivate | module | evdev (ON on Linux) | — | missing | embedded input handling |
| DeviceDiscoverySupportPrivate | module | NOT WASM | — | missing | |
| FbSupportPrivate | module | NOT WASM | — | missing | needed by the linuxfb and vnc platforms |
| KmsSupportPrivate | module | kms (on when libdrm is found) | — | missing | Bazel forces kms off (-1) |
| EglFSDeviceIntegrationPrivate | module | eglfs | — | missing | Bazel forces eglfs off (-1) |
| EglFsKmsSupportPrivate, EglFsKmsGbmSupportPrivate | module | eglfs_kms / eglfs_gbm | — | missing | counted as 2 targets |
| ExampleIconsPrivate, ExamplesAssetDownloaderPrivate | module | examples | — | N/A | |
| BundledPcre2 | 3rdparty | NOT system_pcre2 | `:qt_pcre2` (432) | present | the config header claims `system_pcre2 1` (qtbase_config.bzl:453) |
| BundledZLIB | 3rdparty | NOT system_zlib | `@zlib` | replaced | zlib symbols are exported from libcore; see Feedback cross-check |
| BundledLibpng | 3rdparty | NOT system_png | `@libpng` | replaced | |
| BundledFreetype, BundledHarfbuzz | 3rdparty | — | `@freetype`, `@harfbuzz` | replaced | config says `system_harfbuzz -1` (qtbase_config.bzl:673), but the external library is linked |
| BundledLibjpeg | 3rdparty | jpeg AND NOT system_jpeg | — | **missing** | there is no libjpeg anywhere in the build |

**Plugins**

| CMake target | kind | default gating | Bazel target | status | notes |
|---|---|---|---|---|---|
| QXcbIntegrationPlugin | platform | xcb | `:xcb` (4472) | present | static plugin |
| QXcbGlxIntegrationPlugin | xcbglintegrations | ON on Linux | — | **missing** | means no OpenGL on X11 |
| QXcbEglIntegrationPlugin | xcbglintegrations | ON on Linux | — | **missing** | same |
| QOffscreenIntegrationPlugin | platform | always | `:offscreen` (3980) | **partial** | `qoffscreenintegration_x11.cpp` (offscreen GL via GLX) is missing |
| QMinimalIntegrationPlugin | platform | always | — | missing | |
| QMinimalEglIntegrationPlugin | platform | eglfs | — | missing | |
| QLinuxFbIntegrationPlugin | platform | linuxfb (ON on Linux) | — | missing | |
| QVncIntegrationPlugin | platform | vnc (ON on Linux) | — | missing | |
| QEglFSIntegrationPlugin + eglfs_x11, eglfs_kms, eglfs_kms_egldevice, eglfs_emu | platform | eglfs / kms | — | missing | counted as 5 targets. Vendor-specific eglfs variants are N/A |
| QWindowsDirect2DIntegrationPlugin | platform | direct2d (ON on Windows) | — | missing | |
| QCocoaIntegrationPlugin | platform | macOS | `:cocoa` (4039) | present | only the Vulkan instance file is missing |
| QWindowsIntegrationPlugin | platform | WIN32 | `:windows` (4195) | present | only the Vulkan instance file is missing |
| QWaylandIntegrationPlugin | platform | wayland_client | `:qwayland` (4955) | present | |
| QWaylandBradientDecorationPlugin | decoration | — | `:qwayland_bradient_decoration_plugin` | present | |
| xdg-shell, wl-shell, fullscreen-shell-v1 integration plugins | shell | ON | (BUILD 5055, 5113, 5166) | present | counted as 3 targets |
| QWaylandEglClientBufferPlugin | hardware integration | wayland_egl | (BUILD 5214) | present | |
| ShmServerBufferPlugin, DrmEglServerBufferPlugin | hardware integration | ON | (BUILD 5279, 5338) | present | counted as 2 targets |
| DmaBufServerBufferPlugin | hardware integration | ON (client AND opengl) | — | missing | Bazel forces it off (-1) at qtbase_config.bzl:659 |
| Vulkan, Brcm, LibHybris server-buffer plugins | hardware integration | vulkan / vendor | — | N/A | |
| QComposePlatformInputContextPlugin | input context | xkbcommon (ON on Linux) | — | **missing** | no dead keys or compose sequences on X11 |
| QIbusPlatformInputContextPlugin | input context | xkbcommon AND dbus | — | missing | no IBus input method (e.g. for CJK input) |
| QXdgDesktopPortalThemePlugin | platform theme | dbus, Linux | — | **missing** | no portal file dialogs or portal colour scheme |
| QGtk3ThemePlugin | platform theme | gtk3 (when found) | — | missing | |
| QMacStylePlugin | style | macOS | — | **missing** | Widgets apps get the Fusion style instead of the native one |
| QModernWindowsStylePlugin | style | WIN32 | — | **missing** | Widgets apps don't get the windows11 / windowsvista styles |
| QGifPlugin, QICOPlugin, QJpegPlugin | image formats | ON | — | **missing** | counted as 3 targets; GIF, ICO and JPEG images can't be read |
| QTlsBackendOpenSSLPlugin | tls | openssl | `:qopensslbackend` (2188) | present | static plugin |
| QSecureTransportBackendPlugin | tls | APPLE | — | missing | |
| QSchannelBackendPlugin | tls | WIN32 | — | missing | |
| QTlsBackendCertOnlyPlugin | tls | always | — | missing | |
| NetworkManager, glib, Apple, NLM network-information plugins | network information | per OS | — | **missing** | counted as 4 targets |
| QSQLiteDriverPlugin | SQL driver | sqlite | `:qsqlite` (1854) | present | |
| QODBCDriverPlugin | SQL driver | ODBC_FOUND (usually found on Windows) | — | missing | other SQL drivers need vendor client SDKs (N/A) |
| QCupsPrinterSupportPlugin | print support | cups | — | missing | |
| evdev mouse/touch/keyboard/tablet, QTuioTouchPlugin | generic input | evdev (ON on Linux) | — | missing | counted as 5 targets |
| libinput, tslib, vx*, ctf, Android/iOS/wasm/qnx/directfb/vkkhrdisplay plugins | — | off by default or out of scope | — | N/A | |

**Tools**

| CMake target | Bazel target | status | notes |
|---|---|---|---|
| syncqt, Bootstrap, moc, rcc, uic, qlalr | `:syncqt` (356), `:bootstrap` (539), `:moc` (725), `:rcc` (744), `:uic` (780), `:qlalr` (836) | present | |
| qdbusxml2cpp | `:qdbusxml2cpp` (4536) | present | |
| qtwaylandscanner | `:qtwaylandscanner` (4569) | present | |
| qdbuscpp2xml | — | missing | generates D-Bus XML from C++ classes |
| qtpaths | — | missing | |
| tracegen, tracepointgen | — | missing | Bazel uses stub `*_tracepoints_p.h` headers instead (qtbase_config.bzl:832) |
| qvkgen | — | missing | only needed for Vulkan |
| macdeployqt, windeployqt | — | missing | deployment helpers |
| cmake_automoc_parser, androiddeployqt, qmake | — | N/A | |

### Missing/partial sources (grouped by condition)

**`QT_FEATURE_vulkan`** — CMake default ON when Vulkan headers are found; Bazel forces -1 (qtbase_config.bzl:516). Missing files:
- Gui: `rhi/qrhivulkan.cpp` and the 6 `vulkan/*.cpp` files (`gui/CMakeLists.txt:938`)
- xcb: `qxcbvulkan*.cpp`
- Wayland client: `qwaylandvulkan*.cpp`
- cocoa: `qcocoavulkaninstance.mm`
- windows: `qwindowsvulkaninstance.cpp`
- OpenGL: `qvkconvenience.cpp` (excluded at BUILD 3081)

**`accessibility_atspi_bridge`** — CMake default ON with ATSPI2 + DBus; Bazel -1:
- Missing: the 7 files in `gui/accessible/linux/*.cpp` (`gui/CMakeLists.txt:564`).

**`opengl AND xlib`** — GLX support:
- Missing: `opengl/platform/unix/qglxconvenience.cpp` and `offscreen/qoffscreenintegration_x11.cpp`.

**`graphicsframecapture`**:
- Missing: `util/qgraphicsframecapture*`. Low impact.

**Core**:
- Complete for Linux, macOS and Windows, given Bazel's feature set.
- The files not built are only needed for other configurations: qstorageinfo_unix, qcollator_posix, qtimezoneprivate_icu, qpoll, kqueue.

**Network**:
- Only `qnetconmonitor_win.cpp` is missing (Windows network-list-manager support). Windows uses the stub instead (BUILD 2037).
- Sources for every network feature enabled in Bazel are present (BUILD 1959–2045).

**Gui SIMD**:
- All the per-instruction-set source files are present.
- But on x86_64 the whole of Gui is compiled with `-march=core-avx2` (BUILD 2802–2804); see Other deviations.

**OpenGL**:
- Sources are globbed. The ES2 and compositor files are excluded; the compositor is only used by eglfs.

**Test**:
- Only `qtestregistry.cpp` is missing; it is off by default in CMake too.

**Wayland**:
- Complete except Vulkan and the dmabuf, vulkan, brcm and libhybris server-buffer plugins.

### Feature deviations

| feature | CMake default (Linux / macOS / Windows) | Bazel | impact |
|---|---|---|---|
| xcb_glx_plugin, xcb_glx | ON / – / – | -1 (qtbase_config.bzl:501, 646) | no GLX, so no OpenGL on X11 |
| xcb_egl_plugin | ON / – / – | -1 (:647) | no EGL on X11 either |
| egl_x11 / `QT_EGL_NO_X11` | ON when the EGL-with-X11 test passes / – / – | -1, and `QT_EGL_NO_X11` is defined (:506, :686; BUILD 2908) | EGL code is compiled without X11 types |
| vulkan | ON when headers are found | -1 everywhere (:516) | no Vulkan API, no Vulkan RHI backend |
| accessibility_atspi_bridge | ON / – / – | -1 | no accessibility (screen readers) on Linux |
| eglfs, kms, gbm, eglfs_* | ON when found / – / – | -1 (:676–697) | no eglfs/KMS fullscreen platform |
| libinput, mtdev, tslib, libudev | ON when found (Linux) | -1 | embedded input only |
| gtk3 | ON when found (Linux) | -1 (:802) | no GTK3 theme |
| style_mac | – / ON / – | -1 (:804) | non-native Widgets look on macOS |
| style_windowsvista, style_windows11 | – / – / ON | -1 (:806–807) | non-native Widgets look on Windows |
| jpeg, gif, ico | ON | flags set to 1 (:698–702), but **no plugins are built** | images in these formats fail to load |
| printsupport | ON | module not built; flag claims 1 | no printing |
| schannel / securetransport | – / – / ON and – / ON / – | -1 (:887–888) | TLS only via OpenSSL |
| networklistmanager | Windows ON | -1 (:929) | no online-state detection on Windows |
| sspi | Windows ON | -1 (:906) | no Negotiate/Kerberos authentication on Windows |
| gssapi | Linux/macOS ON when found | -1 (:904) | no Kerberos SPNEGO authentication |
| brotli, zstd | ON when found | -1 | network downloads can't decode `br` or `zstd` content encoding |
| publicsuffix_system | ON on Linux | -1 | bundled public-suffix list only |
| winsdkicu | Windows ON if the test passes | -1 | Low; uncertain whether the MinGW test would pass |
| directwritecolrv1 | Windows ON if the test passes | -1 | Low |
| framework | macOS ON | -1 (:32) | no `.framework` bundles |
| version_tagging | Linux ON | -1, plus `QT_NO_VERSION_TAGGING` | no ELF symbol versions |
| cxx20 | OFF | -1, but code is built with `-std=gnu++20` | flag doesn't match the language standard actually used |
| stdlib_libcpp | depends on the toolchain | forced to 1 | wrong if the toolchain uses libstdc++; uncertain |
| wayland_dmabuf_server_buffer | ON (Linux) | -1 | DmaBuf plugin missing |
| linuxfb, vnc, evdev, tuiotouch | ON (Linux) | flags set to 1, plugins not built | flags overstate what is shipped |
| QT_QPA_DEFAULT_PLATFORM | xcb / cocoa / windows | "offscreen" on Linux (BUILD 2909) | see Other deviations |
| ssl, dtls, ocsp, http, networkdiskcache, socks5, localserver, dnslookup | ON | 1 | match |
| sctp, libproxy | OFF | -1 | match |

### OpenGL / GL integration

**1. XCB GL integrations**

What CMake does:
- On Linux it builds two plugins by default: `qxcb-glx-integration` (links XCB::GLX and libdl) and `qxcb-egl-integration` (links EGL::EGL).
- Both are gated in `xcb/gl_integrations/CMakeLists.txt`.

What Bazel does:
- It compiles the integration factory, `qxcbglintegrationfactory.cpp` (BUILD 4347).
- It builds neither plugin.

Runtime effect on X11:
- When an app asks for an OpenGL context, Qt prints "Cannot create platform OpenGL context, neither GLX nor EGL are enabled" and returns nothing (`qxcbintegration.cpp:243–251`).
- As a result, these all fail under xcb: QOpenGLContext, QOpenGLWindow, QOpenGLWidget, and Qt Quick's default OpenGL rendering backend.
- Vulkan is also off, so Qt Quick on X11 can only use software rendering.
- Wayland is fine: the Wayland EGL plugin is built.

Other Linux GL paths:
- missing: eglfs, linuxfb, vnc, minimal, minimalegl;
- offscreen has no GLX support;
- Vulkan is off.

macOS and Windows are fine: the Cocoa and Windows GL context code is built.

**2. libGL/libEGL linkage**

What CMake does:
- Gui links libGL publicly (`WrapOpenGL::WrapOpenGL`, gui/CMakeLists.txt:363–378). That means libGL, or libOpenGL + libGLX on glvnd systems.
- Gui links libEGL privately (`EGL::EGL`, gui/CMakeLists.txt:521–529).
- So anything that links Qt6::Gui on Linux gets libGL on its link line and can call any `gl*` or `glX*` function directly.

What Bazel does:
- It links no GL or EGL library.
- Instead, generated stub code (qtbase_config.bzl:1239–1590, compiled into Gui at BUILD 2741) exports about 14 `gl*` and about 33 `egl*` functions, and no `glX*` functions.
- Each stub opens libGL.so.1 / libEGL.so.1 at runtime with `RTLD_LOCAL`.

Consequences:
- **Link errors.** Downstream code calling any other GL or GLX function directly gets undefined symbols at link time, unless it links libGL itself.
- **Symbols not shared.** Because of `RTLD_LOCAL`, the GL symbols loaded this way are not visible to code loaded later.
- **Symbol conflicts.** libgui exports names like `glGetString` itself, so an app that also links the real libGL/libEGL can get either version depending on link order.
- **Possible infinite recursion (unverified).** If libGL is missing, the fallback lookup can find the stub itself.

Qt's own code mostly resolves GL functions at runtime through QOpenGLFunctions, so Qt itself is less affected.

**3. Module split**

Qt6::OpenGL and Qt6::OpenGLWidgets are separate modules in CMake too (`src/opengl/CMakeLists.txt:8`, `src/openglwidgets/CMakeLists.txt:8`). The Bazel split matches upstream.

### Feedback cross-check

**(1) XCB GL plugins:** confirmed missing. High.

**(2) GL stubs:** confirmed deviation.
- High for downstream code that calls GL directly; Medium otherwise.

**(3) OpenGL / OpenGLWidgets split:** matches upstream.

**(4) libcore exports bundled third-party libraries** (BUILD 1294–1303)
- What is exported: zlib, glib, ICU, pcre2, and the Bazel runfiles library.
- What CMake does: with system zlib (`system_zlib` ON) it links zlib privately (corelib/CMakeLists.txt:355–364), so libQt6Core does not re-export it.
- Without system zlib, CMake links its bundled copy (`ZlibPrivate`, corelib:811). I believe it is built with prefixed symbols, but that is unverified.
- So re-exporting these libraries from libcore is a deviation. It widens libcore's ABI and can clash with an app's own copies.
- Cause: Bazel's `cc_shared_library` requires each static library to have one owning shared library, so this is a structural constraint rather than an oversight.
- Rating: Medium.

**(5) QtNetwork**
- Match CMake defaults (ON in both), with all sources present: http, networkdiskcache, ssl via OpenSSL, dtls, ocsp, networkproxy, socks5, dnslookup, localserver, topleveldomain.
- sctp and libproxy are off in both.
- Off in Bazel but on by default in CMake:
  - TLS backends: schannel (Windows) and securetransport (macOS); certonly is also missing.
  - networklistmanager and sspi on Windows.
  - gssapi on Linux and macOS.
  - brotli and zstd.
  - publicsuffix_system on Linux.
- All 4 network-information plugins are missing.
- The TLS plugin is not added automatically:
  - unless `@qtbase//:qopensslbackend` is listed as a dependency, `QSslSocket::supportsSsl()` returns false;
  - the comment at BUILD 2186–2187 says so.

### Other deviations

**QtGui is compiled for AVX2 on x86_64**
- Gui is built with `-march=core-avx2` (BUILD 2801–2806) on Linux, macOS and Windows.
- CMake only compiles `qdrawhelper_avx2.cpp` with Haswell flags (gui/CMakeLists.txt:658) and picks the code path at runtime.
- Result: the Bazel library will crash with an illegal-instruction error (SIGILL) on CPUs without AVX2 (pre-Haswell, some VMs).
- In addition, `qconfig_p.h` sets all AVX512 features to 1 (qtbase_config.bzl:115–139).

**Default platform plugin on Linux is "offscreen"**
- Linux builds use `QT_QPA_DEFAULT_PLATFORM_NAME="offscreen"` (BUILD 2909); CMake uses xcb.
- In a desktop session this rarely matters: QGuiApplication still prefers wayland or xcb when `WAYLAND_DISPLAY` or `DISPLAY` is set (`qguiapplication.cpp:1509–1531`).
- But a headless deployment renders offscreen silently instead of failing with an error.
- `qt_cc_binary` sets `QT_QPA_PLATFORM` only for `bazel run` (`QB/qt.bzl:2072–2089`).
- The Wayland plugin is never linked automatically, so apps on Wayland run through XWayland.

**Plugins are static only** (`QB/qt_internal_library.bzl:282–410`)
- Plugins are built only as static libraries, registered at link time with `Q_IMPORT_PLUGIN`.
- There are no loadable .so/.dll/.dylib plugins, so none of the normal discovery works: no `plugins/` directory, no `QT_PLUGIN_PATH`, no `qt.conf`.
- Users must list each plugin as a dependency. Only offscreen and the per-OS platform plugin are added automatically (`QB/qt.bzl:2682, 2734`).
- Binary plugins built against a standard Qt can't be loaded.

**Library names and ABI**
- Libraries are named `lib<name>_shared.so` / `.dylib` / `.dll` (`QB/qt_internal_library.bzl:267–272`).
- There is no SONAME, no symbol versioning, and `QT_NO_VERSION_TAGGING` is set. Binaries are not interchangeable with distro or official Qt builds.
- On Windows every symbol is exported, including private ones (`-fvisibility=default` plus `--export-all-symbols`, `QB/qt_internal_library.bzl:7–39`).

**Private headers**
- Private headers are staged per module (e.g. BUILD 104–124), and the raw `src/<module>/**` directories are also on the include path.
- That exposes more than CMake, where private headers are only reachable through `Qt::XPrivate` targets.

**Metatypes**
- Metatypes JSON is collected for downstream use (qt_tools.bzl:43–296).
- qtbase itself doesn't ship `qt6core_metatypes.json` etc. as module artefacts. Unverified whether downstream QML type registration still resolves qtbase types.

**Resources**
- Built-in resources match CMake: PDF colour profile, shaders, style images, cursors, Windows GL blacklist.
- The MIME database is generated by a small custom tool (BUILD 79–84) instead of CMake's script; it should be equivalent.

**Compiler flags**
- All targets compile with `-w`, which hides all warnings.
- `_FORTIFY_SOURCE=3` and `_GLIBCXX_ASSERTIONS` are applied on every OS, including macOS and MinGW (e.g. BUILD 1397–1398). CMake applies them only on glibc.

**X11 libraries**
- libxcb and xkbcommon are wrapped in a separate shared library, `:x11_shared` (BUILD 2426–2437). This is a packaging difference only.

**Install prefix (uncertain)**
- `qconfig.cpp` has an empty install prefix and `"../"` as the path from the library to the prefix (qtbase_config.bzl:236–259).
- In Bazel runfiles the directory above libcore is not a Qt install tree. So the plugin, translation and QML paths that QLibraryInfo reports are likely meaningless.

### Ranked findings

**High**
1. **No OpenGL on X11.** The GLX and EGL integration plugins are missing (qtbase_config.bzl:501, 646–647, 686; BUILD 4343–4470).
2. **Gui on x86_64 is compiled with `-march=core-avx2`** (BUILD 2803). Crashes with SIGILL on CPUs without AVX2.
3. **JPEG, GIF and ICO image plugins are missing,** and there is no libjpeg (cmp.txt:76–78).
4. **No native widget styles on macOS or Windows** (qtbase_config.bzl:804–807).
5. **PrintSupport module and the CUPS plugin are missing** (`printsupport/CMakeLists.txt:8`).
6. **GL/EGL trampolines instead of linking libGL/libEGL** (qtbase_config.bzl:1239–1590; BUILD 2741). Downstream direct GL/GLX calls fail to link.

**Medium**
1. Compose and IBus input-method plugins are missing (cmp.txt:12–13). No dead keys or CJK input on X11.
2. XDG portal and GTK3 platform themes are missing (cmp.txt:59–60).
3. All QNetworkInformation backends are missing. On Windows the connection monitor is a stub (BUILD 2037; qtbase_config.bzl:929).
4. Vulkan is disabled everywhere (qtbase_config.bzl:516).
5. AT-SPI accessibility is disabled on Linux (qtbase_config.bzl:479, 558).
6. Plugins are static only, with no runtime discovery. TLS, SQLite and Wayland plugins must be added as explicit dependencies (`QB/qt_internal_library.bzl:282`; `QB/qt.bzl:2072`).
7. TLS on macOS and Windows is OpenSSL-only (qtbase_config.bzl:887–888).
8. libcore re-exports zlib, ICU, glib, pcre2 and the Bazel runfiles library (BUILD 1294–1303).
9. The compiled-in default platform plugin on Linux is offscreen, and Wayland is not linked automatically (BUILD 2909; `QB/qt.bzl:2074`).
10. SSPI (Windows) and GSSAPI (Linux/macOS) are off (qtbase_config.bzl:904–907).
11. The ODBC SQL driver is missing (normally built on Windows).

**Low**
1. Embedded-Linux back-ends are missing (eglfs, linuxfb, vnc, minimal, evdev, tuio, and their support modules), while their feature flags say 1 (qtbase_config.bzl:643–668).
2. Tools are missing: qdbuscpp2xml, qtpaths, macdeployqt, windeployqt, qvkgen, tracegen.
3. No SONAME, symbol versioning or framework bundles; non-standard library names; Windows exports every symbol (`QB/qt_internal_library.bzl:7–39, 267–272`).
4. brotli, zstd, publicsuffix_system, directwritecolrv1, winsdkicu and graphicsframecapture are off.
5. Config headers don't match what is built: printsupport, system_pcre2, system_harfbuzz, stdlib_libcpp, cxx20.
6. `-w` on all targets, and glibc-specific hardening defines on all platforms.
7. Missing minor plugins: DmaBuf server buffer, Direct2D platform, cert-only TLS. Offscreen lacks GLX.
