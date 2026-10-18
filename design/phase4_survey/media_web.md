# Survey: media & web modules (Qt 6.10.2) — Bazel vs upstream CMake

Paths used below:
- `UP/<m>` = `/Users/simmers/Library/Caches/bazel/_bazel_simmers/223ccc89642a35e03547a0f3440b8160/external/<m>+`
- `OV/<m>` = `/Users/simmers/development/qt_bazel_workspace/modules/<m>/6.10.2/overlay`
- `QT` = `/Users/simmers/development/qt_bazel` (qt.bzl, qt_tools.bzl, qt_internal_library.bzl)

Platforms: L = Linux glibc, M = macOS, W = Windows (MinGW-style clang). Not in scope: Android/iOS/WASM/QNX.

Cross-cutting facts (they affect every module here):
- Every Qt plugin is a **static** `qt_static_plugin`, compiled with `QT_STATICPLUGIN` and pulled in through `Q_IMPORT_PLUGIN` (`QT/qt_internal_library.bzl:282-340`). No loadable plugin `.so`/`.dll` is built, so `QT_PLUGIN_PATH` discovery doesn't work.
- `qt_cc_binary` adds only the QPA plugins automatically (`QT/qt.bzl:2024-2130`, `_needs_gui_plugin`/`_QPA_PLATFORM_CONFIG`). It doesn't add multimedia, texttospeech or webview backend plugins, or the QML import plugins. Users must list them in `deps` themselves.
- qtbase sets `QT_FEATURE_xlib -1` (`OV/qtbase/qtbase_config.bzl:586,619,652`) and `QT_FEATURE_eglfs -1` (`:687`), with `ssl=1` and `http=1` (`:889,894`). Those values drive several gates below.

---

## qtmultimedia
**Coverage**: 9 of 13 in-scope CMake targets present (2 partial).

### Target inventory
| CMake target | kind | gating condition | Bazel target | status | notes |
|---|---|---|---|---|---|
| Multimedia | module | always | `:multimedia` (`OV/qtmultimedia/BUILD.bazel:361-633`) | **partial** | No Linux audio backend: alsa, pulseaudio and pipewire are all off. See below. |
| BundledTLSF | 3rdparty lib | always (`UP/src/CMakeLists.txt:16`) | `:BundledTLSF` (`:115-123`) | present | |
| BundledResonanceAudio (+pffft, SADIE) | 3rdparty lib | `QT_FEATURE_spatialaudio` | `:BundledResonanceAudio`, `:pffft_bundled` (`:801-916`) | present | The `.cc` list matches CMake exactly (diffed). Uses `@eigen` instead of the bundled eigen. |
| SpatialAudio | module | `spatialaudio` | `:spatialaudio` (`:930-976`) | present | |
| MultimediaQuickPrivate + `quickmultimedia` | qml module + plugin | `TARGET Qt::Quick` | `:multimediaquick` (`:702-780`), `:quickmultimedia` (`:782-796`) | present | |
| Quick3DSpatialAudioPrivate + `quick3dspatialaudio` | qml module + plugin | `Qt::Quick3D AND spatialaudio` | `:spatialaudioquick3d` (`:990-1070`), `:quick3dspatialaudio` (`:1072-1086`) | present | |
| MultimediaWidgets | module | `TARGET Qt::Widgets` | `:multimediawidgets` (`:650-685`) | present | `qgraphicsvideoitem.cpp` is built unconditionally. That's fine because qtbase has `graphicsview=1` (`qtbase_config.bzl:767`). |
| FFmpegMediaPluginImplPrivate | static internal module | `QT_FEATURE_ffmpeg` | `:ffmpegmediaplugin_impl` (`:1147-1321`) | **partial** | Missing all Linux-specific sources (v4l2 camera, X11 capture, VAAPI, EGLFS, PipeWire capture). |
| QFFmpegMediaPlugin | plugin (multimedia) | `ffmpeg` | `:qffmpegmediaplugin` (`:1323-1335`) | present | Static only. `ffmpeg.json` metadata is picked up by moc. |
| GstreamerMediaPluginImplPrivate | static internal module | `QT_FEATURE_gstreamer` (L, autodetect) | — | missing | Needs system GStreamer ≥1.20. Low need, because FFmpeg is the default backend on L. |
| QGstreamerMediaPlugin | plugin | `gstreamer` | — | missing | Same as above. |
| QDarwinMediaPlugin | plugin | `APPLE` (M) | — | missing | Native AVFoundation backend. Low/medium need: FFmpeg is the default on M (`qplatformmediaintegration.cpp:55-61`), but `QT_MEDIA_BACKEND=darwin` won't work. |
| QWindowsMediaPlugin | plugin | `native_windows_backend` (W, AUTODETECT true, deprecated) | — | missing | Deprecated WMF backend. Low need. |
| MultimediaTestLibPrivate, android/, qnx/, wasm/ | — | testlib / other OS | — | N/A | |

### Missing/partial sources (grouped by condition)
**Multimedia core** (`UP/qtmultimedia/src/multimedia/CMakeLists.txt`):
- `QT_FEATURE_pulseaudio` (L): `pulseaudio/*.cpp`, 5 files (`:256-266`). Missing. The library is linked directly (`pa_*`), so libpulse headers and a lib are needed.
- `QT_FEATURE_pipewire` (L): `pipewire/*.cpp`, 14 files (`:195-216`). Missing. These are loaded at runtime through `qpipewire_symbolloader.cpp`, so only PipeWire/SPA **headers** are needed. Cheapest fix.
- `pipewire_screencapture` (L, needs dbus=1): 2 files (`:218-226`). Missing.
- `QT_FEATURE_alsa` (L, AUTODETECT false): off in CMake by default too. OK.
- **Impact**: on Linux, `QPlatformAudioDevices::create()` falls back to the empty base class (`UP/qtmultimedia/src/multimedia/platform/qplatformaudiodevices.cpp:50-95`). The result: no audio inputs or outputs, so QAudioSink/QAudioSource/QSoundEffect are silent and QMediaPlayer has no audio. CMake also refuses to enable `ffmpeg` on Linux unless pulseaudio or pipewire is present (`UP/qtmultimedia/src/multimedia/configure.cmake:235-242`). Bazel force-enables ffmpeg without either.
- Shaders: `qtmultimedia_shaders_gl_macos{,_linear}` (`rectsampler.vert`, `rectsampler_bgra.frag`; `CMakeLists.txt:498-529`) and `externalsampler.frag` (`:436`) are missing from `qt_qsb` (`OV/qtmultimedia/BUILD.bazel:305-359`). `qvideotexturehelper.cpp:419-427` requests `rectsampler_bgra` for `Format_SamplerRect`, which comes from the VideoToolbox + OpenGL RHI path on M (`qffmpeghwaccel_videotoolbox.mm:82,228`). Possible black video with the OpenGL RHI on macOS (uncertain; Metal is the default RHI). `externalsampler` is Android-only in practice.
- All Windows and Darwin sources in Multimedia are present, including the INITGUID split (`:241-303`) and ARC for `qdarwinaudiodevices.mm` (`:195-217`). SIMD parts are present (sse2/ssse3/avx2, x86_64 only), matching `qt_internal_add_simd_part`.

**FFmpeg plugin impl** (`UP/qtmultimedia/src/plugins/multimedia/ffmpeg/CMakeLists.txt`):
| condition | sources | Bazel | impact |
|---|---|---|---|
| `QT_FEATURE_linux_v4l` (L; test is only `#include <linux/videodev2.h>`) | `qv4l2camera.cpp`, `qv4l2filedescriptor.cpp`, `qv4l2memorytransfer.cpp`, `qv4l2cameradevices.cpp` (`:224-230`) | missing (`linux_v4l -1`) | **No camera on Linux**: `createCamera` returns notAvailable (`qffmpegmediaintegration.cpp:225-231`) and `videoInputs()` is empty. Only kernel headers are needed. |
| `QT_FEATURE_vaapi` (L) | `qffmpeghwaccel_vaapi.cpp`, plus va stubs (`:110-127`) | missing | No VAAPI HW decode/encode on Linux. Software decode only. Our ffmpeg build also has `HAVE_VAAPI_*=0` (`ffmpeg+/ffmpeg_config_checks.bzl:557-560`). |
| `QT_FEATURE_xlib` (L) | `qx11surfacecapture.cpp`, `qx11capturablewindows.cpp` (`:189-197`) | missing (qtbase xlib=-1) | Screen/window capture falls back to `QGrabWindowSurfaceCapture`. `QWindowCapture::capturableWindows()` is empty on X11. |
| `eglfs AND OpenGLPrivate` (L) | `qeglfsscreencapture.cpp`, `qopenglvideobuffer.cpp` (`:200-219`) | missing (eglfs=-1) | Embedded-only. Low. |
| `pipewire_screencapture` | (in Multimedia) | missing | No Wayland screen capture. |
| `LINUX OR ANDROID` ffmpeg stubs | `QtAddFFmpegStubs.cmake`, `symbolstubs/*` (`:97-108`) | N/A | Bazel links ffmpeg statically; stubs only matter for shared ffmpeg. |
| `APPLE` / `MACOS` | darwin/*.mm (`:130-159`) | present | `qffmpeghwaccel_videotoolbox.mm` included. |
| `WIN32` | d3d11 / dxgi / gdi / wincapturable / windowscamera (`:161-174`) | present | `mfuuid` is replaced by a generated INITGUID TU (`OV:1136-1145`). |
| `QT_FEATURE_cpp_winrt` (W) | `qffmpegwindowcapture_uwp.cpp` (`:176-187`) | missing (qtbase cpp_winrt=-1) | UWP window capture missing. Falls back to GDI. Low. |
| hw-accel summary | vaapi (L) ✗, vdpau (never in Qt 6.10) —, videotoolbox (M) ✓, d3d11 (W) ✓, mediacodec (Android) N/A | | |

### Feature deviations (`OV/qtmultimedia/qtmultimedia_config.bzl`)
| feature | CMake default (L/M/W) | Bazel | impact |
|---|---|---|---|
| pulseaudio (`:17`) | on if libpulse found / off / off | -1 | **No Linux audio** (with pipewire also off) |
| pipewire (`:21`) | on if found (headers only) / off / off | -1 | Same as above |
| pipewire_screencapture (`:22`) | on if pipewire & dbus / – / – | -1 | No Wayland screen capture |
| alsa (`:5`) | off (AUTODETECT false) | -1 | none |
| linux_v4l (`:30`) | on / – / – | -1 | **No Linux camera** |
| linux_dmabuf, vaapi (`:31-32`) | on if EGL+libva / – / – | -1 | No HW video accel on L |
| gstreamer* (`:23-28`) | on if GStreamer found / – / – | -1 | Backend missing (low) |
| avfoundation (`:6`) | – / **on** / – | -1 on M | No source uses `QT_CONFIG(avfoundation)` (grepped), so this is cosmetic. It's a PUBLIC feature, so user code testing it sees a wrong value. |
| coreaudio, videotoolbox (`:7-13`) | – / on / – | on M only | matches |
| native_windows_backend (`:16`) | – / – / on (deprecated) | -1 | Windows plugin not built (low) |
| wasapi (`:35-39`) | – / – / on | on W only | matches |
| spatialaudio, spatialaudio_quick3d (`:40-41`) | on | 1 | matches |
| ffmpeg (`:42`) | **off on L** unless pulse/pipewire; on M/W | 1 everywhere | Contradicts CMake's requirement on L (see above) |
| ffmpeg_stubs (`:43`) | on (L) when shared ffmpeg | -1 | expected (static ffmpeg) |

### Other deviations
- **ffmpeg linkage**: `:ffmpegmediaplugin_impl` (`create_shared = False`, `OV:1234`) depends directly on the static `@ffmpeg//:avcodec/...` targets (`:1309-1320`). FFmpeg is therefore statically linked into every final binary that pulls in `:qffmpegmediaplugin`. Upstream links shared FFmpeg libs, which users can swap out (CMake comment at `ffmpeg/CMakeLists.txt:98-101`). This matters for LGPL relinking and binary size. `cc_shared_library(name = "ffmpeg_shared")` (`OV:1091-1105`) is **defined but referenced nowhere**: dead code, or an unfinished intent.
- **Backend discovery**: `QPlatformMediaIntegration` uses a `QFactoryLoader("/multimedia")` (`qplatformmediaintegration.cpp:40-42`). If the binary doesn't list `@qtmultimedia//:qffmpegmediaplugin` explicitly, it silently uses `QFallbackIntegration`: no player, no recorder, no camera (`:89-92`). The plugin isn't auto-added by `qt_cc_binary`.
- The `_QTBASE_SHARED_MAP` entry for `:qffmpegmediaplugin` (`QT/qt.bzl:1276-1283`) doesn't include ffmpeg, which is consistent with static ffmpeg.
- `QT_DEFAULT_MEDIA_BACKEND` is not supported (CMake `CMakeLists.txt:531-534`). Harmless.
- QML: the qmldir for QtMultimedia is hand-written (`OV:760-769`) and includes `Video.qml`. `qml_args["version"]` is not a key read by `qt_internal_library` (`QT/qt_internal_library.bzl:176-201` reads `major_version`/`minor_version`). Defaults of 6/10 happen to match, so no functional effect.
- `ffmpeg@7.1.1.bcr.beta.13` has VideoToolbox on M (`ffmpeg+/ffmpeg_config_checks.bzl:598`, `BUILD.bazel:757`). d3d11va on W is not verified (uncertain).

### Ranked findings
- **High**: No Linux audio backend. pulseaudio/pipewire are -1 (`OV/qtmultimedia/qtmultimedia_config.bzl:17,21`), so `qplatformaudiodevices.cpp:95` returns the null device list. Cheapest fix: build `src/multimedia/pipewire/*.cpp`, which dlopens libpipewire and needs headers only.
- **High**: No camera on Linux. `linux_v4l -1` (`qtmultimedia_config.bzl:30`) and the `qv4l2*.cpp` files are absent from `OV/qtmultimedia/BUILD.bazel:1149-1220`. Only `<linux/videodev2.h>` is needed.
- **High**: Media backend plugin isn't auto-linked. Without an explicit `@qtmultimedia//:qffmpegmediaplugin` dep, apps fall back to `QFallbackIntegration` (`QT/qt.bzl:2024-2090` covers only QPA).
- **Medium**: FFmpeg is statically linked into user binaries. `ffmpeg_shared` is unused (`OV/qtmultimedia/BUILD.bazel:1091-1105,1309-1320`). LGPL relink/swap and size implications.
- **Medium**: No VAAPI HW accel or X11/PipeWire screen and window capture on Linux (`ffmpeg/CMakeLists.txt:110-127,189-197`; qtbase `xlib -1`).
- **Medium**: Native backends QDarwinMediaPlugin (M), QWindowsMediaPlugin (W) and GStreamer (L) are missing, so `QT_MEDIA_BACKEND=darwin|windows|gstreamer` can't be used.
- **Low**: The `rectsampler_bgra` / `externalsampler` shader sets are missing (`OV/qtmultimedia/BUILD.bazel:305-359` vs `CMakeLists.txt:436,498-529`). Possible black video on macOS with the OpenGL RHI (uncertain).
- **Low**: `QT_FEATURE_avfoundation` is -1 on macOS (`qtmultimedia_config.bzl:6`), which reports a wrong PUBLIC feature value.
- **Low**: cpp_winrt UWP window capture is missing on W.

---

## qtspeech
**Coverage**: 3 of 7 in-scope CMake targets present (0 partial).

### Target inventory
| CMake target | kind | gating condition | Bazel target | status | notes |
|---|---|---|---|---|---|
| TextToSpeech | module | always | `:tts` (`OV/qtspeech/BUILD.bazel:43-75`) | present | Sources match `UP/qtspeech/src/tts/CMakeLists.txt:4-23`. |
| TextToSpeechQml (`PLUGIN_TARGET TextToSpeechQml`) | qml module (module = plugin) | `TARGET Qt::Qml` | `:ttsqml` (`:86-135`) + `:ttsqmlplugin` (`:137-156`) | present | Bazel splits the backing lib and the plugin. The qmldir `linktarget`/`plugin` name differs from upstream (`ttsqmlplugin` vs `TextToSpeechQml`). Harmless for static linking. |
| QTextToSpeechMockPlugin | plugin (texttospeech) | always | `:qtexttospeech_mock` (`:158-179`) | present | Test/mock engine only. |
| QTextToSpeechDarwinPlugin | plugin | `APPLE` (M) | — | **missing** | Uses only AVFoundation (`UP/qtspeech/src/plugins/tts/darwin/CMakeLists.txt`). No external deps, so easy to add. **Needed.** |
| QTextToSpeechSapiPlugin | plugin | `WIN32` (W, incl. MinGW: ole32, sapi, uuid) | — | **missing** | The only W engine available under MinGW. **Needed.** |
| QTextToSpeechSpeechdPlugin | plugin | `speechd AND UNIX` (L; AUTODETECT UNIX) | — | missing | Needs libspeechd. The only standard L engine. Plausibly needed. |
| QTextToSpeechFlitePlugin | plugin | `flite` (L, if found) | — | missing | Needs flite + Multimedia. Optional. |
| QTextToSpeechWinRTPlugin | plugin | `WIN32 AND NOT MINGW` | — | N/A | CMake excludes MinGW too (`src/plugins/tts/CMakeLists.txt:8-12`). |
| android | plugin | ANDROID | — | N/A | |

### Missing/partial sources
- No partial targets. The missing engines are listed above.

### Feature deviations (`OV/qtspeech/qtspeech_config.bzl`)
| feature | CMake default (L/M/W) | Bazel | impact |
|---|---|---|---|
| speechd (PUBLIC, `:5`) | on if libspeechd found / off / off | -1 | No real TTS engine on Linux |
| flite (`:9`) | on if found / off / off | -1 | same |

### Other deviations
- On M and W, `QTextToSpeech` gets only the `mock` engine, and only if the user links it. Upstream would load `darwin` or `sapi` by default.
- Engines are static plugins and must be linked explicitly. The shared map exists only for mock (`QT/qt.bzl:1316-1324`).
- `_COMMON_DEFINES` contains `QT_NO_AS_CONST=1` (`OV/qtspeech/BUILD.bazel:21`). Upstream spells it `QT_NO_QASCONST`, so this define is a no-op typo.

### Ranked findings
- **High**: No macOS TTS engine. `src/plugins/tts/darwin/*` isn't built (`OV/qtspeech/BUILD.bazel`; CMake `src/plugins/tts/CMakeLists.txt:14-16`).
- **High**: No Windows TTS engine. SAPI isn't built, although it's the only MinGW-compatible engine (`src/plugins/tts/sapi/CMakeLists.txt`).
- **Medium**: No Linux engine (speechd/flite) because the features are -1 (`qtspeech_config.bzl:5,9`).
- **Low**: The `QT_NO_AS_CONST` typo (`OV/qtspeech/BUILD.bazel:21`). qmldir plugin naming differs from upstream.

---

## qtwebview
**Coverage**: 2 of 3 in-scope CMake targets present (0 partial). The webengine plugin is conditional on QtWebEngine, which isn't in the workspace.

### Target inventory
| CMake target | kind | gating condition | Bazel target | status | notes |
|---|---|---|---|---|---|
| WebView | module | always | `:webview` (`OV/qtwebview/BUILD.bazel:88-124`) | present | Sources match `UP/qtwebview/src/webview/CMakeLists.txt`. |
| WebViewQuick + `qtwebviewquickplugin` | qml module + plugin | always | `:webviewquick` (`:164-239`), `:qtwebviewquickplugin` (`:248-285`) | present | |
| QDarwinWebViewPlugin | plugin (webview) | `MACOS OR IOS` | — | **missing** | WKWebView. Needs only WebKit/AppKit/Foundation. **Needed on M.** |
| QWebEngineWebViewPlugin | plugin | `Qt::WebEngineCore AND WebEngineQuick` | — | N/A (no qtwebengine module) | The only L backend upstream too. |
| QWebView2WebViewPlugin | plugin | `WIN32 AND MSVC AND WebView2_FOUND` | — | N/A for MinGW | CMake also skips it for MinGW. |
| winrt / android / wasm | plugin | other | — | N/A | |

### Missing/partial sources
- None inside present targets.

### Feature deviations (`OV/qtwebview/qtwebview_config.bzl:9-14`)
| feature | CMake default (L/M/W) | Bazel | impact |
|---|---|---|---|
| webview_darwin_plugin | – / **on** / – | -1 | No backend on macOS |
| webview_webengine_plugin | on if WebEngine / same / same | -1 | consistent (no WebEngine) |
| webview_webview2_plugin | – / – / on with MSVC only | -1 | consistent for MinGW |
| winrt / android / wasm | off | -1 | matches |

### Other deviations
- With no backend linked, `QWebViewFactory` prints "No WebView plug-in found!" and uses `QNullWebView` (`UP/qtwebview/src/webview/qwebviewfactory.cpp:84-85`). **QtWebView doesn't work on any target platform.**
- The overlay redefines its own `common_copts` (`-w`, `-DQT_BUILDING_QT`) and `_COMMON_DEFINES`, and hard-codes `QT_VERSION` (`OV/qtwebview/BUILD.bazel:30-58`) instead of loading `@qt//:qt.bzl`. `-w` hides all warnings.
- `qtwebviewquickplugin` uses `-I$(GENDIR)/external/qtwebview+/qtwebviewquickplugin_gen_out` (`:251`). This hard-codes the canonical repo name `qtwebview+` and will break if bzlmod gives the repo a different canonical name (e.g. via `override`/`~`).
- The qmldir is hand-written (`:220-227`) with `optional plugin` and `depends QtQuick`. That matches upstream minus the WebEngine dependency.

### Ranked findings
- **High**: No macOS WKWebView backend. `src/plugins/darwin/*` isn't built and `webview_darwin_plugin -1` (`OV/qtwebview/qtwebview_config.bzl:12`). As a result QtWebView is a non-functional null view everywhere.
- **Medium**: L and W have no possible backend: no QtWebEngine module, and WebView2 needs MSVC. This should be documented as an unsupported module.
- **Low**: The hard-coded `external/qtwebview+` include path (`OV/qtwebview/BUILD.bazel:251`). Local copts/defines and `-w` diverge from the shared `common_copts`.

---

## qtwebchannel
**Coverage**: 2 of 2 in-scope CMake targets present (0 partial).

### Target inventory
| CMake target | kind | gating condition | Bazel target | status | notes |
|---|---|---|---|---|---|
| WebChannel | module | always; `Qt::Qml` public if present | `:webchannel` (`OV/qtwebchannel/BUILD.bazel:131-156`), `:webchannel_shared` (`:158-170`) | present | Sources match. `qwebchannel.js` is served at `/qtwebchannel/qwebchannel.js`, since `qt_generate_qrc` aliases to the basename (`QT/qt_tools.bzl:706-714`). Matches CMake's `QT_RESOURCE_ALIAS`. |
| WebChannelQuick (auto plugin `webchannelquickplugin`) | qml module | `TARGET Qt::Quick` | `:webchannelquick` (`:184-223`), `:webchannelquickplugin` (`:247-261`) | present | `PAST_MAJOR_VERSIONS 1` is honored. |

### Missing/partial sources
- None.

### Feature deviations
- No `qt_feature`s in this module. Upstream's `QT_NO_JSVALUE` path for builds without Qml isn't relevant here.

### Other deviations
- Hand-written export header (`:42-94`). This is fine.
- `-std=gnu++20` is added in copts. Fine.

### Ranked findings
- **Low**: None functional. Style only: a local `_COMMON_DEFINES` copy (`OV/qtwebchannel/BUILD.bazel:20-40`).

---

## qtwebsockets
**Coverage**: 2 of 2 in-scope CMake targets present (0 partial).

### Target inventory
| CMake target | kind | gating condition | Bazel target | status | notes |
|---|---|---|---|---|---|
| WebSockets | module | always (`qwebsocket_wasm_p.cpp` WASM only) | `:websockets` (`OV/qtwebsockets/BUILD.bazel:90-125`), `:websockets_shared` (`:127-139`), plus `websocket` aliases | present | All 13 sources match. |
| qmlwebsockets (module = plugin) | qml module | `TARGET Qt::Quick` | `:qmlwebsockets_lib` (`:151-189`), `:qmlwebsockets` (`:198-212`) | present | |

### Missing/partial sources
- None for L/M/W.

### Feature deviations
- None. The `ssl` gates in the sources are satisfied, since qtbase has `ssl=1`.

### Other deviations
- `@qtwebsockets//:qmlwebsockets` has no `_QTBASE_SHARED_MAP` entry (only `:websockets` does, `QT/qt.bzl:385`). A binary that lists only the QML plugin won't automatically get `websockets_shared`/`qml_shared` in `dynamic_deps`. That could statically duplicate QtWebSockets alongside a shared copy (uncertain; depends on how the binary is assembled).

### Ranked findings
- **Low**: The missing shared-map entry for `:qmlwebsockets` (`QT/qt.bzl:385-390`).

---

## qthttpserver
**Coverage**: 1 of 1 in-scope CMake targets present (0 partial).

### Target inventory
| CMake target | kind | gating condition | Bazel target | status | notes |
|---|---|---|---|---|---|
| HttpServer | module | always; WebSockets optional | `:httpserver` (`OV/qthttpserver/BUILD.bazel:89-129`), `:httpserver_shared` (`:131-144`) | present | |

### Missing/partial sources
- `qhttpserverhttp2protocolhandler.cpp` is gated on `QT_FEATURE_http AND QT_FEATURE_ssl` (`UP/qthttpserver/src/httpserver/CMakeLists.txt` extend_target). Bazel builds it unconditionally (`:96`). That's correct given qtbase `http=1`, `ssl=1` (`qtbase_config.bzl:889,894`), but it would break if ssl were ever turned off.
- WebSockets is a hard dependency (`:127`). Upstream makes it optional (`CONDITION TARGET Qt::WebSockets`). No functional impact.

### Feature deviations
- None.

### Other deviations
- The license is declared as GPL-3.0-only (`:12-16`), which matches upstream `QT_COMMERCIAL_OR_GPL3`. Worth flagging to consumers: this is not LGPL.

### Ranked findings
- **Low**: The http2 handler isn't conditional on `ssl` (`OV/qthttpserver/BUILD.bazel:96`). Hard dependency on websockets.

---

## qtnetworkauth
**Coverage**: 1 of 1 in-scope CMake targets present (0 partial).

### Target inventory
| CMake target | kind | gating condition | Bazel target | status | notes |
|---|---|---|---|---|---|
| NetworkAuth | module | always; `qoauthurischemereplyhandler.cpp` if `urischeme_replyhandler` (needs gui) | `:networkauth` (`OV/qtnetworkauth/BUILD.bazel:107-145`), `:networkauth_shared` (`:147-161`) | present | All 10 sources. Gui dep included. |

### Missing/partial sources
- None.

### Feature deviations
| feature | CMake default (L/M/W) | Bazel | impact |
|---|---|---|---|
| urischeme_replyhandler (`:76`) | on (gui) | 1 | matches |

### Other deviations
- Only a private config header is generated (`:69-80`), at a versioned path. No public `qtnetworkauth-config.h`; no header includes one (grepped), so this is OK.
- GPL-3.0-only license, same as qthttpserver.

### Ranked findings
- **Low**: None functional.

---

## Overall top findings (cross-module)
1. **High**: qtmultimedia on Linux has no audio I/O (pulse and pipewire off) and no camera (v4l off). `OV/qtmultimedia/qtmultimedia_config.bzl:17,21,30`.
2. **High**: QtWebView has no functional backend on any platform. The macOS WKWebView plugin is missing (`OV/qtwebview/qtwebview_config.bzl:12`).
3. **High**: QtTextToSpeech has no real engine on M (darwin) or W (sapi), only mock. Linux speechd/flite are also absent.
4. **High/Medium**: Backend plugins (multimedia, texttospeech, webview) are static and never auto-linked. Forgetting the dep leads to silent fallbacks.
5. **Medium**: FFmpeg is statically linked into user binaries. `ffmpeg_shared` is dead code (`OV/qtmultimedia/BUILD.bazel:1091`).
6. **Medium**: No Linux VAAPI or X11/PipeWire screen capture. Native multimedia backends (darwin/windows/gstreamer) are missing.
7. **Low**: macOS GL rect-sampler shaders missing; `qtwebview+` hard-coded GENDIR path; `QT_NO_AS_CONST` typo; `:qmlwebsockets` missing from the shared map.
