# PHASE 4: Upstream Parity & Completeness (Qt 6.10.2)

## 1. Executive Summary

Phases 1–3 delivered a hermetic, cross-platform Bazel build of Qt 6.10.2 that builds and tests on
6 native hosts and 30 host→target cross pairs. CI passing tells us that what we build compiles
and that our smoke tests run under the `offscreen` QPA. It does **not** tell us that we build
what upstream Qt builds, or that a real application behaves the same as with an official Qt.

To quantify that gap we surveyed every module's CMake build (`CMakeLists.txt`,
`configure.cmake`, `qt_internal_extend_target(... CONDITION ...)` branches) against our
overlays and the shared macros in `asymingt/qt_bazel`, and cross-referenced the result with
downstream migration feedback from the ROS Central Registry ([FEEDBACK1.md](FEEDBACK1.md)).

**Headline results**

- **Sources are rarely the problem.** For targets we do build, almost every source file CMake
  lists (including conditional branches) is present. Script-based diffs found only a handful of
  justified exclusions.
- **Missing targets and runtime wiring are the problem.** Whole modules, plugins and QML
  imports are absent (qtdeclarative: 18/96 targets; qtbase: 33/72), and several pieces that
  *are* built are wired so they fail at runtime:
  - QML plugins register no types.
  - qmldir files lack imports.
  - Linux defaults to the `offscreen` platform.
  - X11 has no OpenGL.
- **Some defects affect every consumer today**:
  - QtGui is compiled for AVX2: SIGILL on older x86_64 CPUs and under Rosetta 2.
  - JPEG and GIF images cannot be loaded.
  - A default `import QtQuick.Controls` app should fail on all three desktop OSes.
  - The public `qt_cc_library` macro leaks Qt-internal defines into user code.
- Most gaps trace back to a **small number of shared root causes** in `qt.bzl` /
  `qt_tools.bzl` / `qt_internal_library.bzl` (qmldir generation, QML plugin registration,
  static-only plugins with no auto-import, global flags). Fixing those first unblocks a large
  share of the module-level work.

Phase 4's goal: **reach functional parity with a default upstream CMake build on Linux, macOS
and Windows for the modules we ship, and prove it with runtime tests rather than build
success.**

> [!IMPORTANT]
> All findings come from static analysis (reading/grepping CMake and Bazel files, plus
> source-list diff scripts). Nothing in the survey was build- or runtime-verified. Items marked
> **(?)** are inferences. Milestone 4.0 starts by converting the High findings into failing tests.

---

## 2. Scope, Method & Sources

**In scope**: Linux (x86_64/aarch64, glibc 2.34), macOS (x86_64/arm64), Windows (x86_64/arm64,
MinGW-UCRT clang/lld via `@llvm`), all 36 Qt modules under `modules/`, and the shared rules in
`asymingt/qt_bazel` (`qt.bzl`, `qt_tools.bzl`, `qt_internal_library.bzl`, `qt_wayland.bzl`,
`qt_protobuf.bzl`).

**Out of scope**: iOS, Android, WASM, QNX/INTEGRITY/VxWorks, MSVC ABI, and QtWebEngine (not in
the workspace).

**Method**: nine parallel surveys, each using the same rubric:

1. Inventory the CMake targets.
2. Map each to a Bazel target and classify it (present / partial / missing / N/A).
3. Diff source lists by condition.
4. Compare `qt_feature` defaults to our generated config headers.
5. Check other deviations: flags, qmldir/qmltypes, plugin metadata, linking, runtime layout.

Full reports (with file/line citations) are checked in under [phase4_survey/](phase4_survey/):

| Report | Modules |
|---|---|
| [qtbase.md](phase4_survey/qtbase.md) | qtbase (incl. OpenGL and FEEDBACK1 cross-check) |
| [qtdeclarative.md](phase4_survey/qtdeclarative.md) | qtdeclarative (incl. FEEDBACK1 §6a cross-check) |
| [graphics3d.md](phase4_survey/graphics3d.md) | qtquick3d, qtquick3dphysics, qtquicktimeline, qtshadertools, qtlottie, qt3d, qtdatavis3d, qtgraphs, qtcharts |
| [tools_formats.md](phase4_survey/tools_formats.md) | qttools, qttranslations, qtlanguageserver, qtsvg, qtimageformats, qt5compat |
| [media_web.md](phase4_survey/media_web.md) | qtmultimedia, qtspeech, qtwebview, qtwebchannel, qtwebsockets, qthttpserver, qtnetworkauth |
| [io_connectivity.md](phase4_survey/io_connectivity.md) | qtgrpc, qtremoteobjects, qtscxml, qtserialport, qtserialbus, qtsensors, qtpositioning, qtlocation, qtconnectivity |
| [wayland_vkb.md](phase4_survey/wayland_vkb.md) | qtwayland, qtvirtualkeyboard |
| [cross_cutting.md](phase4_survey/cross_cutting.md) | build-wide flags, linking/ABI, platform coverage, plugin system, 3rd-party deps, tooling, deployment |

The source-diff helpers used by the surveys are kept alongside the reports
([srcdiff.py](phase4_survey/srcdiff.py), [qtbase_cmp.py](phase4_survey/qtbase_cmp.py)); see
§7 for turning them into a standing parity check.

---

## 3. Coverage Summary

"Present" counts in-scope CMake targets that have a Bazel equivalent; "partial" means present
but functionally incomplete.

| Module | Present / in-scope | Partial | Assessment |
|---|---|---|---|
| qtbase | 33 / 72 | 2 | Core libs complete. Many plugins missing (image formats, styles, print, input methods, themes, network info, embedded QPA) |
| qtdeclarative | 18 / 96 | 4 | QtQml/QtQuick/Controls.Basic only. No other styles, Dialogs, QtCore QML, labs, Effects, Particles or tools |
| qtquick3d | 29 / 33 | 1 | Missing QtQuick3D.Xr (default-on upstream) |
| qtquick3dphysics | 6 / 6 | 2 | QML modules register no types |
| qt3d | 30 / 30 | 8 | All 8 QML modules register no types |
| qtgraphs / qtdatavis3d | 3/3, 3/3 | 2, 2 | QML modules register no types; GraphsWidgets breaks at runtime |
| qtcharts, qtquicktimeline, qtshadertools | full | 0 | Match upstream (`qt_qsb` lacks some options) |
| qtlottie | 4 / 7 | 0 | Vector-image generator blocked on qtdeclarative |
| qttools | 24 / 27 | 3 | Missing Assistant; Designer/Linguist reduced |
| qttranslations | 1 / 1 | 1 | `.qm` built but not where `QLibraryInfo` looks |
| qtsvg, qtlanguageserver, qt5compat | full | 0 | Match upstream |
| qtimageformats | 5 / 7 | 0 | Missing macOS HEIF / JPEG 2000 |
| qtmultimedia | 9 / 13 | 2 | No audio or camera on Linux; native backends missing |
| qtspeech | 3 / 7 | 0 | Mock engine only |
| qtwebview | 2 / 3 | 0 | No backend on any platform |
| qtwebchannel, qtwebsockets, qthttpserver, qtnetworkauth | full | 0 | Match upstream |
| qtgrpc, qtremoteobjects, qtscxml, qtserialport, qtlocation | full | 0 | Match upstream (qtlocation builds 3 extra plugins) |
| qtserialbus | 7 / 9 | 0 | Missing Windows CAN backends |
| qtsensors | 5 / 6 | 0 | Missing WinRT; qmltypes possibly incomplete |
| qtpositioning | 9 / 11 | 0 | No CoreLocation (macOS) or WinRT backend |
| qtconnectivity | 2 / 4 | 2 | Bluetooth dummy on Linux/Windows; NFC no-op everywhere |
| qtwayland | 18 / 21 | 2 | Missing dmabuf; texture-sharing QML modules lack qmldir |
| qtvirtualkeyboard | 14 / 15 | 5 | CJK/Thai input and styles plugin likely broken |

---

## 4. Findings by Theme

Each finding carries an ID (`F-…`) used by the milestones in §6. Severity: **H** = a typical app
on Linux/macOS/Windows hits it; **M** = a significant feature or ergonomics gap; **L** = minor.

### 4.1 Shared rule infrastructure (`asymingt/qt_bazel`)

These are root causes. Fixing them unblocks many module-level findings.

| ID | Sev | Finding | Evidence |
|---|---|---|---|
| F-INF-1 | H | **`qt_qml_plugin_cpp` ignores the registration function and emits an empty plugin.** Modules without `qml_args` (qt3d ×8, qtquick3dphysics, qtgraphs, qtdatavis3d) register no QML types, and get no qmldir/qmltypes. `import Qt3D.*`, `QtQuick3D.Physics`, `QtGraphs`, `QtDataVisualization` likely fail. | `qt_tools.bzl:1051-1090`; [graphics3d.md](phase4_survey/graphics3d.md) |
| F-INF-2 | H | **Generated qmldir has no `import`/`depends` lines and no entries for `.qml`-defined types.** Breaks re-exports (VKB `Plugins` → Pinyin/TCIme/Hangul/OpenWNN/Thai, `QtQuick.VirtualKeyboard` → Core/Layouts/Components) and QML-file types (qtlocation `MapView`, VKB `InputPanel`) **(?)**. Also always writes `optional plugin` (ignores `NO_PLUGIN_OPTIONAL`) and omits `linktarget`/`designersupported`. | `qt.bzl:2288-2296`; [wayland_vkb.md](phase4_survey/wayland_vkb.md), [cross_cutting.md](phase4_survey/cross_cutting.md) |
| F-INF-3 | M | **qmlcachegen runs with `--bare`, no `-I` import paths, no resources and no dependency qmltypes**, so AOT compilation effectively falls back to bytecode. QML plugins outside qtdeclarative may lack `-Muri` metadata **(?)**. | `qt_tools.bzl:1304-1313`, `qt_internal_library.bzl:220-229` |
| F-INF-4 | M | **All plugins are static-only and only QPA plugins are auto-added** by `qt_cc_binary`. Image formats, TLS, SQL, multimedia/speech/positioning/sensors/CAN/geoservices backends, Wayland and the scxml ECMAScript datamodel must be listed by hand, or features silently fall back (e.g. `QFallbackIntegration` for multimedia). No `qmlimportscanner` equivalent; plugin maps are hand-maintained and incomplete. | `qt.bzl:2024-2130, 289-367`, `qt_internal_library.bzl:282` |
| F-INF-5 | M | **Hand-written qmldir strings** in overlays (qtdeclarative) instead of generation; risk of drift on version bumps. QtQuick.Controls omits `optional import` for styles. | qtdeclarative `BUILD.bazel:2724` |
| F-INF-6 | M | **Raw string labels** (`"@qtbase//:core_shared"`, `"@qtbase//:uic"`, …) resolve in the *caller's* repo, forcing downstream `bazel_dep`s on qtbase/qtsvg/qtdeclarative. Wrap them in `Label()` and compare user deps as resolved labels. *(FEEDBACK1 §3a)* | `qt.bzl` mapping table (~1.8k raw labels) |
| F-INF-7 | M | **Public `qt_cc_library` adds `QT_BUILDING_QT` and `-w` to user code** (breaks `Q_LOGGING_CATEGORY` linkage in 6.9+, hides warnings). 7 overlays (qtbase, qtconnectivity, qtmultimedia, qtpositioning, qtremoteobjects, qtscxml, qtvirtualkeyboard) rely on the public macro for Qt-internal code and must be switched to set it explicitly. *(FEEDBACK1 §3b)* | `qt.bzl:2356-2367, 2564, 2630` |
| F-INF-8 | M | **`testonly` is not forwarded** to generated `_moc`, `_moc_lib`, `_hdrs_lib`, `_compile_srcs`, `_uic`, `_rcc` targets. *(FEEDBACK1 §2)* | `qt.bzl` (no `testonly` anywhere) |
| F-INF-9 | M | **No `qt_cc_shared_library`**: consumers building their own `cc_shared_library` must hand-list every `*_shared` dep, or Qt is silently linked statically into their `.so` (LGPL risk), or the link fails with duplicate exports. *(FEEDBACK1 §3c)* | — |
| F-INF-10 | M | **Export-header template drops `QT_*_REMOVED_SINCE` / `INLINE_SINCE` handling**, so there's no binary compatibility with official Qt builds. | `qt_tools.bzl:611-637` |
| F-INF-11 | L | `qt_qsb` lacks several `qt6_add_shaders` options (NOGLSL/NOHLSL/NOMSL, tessellation, DEBUGINFO, PRECOMPILE, MEDIUMP, replacements, depfiles). Scenegraph shaders are built without `-O`. | [graphics3d.md](phase4_survey/graphics3d.md) |

### 4.2 Compile flags & ABI

| ID | Sev | Finding | Evidence |
|---|---|---|---|
| F-ABI-1 | **H** | **All of QtGui is compiled with `-march=core-avx2` on x86_64** (Linux/macOS/Windows). SIGILL on CPUs without AVX2 and under Rosetta 2. CMake only uses Haswell flags for `qdrawhelper_avx2.cpp` and dispatches at runtime; qtmultimedia already does per-file flags correctly. | qtbase `BUILD.bazel:2800-2805` (verified) |
| F-ABI-2 | M | **Release-only, warnings-off builds**: `-O2 -DNDEBUG -w` hardcoded in `common_copts`, and `QT_NO_DEBUG` is a *public* define, so `-c dbg` never yields a debug Qt and consumer `Q_ASSERT`s are disabled. | qtbase `BUILD.bazel:51`, `qt.bzl:2344-2352` |
| F-ABI-3 | M | `QT_REDUCE_RELOCATIONS` defined without `-Bsymbolic-functions`; risks the `-fPIC` `#error` for consumers. | `qtbase_config.bzl:15-19` |
| F-ABI-4 | M | No ELF symbol versioning (`Qt_6`, `Qt_6_PRIVATE_API`), generic unversioned sonames (`libcore_shared.so`), no macOS frameworks; Windows uses `--export-all-symbols` with inconsistent visibility. | `qt_internal_library.bzl:7-39, 267-280` |
| F-ABI-5 | L | Mixed C++17 / gnu++20 across modules **(?)**; inconsistent hardening (glibc `_FORTIFY_SOURCE` on all OSes; `-mcx16` missing on Windows); exceptions on where CMake disables them; pcre2 JIT off; hand-rolled moc predefs. | [cross_cutting.md](phase4_survey/cross_cutting.md) |
| F-ABI-6 | L | **Feature headers contradict the build graph**: printsupport, styles, particles, xmllistmodel, designhelpers, embedded QPAs, system_pcre2/harfbuzz claim `1` while the modules aren't built. | multiple `*_config.bzl` |

### 4.3 Third-party linkage & shared-library exports

| ID | Sev | Finding | Evidence |
|---|---|---|---|
| F-DEP-1 | **H** | **`core_shared` exports `@rules_cc//cc/runfiles:runfiles` and `@zlib//zlib-1.3.2:z`**, which claims exclusive ownership and collides with other shared libs (e.g. `librclcpp.so`). *(FEEDBACK1 §1)* | qtbase `BUILD.bazel:1294-1303` (verified) |
| F-DEP-2 | H **(?)** | **Third-party static libs are linked into Qt `.so`s and re-exported** (glib/ICU/zlib/pcre2 from core; expat/fontconfig/freetype/harfbuzz/png from gui; xcb/X11/xkbcommon from `x11_shared`; wayland/libffi from qwayland). System GL/EGL/Wayland drivers load their own copies, so symbol interposition crashes are likely, and downstream builds need `LINKABLE_MORE_THAN_ONCE` patches. *(FEEDBACK1 §4)* | qtbase `BUILD.bazel:1294-1303, 2817-2825, 2427-2436, 4795-4798` |
| F-DEP-3 | M | qtquick3d/qt3d use BCR assimp 6.0.5 instead of Qt's bundled, curated copy; importer behaviour may differ. libtiff/libwebp from BCR is intentional and fine. | [graphics3d.md](phase4_survey/graphics3d.md) |
| F-DEP-4 | M | FFmpeg is statically linked into every app using the multimedia plugin (upstream: swappable shared libs; LGPL relink concern). `:ffmpeg_shared` exists but is unused. | qtmultimedia `BUILD.bazel:1091` |
| F-DEP-5 | L | qtserialport `dlopen`s libudev; positioning bundles 3rdparty libs into its shared lib; ICU on Linux uses stubdata plus a runfiles `.dat` **(?)**. | [io_connectivity.md](phase4_survey/io_connectivity.md), [cross_cutting.md](phase4_survey/cross_cutting.md) |

### 4.4 Linux graphics & desktop integration

| ID | Sev | Finding | Evidence |
|---|---|---|---|
| F-LNX-1 | **H** | **No OpenGL under X11.** `xcb_glx_plugin`, `xcb_glx`, `xcb_egl_plugin` and `egl_x11` are `-1` with `QT_EGL_NO_X11=1`; `gl_integrations/xcb_glx` and `xcb_egl` are never built. `QOpenGLContext`, `QOpenGLWidget` and Qt Quick's GL backend fail under xcb (Qt warns "neither GLX nor EGL are enabled"). With Vulkan also off, Qt Quick on X11 is software-only. *(FEEDBACK1 §5a)* | `qtbase_config.bzl:500-506, 646-647, 686`; `qxcbintegration.cpp:243-251` |
| F-LNX-2 | **H** | **GL/EGL reached via `dlopen(RTLD_LOCAL)` trampolines instead of linking libGL/libEGL.** About 14 `gl*` and 33 `egl*` stubs, **no `glX*`**; Linux `gui`/`opengl` have no GL linkopts (macOS/Windows link `OpenGL`/`opengl32`). Downstream code calling GL/GLX directly fails to link; symbols aren't globally visible. CMake links `OpenGL::GL`/`EGL::EGL` publicly. Possible infinite recursion if `libGL.so.1` is absent **(?)**. *(FEEDBACK1 §5b)* | `qtbase_config.bzl:1239-1590`; qtbase `BUILD.bazel:2741` |
| F-LNX-3 | **H** | **Compiled-in default QPA on Linux is `offscreen`**; `xcb;offscreen` is only set in the `bazel run` env. A deployed Linux app shows no window. Wayland is never auto-linked. | qtbase `BUILD.bazel:2909` (verified); `qt.bzl:2072-2089` |
| F-LNX-4 | M | No compose / IBus input contexts (no dead keys or CJK IME on X11); no xdg-desktop-portal / GTK3 platform themes (native file dialogs, theming); AT-SPI accessibility off. | `qtbase_config.bzl:479, 558`; [qtbase.md](phase4_survey/qtbase.md) |
| F-LNX-5 | M | Vulkan disabled everywhere (QRhi Vulkan backend, `QVulkanWindow`). | `qtbase_config.bzl:516` |
| F-LNX-6 | L | Embedded back-ends missing (eglfs + device integrations, linuxfb, vnc, minimal; libinput/evdev/tslib/tuio). | `qtbase_config.bzl:643-668` |
| F-LNX-7 | — | `opengl` / `openglwidgets` being separate from `widgets` **matches upstream** (`Qt6::OpenGL` and `Qt6::OpenGLWidgets` are separate modules). It's a downstream migration note, not a deviation. *(FEEDBACK1 §5c)* | — |

### 4.5 qtbase modules & plugins

| ID | Sev | Finding |
|---|---|---|
| F-BASE-1 | **H** | **`qjpeg`, `qgif`, `qico` image plugins missing; no libjpeg.** JPEG/GIF/ICO can't be loaded on any platform (features claim `jpeg=1`, `gif=1`). *(verified)* |
| F-BASE-2 | **H** | **No native widget styles**: macOS style and Windows vista/windows11 style plugins missing (`style_*` forced off, `qtbase_config.bzl:804-807`), so Widgets apps get Fusion / classic Windows. |
| F-BASE-3 | **H** | **QtPrintSupport and the CUPS plugin are missing entirely** (feature header still says `printsupport=1`). Also blocks Designer/Linguist printing (F-TOOL-3). |
| F-BASE-4 | M | All QNetworkInformation backends missing; Windows connection monitor is the network-list-manager stub. |
| F-BASE-5 | M | TLS is OpenSSL-only: schannel (Windows) and securetransport (macOS) off; cert-only backend missing. SSPI/GSSAPI off. |
| F-BASE-6 | M | ODBC SQL driver missing (normally built on Windows); SQLite is the only driver. |
| F-BASE-7 | L | Tools missing: qdbuscpp2xml, qtpaths, macdeployqt, windeployqt, qvkgen, tracegen. Minor plugins: DmaBuf server buffer, Direct2D. brotli/zstd/publicsuffix off. |
| F-BASE-8 | ✓ | QtNetwork `http`, `networkdiskcache`, `ssl`, `dtls`, `ocsp`, `socks5`, `localserver`, `dnslookup` are on, with all sources present. *(FEEDBACK1 §6b: no action)* |

### 4.6 QML / Qt Quick (qtdeclarative)

| ID | Sev | Finding |
|---|---|---|
| F-QML-1 | **H** | **A default `import QtQuick.Controls` app should fail on L/M/W.** With no style configured, `QQuickStyle` picks Fusion (Linux), macOS (macOS) or Windows (Windows) (`qquickstyle.cpp:166-177`). None are built, and the Controls qmldir has no optional style imports. *(needs runtime confirmation)* |
| F-QML-2 | **H** | **21 Controls style targets missing**: Material, Universal, Imagine, FluentWinUI3, Fusion, macOS, iOS, Windows, NativeStyle (+impl). *(FEEDBACK1 §6a: Material)* |
| F-QML-3 | **H** | Missing commonly used imports: **QtQuick.Dialogs** (QuickDialogs2, QuickImpl, Utils), **QtCore** QML (`qtqmlcoreplugin`: Settings, StandardPaths), **Qt.labs.platform**, **QtQuick.Effects** (MultiEffect). *(FEEDBACK1 §6a)* |
| F-QML-4 | M | Also missing: QtQuick.Particles, Qt.labs.qmlmodels, Qt.labs.settings, Qt.labs.folderlistmodel, QtQuick.LocalStorage, QtQml.XmlListModel; QtQuick.VectorImage (which blocks qtlottie, F-3D-4). |
| F-QML-5 | M | Developer tools missing: `qml`, `qmllint`, `qmlformat`, `qmlls`, `qmlimportscanner`, `qmlplugindump`, `svgtoqml`, `qmlscene` and ~9 more (only `qmltyperegistrar`/`qmlcachegen` exist). No on-disk `qml/` import tree with qmldir+qmltypes for tooling. |
| F-QML-6 | M | QML debugging/profiling non-functional: 11 qmltooling plugins and PacketProtocol/QmlDebug missing; `qml-profiler`/`qml-preview` forced `-1` while `qml-debug=1`. |
| F-QML-7 | M | `qml-jit` off on Windows x86_64 (upstream on). Native macOS/Windows style features `-1`. |
| F-QML-8 | L | No `qmlplugin`/`modelsplugin`/`workerscriptplugin` targets (works via alwayslink). FEEDBACK1's `qmlmodelsplugin`/`qml_plugins` names don't exist, and `qtquicklayoutsplugin` is really `:qquicklayoutsplugin`. QtQuick sources are globbed, so feature toggles don't affect compilation. Tracepoints stubbed; `RegExpJitTables.h` pre-generated. |

### 4.7 Other modules

**3D & graphics** ([graphics3d.md](phase4_survey/graphics3d.md))

- **F-3D-1 (H)**: QML registration missing in qt3d / qtquick3dphysics / qtgraphs / qtdatavis3d.
  Same root cause as F-INF-1: switch to the `qml_args` + `qt_qml_plugin_gen` pattern used by
  qtquick3d / qtcharts.
- **F-3D-2 (H)**: QtQuick3D.Xr (Quick3DXr, its plugin, BundledOpenXR) is missing; it is
  default-on upstream on L/M/W.
- **F-3D-3 (M)**: `:assetutils` RuntimeLoader doesn't pull in the assimp importer plugin.
  materialeditor/lightmapviewer probably lack Controls/Dialogs plugin deps **(?)**.
- **F-3D-4 (M)**: qtlottie is 4/7. LottieVectorImageGenerator, `qlottievectorimage` and
  `lottietoqml` are blocked on qtdeclarative's QuickVectorImageGenerator.
- **F-3D-5 (L)**: `qmldbg_quick3dprofiler` missing. Hand-written `cc_shared_library`s in
  timeline/lottie/charts add `dbus_shared` on Windows **(?)**. qtgraphs/qtcharts features are
  hard-coded.

**Tools, translations & formats** ([tools_formats.md](phase4_survey/tools_formats.md))

- **F-TOOL-1 (H)**: Assistant is missing (`QT_FEATURE_assistant -1`; upstream default-on).
  Needs qlitehtml/litehtml/gumbo.
- **F-TOOL-2 (M)**: `:help` lacks `@qtbase//:qsqlite`, so QHelpEngine fails at runtime
  without it.
- **F-TOOL-3 (M)**: Linguist and Designer have printing forced off. Designer can't load
  custom-widget plugins (static-only). Linguist doesn't link the QML plugins its form preview
  needs **(?)**.
- **F-TOOL-4 (M)**: All `.qm` files are built into `translations_qm/`, but
  `QLibraryInfo::TranslationsPath` points at `<prefix>/translations`, so standard lookup finds
  nothing. `catalogs.json` isn't generated.
- **F-TOOL-5 (M)**: macOS `qmacheif` / `qmacjp2` image plugins are missing.
- **F-TOOL-6 (L)**: qdoc is always built against LLVM 21 from source. qtlanguageserver lacks
  `-mbig-obj` and has a `QT_NO_AS_CONST` typo.

**Multimedia, speech & web** ([media_web.md](phase4_survey/media_web.md))

- **F-MM-1 (H)**: **No audio on Linux.** `pulseaudio` / `pipewire` are `-1`, so
  QPlatformAudioDevices is a null device list. Cheapest fix: build the pipewire sources, which
  `dlopen` libpipewire and need headers only.
- **F-MM-2 (H)**: **No camera on Linux.** `linux_v4l` is `-1`; it only needs
  `<linux/videodev2.h>`. Also missing: VAAPI, X11 / PipeWire screen capture.
- **F-MM-3 (H)**: QtTextToSpeech ships only the mock engine. Missing: darwin (AVFoundation),
  sapi (Windows, MinGW-compatible), speechd/flite (Linux).
- **F-MM-4 (H)**: QtWebView has no backend anywhere. The macOS WKWebView plugin is feasible.
  The Linux backend needs WebEngine and WebView2 needs MSVC; both are out of scope, and this
  should be documented.
- **F-MM-5 (M)**: The multimedia plugin isn't auto-linked (F-INF-4), and FFmpeg is static
  (F-DEP-4). Native darwin, windows and gstreamer media backends are missing. `avfoundation=-1`
  on macOS. Possibly missing macOS GL `rectsampler_bgra` shaders **(?)**.
- **F-MM-6 (L)**: qtwebview hard-codes `$(GENDIR)/external/qtwebview+`. qtspeech has a
  `QT_NO_AS_CONST` typo. `:qmlwebsockets` is missing from the shared map.

**IO & connectivity** ([io_connectivity.md](phase4_survey/io_connectivity.md))

- **F-IO-1 (H)**: Bluetooth uses the dummy backend on Linux and Windows. BlueZ needs only DBus,
  so it's feasible.
- **F-IO-2 (H)**: No CoreLocation positioning plugin on macOS. WinRT geolocation is missing on
  Windows (MinGW headers uncertain).
- **F-IO-3 (M)**: NFC is a no-op everywhere. Upstream enables PC/SC (macOS/Windows) and neard
  (Linux, DBus only).
- **F-IO-4 (M)**: systeccan / vectorcan Windows CAN plugins are missing. `canbusutil` links no
  CAN plugins.
- **F-IO-5 (M)**: `sensorsquick` passes library labels as `foreign_types`, so its qmltypes may
  be incomplete **(?)**. WinRT sensors plugin is missing.
- **F-IO-6 (L)**: qtlocation builds esri/mapbox/nokia, which upstream disables. qtgrpc's
  `qt_protobuf_gen` has no QML mode.

**Wayland & virtual keyboard** ([wayland_vkb.md](phase4_survey/wayland_vkb.md))

- **F-WL-1 (M)**: linux-dmabuf compositor plugin missing (default-on with EGL + libdrm). The two
  texture-sharing QML modules have no qmldir/URI.
- **F-VKB-1 (H)**: CJK/Thai input is likely broken by F-INF-2 (missing qmldir re-exports).
- **F-VKB-2 (H)**: Builtin styles plugin: `Q_INIT_RESOURCE(virtualkeyboard_*_style)` doesn't
  match Bazel's `vkb_*_style_qrc` names, which is a likely link error. Styles/Builtin plugins are
  compiled without `QT_STATICPLUGIN` / `Q_IMPORT_PLUGIN`, risking a duplicate
  `qt_plugin_instance`. The Styles plugin is never instantiated, so `qtvkbsvg` and the retro
  style break.
- **F-VKB-3 (M)**: No `PAST_MAJOR_VERSIONS`, so `import QtQuick.VirtualKeyboard 2.x` fails.
  Hunspell is missing. On Windows, the platforminputcontext plugin doesn't link `imm32`.

### 4.8 Runtime layout & deployment

| ID | Sev | Finding |
|---|---|---|
| F-RT-1 | M | `QLibraryInfo` paths point inside `_solib`/runfiles; there is no coherent prefix layout (plugins, qml, translations, data). Translations (F-TOOL-4) and ICU data **(?)** depend on it. |
| F-RT-2 | M | No deployment tooling (windeployqt / macdeployqt equivalents), no macOS `.app` bundles, Windows apps are console-subsystem with no `WinMain` entrypoint shim, no CMake/pkg-config files for non-Bazel consumers. |

---

## 5. FEEDBACK1 Cross-Reference

| FEEDBACK1 item | Survey verdict | Phase 4 ID |
|---|---|---|
| §1 `core_shared` exports runfiles/zlib | **Confirmed**; part of a broader re-export problem | F-DEP-1, F-DEP-2 |
| §2 `testonly` not propagated | **Confirmed** | F-INF-8 |
| §3a raw labels in `qt.bzl` | **Confirmed** | F-INF-6 |
| §3b `QT_BUILDING_QT` leaks to consumers | **Confirmed**; 7 overlays depend on it internally | F-INF-7 |
| §3c no shared-library-aware macro | **Confirmed** | F-INF-9 |
| §4 `LINKABLE_MORE_THAN_ONCE` patches downstream | Consequence of F-DEP-2 | F-DEP-2 |
| §5a no XCB GLX/EGL | **Confirmed** | F-LNX-1 |
| §5b incomplete GL stubs, `RTLD_LOCAL` | **Confirmed** | F-LNX-2 |
| §5c opengl/openglwidgets separate | **Matches upstream**; document only | F-LNX-7 |
| §6a QML plugins: Material, Dialogs, labs.platform, QtCore | **Confirmed missing** | F-QML-2, F-QML-3 |
| §6a QtCharts QML, Qt5Compat.GraphicalEffects | **Already present** (`qtchartsqml2`, `qtgraphicaleffects*`) | — |
| §6a `qtquicklayoutsplugin`, `qmlmodelsplugin`, `qml_plugins` | Names don't exist (`:qquicklayoutsplugin`; the others are via alwayslink) | F-QML-8 |
| §6b QtNetwork disk cache / HTTP | **OK**, no action | F-BASE-8 |

---

## 6. Milestones

Ordered by user impact and by dependency (infrastructure before module work). Each milestone
ends with the corresponding runtime tests from §7 passing in CI on all 6 native hosts.

### Milestone 4.0: Correctness blockers & downstream unblockers (small, do first)

1. **F-ABI-1**: replace QtGui's global `-march=core-avx2` with per-file SIMD flags mirroring
   CMake (`qdrawhelper_avx2.cpp`, SSE4.1/SSSE3 files), using qtmultimedia's pattern.
2. **F-LNX-3**: default Linux QPA to `xcb` (matching upstream), keeping `offscreen` for tests
   via env.
3. **F-BASE-1**: add `qjpeg` (libjpeg-turbo from BCR), `qgif`, `qico`, and include them in the
   default image-format plugin set.
4. **F-DEP-1**: drop runfiles/zlib from `core_shared`'s `exports_filter`, and add
   `LINKABLE_MORE_THAN_ONCE` overrides in this repo so our own builds keep working.
5. **F-INF-8, F-INF-6, F-INF-7**: `testonly` propagation, `Label()`-wrapped labels, and
   removing `QT_BUILDING_QT`/`-w` from the public macro (switching the 7 internal overlays to
   explicit defines).
6. **F-ABI-2**: make optimization/NDEBUG follow `-c opt|dbg`; make `QT_NO_DEBUG` follow the
   compilation mode rather than always being public.
7. **F-VKB-2**: fix the VKB resource-name mismatch and the static-plugin macros.
8. Re-release `qt_bazel` and notify the ROS Central Registry.

### Milestone 4.1: QML infrastructure (`qt_bazel`)

1. **F-INF-1**: make `qt_qml_plugin_cpp` honour the registration function; migrate qt3d,
   qtquick3dphysics, qtgraphs and qtdatavis3d to `qml_args` (fixes F-3D-1).
2. **F-INF-2 / F-INF-5**: generate qmldir from `qt_qml_module` metadata, including:
   - `import`/`depends`/`optional import`;
   - QML-file type entries;
   - `linktarget`, `designersupported`, `PAST_MAJOR_VERSIONS`, `NO_PLUGIN_OPTIONAL`.

   Then replace hand-written qmldir strings (fixes F-VKB-1, F-VKB-3, F-WL-1 qmldir).
3. **F-INF-3**: pass import paths, resources and dependency qmltypes to qmlcachegen; add `-Muri`
   to QML plugin moc.
4. Produce an on-disk `qml/` import tree (qmldir + qmltypes) as a build output for tooling.

### Milestone 4.2: Linux graphics stack

1. **F-LNX-1**: enable and build `qxcb-glx-integration` and `qxcb-egl-integration`
   (`QT_FEATURE_xcb_glx*`, `egl_x11`, drop `QT_EGL_NO_X11`), auto-linked with `:xcb`.
2. **F-LNX-2**: replace the `RTLD_LOCAL` trampolines with real linkage against
   libglvnd-compatible stub libraries (`libGL.so.1` / `libGLX.so.0` / `libOpenGL.so.0` /
   `libEGL.so.1`, generated from Khronos registries, with the matching `DT_NEEDED`), exported to
   consumers as upstream does. See D-3.
3. **F-DEP-2**: stop re-exporting third-party libraries from Qt `.so`s (link privately or via
   shared wrappers), and validate GLX/EGL/Wayland interop under real drivers.
4. **F-LNX-5**: enable Vulkan (headers-only + loader `dlopen`, as upstream does).
5. Auto-link Wayland alongside xcb on Linux.

### Milestone 4.3: Qt Quick completeness (qtdeclarative)

1. **F-QML-1 / F-QML-2**: Fusion, Material, Universal, Imagine, FluentWinUI3, plus the native
   macOS/iOS/Windows styles and NativeStyle. Add optional style imports to QtQuick.Controls.
2. **F-QML-3**: QtQuick.Dialogs, QtCore QML, Qt.labs.platform, QtQuick.Effects.
3. **F-QML-4**: Particles, labs (qmlmodels/settings/folderlistmodel), LocalStorage,
   XmlListModel, VectorImage (unblocks F-3D-4).
4. **F-QML-7**: `qml-jit` on Windows x86_64; fix contradictory feature headers (F-ABI-6).
5. **F-QML-5 / F-QML-6**: developer tools (`qml`, `qmllint`, `qmlformat`, `qmlls`,
   `qmlimportscanner`, `svgtoqml`) and qmltooling/debug plugins.

### Milestone 4.4: qtbase plugins & desktop integration

1. **F-BASE-2**: macOS and Windows (vista/windows11) widget styles.
2. **F-BASE-3**: QtPrintSupport and the CUPS plugin (then re-enable printing in Designer and
   Linguist, F-TOOL-3).
3. **F-LNX-4**: compose/IBus input contexts, xdg-desktop-portal/GTK3 themes, AT-SPI.
4. **F-BASE-4 / F-BASE-5 / F-BASE-6**: NetworkInformation backends, schannel/securetransport
   TLS, SSPI/GSSAPI, ODBC.

### Milestone 4.5: Module backends

Each item is independent and can be parallelised:
- **multimedia**: F-MM-1 (pipewire audio), F-MM-2 (V4L2 camera, screen capture), F-MM-5
  (native backends).
- **speech, web & positioning**: F-MM-3 (darwin/sapi/speechd/flite TTS), F-MM-4 (WKWebView),
  F-IO-2 (CoreLocation, WinRT).
- **connectivity & devices**: F-IO-1 / F-IO-3 (BlueZ, neard, PC/SC), F-IO-4 (Windows CAN),
  F-IO-5 (sensors).
- **3D & tools**: F-3D-2 (QtQuick3D.Xr), F-3D-3, F-TOOL-1 (Assistant), F-TOOL-2, F-TOOL-5
  (macOS image formats).

### Milestone 4.6: Plugin & consumer ergonomics

1. **F-INF-4**: an import-scanner-style mechanism (or per-module default plugin sets) so
   binaries automatically link the plugins their deps imply: image formats, TLS, SQL,
   multimedia/speech/positioning/sensors backends, Wayland, QML plugins. Complete the shared maps.
2. **F-INF-9**: `qt_cc_shared_library` macro and an exported `QT_SHARED_LIBS` helper.
3. **F-DEP-4**: link FFmpeg via `:ffmpeg_shared` for LGPL compliance.

### Milestone 4.7: ABI, runtime layout & deployment

1. **F-ABI-3 / F-ABI-4**: `-Bsymbolic-functions`, ELF symbol versioning, proper sonames,
   consistent visibility (no `--export-all-symbols`).
2. **F-INF-10**: restore `REMOVED_SINCE`/`INLINE_SINCE` handling in export headers.
3. **F-RT-1 / F-TOOL-4**: define a prefix layout (plugins/qml/translations/data) and point
   `QLibraryInfo` at it; deploy `.qm` files accordingly.
4. **F-RT-2**: deployment rules (bundle/zip with plugins + qml + translations), macOS `.app`,
   Windows GUI subsystem/entrypoint.

---

## 7. Verification Strategy

Build success has proven insufficient. Phase 4 adds tests that exercise runtime behaviour,
written **before** the fix (red → green):

| Test | Catches | Notes |
|---|---|---|
| `qml_controls_default_style_test`: load `import QtQuick.Controls; ApplicationWindow {}` with no style set | F-QML-1/2 | All platforms |
| `qml_imports_test`: a matrix of every shipped QML URI (`import X; Item {}`), incl. Dialogs, QtCore, Effects, Qt3D.*, QtGraphs, VKB layouts | F-INF-1/2, F-QML-3, F-3D-1, F-VKB-1 | Generated from the module list |
| `image_formats_test`: `QImageReader::supportedImageFormats()` ⊇ {png, jpeg, gif, ico, svg, webp, tiff, …} and decode samples | F-BASE-1, F-TOOL-5 | |
| `gl_context_test`: create a `QOpenGLContext` + `QOpenGLWidget` under Xvfb with Mesa (xcb), and natively on macOS/Windows | F-LNX-1/2/3 | Needs Xvfb + Mesa on Linux CI |
| `gl_direct_call_test`: consumer library calling `glClear`/`glXGetCurrentContext` links and runs | F-LNX-2 | Mirrors rviz/octovis usage |
| `no_avx2_test`: run a Gui test under an emulated non-AVX2 CPU (e.g. `qemu-x86_64 -cpu Nehalem`) or check the binary with an ISA scanner | F-ABI-1 | |
| `default_qpa_test`: assert `QGuiApplication::platformName()` is `xcb`/`cocoa`/`windows` when `QT_QPA_PLATFORM` is unset (under Xvfb) | F-LNX-3 | |
| `plugin_autolink_test`: binary depending only on `@qtmultimedia//:multimedia` gets a non-fallback backend | F-INF-4 | |
| `downstream_consumer_test`: a separate module with **no** `bazel_dep` on qtbase/qtsvg/qtdeclarative uses `qt_cc_binary`, `qt_cc_test(testonly deps)`, `qt_cc_shared_library`; links next to another `cc_shared_library` exporting runfiles/zlib | F-INF-6/7/8/9, F-DEP-1 | Reproduces the FEEDBACK1 setup |
| **Parity check** (`tools/parity/`): promote `srcdiff.py` / `qtbase_cmp.py` into a script that diffs CMake target/source/feature inventories against overlays, with an allowlist of intentional deviations | regressions, version bumps | Run in CI; output becomes the living coverage table in §3 |

Linux CI additionally needs a real display path (Xvfb + Mesa llvmpipe) for the GL/QPA tests.

---

## 8. Design Decisions to Resolve

| ID | Question | Options / recommendation |
|---|---|---|
| D-1 | **Release/versioning of `qt_bazel`** now that a downstream registry depends on it | Keep re-releasing `6.10.2` in place (current policy), or move to `6.10.2.N` patch tags. **Recommend `6.10.2.N`** from M4.0 onward: in-place re-tags break pinned integrity hashes downstream. |
| D-2 | **Static vs dynamic plugins** | Keep static (hermetic, simple) with auto-linking (M4.6), or also emit loadable plugins for Designer/custom-widget and app-extensible use cases. Recommend static + auto-link now; dynamic later as opt-in. |
| D-3 | **Linux GL linkage model** | (a) libglvnd-style stub `.so`s with correct sonames (link-time only, real libs at runtime); (b) dlopen-based loader with `RTLD_GLOBAL` and full GL/GLX tables. **Recommend (a)**: it matches upstream's `OpenGL::GL` link semantics for consumers. |
| D-4 | **Bundled vs BCR third-party libs** (assimp, libjpeg, harfbuzz, pcre2, …) | Prefer BCR where it is a superset and behaviour matches; use Qt's bundled copy where Qt curates it (assimp). Decide per library and record it in the parity allowlist. |
| D-5 | **Scope exclusions** | Document as unsupported: QtWebEngine (and thus the Linux WebView backend), WebView2 (MSVC-only), the MSVC ABI, eglfs/embedded (unless requested). |
| D-6 | **Debug builds** | Support `-c dbg` for Qt itself (F-ABI-2) at the cost of a second cache set in CI, or restrict to consumer code. Recommend supporting it, with CI running opt only. |
| D-7 | **ABI compatibility with official Qt binaries** (F-ABI-4, F-INF-10) | Required only if mixing with prebuilt Qt plugins/apps. Recommend implementing symbol versioning and REMOVED_SINCE (cheap, avoids surprises), but not frameworks/install layout parity. |

---

## Appendix: Survey caveats

- Coverage counts exclude tests, examples, benchmarks, docs, and out-of-scope platforms.
- Several "missing" items may be deliberately excluded. Each needs a keep/drop decision
  recorded in the parity allowlist (§7) rather than being implemented blindly.
- Severity reflects expected impact on a typical desktop application. Downstream consumers
  (e.g. ROS: rviz, gz-gui, octovis) should re-prioritise based on their actual imports and
  plugins.
