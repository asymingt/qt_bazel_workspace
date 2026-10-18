# qtdeclarative 6.10.2: Bazel vs. upstream CMake survey

Paths used below:
- `U` = upstream sources: `/Users/simmers/Library/Caches/bazel/_bazel_simmers/223ccc89642a35e03547a0f3440b8160/external/qtdeclarative+/`
- `B` = overlay: `/Users/simmers/development/qt_bazel_workspace/modules/qtdeclarative/6.10.2/overlay/BUILD.bazel`
- `C` = overlay config: `.../overlay/qtdeclarative_config.bzl`
- Shared macros: `/Users/simmers/development/qt_bazel/qt_internal_library.bzl`, `qt_tools.bzl`, `qt.bzl`

Method: static reading only (I did not run bazel). Upstream gating comes from `U/src/CMakeLists.txt`, `U/tools/CMakeLists.txt`, `U/src/labs/CMakeLists.txt`, `U/src/plugins/**/CMakeLists.txt` and `U/src/quickcontrols/CMakeLists.txt:65-101`. To check sources, I extracted every `.cpp/.mm/.c` named in each present module's CMakeLists.txt and grepped for it in `B`.

## qtdeclarative

**Coverage**: 18 of 96 in-scope CMake targets are present, plus 4 partial (22 counting partial). Out of scope: QuickTestUtilsPrivate, QuickControlsTestUtilsPrivate and QmlAssetDownloader, which are test-only or need a target that doesn't exist; QSGOpenVGAdaptationPlugin (openvg=-1); and qmljs (needs private_tests).

### Target inventory

Legend: **P** present · **Pa** partial · **M** missing. "App hit?" says whether a typical QML app would run into the gap.

#### C++ modules (non-QML)
| CMake target | kind | gating (L/M/W) | Bazel target | status | notes |
|---|---|---|---|---|---|
| QmlIntegration | module (header-only) | always | `:qmlintegration` (B:153) | P | |
| QmlToolingSettingsPrivate | module (static) | always | `:qmltoolingsettings` (B:194) | P | |
| QmlTypeRegistrarPrivate | module (static) | always | `:qmltyperegistrar_lib` (B:259) | P | |
| QmlCompiler | module | always | `:qmlcompiler` (B:1127) | P | builtins.qrc embedded |
| QmlDomPrivate | module | always | — | M | tooling only (qmlformat/qmlls/qmltc) |
| QmlFormatPrivate | module | commandlineparser | — | M | tooling |
| QmlLSPrivate | module | thread+fswatcher+LanguageServer, host | — | M | tooling; `qtlanguageserver` is a bazel_dep but nothing uses it |
| QmlDebugPrivate | module | qml_network+localserver+qml_debug | — | M | used by qmlprofiler/qmlpreview |
| PacketProtocolPrivate | module (static) | qml_debug+thread | — | M | needed by all qmltooling plugins |
| QuickWidgets | module | Qt::Widgets | `:quickwidgets` (B:3191) | P | |
| QuickVectorImageGeneratorPrivate | module | Qt::Svg | — | M | used by svgtoqml / VectorImage; `qtsvg` is a bazel_dep but unused |
| QuickDialogs2Utils | module | QuickTemplates2+container | — | M | needed by QtQuick.Dialogs |

#### QML modules (URI → plugin)
| CMake target | URI | PLUGIN_TARGET | gating | Bazel target | status | App hit? / notes |
|---|---|---|---|---|---|---|
| Qml | `QML` | NO_PLUGIN | always | `:qml` (B:355) | P | jsroot.qmltypes generated |
| QmlModels | `QtQml.Models` | modelsplugin | always | `:qmlmodels` (B:787) | Pa | no `modelsplugin` target. qmldir says `optional plugin modelsplugin` (B:883). Types register through alwayslink (qt_internal_library.bzl:251), so this works |
| QmlWorkerScript | `QtQml.WorkerScript` | workerscriptplugin | qml_worker_script | `:qmlworkerscript` (B:928) | Pa | same as above: no plugin target |
| QmlMeta | `QtQml` | qmlplugin | always | `:qmlmeta` (B:1011) | Pa | same as above: no `qmlplugin` target |
| QmlCore | `QtCore` | qtqmlcoreplugin | always | — | **M** | **Yes.** `import QtCore` is needed for `Settings` and `StandardPaths` (the replacement for Qt.labs.settings) |
| QmlNetwork | `QtNetwork` | qmlnetworkplugin | qml_network | `:qmlnetwork` / `:qmlnetworkplugin` (B:3439/3507) | P | |
| QmlLocalStorage | `QtQuick.LocalStorage` | qmllocalstorageplugin | Qt::Sql | — | M | Sometimes. `@qtbase//:sql` exists, so upstream would build it |
| QmlXmlListModel | `QtQml.XmlListModel` | qmlxmllistmodelplugin | qml_xmllistmodel | — | M | Sometimes. The feature is set to 1 (C:34) but the module isn't built |
| Quick | `QtQuick` | qtquick2plugin | Gui+qsb+qml_animation | `:quick` / `:qtquick2plugin` (B:1521/1793) | P | |
| quickwindow | `QtQuick.Window` | quickwindow | (with Quick) | `:quickwindow` (B:1850) | P | |
| QuickLayouts | `QtQuick.Layouts` | qquicklayoutsplugin | (with Quick) | `:quicklayouts` / `:qquicklayoutsplugin` (B:1911/1998) | P | Downstream asks for `qtquicklayoutsplugin`; the real name is `qquicklayoutsplugin`, same as upstream |
| QuickTest | `QtTest` | QuickTestplugin | testlib | `:quicktest` / `:quicktestplugin` (B:3285/3370) | P | QML_FILES match |
| QuickParticlesPrivate | `QtQuick.Particles` | particlesplugin | quick_particles | — | **M** | Sometimes. The feature is set to 1 (C:101) but the module isn't built |
| QuickEffects | `QtQuick.Effects` | effectsplugin | quick_shadereffect | — | **M** | **Yes, often.** `MultiEffect` is the Qt 6 replacement for GraphicalEffects |
| QuickShapesPrivate | `QtQuick.Shapes` | qmlshapesplugin | quick_path | `:quickshapes` / `:qmlshapesplugin` (B:2089/2147) | P | |
| QuickShapesDesignHelpersPrivate | `QtQuick.Shapes.DesignHelpers` | (default) | quickshapes_designhelpers | — | M | Rarely (mostly Design Studio). The feature is set to 1 (C:165) |
| QuickVectorImage | `QtQuick.VectorImage` | (default) | Qt::Svg | — | M | Rarely |
| QuickVectorImageHelpers | `QtQuick.VectorImage.Helpers` | (default) | Qt::Svg | — | M | Rarely |
| QuickTemplates2 | `QtQuick.Templates` | qtquicktemplates2plugin | always (Quick) | `:quicktemplates2` / `:qtquicktemplates2plugin` (B:2216/2444) | P | |
| QuickControls2Impl | `QtQuick.Controls.impl` | qtquickcontrols2implplugin | always | `:quickcontrols2impl` / `…implplugin` (B:2510/2602) | P | |
| QuickControls2 | `QtQuick.Controls` | qtquickcontrols2plugin | always | `:quickcontrols2` / `:qtquickcontrols2plugin` (B:2673/2753) | Pa | qmldir (B:2724) is missing the 8 `optional import QtQuick.Controls.<Style> auto` lines (U/src/quickcontrols/CMakeLists.txt:15-23). See High finding #1 for the default-style failure |
| QuickControls2Basic | `QtQuick.Controls.Basic` | qtquickcontrols2basicstyleplugin | always | `:quickcontrols2basic` / `…basicstyleplugin` (B:3047/3118) | P | all 68 QML files and 28 images present |
| QuickControls2BasicStyleImpl | `QtQuick.Controls.Basic.impl` | …basicstyleimplplugin | always | `:quickcontrols2basicstyleimpl` / plugin (B:2824/2895) | P | |
| QuickControls2Fusion (+FusionStyleImpl) | `QtQuick.Controls.Fusion` (+`.impl`) | qtquickcontrols2fusionstyleplugin (+impl) | quickcontrols2_fusion (L/M/W) | — | **M** (2 targets) | **Yes.** Fusion is the default style on Linux (U/src/quickcontrols/qquickstyle.cpp:174-175) |
| QuickControls2Material (+MaterialStyleImpl) | `QtQuick.Controls.Material` (+`.impl`) | qtquickcontrols2materialstyleplugin (+impl) | quickcontrols2_material (L/M/W) | — | **M** (2) | **Yes**, very commonly used. Confirms downstream feedback |
| QuickControls2Universal (+impl) | `QtQuick.Controls.Universal` | qtquickcontrols2universalstyleplugin | L/M/W | — | M (2) | Sometimes |
| QuickControls2Imagine (+impl) | `QtQuick.Controls.Imagine` | qtquickcontrols2imaginestyleplugin | L/M/W | — | M (2) | Rarely |
| qtquickcontrols2fluentwinui3styleplugin (+FluentWinUI3StyleImpl) | `QtQuick.Controls.FluentWinUI3` | itself | fusion (L/M/W) | — | M (2) | Sometimes |
| qtquickcontrols2macosstyleplugin (+MacOSStyleImpl) | `QtQuick.Controls.macOS` | itself | fusion AND MACOS | — | **M** (2) | **Yes, on macOS.** It is the default macOS style (qquickstyle.cpp:168-169) |
| qtquickcontrols2iosstyleplugin (+IOSStyleImpl) | `QtQuick.Controls.iOS` | itself | basic AND (IOS OR **MACOS**) | — | M (2) | Rarely; built-in style on macOS |
| qtquickcontrols2windowsstyleplugin (+WindowsStyleImpl) | `QtQuick.Controls.Windows` | itself | basic AND WIN32 | — | **M** (2) | **Yes, on Windows.** It is the default Windows style (qquickstyle.cpp:170-171) |
| qtquickcontrols2nativestyleplugin | `QtQuick.NativeStyle` | itself | macos OR windows style | — | M | Needed by the macOS and Windows styles |
| QuickDialogs2 | `QtQuick.Dialogs` | qtquickdialogsplugin | templates2_container | — | **M** | **Yes.** FileDialog, MessageDialog, ColorDialog. Confirms feedback |
| QuickDialogs2QuickImpl | `QtQuick.Dialogs.quickimpl` | qtquickdialogs2quickimplplugin | same | — | **M** | **Yes** (non-native dialog fallback). Confirms feedback |
| LabsSettings | `Qt.labs.settings` | qmlsettingsplugin | settings | — | M | Sometimes (deprecated in favour of QtCore.Settings) |
| LabsSynchronizer | `Qt.labs.synchronizer` | (default) | always | — | M | Rarely |
| LabsQmlModels | `Qt.labs.qmlmodels` | labsmodelsplugin | always | — | M | Sometimes (TableModel, DelegateChooser) |
| LabsFolderListModel | `Qt.labs.folderlistmodel` | qmlfolderlistmodelplugin | qml_itemmodel | — | M | Sometimes |
| LabsAnimation | `Qt.labs.animation` | labsanimationplugin | Quick | — | M | Rarely |
| LabsPlatform | `Qt.labs.platform` | qtlabsplatformplugin | QuickTemplates2 | — | **M** | **Yes** for desktop apps (native menus, FileDialog, SystemTrayIcon). Confirms feedback |
| LabsWavefrontMesh | `Qt.labs.wavefrontmesh` | qmlwavefrontmeshplugin | shadereffect | — | M | Rarely |
| LabsSharedImage | `Qt.labs.sharedimage` | sharedimageplugin | systemsemaphore | — | M | Rarely |
| quicktooling | `QtQuick.tooling` | quicktooling | always | — | M | No (only qmlplugindump uses it) |

#### Plugins (non-QML)
| CMake target | gating | Bazel | status | notes |
|---|---|---|---|---|
| QQmlNativeDebugConnectorFactoryPlugin, QDebugMessageServiceFactoryPlugin, QQmlProfilerServiceFactoryPlugin, QQmlDebuggerServiceFactoryPlugin, QQmlNativeDebugServiceFactoryPlugin, QQmlDebugServerFactoryPlugin, QTcpServerConnectionFactoryPlugin, QLocalClientConnectionFactoryPlugin, QQmlInspectorServiceFactoryPlugin, QQuickProfilerAdapterFactoryPlugin, QQmlPreviewServiceFactoryPlugin (11) | qml_debug AND thread (U/src/plugins/CMakeLists.txt) | — | M (11) | `-qmljsdebugger=` / Qt Creator QML debugging and profiling won't work even though qml-debug=1 |
| QmlLintQuickPlugin, QmlLintQdsPlugin | host build | — | M (2) | only matters for qmllint |
| QmlLSQuickPlugin | QmlLSPrivate | — | M | only matters for qmlls |

#### Tools
| CMake target | gating | Bazel | status | notes |
|---|---|---|---|---|
| qmltyperegistrar | commandlineparser | `:qmltyperegistrar` (B:285) | P | |
| qmlcachegen | xmlstreamwriter | `:qmlcachegen` (B:1191) | P | see "Other deviations" for how it is invoked |
| qmlaotstats | xmlstreamwriter | — | M | |
| qmllint | commandlineparser | — | M | |
| qmlformat | commandlineparser | — | M | |
| qmlls | LanguageServer, host | — | M | |
| qmlimportscanner | always | — | M | Bazel links plugins by hand instead (qt.bzl:289-367) |
| qmltc | commandlineparser | — | M | |
| qmljsrootgen | commandlineparser | — | M | |
| qmldom | always | — | M | |
| qml (qml_tool) | desktop | — | M | runtime launcher |
| qmlscene | shared build + Quick | — | M | |
| qmltime | shared build + Quick | — | M | |
| qmlplugindump | shared+process+regex+Quick | — | M | |
| qmltestrunner | QuickTest | — | M | |
| qmlprofiler | qml_profiler | — | M | the feature is also forced off (C:27) |
| qmlpreview | qml_preview | — | M | the feature is also forced off (C:28) |
| qmleasing | Widgets + widget features | — | M | |
| svgtoqml | Quick + Svg | — | M | |

Count: 96 in-scope = 12 C++ modules + 51 QML-module targets (styles count as style + impl) + 14 plugins + 19 tools. Present: qmlintegration, qmltoolingsettings, qmltyperegistrar_lib, qmlcompiler, quickwidgets, qml, qmlnetwork, quick, quickwindow, quicklayouts, quicktest, quickshapes, quicktemplates2, quickcontrols2impl, quickcontrols2basic, quickcontrols2basicstyleimpl, qmltyperegistrar, qmlcachegen = 18. Partial: qmlmodels, qmlworkerscript, qmlmeta, quickcontrols2 = 4.

### Feedback cross-check
FEEDBACK1.md §6a claims these are missing. Every claim holds:
| Requested | Upstream target | Bazel target | Verdict |
|---|---|---|---|
| QtQuick.Controls.Material | QuickControls2Material / qtquickcontrols2materialstyleplugin (+ QuickControls2MaterialStyleImpl) | none | **Missing (confirmed)** |
| QtQuick.Dialogs | QuickDialogs2 / qtquickdialogsplugin + QuickDialogs2QuickImpl / qtquickdialogs2quickimplplugin + QuickDialogs2Utils | none | **Missing (confirmed)** |
| Qt.labs.platform | LabsPlatform / qtlabsplatformplugin | none | **Missing (confirmed)** |
| QtCore QML (Settings) | QmlCore / qtqmlcoreplugin | none | **Missing (confirmed)** |
| Qt.labs.settings | LabsSettings / qmlsettingsplugin | none | **Missing (confirmed)** |

Also, three target names in that feedback (§6a lines 171-174) don't exist in B:
- `qtquicklayoutsplugin`: the real target is `:qquicklayoutsplugin`, which matches upstream.
- `qmlmodelsplugin`: there is no plugin target; the types come from `:qmlmodels`.
- `qml_plugins`: no such target.

### Missing/partial sources (grouped by condition)
For present modules there are essentially no missing source groups. The script found every `.cpp/.mm` that upstream CMake names for qml, qmlmodels, qmlworkerscript, qmlmeta, qmlcompiler, quicklayouts, quickshapes, quicktemplates, quickcontrolsimpl, quickcontrols, quickwidgets, qmltest, qmlnetwork, qmltyperegistrar and qmltoolingsettings in B. The only things not found are correctly excluded:

| Condition (upstream file:line) | Files | Bazel | OK? |
|---|---|---|---|
| `DEFINES contains WTF_USE_UDIS86=1` (U/src/qml/CMakeLists.txt:762) | `masm/disassembler/udis86/*.c` (7) | not compiled; `WTF_USE_UDIS86=0` (B:109) matches upstream `NOT disassembler` (:734) | yes |
| `INTEGRITY` (:752) | `OSAllocatorIntegrity.cpp` | — | N/A |
| `NOT qml_type_loader_thread` (:543) | `qqmlthread_stub.cpp` | — (feature=1, so `qqmlthread_impl.cpp` is used) | yes |
| `ANDROID` (qmltest) | `android/quickforandroidtest.cpp` | — | N/A |
| template | `QmlPluginTemplate.cpp` | — | N/A |

Things to watch:
- **QtQuick uses globs** (B:1523-1534) instead of per-feature lists. That's fine today because every quick-* feature is 1 (C:91-106). Turning any feature off would leave its sources compiled, so the globs would have to be split.
- **QtQml compiles debugger and JIT sources unconditionally** (B:406-419). That's consistent: qml-debug=1, and the JIT files guard themselves with `#if QT_CONFIG(qml_jit)`.
- **Scenegraph shaders are not optimized.** Upstream `scenegraph_shaders` uses `OPTIMIZED` and `PRECOMPILE` (U/src/quick/CMakeLists.txt:262-268). The Bazel `:quick_scenegraph_shaders` (B:1258-1325) has no `optimized = True`, so qsb isn't passed `-O`. PRECOMPILE (fxc DXBC) is MSVC-only and doesn't apply to the MinGW-style Windows target. Only the quickshapes shaders set `optimized` (B:2082).

### Feature deviations
| feature | CMake default (L / M / W) | Bazel | impact |
|---|---|---|---|
| qml-jit | L x86_64 **on**, L aarch64 **on** / M x86_64 **on**, M arm64 off / W x86_64 **on** (x86_64+sse2, U/src/qml/configure.cmake:92-101), W arm64 off | L on, M x86_64 on, M arm64 off, **W off on both arches** (C:21-50, `!defined(_WIN32)`) | **Windows x86_64 loses the baseline JIT**: slower JS and bindings, but behaviour is the same. Medium |
| qml-profiler | on / on / on (needs commandlineparser, qml_debug, localserver, xmlstreamwriter, process; all 1 in qtbase_config.bzl) | -1 (C:27) | QML profiling service and qmlprofiler unavailable. Low (the tooling plugins are missing anyway) |
| qml-preview | on / on / on | -1 (C:28) | QML live preview unavailable. Low |
| qml-debug | on / on / on | 1 (C:11) | Matches the flag, but no qmltooling plugins or packetprotocol exist, so turning debugging on does nothing. Low/Medium |
| qml-python | depends on host Python | 1 (C:36) | build-time only; harmless |
| qml-xmllistmodel | on | 1 (C:34) | Feature says yes but `QtQml.XmlListModel` isn't built. Low |
| quick-particles | on | 1 (C:101) | Feature says yes but `QtQuick.Particles` isn't built. Medium |
| quickshapes-designhelpers | on | 1 (C:165) | module not built. Low |
| quickcontrols2-fusion / -imagine / -material / -universal / -fluentwinui3 | on / on / on | 1 (C:142-146) | **Features say yes but no style modules are built.** Header and build graph disagree. High (see finding #1) |
| quickcontrols2-macos | off / **on** / off | -1 (C:147) | Upstream default style on macOS is missing. High |
| quickcontrols2-ios | off / **on** (`IOS OR MACOS`, U/src/quickcontrols/configure.cmake:59-64) / off | -1 (C:148) | Low |
| quickcontrols2-windows | off / off / **on** | -1 (C:149) | Upstream default style on Windows is missing. High |
| qml-network, qml-ssl, qml-locale, qml-animation, qml-worker-script, qml-itemmodel, qml-type-loader-thread, qml-xml-http-request | on | 1 | matches (qtbase ssl=1, localserver=1) |
| qmlmodels qml-* (object/list/delegate/table/tree/sfpm) | on | 1 | matches |
| quick-* (16 private + 3 public) | on (movie=1, opengl=1, draganddrop) | 1 | matches |
| quicktemplates2-hover/multitouch/calendar/container | on | 1 | matches |

All the quickcontrols2-* features are only read by CMake; no C++ code uses them, so the -1/1 values don't change compiled code. The real deviation is which modules exist.

### Other deviations
1. **Default Controls style resolution.** With no style configured (no `QQuickStyle::setStyle`, `QT_QUICK_CONTROLS_STYLE` or `qtquickcontrols2.conf`), `QQuickStyleSpec::resolve()` picks `Fusion` on Linux, `macOS` on macOS and `Windows` on Windows (U/src/quickcontrols/qquickstyle.cpp:166-177). The Controls plugin then calls `qmlRegisterModuleImport("QtQuick.Controls", "QtQuick.Controls.<style>")` (U/src/quickcontrols/qtquickcontrols2plugin.cpp:145-193). None of those style modules are built, so a plain `import QtQuick.Controls` should fail with "module QtQuick.Controls.Fusion is not installed" on every in-scope platform. I found no workaround anywhere in qt_bazel or qt_bazel_workspace (grep for `QT_QUICK_CONTROLS_STYLE`, `setStyle(`, `qtquickcontrols2.conf`). *Needs runtime confirmation.*
2. **Missing plugin targets for QtQml, QtQml.Models and QtQml.WorkerScript.** Upstream builds `qmlplugin`, `modelsplugin` and `workerscriptplugin`. Bazel registers the types by linking the backing libraries with alwayslink, which runs the static `QQmlModuleRegistration` initialiser (qt_internal_library.bzl:251). The qmldirs still say `optional plugin …`. The engine skips optional plugins once the types are registered, so this works, but downstream code that expects those plugin labels won't find them.
3. **qmldir files are hand-written** (`qmldir_content` strings in B) instead of generated by `qt_add_qml_module`. Spot checks of QtQuick, QtQml, Controls.Basic and QtTest match upstream. QtQuick.Controls omits `optional import` lines (B:2724). There is a risk of drift on future version bumps.
4. **qmlcachegen invocation** (qt_tools.bzl:1304-1309): it runs with `--bare --resource-path <p> -i <qmldir>` and passes no `-I` import paths and no dependency qmltypes; the `qmltypes` attribute is declared but no caller in qt_internal_library.bzl:220-229 sets it. Upstream passes the import paths and dependency qmltypes, which lets qmlcachegen AOT-compile bindings to C++. In Bazel, AOT will likely fail type resolution for most functions and fall back to bytecode. Correctness is fine, startup and runtime are slower. *Uncertain: compile warnings would confirm it.* This only affects Basic (68 files) and QtTest (4 files) today.
5. **qmltypes are generated but only exposed through an output group.** `qt_qml_type_registration` writes `plugins.qmltypes` per module (qt_tools.bzl:838) but there's no on-disk `qml/` import tree with qmldir + qmltypes. Only QML's qmltypes is consumed (builtins rcc, B:741-1125). Tools like qmllint, qmlls and Qt Creator have nothing to read; runtime doesn't need it.
6. **No qmlimportscanner.** Apps must list static QML plugins by hand. qt.bzl:289-367 keeps hand-maintained plugin dependency maps for 6 qtdeclarative plugins.
7. **Tracepoint headers are stubbed.** `qtqml_tracepoints_p.h` and `qtquick_tracepoints_p.h` only include `qtrace_p.h` (C:170-176), whereas upstream runs `qt_internal_generate_tracepoints` (U/src/quick/CMakeLists.txt:747). Fine as long as the tracing backend stays off. Low.
8. **`RegExpJitTables.h` is pre-generated** (`overlay/bazel_prebuilt/yarr/`) instead of produced at build time by Python `create_regex_tables`. Deterministic; it must be regenerated on version bumps. Low.
9. **Global compile flags.** `common_copts` forces `-O2 -DNDEBUG -w -fPIC` on every target (B:65-73), so debug builds aren't possible and all warnings are hidden. This probably affects all modules and should be cross-checked with the qtbase survey.
10. **Unused dependencies.** `qtlanguageserver` and `qtsvg` are declared in `overlay/MODULE.bazel` but no target uses them; they would be needed for qmlls, QuickVectorImage and svgtoqml.
11. **The Basic style plugin depends on `:qtquickcontrols2basicstyleimplplugin`** (B:3139). That mirrors upstream's `_qt_internal_add_qml_static_plugin_dependency` (U/src/quickcontrols/basic/CMakeLists.txt:251). Good.

### Ranked findings
**High**
1. A default `import QtQuick.Controls` app should fail on L/M/W. The default style (Fusion, macOS or Windows) isn't built, and the QtQuick.Controls qmldir has no optional style imports (qquickstyle.cpp:166-177, B:2724, C:142-149).
2. QtQuick.Controls.Material, Universal, Imagine, FluentWinUI3, Fusion, macOS, iOS, Windows and NativeStyle are missing (21 targets). Material is explicitly requested in FEEDBACK1 §6a (U/src/quickcontrols/CMakeLists.txt:70-101).
3. QtQuick.Dialogs (QuickDialogs2, QuickDialogs2QuickImpl, QuickDialogs2Utils) is missing and commonly used (U/src/quickdialogs/*).
4. `QtCore` QML (qtqmlcoreplugin: Settings, StandardPaths) is missing (U/src/core/CMakeLists.txt:4).
5. Qt.labs.platform is missing (U/src/labs/platform/CMakeLists.txt:4).
6. QtQuick.Effects (MultiEffect) is missing (U/src/effects/CMakeLists.txt:8).

**Medium**
1. qml-jit is off on Windows x86_64; upstream enables it (C:21-25 vs U/src/qml/configure.cmake:97-98).
2. QtQuick.Particles, Qt.labs.qmlmodels, Qt.labs.settings, Qt.labs.folderlistmodel, QtQuick.LocalStorage and QtQml.XmlListModel are missing (U/src/CMakeLists.txt:30-51, U/src/labs/CMakeLists.txt).
3. QML debugging and profiling are non-functional: the 11 qmltooling plugins and PacketProtocol/QmlDebug are missing, and qml-profiler/qml-preview are forced -1 (C:27-28, U/src/plugins/qmltooling/CMakeLists.txt).
4. qmlcachegen runs without import paths or qmltypes, so AOT likely falls back to bytecode (qt_tools.bzl:1304-1313, qt_internal_library.bzl:220-229). *Uncertain.*
5. Developer tools are missing: qml, qmllint, qmlformat, qmlls, qmlimportscanner, qmlplugindump, svgtoqml, qmlscene and 9 more (U/tools/CMakeLists.txt).
6. Feature headers claim styles, particles, xmllistmodel and designhelpers are built when they aren't (C:34, C:101, C:142-146, C:165).

**Low**
1. There are no `modelsplugin`, `workerscriptplugin` or `qmlplugin` targets; it works through alwayslink, but the FEEDBACK1 names `qmlmodelsplugin` and `qml_plugins` don't exist (B:883, B:962, B:1059).
2. Scenegraph shaders are built without `-O` (B:1258 vs U/src/quick/CMakeLists.txt:262-268).
3. QtQuick sources are globbed, so feature toggles have no effect on what compiles (B:1523-1534).
4. The tracepoints header is stubbed and RegExpJitTables.h is pre-generated (C:170-176, overlay/bazel_prebuilt).
5. Unused `qtlanguageserver`/`qtsvg` deps (overlay/MODULE.bazel), and global `-O2 -DNDEBUG -w` (B:65-73).
6. QtQuick.VectorImage(+Helpers), Shapes.DesignHelpers, labs animation/wavefrontmesh/sharedimage/synchronizer and QtQuick.tooling are missing; rarely used.
