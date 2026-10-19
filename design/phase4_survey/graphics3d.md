# Graphics/3D modules survey: Bazel vs Qt CMake (Qt 6.10.2)

Scope: qtquick3d, qtquick3dphysics, qtquicktimeline, qtshadertools, qtlottie, qt3d, qtdatavis3d, qtgraphs, qtcharts.
Read-only survey (bazel was not run). Platforms: Linux / macOS / Windows (MinGW clang).

## Cross-cutting findings (read first)

- **HIGH – broken QML type registration in 4 modules.** `qt_qml_plugin_cpp`
  (`/Users/simmers/development/qt_bazel/qt_tools.bzl:1051-1090`) declares a `registration_func` attr and never uses it.
  The template creates an empty `QQmlEngineExtensionPlugin`.
  The modules that use it (qtquick3dphysics `BUILD.bazel:890`; qt3d `BUILD.bazel:1758,1838,2410`; qtgraphs `BUILD.bazel:579`;
  qtdatavis3d `BUILD.bazel:492`) also have no `qml_args` on their QML-backing libraries. qmltyperegistrar therefore never runs:
  no types are registered and no qmldir/qmltypes are generated.
  qtquick3d and qtcharts use `qml_args` + `qt_qml_plugin_gen` and work. That is the pattern to copy.
- **HIGH – QtQuick3D.Xr is missing** (Quick3DXr + quick3dxrplugin + BundledOpenXR). Upstream enables it by default on L/M/W
  (`qtquick3d+/src/xr/configure.cmake:21-24`).
- **MEDIUM – assimp from BCR replaces the bundled copy** in both qtquick3d (`@assimp` 6.0.5 vs bundled 6.0.2 with 7 curated importers) and
  qt3d (system assimp 6.0.5 vs bundled). Importer coverage and behaviour may differ; not verified.
- **MEDIUM – qtlottie vector-image pipeline is missing**: LottieVectorImageGeneratorPrivate, the qlottievectorimage plugin and lottietoqml.
  They are blocked because the @qtdeclarative overlay has no QuickVectorImageGeneratorPrivate target.
- **MEDIUM (uncertain) – hand-written `cc_shared_library` targets** in qtquicktimeline, qtlottie and qtcharts add `@qtbase//:dbus_shared` on
  Windows. The `qt_internal_library` macro filters that out, so this may cause a Windows link problem (unconfirmed).
- **LOW – the generated qmldir lacks `depends`/`designersupported` lines.** No designer metainfo is installed (only Qt Design Studio uses it).
- C++ source, shader and resource lists match CMake in every present target (checked by scripted diffs), and so do PhysX/glslang/SPIRV-Cross.

| Module | Coverage |
|---|---|
| qtquick3d | 29/33 present (1 partial). Missing: Xr, xr plugin, OpenXR, qmldbg_quick3dprofiler |
| qtquick3dphysics | 6/6 (2 partial – QML registration) |
| qtquicktimeline | 4/4 |
| qtshadertools | 6/6 (`qt_qsb` API partial) |
| qtlottie | 4/7 (3 blocked on qtdeclarative) |
| qt3d | 30/30 (8 partial – QML registration/qmldir) |
| qtdatavis3d | 3/3 (2 partial – QML registration) |
| qtgraphs | 3/3 (2 partial – QML registration; GraphsWidgets broken at runtime) |
| qtcharts | 3/3 |

---

## qtquick3d

**Coverage**: 29 of 33 in-scope CMake targets present (1 partial: AssimpImporterPlugin). Missing: Quick3DXr, its QML plugin `quick3dxrplugin`, BundledOpenXR, and `qmldbg_quick3dprofiler`.

Paths: upstream = `external/qtquick3d+/` (U); overlay = `modules/qtquick3d/6.10.2/overlay/BUILD.bazel` (B); config = `overlay/qtquick3d_config.bzl` (C).

Method: I built file lists from every in-scope `CMakeLists.txt`, collecting `.cpp/.c/.cc/.mm` sources plus qml/shader/mesh/png/json/hdr/conf resources with paths normalised, and diffed them against every string literal and glob in B (scripts in `/tmp/q3d_*.py|txt`). Apart from XR, the profiler plugin and bundled assimp, **no source or resource file is missing**. I did not run bazel, so nothing here has been build-verified.

### Target inventory

| CMake target | kind | gating condition | Bazel target | status | notes |
|---|---|---|---|---|---|
| Quick3DGlslParserPrivate | module (internal) | always | `:glslparser` (B:300) | present | 14/14 cpp |
| Quick3DUtils | module | always | `:utils` (B:377) | present | meshoptimizer + xatlas are inline sources in CMake; split into `:meshoptimizer_bundled` / `:xatlas_bundled` (B:216,247) and re-exported via `exports_filter` |
| Quick3DAssetImport | module | always | `:assetimport` (B:475) | present | |
| Quick3DRuntimeRender | module | always | `:runtimerender` (B:1113) | present | all 55 cpp; 46 qsb groups incl. 24 particle variants; tinyexr (B:260) + `@zlib` |
| Quick3D | qml module | always | `:quick3d` (B:1315) | present | QML_FILES LightmapperOutputWindow.qml present |
| qquick3dplugin | qml plugin (PLUGIN_TARGET) | always | `:qquick3dplugin` (B:1483) | present | static only |
| Quick3DAssetUtils | qml module | always (+QuickTimeline ext) | `:assetutils` (B:1555) | present | `QT_QUICK3D_ENABLE_RT_ANIMATIONS` set (B:1597) |
| qtquick3dassetutilsplugin | qml plugin | always | B:1639 | present | |
| Quick3DParticles | qml module | always | `:quick3dparticles` (B:1714) | present | |
| qtquick3dparticles3dplugin | qml plugin | always | B:1843 | present | |
| Quick3DParticleEffects | qml module | always | `:quick3dparticleeffects` (B:1873) | present | |
| qtquick3dparticleeffectsplugin | qml plugin | always | B:1911 | present | |
| Quick3DEffects | qml module | always | `:quick3deffects` (B:2405) | present | qteffectlibrary qrc (B:2390) |
| qtquick3deffectplugin | qml plugin | always | B:2468 | present | |
| Quick3DHelpersImpl | qml module | always | `:quick3dhelpersimpl` (B:2016) | present | |
| qtquick3dhelpersimplplugin | qml plugin | always | B:2103 | present | |
| Quick3DHelpers | qml module | always (+concurrent) | `:quick3dhelpers` (B:2190) | present | links concurrent; depends on the impl plugin (B:2331), matching `_qt_internal_add_qml_static_plugin_dependency` |
| qtquick3dhelpersplugin | qml plugin | always | B:2314 | present | |
| Quick3DIblBaker | module | always | `:iblbaker` (B:2534) | present | |
| **Quick3DXr** | qml module | `QT_FEATURE_quick3dxr_openxr OR VISIONOS` (U/src/xr/CMakeLists.txt:13); feature AUTODETECT on Linux/Win32/macOS (U/src/xr/configure.cmake:21-24) | — | **missing** | 16 core cpp + 5 openxr cpp + backends (d3d11/d3d12 on Win, opengl on Linux/Win, vulkan, metal .mm on macOS) + XrErrorDialog.qml. Users plausibly need it (`import QtQuick3D.Xr`, VR/AR headsets); niche but on by default |
| **quick3dxrplugin** | qml plugin (implicit) | same | — | **missing** | |
| **BundledOpenXR** | 3rdparty lib | `quick3dxr_openxr AND NOT system_openxr` (U/src/3rdparty/CMakeLists.txt:14) | — | **missing** | OpenXR loader; only needed for XR |
| BundledEmbree | 3rdparty lib | Win x86_64, macOS x86_64/arm64, Linux x86_64/arm64 (U/src/3rdparty/CMakeLists.txt:6-12) | `:embree_bundled` (B:128) | present | 43/43 cpp, same defines; incompatible only on Windows arm64 (B:207), which is equivalent within scope |
| AssimpImporterPlugin | plugin (assetimporters) | `QT_FEATURE_quick3d_assimp` | `:assimpimporterplugin` (B:2597) | **partial** | links BCR `@assimp` 6.0.5 (B:2635, MODULE.bazel `bazel_dep(assimp, 6.0.5)`) instead of bundled assimp 6.0.2 (~100 sources, U/src/plugins/assetimporters/assimp/CMakeLists.txt:111-391); static plugin only |
| **QQuick3DProfilerAdapterFactoryPlugin** | plugin (qmltooling) | `QT_FEATURE_qml_debug AND TARGET Qt::PacketProtocolPrivate` (U/src/plugins/CMakeLists.txt:7) | — | **missing** | qtdeclarative overlay has no PacketProtocol / qmldbg targets, so this is effectively blocked upstream. Low user need (QML profiler only) |
| balsam | tool | always | `:balsam` (B:2646) | present | statically links the assimp plugin |
| balsamui | tool | Widgets AND Concurrent | `:balsamui` (B:2700) | present | |
| meshdebug | tool | always | `:meshdebug` (B:2764) | present | |
| shadergen | tool | TARGET Qt::ShaderTools | `:shadergen` (B:2734) | present | |
| instancer | tool | always | `:instancer` (B:2783) | present | |
| shapegen | tool | cborstreamwriter | `:shapegen` (B:2832) | present | |
| materialeditor | tool + qml module (NO_PLUGIN) | localserver | `:materialeditor` (B:3004), `:materialeditor_lib` (B:2912) | present | see runtime QML import note below |
| lightmapviewer | tool + qml module (NO_PLUGIN) | always | `:lightmapviewer` (B:3108) | present | same note |
| designer/ subdirs (5) | Qt Design Studio metadata install | `quick_designer AND shared` | — | N/A | install-only `.metainfo` files and icons |

### Missing/partial sources (grouped by condition)
- **`quick3dxr_openxr` (default ON on L/M/W)**: all of `src/xr/quick3dxr/*.cpp` (16 files), `XrErrorDialog.qml`, and `openxr/{qquick3dxrmanager_openxr,qabstractopenxrgraphics,qopenxrinputmanager,qquick3dxranchormanager_openxr,qopenxrhelpers}.cpp`.
  - WIN32: `qopenxrgraphics_d3d11.cpp`, `qopenxrgraphics_d3d12.cpp`
  - opengl && !gles2 && !MACOS: `qopenxrgraphics_opengl.cpp`
  - vulkan: `qopenxrgraphics_vulkan.cpp`
  - MACOS && metal: `qopenxrgraphics_metal.mm`
  - also all of `src/3rdparty/openxr/src/**` (the loader)
- **`qml_debug AND PacketProtocolPrivate`**: `src/plugins/qmldbg_quick3dprofiler/qquick3dprofileradapter{,factory}.cpp`
- **`NOT system_assimp` (CMake default when no system assimp)**: ~100 bundled assimp sources (Common 31, PostProcessing 30, FBX 15, Collada 3, Obj 3, Ply 2, STL/glTF/glTF2/glTFCommon/CApi/Geometry/Material 1 each, plus poly2tri/pugixml/unzip). Replaced by `@assimp`, which is intentional.
- Other modules: 0 missing cpp and 0 missing qml/shader/mesh/png/json/hdr/conf resources.
- Shader groups: GLSL version lists, MULTIVIEW, PERTARGETCOMPILE and DEFINES match U/src/runtimerender/CMakeLists.txt:209-700 for every group (B:594-1111).

### Feature deviations

| feature | CMake default (L/M/W) | Bazel | impact |
|---|---|---|---|
| quick3dxr-openxr | ON/ON/ON (autodetect, U/src/xr/configure.cmake:21-24) | effectively OFF (no target, no `qtquick3dxr-config.h`) | `QtQuick3D.Xr` cannot be imported; no XR support |
| system-openxr | OFF unless system OpenXR found | N/A | — |
| quick3d-assimp | ON/ON/ON | ON (plugin built) | none |
| system-assimp | OFF by default (bundled 6.0.2 with 7 importers: FBX, Collada, Obj, Ply, STL, glTF, glTF2), ON only if WrapQuick3DAssimp is found | effectively ON (BCR assimp 6.0.5) | importer set, post-process flags and bugfix level may differ. Bundled defines such as `ASSIMP_BUILD_NO_LIMITBONEWEIGHTS_PROCESS` / `NOUNCRYPT` (CMake:360-363) are not applied. **Uncertain**: BCR assimp config not inspected (repo not fetched in cache) |
| QT_QUICK3D_HAS_LIGHTMAPPER (embree) | L x64/arm64, M x64/arm64, W x64 | everything except Windows arm64 (B:1218-1221) | equivalent in scope |
| QT_QUICK3D_HAS_RUNTIME_SHADERS | ON (ShaderTools present) | ON unconditionally (B:1215) | none |
| QT_QUICK3D_ENABLE_RT_ANIMATIONS | ON (QuickTimeline present) | ON (B:1597) | none |
| generated `qtquick3d-config.h`, `qtassetimporters-config(_p).h`, `qtquick3dxr-config(_p).h` | generated | not generated | low: no qtquick3d source uses `QT_CONFIG(quick3d_assimp/quick3dxr_openxr)` or `QT_NO_QUICK3D_ASSIMP` (grep confirmed); only affects downstream code testing these macros |

### Other deviations
- **All plugins are static-only** (`qt_static_plugin`, qt_internal_library.bzl:282-411). CMake builds shared QML plugins and a loadable `assetimporters/libassimp`.
  - QML apps must link the `*plugin` targets; there is no filesystem QML import or QT_PLUGIN_PATH discovery.
  - `:assetutils` (RuntimeLoader) does not depend on `:assimpimporterplugin`. Apps using `RuntimeLoader` with `.gltf`/`.obj` must add the plugin dep themselves, or loading fails at runtime. CMake finds the plugin dynamically.
- **Tracepoints**: C:5-11 writes a stub `qtquick3d_tracepoints_p.h` that only includes `qtrace_p.h`; CMake generates real tracepoints (U/src/quick3d/CMakeLists.txt `qt_internal_generate_tracepoints`). This is a no-op unless lttng/etw tracing is enabled.
- **Dead stub**: C:20-29 declares `gen_quick3d_qmltypereg_cpp` with an empty `qml_register_types_QtQuick3D()`. It is not referenced anywhere in B; real registration comes from `qml_args` (B:1456). It is harmful only if someone wires it in by mistake.
- **materialeditor / lightmapviewer runtime imports**: their QML imports QtQuick.Controls, QtQuick.Dialogs, QtQuick.Layouts, QtQuick.Window, QtCore, Qt.labs.platform and QtQuick3D.Helpers. The Bazel binaries depend only on `:qquick3dplugin` (B:3023, B:3125). **Uncertain**: without those static QML plugins or an import path they will probably fail at startup.
- **qmldir**: hand-written in each `qml_args.qmldir_content` (e.g. B:1470, B:2288, B:2445). Spot-checked as consistent with the CMake URIs and dependencies.
- **qmltypes / metatypes install**: I did not verify whether `plugins.qmltypes` files are emitted or installed (relevant to qmllint/IDE tooling).
- **Upstream inconsistency, mirrored faithfully** (not a Bazel bug): the OIT particle variant is emitted as `particles_vlight_oit_weightedblended` (U/src/runtimerender/CMakeLists.txt:588, B:936). The renderer looks up `particles_vlight_simple` + method suffix (U/src/runtimerender/rendererimpl/qssgrendererimplshaders_rhi.cpp:144). That name mismatch looks like a possible upstream bug; I have not checked whether `particleShaderName` maps it.
- assimp plugin built with `-fexceptions` and without `QT_NO_EXCEPTIONS` (B:2610-2612), matching CMake `EXCEPTIONS`.
- embree `-fexceptions` and `_USE_MATH_DEFINES` for MinGW (B:181-206) are reasonable additions.

### Ranked findings
**High**
- Quick3DXr, its QML plugin and BundledOpenXR are missing even though `quick3dxr-openxr` autodetects ON for Linux/macOS/Windows (U/src/xr/configure.cmake:21-24; U/src/xr/CMakeLists.txt:13; U/src/3rdparty/CMakeLists.txt:14). `import QtQuick3D.Xr` is unavailable; nothing in B mentions xr.

**Medium**
- AssimpImporterPlugin uses BCR `@assimp` 6.0.5 instead of bundled 6.0.2 with its curated importer set and defines (B:2635 vs U/src/plugins/assetimporters/assimp/CMakeLists.txt:111-391). Format and behaviour differences are possible (unverified).
- RuntimeLoader / AssetUtils has no dependency on the static assimp plugin (B:1618-1629), so runtime model loading silently fails unless apps add `:assimpimporterplugin`. CMake loads it dynamically.
- materialeditor / lightmapviewer link only `:qquick3dplugin` (B:3021-3029, B:3123-3130) but import QtQuick.Controls, Dialogs, Layouts, Helpers and Qt.labs.platform, so they will probably fail at runtime (uncertain).

**Low**
- `qmldbg_quick3dprofiler` is missing (U/src/plugins/CMakeLists.txt:7); blocked by the absence of PacketProtocol in the qtdeclarative overlay.
- No generated qtquick3d / qtassetimporters / qtquick3dxr config headers. Unused internally; downstream `QT_CONFIG(quick3d_assimp)` checks would fail to compile.
- Stub tracepoints header (C:5-11) and dead `gen_quick3d_qmltypereg_cpp` stub (C:20-29). The dead stub should be removed to avoid confusion.
- Designer metadata subdirs not packaged (N/A for builds).
- Upstream particle OIT shader name mismatch (U/src/runtimerender/CMakeLists.txt:588 vs qssgrendererimplshaders_rhi.cpp:144) is carried over by B:936. Informational.


---

# Survey part C: qtquick3dphysics, qtshadertools, qtquicktimeline

Path conventions:
- `UP/<m>` = `/Users/simmers/Library/Caches/bazel/_bazel_simmers/223ccc89642a35e03547a0f3440b8160/external/<m>+`
- `OV/<m>` = `/Users/simmers/development/qt_bazel_workspace/modules/<m>/6.10.2/overlay/BUILD.bazel`
- `QB` = `/Users/simmers/development/qt_bazel/` (qt.bzl, qt_tools.bzl, qt_internal_library.bzl)

Method: I read the CMake files by hand and diffed the source lists with shell scripts (scratch lists are in `/tmp/c_survey/`). Bazel was not run.

---

## qtquick3dphysics

**Coverage**: all 6 in-scope CMake targets are present (2 partial). The partial ones are the two QML modules: their C++ builds, but they do no QML type registration. The designer install-only subdirectory is N/A.

### Target inventory
| CMake target | kind | gating condition | Bazel target | status | notes |
|---|---|---|---|---|---|
| BundledPhysX | 3rdparty_library STATIC (`UP/src/3rdparty/PhysX/CMakeLists.txt:6`) | none (whole repo skipped on INTEGRITY/QNX/VXWORKS/WATCHOS or without Qt::Quick3D, `UP/CMakeLists.txt:25-32`) | `:physx_bundled` (`OV:74-619`) | present | Source lists match exactly |
| Quick3DPhysics | qml_module (`UP/src/quick3dphysics/CMakeLists.txt:8`), URI QtQuick3D.Physics, NO_PLUGIN_OPTIONAL, NO_GENERATE_PLUGIN_SOURCE, DESIGNER_SUPPORTED | – | `:quick3dphysics` + `_shared` (`OV:670-772`, `qt_internal_library`) | **partial** | All 30 .cpp present. **No qmltyperegistrar, no qmldir, no qmltypes.** `qml_register_types_QtQuick3D_Physics()` is an empty stub (`OV:659-668`) |
| qquick3dphysicsplugin | PLUGIN_TARGET + plugin.cpp (`:81-86`) | – | `:qquick3dphysicsplugin` (`OV:774-806`) | partial | Static plugin only. It references the stub, so it registers no types |
| Quick3DPhysicsHelpers | qml_module (`UP/src/helpers/CMakeLists.txt:8`), URI QtQuick3D.Physics.Helpers, PAST_MAJOR_VERSIONS 1 | – | `:quick3dphysicshelpers` (`OV:844-888`) | **partial** | .cpp present. No `qml_args`, so `CapsuleGeometry` (`QML_NAMED_ELEMENT`, `qcapsulegeometry_p.h:25`) is never registered. No qmldir |
| qtquick3dphysicshelpersplugin | PLUGIN_TARGET | – | `:qtquick3dphysicshelpersplugin` (`OV:890-930`) | partial | Built with `qt_qml_plugin_cpp`, whose template ignores the `registration_func` attr (`QB/qt_tools.bzl:1051-1091`). The plugin is an empty class |
| cooker | qt_internal_add_app (`UP/tools/cooker/CMakeLists.txt:5`) | NOT WASM/IOS/ANDROID/QNX/INTEGRITY/WEBOS (`UP/tools/CMakeLists.txt:4`) | `:cooker` (`OV:936-952`) | present | Defines match (NDEBUG, PX_PHYSX_STATIC_LIB) |
| designer files (28 .qml + metainfo + 39 png) | qt_copy_or_install | `QT_FEATURE_quick_designer AND QT_BUILD_SHARED_LIBS` (`:101`) | – | N/A | Only used by Qt Design Studio; normal users don't need them |

### Missing/partial sources (grouped by condition)
- **PhysX generic** (CMake `:15-1181`): 388 .cpp in CMake, 388 in Bazel; `comm` diff is empty.
- **PhysX `WIN32 OR MSVC`** (`:1183-1217`): 16 .cpp, matching Bazel `select(windows)` (16); identical.
- **PhysX `UNIX`** (`:1219-1244`, includes macOS): 11 .cpp, matching Bazel `//conditions:default` (11); identical.
- **PhysX `ANDROID`** (cpu-features): N/A.
- **Quick3DPhysics**: all 30 .cpp identical. All 17 headers with `QML_ELEMENT` are in `moc_hdrs` (`OV:739-757`). plugin.cpp is in the plugin target, as upstream.
- **Helpers**: 1 of 1 .cpp present.
- Upstream has no QML_FILES, RESOURCES or shaders.

### Feature deviations
`configure.cmake` defines no qt_feature (only QT_VERSION_* extra definitions, `UP/configure.cmake:20-23`). The table below therefore compares defines and link flags instead.

| define | CMake (L/M/W) | Bazel | impact |
|---|---|---|---|
| PX_COOKING, QT_BUILD_PHYSXSTATIC_LIB, NDEBUG | all three (`:1335,12,1348-1355`) | all (`OV:607-610`) | none |
| PX_PHYSX_STATIC_LIB on the PhysX lib | Windows only (`:1338`) | Windows (`OV:614`) | none |
| PX_SIMD_DISABLED, PX_GCC_FAMILY | MinGW only (`:1344`) | Windows (`OV:613-615`) | none (MinGW is the only Windows target) |
| UNICODE undefined | Windows (`qt_disable_unicode_defines`) | `-UUNICODE -U_UNICODE` | none |
| link libraries | `${CMAKE_DL_LIBS}` (dl on Linux only) | `-ldl -lpthread` on Linux and macOS; `-lws2_32` on Windows (`OV:598-606`) | benign |
| QT_NO_CONTEXTLESS_CONNECT=1 (from `.cmake.conf`) | yes | missing from `_QT_INTERNAL_DEFINES` (`OV:37-56`) | low (compile strictness only) |

### Other deviations
- **Neither QtQuick3D.Physics nor QtQuick3D.Physics.Helpers registers any QML types.** There is no qmldir in resources, no plugins.qmltypes and no metatypes JSON.
  - Likely effect: `import QtQuick3D.Physics` fails or yields no types at runtime. Not verified at runtime.
  - The smoke test (`tests/m4_smoke/main.cc:21`) only does `Q_IMPORT_QML_PLUGIN`, so it would not catch this.
  - `qt_qml_module` (`QB/qt.bzl:2247`) would provide type registration and a qmldir, but the overlay uses `qt_internal_library` without `qml_args`.
- The upstream qmldir would carry `plugin qquick3dphysicsplugin` (non-optional), `depends QtQuick3D` and `designersupported`. The Helpers qmldir would carry imports of QtQuick3D.Physics/QtQuick3D/QtQuick (auto).
- `quick3dphysics_shared` also depends on runtimerender/utils/shadertools_shared (`OV:715-730`). This is over-linking and benign.

### Ranked findings
- **High**: the empty stub `qml_register_types_QtQuick3D_Physics()` means no QML types are registered for QtQuick3D.Physics (`OV:659-668`, `OV:670`). Fix: use `qt_qml_module`/`qml_args`.
- **High**: Helpers has no `qml_args`, and its plugin template ignores `registration_func`, so `CapsuleGeometry` is unregistered (`OV:844-930`, `QB/qt_tools.bzl:1051-1091`).
- **Medium**: neither module has a qmldir, qmltypes or metatypes file. This breaks qmllint/qmlls/qmlcachegen consumers and dynamic `import` (`OV:670`, `OV:844`).
- **Low**: missing `QT_NO_CONTEXTLESS_CONNECT=1` (`OV:37-56` vs `UP/.cmake.conf`).
- **Low**: `-ldl -lpthread` is also linked on macOS, plus an extra `ws2_32` on MinGW (`OV:598-606`). Benign.
- **Low**: designer files are not installed (`UP/src/quick3dphysics/CMakeLists.txt:101`). N/A.

---

## qtshadertools

**Coverage**: 6 of 6 in-scope CMake targets present (0 partial): ShaderTools, BundledGlslang_Spirv, BundledGlslang_Osdependent, BundledGlslang_Glslang, BundledSpirv_Cross, qsb. The CMake API `qt6_add_shaders` maps to `qt_qsb`, which is partial.

### Target inventory
| CMake target | kind | gating | Bazel target | status | notes |
|---|---|---|---|---|---|
| ShaderTools | module, EXCEPTIONS, RHI_HEADER_FILTERS qshaderbaker.h (`UP/src/shadertools/CMakeLists.txt`) | – | `:shadertools` + `_shared` (`OV:277-308`); `:shadertools_sync_headers` with `rhi_filter` (`OV:259-275`) | present | 5 of 5 .cpp; `-fexceptions` |
| BundledGlslang_Spirv | 3rdparty STATIC (`UP/src/glslang/CMakeLists.txt`) | – | `:glslang_spirv` (`OV:184-204`) | present | 9 of 9 .cpp |
| BundledGlslang_Osdependent | 3rdparty STATIC | WIN32 uses Windows/ossource.cpp, UNIX uses Unix/ossource.cpp; pthread on Linux | `:glslang_osdependent` (`OV:122-138`) | present | matches |
| BundledGlslang_Glslang | 3rdparty STATIC, UNITY_BUILD OFF | – | `:glslang_glslang` (`OV:140-182`) | present | 31 of 31 .cpp |
| BundledSpirv_Cross | 3rdparty STATIC, EXCEPTIONS | – | `:spirv_cross` (`OV:210-238`) | present | 11 of 11 .cpp; defines `SPIRV_CROSS_C_API_{GLSL,HLSL,MSL}=1` match |
| qsb | tool (`UP/tools/qsb/CMakeLists.txt`) | – | `:qsb` cc_binary, linkstatic (`OV:314-330`) | present | links Qt statically instead of shared |
| Qt6ShaderToolsMacros.cmake (`qt6_add_shaders`) | CMake API | – | `qt_qsb` (`QB/qt_tools.bzl:1092-1267`) | partial | see Other deviations |

### Missing/partial sources
- None. I hand-diffed the glslang lists (SPIRV 9, Glslang 31, OSDependent 1) and SPIRV-Cross (11) against `OV:143-223`; they are identical.
- Not built by either system: glslang `HLSL/`, `CInterface/`, `OSDependent/Windows/main.cpp`.
- `build_info.h` is pre-shipped, and Qt's `src/3rdparty/patches/*` are already applied in the tarball.

### Feature deviations
There is no `configure.cmake`, so there are no features to compare. The table compares defines and flags instead.

| item | CMake (L/M/W) | Bazel | impact |
|---|---|---|---|
| GLSLANG_OSINCLUDE_UNIX / _WIN32 | UNIX / WIN32 | `select` (`OV:117-120`) | none |
| pthread | Linux only | Linux only (`OV:132-135`) | none |
| hidden visibility, warnings off | yes | `third_party_copts` (`OV:97-105`) | none |
| QT_LEAN_HEADERS=1, QT_NO_AS_CONST=1, QT_NO_FOREACH=1 (`.cmake.conf`) | yes | yes (`OV:70-93`) | none |

### Other deviations
- **`qt_qsb` lacks several `qt6_add_shaders` options** (`UP/tools/qsb/Qt6ShaderToolsMacros.cmake:73-300`):
  - NOGLSL/NOHLSL/NOMSL: `qt_qsb` always passes all three (`qt_tools.bzl:1114-1116`).
  - DEBUGINFO (`-g`) and PRECOMPILE (`-c`).
  - TESSELLATION (`--msltess`), TESSELLATION_VERTEX_COUNT and TESSELLATION_MODE.
  - MEDIUMP and BASE.
  - File `@` replacement specs (`-r`).
  - Depfile support: headers must be listed manually in `hdrs`.
- **Multiview GLSL is hardcoded** to `330,300es`. CMake switches to the user's GLSL list when it contains 4x0/310/320 (`:128-130`).
- `qt_qsb`'s `silent` defaults to True (CMake: False). Cosmetic.
- `qsb` uses `linkstatic=True` against static Qt core/gui (`OV:318`). This is a different link model, but fine for a host tool.

### Ranked findings
- **Medium**: `qt_qsb` lacks NOHLSL/NOMSL/NOGLSL, TESSELLATION*, DEBUGINFO, PRECOMPILE and replacement specs. Tessellation shaders (e.g. in qtquick3d) and GLSL-only sets can't be expressed (`QB/qt_tools.bzl:1114-1124,1224-1252`).
- **Low**: multiview GLSL is hardcoded and doesn't follow the user's GLSL list (`QB/qt_tools.bzl:1150` vs `Qt6ShaderToolsMacros.cmake:128-130`).
- **Low**: qsb is statically linked (`OV:314-330`). Benign.
- **Info**: all library and 3rdparty sources and defines match (`OV:122-308`).

---

## qtquicktimeline

**Coverage**: 4 of 4 in-scope CMake targets present (0 partial): QuickTimeline, qtquicktimelineplugin, QuickTimelineBlendTrees, qtquicktimelineblendtreesplugin.

### Target inventory
| CMake target | kind | gating | Bazel target | status | notes |
|---|---|---|---|---|---|
| QuickTimeline | qml_module QtQuick.Timeline, DESIGNER_SUPPORTED (`UP/src/timeline/CMakeLists.txt`) | – | `qt_qml_module(:quicktimeline)` (`OV:48-73`) + `:quicktimeline_shared` (`OV:75-94`) | present | 3 of 3 .cpp; type registration and qmldir come from the macro |
| qtquicktimelineplugin | PLUGIN_TARGET | – | generated by `qt_qml_module` (`QB/qt.bzl:2323-2342`) | present | static plugin |
| QuickTimelineBlendTrees | qml_module QtQuick.Timeline.BlendTrees (`UP/src/timeline/blendtrees/CMakeLists.txt`) | – | `:quicktimelineblendtrees` (`OV:121-145`) + `_shared` (`OV:147-168`) | present | 3 of 3 .cpp |
| qtquicktimelineblendtreesplugin | PLUGIN_TARGET | – | generated | present | |

### Missing/partial sources
None. All 6 .cpp and 11 headers are listed. Upstream has no QML_FILES, resources or shaders.

### Feature deviations
There is no `configure.cmake`, so there are no features to compare.

| define | CMake | Bazel | impact |
|---|---|---|---|
| QT_NO_CONTEXTLESS_CONNECT=1, QT_NO_FOREACH=1, QT_NO_QASCONST=1 (`UP/.cmake.conf`) | yes | missing (`OV:57-60,131-133`) | low (compile strictness only) |

### Other deviations
- The qmldir generated by `qt_qml_module` (`QB/qt.bzl:2289-2296`) omits `depends QtQuick` (BlendTrees: `depends QtQuick.Timeline`), `designersupported` and `linktarget`. This applies to every module built with the macro.
- The hand-written `cc_shared_library`s list `@qtbase//:dbus_shared` unconditionally (`OV:90,160`). `qt_internal_library` drops it on Windows (`QB/qt_internal_library.bzl:262-266`), so this could fail or over-link on MinGW. **Uncertain.**
- Those `cc_shared_library`s also omit `@qtbase//:opengl_shared`, which `qt_internal_library` adds automatically when `quick_shared` is present (`qt_internal_library.bzl:260-261`). **Uncertain** whether this causes problems.

### Ranked findings
- **Medium (uncertain)**: unconditional `dbus_shared` in `quicktimeline_shared` / `quicktimelineblendtrees_shared` on Windows (`OV/qtquicktimeline:90,160`).
- **Low**: `opengl_shared` is missing from the dynamic_deps of the timeline shared libraries, unlike the `qt_internal_library` convention (`OV/qtquicktimeline:79-91`).
- **Low**: qmldir lacks `depends` and `designersupported` lines (`QB/qt.bzl:2289-2296`). Applies to every module built with the macro.
- **Low**: the extra defines from `.cmake.conf` are missing (`OV/qtquicktimeline:57-60`).


---

## qt3d

**Coverage**: 30 of 30 in-scope CMake targets present (8 partial: all eight QML plugin/module targets, because QML type registration and qmldir are stubbed out). 1 target N/A (FBX geometry loader).

Paths: `UP` = `…/external/qt3d+/`, `OV` = `modules/qt3d/6.10.2/overlay/`. There are no CMake targets under `tools/`: it only holds Blender exporter scripts and QtCreator templates, so there is nothing to build. qt3d has no `qgltf` tool in 6.x.

### Target inventory

| CMake target | kind | gating condition | Bazel target | status | notes |
|---|---|---|---|---|---|
| 3DCore | module | always (UP/src/CMakeLists.txt:5) | `:3dcore` (OV/BUILD.bazel:103) | present | 54/54 srcs. The SSE sources are always listed; CMake adds them only on x86 (UP/src/core/CMakeLists.txt:115). The .cpp files have `#ifdef __SSE2__` guards, so arm64 is OK |
| 3DRender | module (PLUGIN_TYPES sceneparsers geometryloaders renderplugins renderers) | qt3d_render | `:3drender` (:294) | present | 248/248, materialsystem qrc OK. `vulkaninstance.cpp` (CMake: QT_FEATURE_vulkan, UP/src/render/CMakeLists.txt:381) is always compiled, but the file is guarded by `QT_CONFIG(qt3d_vulkan)` |
| 3DInput | module (PLUGIN_TYPES 3dinputdevices) | qt3d_input | `:3dinput` (:742) | present | `qgamepadinput.cpp` dropped: its condition `TARGET Qt::Gamepad` (UP/src/input/CMakeLists.txt:99) is never met in Qt6, so this is correct |
| 3DLogic | module | qt3d_logic | `:3dlogic` (:895) | present | 6/6 |
| 3DExtras | module + "extras" resource | qt3d_extras (render∧input∧logic) | `:3dextras` (:1052) | present | All .cpp files plus all 59 shader resources (`_EXTRAS_SHADER_SRCS`) match |
| 3DAnimation | module | qt3d_animation (render) | `:3danimation` (:1232) | present | 56/56 |
| 3DQuick (Qt3D.Core) | qml_module backing lib | Qt::Quick ∧ 3DRender | `:3dquick` (:1389) | partial | srcs match. Its qmltyperegistrations source is an empty stub (`:1404`, qt3d_config.bzl:74-82) |
| quick3dcoreplugin | QML PLUGIN_TARGET, NO_GENERATE_PLUGIN_SOURCE | same | `:quick3dcoreplugin` (:1460) | partial | Upstream plugin source is used. Only Entity and Geometry get registered (manual `Quick3D_registerType`); the 46 QML_* foreign types are lost |
| 3DQuickRender (Qt3D.Render) | qml_module lib | same | `:3dquickrender` (:1529) | partial | Stub registration (`:1552`). The 346 QML_* macros in `qt3dquick3drenderforeign_p.h` are never registered |
| quick3drenderplugin | QML plugin | same | `:quick3drenderplugin` (:1618) | partial | Plugin .cpp has 0 `qmlRegister*` calls, so no Render types at all |
| 3DQuickScene2D (QtQuick.Scene2D) | qml_module lib | same | `:3dquickscene2d` (:2207) | partial | Stub registration (`:2218`) |
| qtquickscene2dplugin | QML plugin | same | `:qtquickscene2dplugin` (:2271) | partial | `Scene2D` (QML_NAMED_ELEMENT in qscene2d_p.h) is not registered |
| 3DQuickScene3D (QtQuick.Scene3D) | qml_module lib + "scene3d" qsb resource | same | `:3dquickscene3d` (:2347) | present | srcs and the 2 .qsb resources match. Optional input/logic/animation links are always on, which matches the defaults |
| qtquickscene3dplugin | QML plugin (generated source) | same | `:qtquickscene3dplugin` (:2418) | partial | Generated plugin ignores `registration_func` (qt_tools.bzl:1051-1090), and no `qml_register_types_QtQuick_Scene3D` exists. `Scene3D` (scene3ditem_p.h) is not registered |
| 3DQuickExtras (Qt3D.Extras) | qml_module lib | Qt::Quick ∧ 3DExtras | `:3dquickextras` (:1915) | partial | Stub registration (`:1924`), 117 QML_* macros lost |
| quick3dextrasplugin | QML plugin | same | `:quick3dextrasplugin` (:1984) | partial | No types registered |
| 3DQuickInput | module | Qt::Quick ∧ 3DInput | `:3dquickinput` (:1690) | present | 8/8 |
| quick3dinputplugin (Qt3D.Input) | qml_module with generated plugin | same | `:quick3dinputplugin` (:1766) | partial | Generated empty plugin. The 78 QML_* macros in `imports/input/qt3dquick3dinputforeign_p.h` are never registered |
| 3DQuickLogic (Qt3D.Logic) | qml_module (header only) | Qt::Quick ∧ 3DLogic | `:3dquicklogic` (:1804) | present | srcs=[] is correct |
| quick3dlogicplugin | QML plugin (generated) | same | `:quick3dlogicplugin` (:1846) | partial | Empty plugin, so `FrameAction` (QML_FOREIGN) is not registered |
| 3DQuickAnimation (Qt3D.Animation) | qml_module lib | Qt::Quick ∧ 3DAnimation | `:3dquickanimation` (:2062) | partial | Stub registration (`:2074`), 78 QML_* macros lost |
| quick3danimationplugin | QML plugin | same | `:quick3danimationplugin` (:2131) | partial | No types registered |
| OpenGLRendererPlugin | plugin (renderers) | qt3d_opengl_renderer | `:openglrenderer` (:2488) | present | Matches, including the imgui debug overlay via `:imgui_bundled` (:2461), which CMake adds under NOT INTEGRITY |
| RhiRendererPlugin | plugin (renderers) | qt3d_rhi_renderer (TARGET Qt::ShaderTools) | `:rhirenderer` (:2557) | present | 17/17, links `@qtshadertools` |
| Scene2DPlugin | plugin (renderplugins) | Qt::Quick ∧ qt3d_render | `:scene2d` (:2614) | present | match |
| DefaultGeometryLoaderPlugin | plugin (geometryloaders) | render ∧ regularexpression | `:defaultgeometryloader` (:2652) | present | match |
| GLTFGeometryLoaderPlugin | plugin (geometryloaders) | render | `:gltfgeometryloader` (:2691) | present | match |
| fbxGeometryLoaderPlugin | plugin | qt3d_fbxsdk (Autodesk SDK, `libs.fbx`) | — | N/A | Off by default and needs a proprietary SDK; users are unlikely to need it |
| AssimpSceneImportPlugin | plugin (sceneparsers), EXCEPTIONS | qt3d_assimp ∧ (clang∨gcc∨system∨msvc) | `:assimpsceneimport` (:2722) | present | Uses BCR `@assimp` 6.0.5 (:2761) instead of the bundled 3rdparty assimp (UP/src/3rdparty/assimp/assimp.cmake) |
| GLTFSceneImportPlugin | plugin (sceneparsers) | render | `:gltfsceneimport` (:2767) | present | match |
| GLTFSceneExportPlugin | plugin (sceneparsers) | render ∧ regularexpression ∧ temporaryfile ∧ 3DExtras | `:gltfsceneexport` (:2805) | present | match |
| dummy (renderers/dummy) | executable | not added by UP/src/plugins/renderers/CMakeLists.txt | — | N/A | Dead directory |
| OpenGLRendererLib / RhiRendererLib | static libs | QT_FEATURE_private_tests | — | N/A | Test-only |
| BundledAssimp sources | 3rdparty (compiled into the plugin) | NOT qt3d_system_assimp | replaced by `@assimp` | N/A (by design) | See feature table |

### Missing/partial sources (grouped by condition)

I diffed the file lists with a script (`scratch/diff3d.py` in conv 66d4162e). Source (.cpp), shader and resource lists match exactly for every present target, apart from the items below:
- **`TARGET Qt::Gamepad`** (UP/src/input/CMakeLists.txt:99, imports/input/CMakeLists.txt:42): `src/input/frontend/qgamepadinput.cpp` and the `HAVE_QGAMEPAD` define are omitted, and the header is excluded at OV:727,802. This is correct: Qt6 has no QtGamepad.
- **`QT_FEATURE_vulkan`** (UP/src/render/CMakeLists.txt:381): `vulkaninstance.cpp` is always compiled (OV:531). It is harmless because the body is under `#if QT_CONFIG(qt3d_vulkan)`, which is -1.
- **x86-only SSE sources** (UP/src/core/CMakeLists.txt:115-126): always compiled (OV:151,158,159). Harmless because of the `#ifdef __SSE2__` guards.
- **Bundled assimp** (`qt3d_extend_target_for_assimp`, assimp.cmake): about 100 bundled assimp/pugixml/unzip/zlib sources are not compiled. BCR `@assimp` is used instead.
- **Generated sources** (not in CMakeLists, produced by `qt_internal_add_qml_module`):
  - `*_qmltyperegistrations.cpp` is replaced by empty stubs for 5 modules (qt3d_config.bzl:74-122).
  - Nothing at all is generated for Qt3D.Input, Qt3D.Logic and QtQuick.Scene3D.
  - The generated plugin .cpp (qt_tools.bzl:1051-1068) never calls the registration function.

### Feature deviations

| feature | CMake default (L/M/W) | Bazel | impact |
|---|---|---|---|
| qt3d_assimp | ON/ON/ON | 1 (qt3d_config.bzl:8) | none |
| qt3d_system_assimp | auto: ON only if an assimp ≥5 package is found, else bundled. Usually OFF on M/W, distro-dependent on L | 1 (qt3d_config.bzl:23), `@assimp` 6.0.5 | Low–Med: a different assimp major version and importer set than the bundled 5.x snapshot. Only affects which formats load and the binary size/licence notices. Not verified that the plugin builds against assimp 6 (uncertain) |
| qt3d_render / input / logic / extras / animation | ON | 1 | none |
| qt3d_opengl_renderer | ON | 1 (:38) | none |
| qt3d_rhi_renderer | ON if Qt::ShaderTools | 1 | none (dep present) |
| qt3d_vulkan | Vulkan_FOUND ∧ rhi ∧ QT_FEATURE_vulkan: usually ON on L/W when the Vulkan SDK/headers are present, OFF on M | -1 (:39) | Low: the RHI Vulkan backend is not available to Qt3D. Consistent with qtbase Bazel `QT_FEATURE_vulkan -1` (qtbase_config.bzl:516) |
| qt3d_fbxsdk | OFF (needs SDK) | not defined | none |
| qt3d_simd_sse2 / qt3d_simd_avx2 | not features in 6.10 (only in `src/core/.prev_configure.cmake`) | defined in qt3dcore-config_p.h (:25-29) | none (stale, unused) |
| (CMake EXCEPTIONS on AssimpSceneImportPlugin) | -fexceptions | `QT_NO_EXCEPTIONS` passed via `_QT_INTERNAL_DEFINES` (OV:52, used at :2748). No -fno-exceptions on the plugin | Low: assimp exceptions are caught inside libassimp; the plugin has no try/catch (uncertain) |

### Other deviations

- **QML type registration is missing (all Qt3D QML imports).** Qt3D 6.x registers almost every QML type declaratively, through `QML_ELEMENT` / `QML_FOREIGN` in `*foreign_p.h` and qmltyperegistrar. Bazel replaces this with empty functions (qt3d_config.bzl:74-122) or generates plugins that call nothing (qt_tools.bzl:1051-1090; `registration_func` is accepted but unused).
  - Result: `Transform`, `Camera`, `Material`, `PhongMaterial`, `Scene3D`, `Scene2D`, the input and animation types, etc. cannot be created from QML. Only `Entity` and `Geometry` (manual, quick3dcoreplugin) and C++ `Quick3D_registerType` users work.
  - `qt_qml_type_registration` exists in qt_tools.bzl:833-922 but this overlay never uses it.
- **No qmldir / qmltypes / plugin import metadata.** None of the 8 QML modules gets a `qmldir` (`qt_qmldir` exists at qt_tools.bzl:957 but is unused).
  - CMake declares `DEPENDENCIES`, `PAST_MAJOR_VERSIONS 2` and `NO_PLUGIN_OPTIONAL`.
  - Without a qmldir (or a `:/qt-project.org/imports/...` resource), `import Qt3D.Core 2.x` will not resolve to the static plugins unless the consuming app wires the imports by hand (uncertain how the qt_bazel app macro handles this).
- **metatypes JSON** for the 3D modules is not exported for foreign-type use by downstream modules. Low impact.
- **Plugin metadata JSON** is supplied via `textual_hdrs` to all 8 C++ plugins, so the factory keys ("opengl", "rhi", "gltf", "assimp", …) should be correct. All plugins are static (`alwayslink`), so the consuming binary must link the specific plugin targets: renderer loading is by key through QFactoryLoader.
- **Plugin class names** match CMake (e.g. `QtQuick_Scene3DPlugin` is the CMake default when no CLASS_NAME is given).
- **Resources:** materialsystem (1), extras (59) and scene3d (2 .qsb) all match the CMake lists.
- **Copts:** the hard-coded `-fPIC -fvisibility=hidden -O2 -fno-exceptions` on libraries is GCC/Clang only. That is fine for L/M/MinGW-clang; `-fPIC` is a no-op warning on Windows.

### Ranked findings

**High**
- Every Qt3D QML module registers no types (or only Entity/Geometry). The empty qmltyperegistration stubs are at qt3d_config.bzl:74-122, used at OV/BUILD.bazel:1404,1552,1924,2074,2218. QML-based Qt3D apps (`Transform`, `Camera`, `PhongMaterial`, ...) will fail at runtime.
- The generated QML plugins for Qt3D.Input, Qt3D.Logic and QtQuick.Scene3D ignore `registration_func` (qt_tools.bzl:1051-1090; OV/BUILD.bazel:1758-1764,1838-1844,2410-2416). `Scene3D`, the most common Qt3D-in-QtQuick entry point, is unregistered.
- No `qmldir` is generated for any of the 8 QML modules (OV/BUILD.bazel: no `qt_qmldir` calls). `import Qt3D.*` / `QtQuick.Scene3D` resolution in static builds probably fails (uncertain; depends on how the app links the imports).

**Medium**
- The assimp plugin uses BCR `@assimp` 6.0.5 as "system" assimp instead of the bundled 5.x (qt3d_config.bzl:23; OV/BUILD.bazel:2761; MODULE.bazel:13). Expect API, importer-set and licence differences; whether it builds against 6.x is unverified.
- QML metatypes and qmltypes are not produced for the 3D modules, so downstream QML tooling (qmllint, qmlcachegen, foreign types) has no type information (OV/BUILD.bazel whole file).

**Low**
- `qt3d_vulkan` is -1 (qt3d_config.bzl:39). This matches qtbase vulkan -1 but differs from a typical Linux/Windows CMake build with the Vulkan SDK.
- `QT_NO_EXCEPTIONS` is defined for the assimp plugin, which CMake builds with EXCEPTIONS (OV/BUILD.bazel:52,2748 vs UP/src/plugins/sceneparsers/assimp/CMakeLists.txt:13).
- Stale `qt3d_simd_*` macros in qt3dcore-config_p.h (qt3d_config.bzl:24-29). Harmless.
- `vulkaninstance.cpp` and the SSE sources are compiled unconditionally (OV/BUILD.bazel:531,151-159). They are guarded in source, so harmless.
- `qgamepadinput.cpp` is omitted (OV/BUILD.bazel:727). Correct, since Qt6 has no QtGamepad.
- The FBX geometry loader is absent. N/A: it needs the proprietary SDK and is off by default.


---

# Survey part D — qtlottie, qtdatavis3d, qtgraphs, qtcharts (Qt 6.10.2)

Path abbreviations:
- `UP/<m>` = `/Users/simmers/Library/Caches/bazel/_bazel_simmers/223ccc89642a35e03547a0f3440b8160/external/<m>+`
- `OV/<m>` = `/Users/simmers/development/qt_bazel_workspace/modules/<m>/6.10.2/overlay`
- `QB` = `/Users/simmers/development/qt_bazel`

How I compared sources: a shell script pulled every `*.cpp/*.c/*.mm` (then resource extensions) out of the upstream CMakeLists and every quoted literal out of the overlay, then ran `comm` on the two lists. I checked by hand the overlay entries built from list comprehensions or globs. Script: `/Users/simmers/.gemini/jetski/brain/a02f7da5-f6cb-45c2-969b-9540a65cf322/scratch/diff.sh`.

## Cross-cutting finding (affects qtgraphs + qtdatavis3d)

`OV/qtgraphs/BUILD.bazel:324` (`graphs`) and `OV/qtdatavis3d/BUILD.bazel:425` (`datavisualizationqml`) use plain `qt_internal_library` **without `qml_args`**. As a result:
- qmltyperegistrar never runs (`QB/qt_internal_library.bzl:175-216` only runs it when `qml_args != None`).
- No qmldir resource is embedded, and no `plugins.qmltypes` is produced.

The plugins are generated with `qt_qml_plugin_cpp` (`OV/qtgraphs/BUILD.bazel:579`, `OV/qtdatavis3d/BUILD.bazel:492`). Its template (`QB/qt_tools.bzl:1051-1090`) **ignores `registration_func`**: it writes an empty `QQmlEngineExtensionPlugin` that never references `qml_register_types_<uri>`.
- It links cleanly, but **no QML types are registered**: `import QtGraphs` / `import QtDataVisualization` will not work.
- Compare qtcharts, which does this correctly: `qml_args` (`OV/qtcharts/BUILD.bazel:447`) plus `qt_qml_plugin_gen`, which uses `QT_KEEP_SYMBOL` (`QB/qt_tools.bzl:999-1020`).
- This also breaks the C++ `QtGraphsWidgets` API. `Q3DGraphsWidgetItem` builds its graph from inline QML `import QtGraphs; ...` via `QQmlComponent` (`UP/qtgraphs/src/graphs3d/widget/q3dgraphswidgetitem.cpp:1403-1411`).
- Scale: 58 qtgraphs files and 10 qtdatavis3d files use `QML_ELEMENT` / `QML_NAMED_ELEMENT`.

---

## qtlottie
**Coverage**: 4 of 7 in-scope CMake targets present (0 partial). The 3 missing ones are blocked by a missing qtdeclarative dependency.

### Target inventory
| CMake target | kind | gating condition | Bazel target | status | notes |
|---|---|---|---|---|---|
| Lottie (URI `Qt.labs.lottieqt`) | qml_module | always (`UP/qtlottie/src/CMakeLists.txt:3`) | `:lottie` (`OV/qtlottie/BUILD.bazel:73`), `:lottie_shared` (:128) | present | Uses `qt_qml_module`, so typeregistrar, qmldir and plugin are generated |
| lottieqtplugin | qml plugin (default PLUGIN_TARGET) | always | `:lottieqtplugin` (generated by `qt_qml_module`, `plugin_target` at :117) | present | class `Qt_labs_lottieqtPlugin`, which matches the CMake default (uncertain: default name derivation) |
| LottieVectorImageHelpers (URI `Qt.labs.lottieqt.VectorImageHelpers`) | qml_module | always (`src/vectorimagehelpers/CMakeLists.txt:4`) | `:lottievectorimagehelpers` (:170), `_shared` (:193) | present | |
| lottievectorimagehelpersplugin | qml plugin | always | `:lottievectorimagehelpersplugin` (:182) | present | CLASS_NAME `QLottieVectorImageHelpersPlugin` matches |
| LottieVectorImageGeneratorPrivate | internal module | `TARGET Qt::QuickPrivate AND Qt::QuickShapesPrivate AND Qt::QuickVectorImageGeneratorPrivate` (`src/CMakeLists.txt:24-29`) | — | missing | `@qtdeclarative` overlay has no QuickVectorImageGenerator target (grep for "vectorimage" in `OV/qtdeclarative/BUILD.bazel` finds nothing) |
| QLottieVectorImagePlugin (`vectorimageformats/qlottievectorimage`) | plugin | always added (`src/plugins/vectorimageformats/lottie/CMakeLists.txt:4`), but needs LottieVectorImageGeneratorPrivate | — | missing | Lets QtQuick.VectorImage load `.json` Lottie files. Plausibly needed by users of QtQuick.VectorImage |
| lottietoqml | tool (TOOLS_TARGET Lottie) + `qml` resource (`main.qml`) | needs QuickVectorImageGeneratorPrivate (`tools/lottietoqml/CMakeLists.txt:12`) | — | missing | Developer tool; moderately useful |

### Missing/partial sources
- Unconditional (Lottie, VectorImageHelpers): none missing. 28/28 `.cpp` match.
- Generator group: `src/lottiegenerator/qlottievisitor.cpp`, `src/plugins/vectorimageformats/lottie/main.cpp`, `tools/lottietoqml/main.cpp` + `tools/lottietoqml/main.qml` resource.

### Feature deviations
| feature | CMake default (L/M/W) | Bazel | impact |
|---|---|---|---|
| — (no `configure.cmake` in qtlottie) | — | — | none |

### Other deviations
- The generated qmldir (`QB/qt.bzl:2289-2296`) has no `depends QtQuick` line. CMake emits one (`DEPENDENCIES QtQuick`). Low impact: tooling / qmllint only.
- `lottie_shared` / `lottievectorimagehelpers_shared` are raw `cc_shared_library` (`OV/qtlottie/BUILD.bazel:128-147, 193-212`). They skip the `qt_internal_library` fixups:
  - They list `@qtbase//:dbus_shared` unconditionally (lines 137, 202), including on Windows. The macro drops it there (`QB/qt_internal_library.bzl:262-266`).
  - They do not add `opengl_shared` even though `quick_shared` is a dynamic dep (macro: :260-261).
  - Impact is uncertain: possibly a Windows link error, or duplicated static code.
- Deps list only `@qtbase//:gui` and `@qtdeclarative//:quick`. CMake also needs GuiPrivate and QuickPrivate. This works if those targets re-export private headers (they appear to).

### Ranked findings
- **High**: none.
- **Medium**: The Lottie VectorImage plugin and LottieVectorImageGeneratorPrivate are missing (`UP/qtlottie/src/CMakeLists.txt:24-29`, `src/plugins/vectorimageformats/lottie/CMakeLists.txt:4`). They are blocked on a missing `QuickVectorImageGeneratorPrivate` in the `@qtdeclarative` overlay.
- **Medium**: Raw `cc_shared_library` puts dbus_shared on Windows (`OV/qtlottie/BUILD.bazel:137,202`) and misses the opengl_shared fixup. Uncertain whether it actually breaks.
- **Low**: The `lottietoqml` tool is missing (`UP/qtlottie/tools/lottietoqml/CMakeLists.txt:12`).
- **Low**: qmldir lacks `depends QtQuick` (`QB/qt.bzl:2289-2296`).

---

## qtdatavis3d
**Coverage**: 3 of 3 in-scope CMake targets present (2 partial: the QML module and its plugin are functionally broken).

### Target inventory
| CMake target | kind | gating condition | Bazel target | status | notes |
|---|---|---|---|---|---|
| DataVisualization | module | repo needs Gui, Widgets, OpenGL, Quick, Qml, QuickTest, Test (`UP/qtdatavis3d/CMakeLists.txt` assertTargets) | `:datavisualization` (`OV/qtdatavis3d/BUILD.bazel:193`), `_shared` (auto) | present | All 72 srcs match; shader (39) and mesh (29) qrc with aliases match |
| DataVisualizationQml (URI `QtDataVisualization`, PAST_MAJOR_VERSIONS 1) | qml_module | `TARGET Qt::Quick` (`src/CMakeLists.txt`) | `:datavisualizationqml` (:425) | **partial** | No `qml_args`, so no type registration, qmldir or qmltypes; QML_FILES and RESOURCES missing |
| datavisualizationqmlplugin (class `QtDataVisualizationPlugin`) | qml plugin | as above | `:datavisualizationqmlplugin` (:500) | **partial** | `qt_qml_plugin_cpp` produces an empty plugin (no `qml_register_types_QtDataVisualization` reference) |

### Missing/partial sources
- C++ sources: none missing (84/84 `.cpp` match).
- DataVisualizationQml `QML_FILES` (`UP/qtdatavis3d/src/datavisualizationqml/CMakeLists.txt:4-11,49`, `QT_QML_SKIP_QMLDIR_ENTRY`): `designer/{Bars3D,Scatter3D,Surface3D}Specifics.qml` and `designer/default/{Bars3D,Scatter3D,Surface3D}.qml`. All missing.
- DataVisualizationQml `RESOURCES` (:13-21, 51): `designer/qtdatavisualization.metainfo` and 6 `designer/images/*.png`. Missing. Only Qt Design Studio uses them.
- MINGW `NO_UNITY_BUILD_SOURCES` (`src/datavisualization/CMakeLists.txt:416`): N/A (Bazel does not do unity builds).

### Feature deviations
| feature | CMake default (L/M/W) | Bazel | impact |
|---|---|---|---|
| — (`configure.cmake` only has `qt_extra_definition` version macros) | — | — | none |

### Other deviations
- MACOS `Qt::GuiPrivate` (`src/datavisualization/CMakeLists.txt:407`): Bazel uses `@qtbase//:gui`. This is fine if gui exposes private headers.
- `qtdatavisualizationqml` has no exports header. CMake uses `NO_GENERATE_CPP_EXPORTS`, so this is consistent.

### Ranked findings
- **High**: The QtDataVisualization QML module registers no types (no `qml_args`, `OV/qtdatavis3d/BUILD.bazel:425`; empty plugin from `qt_qml_plugin_cpp`, :492 / `QB/qt_tools.bzl:1051-1080`). `import QtDataVisualization` is non-functional. The C++ widget API (`Q3DBars` etc.) is unaffected.
- **Medium**: No embedded qmldir, no `plugins.qmltypes`, no metatypes output for QtDataVisualization. This breaks qmllint, qmlls and qmlcachegen consumers.
- **Low**: Designer QML_FILES and RESOURCES are missing (`src/datavisualizationqml/CMakeLists.txt:4-21`).

---

## qtgraphs
**Coverage**: 3 of 3 in-scope CMake targets present (2 partial: the Graphs QML module and its plugin register no types). GraphsWidgets is fully built but breaks at runtime as a consequence.

### Target inventory
| CMake target | kind | gating condition | Bazel target | status | notes |
|---|---|---|---|---|---|
| Graphs (URI `QtGraphs`, PAST_MAJOR_VERSIONS 1) | qml_module | Repo needs Gui, Widgets, Quick, QuickWidgets. Three shapes by `graphs_2d`/`graphs_3d` (`UP/qtgraphs/src/CMakeLists.txt:166,327`): 3D+2D (with Quick3D deps), 3D-only, 2D-only | `:graphs` (`OV/qtgraphs/BUILD.bazel:324`), `:graphs_shared` (auto) | **partial** | Hard-wired to the 3D+2D shape. All C++ sources and resources match. **No `qml_args`**, so no type registration, qmldir or qmltypes |
| graphsplugin (class `GraphsPlugin`) | qml plugin | as above | `:graphsplugin` (:587) | **partial** | Empty plugin from `qt_qml_plugin_cpp` (:579). Registers nothing |
| GraphsWidgets | module | `TARGET Qt::Widgets` (`src/graphs3d/CMakeLists.txt:163`) and inside `graphs_3d` | `:graphswidgets` (:660) | present (but broken at runtime) | All 4 srcs. Runtime needs QML `import QtGraphs` (`q3dgraphswidgetitem.cpp:1403`), which does not resolve. The static-build-only `qml_for_widget` resource and `qt_import_qml_plugins` (`widget/CMakeLists.txt:39-50`) are not replicated, and the target has no dep on `:graphsplugin` |

### Missing/partial sources
- C++: none missing. The raw diff showed 21 "missing" files, but these are false positives from the tracepoint source lists (`graphs2d/graphs2d/...`); every real `.cpp` (88) is present. All `CONDITION` branches (area, bar, donut-pie, line, scatter, spline, bars3d, scatter3d, surface3d) are compiled in, matching all-ON defaults.
- Resources:
  - `graphsshaders2D`: 10 `.qsb` files (pre-built in the source tree, so no `qt_internal_add_shaders`). Present.
  - `graphsshaders`: 27 raw GLSL files, aliases are the filename minus the dot. Present.
  - `graphsmeshes`: 30 `.mesh` files. Present. `${obj_resource_files}` is undefined/empty upstream.
  - `qml_component_resources` (2), `qml_axis_resources` (3), `qml_material_resources` (16) with aliases: present.
- Missing: designer files (`graphs2d/qml/designer/*`, `graphs3d/qml/designer/*`; metainfo, Specifics.qml, default/*.qml, images). Upstream only installs these via `qt_copy_or_install`. The `set(resources ...)` at `src/CMakeLists.txt:76-84` is never used. Low impact.
- Missing: `graphs3d/widget/qml/dummy.qml` (static builds only, `widget/CMakeLists.txt:39-50`).

### Feature deviations
| feature | CMake default (L/M/W) | Bazel | impact |
|---|---|---|---|
| graphs-2d, -2d-area, -2d-bar, -2d-donut-pie, -2d-line, -2d-scatter, -2d-spline | ON/ON/ON (no condition, `src/configure.cmake:1-37`) | 1 (`OV/qtgraphs/qtgraphs_config.bzl:5-11`); `USE_*` local_defines (`BUILD.bazel:471-477`) | matches; no knob to disable |
| graphs-3d, -3d-bars3d, -3d-scatter3d, -3d-surface3d | ON if `TARGET Qt6::Quick3D` (`configure.cmake:39-61`) | 1 (`qtgraphs_config.bzl:12-15`), hard dep on `@qtquick3d` | matches when qtquick3d is available. You cannot build 2D-only without Quick3D (CMake would just turn 3D off) |
| tracepoints (`qt_internal_generate_tracepoints`) | generated (no-op unless lttng/etw) | stub header `qtgraphs_tracepoints_p.h` (`qtgraphs_config.bzl:21-27`) | none on desktop. The stub is also listed in public generated hdrs (`BUILD.bazel:91`), which is cosmetic |

### Other deviations
- No qmldir, `plugins.qmltypes` or metatypes JSON for QtGraphs, so tooling cannot see the module.
- Deps: `@qtshadertools//:shadertools_shared` and `@qtbase//:concurrent_shared` are in `dynamic_deps` (:425, :439) but not in CMake LIBRARIES. Probably transitive; harmless.
- `graphs_sync_headers` excludes `src/graphs3d/widget/**` correctly (:81-84).

### Ranked findings
- **High**: The QtGraphs QML module registers no types. `graphs` lacks `qml_args` (`OV/qtgraphs/BUILD.bazel:324`), and `graphsplugin_gen` uses `qt_qml_plugin_cpp`, which drops `registration_func` (`:579-585`, `QB/qt_tools.bzl:1051-1090`). `import QtGraphs` (GraphsView, Bars3D, ...) is non-functional.
- **High**: QtGraphsWidgets (`Q3DBarsWidgetItem` etc.) is broken at runtime as a consequence. It instantiates QML `import QtGraphs` (`UP/qtgraphs/src/graphs3d/widget/q3dgraphswidgetitem.cpp:1403-1411`), and `:graphswidgets` (`OV/qtgraphs/BUILD.bazel:660`) does not link `:graphsplugin`.
- **Medium**: No qmldir, qmltypes or metatypes for QtGraphs (same root cause).
- **Medium**: Features are hard-wired (`OV/qtgraphs/qtgraphs_config.bzl:5-15`). There is no 2D-only build without qtquick3d, unlike CMake's `CONDITION TARGET Qt6::Quick3D` (`UP/qtgraphs/src/configure.cmake:39-61`).
- **Low**: Designer metainfo/QML/images are not shipped (`src/graphs{2d,3d}/qml/designer/CMakeLists.txt`). Install-only upstream.
- **Low**: The tracepoints stub header is listed as a public generated header (`OV/qtgraphs/BUILD.bazel:91`).

---

## qtcharts
**Coverage**: 3 of 3 in-scope CMake targets present (0 partial). The designer plugin is N/A.

### Target inventory
| CMake target | kind | gating condition | Bazel target | status | notes |
|---|---|---|---|---|---|
| Charts | module | Repo needs Gui and Widgets, plus `QT_FEATURE_graphicsview` (`UP/qtcharts/CMakeLists.txt`) | `:charts` (`OV/qtcharts/BUILD.bazel:164`), `:charts_shared` (:383) | present | Srcs are a glob of `src/charts/**/*.cpp`. Set-equal to the CMake union of all branches |
| ChartsQml (URI `QtCharts`, PLUGIN_TARGET `qtchartsqml2`, CLASS_NAME `QtChartsQml2Plugin`, PAST_MAJOR 1 2, DESIGNER_SUPPORTED) | qml_module | `TARGET Qt::Quick` (`src/CMakeLists.txt:9`) | `:chartsqml` (:415) with `qml_args`; `:chartsqml_shared` (:484) | present | Hand-written qmldir has linktarget, depends and designersupported (:461-469) |
| qtchartsqml2 | qml plugin | as above | `:qtchartsqml2` (:516) via `qt_qml_plugin_gen` | present | Correctly keeps `qml_register_types_QtCharts` |
| qtchartsdesigner | Designer plugin (`plugins/designer/CMakeLists.txt:10`) | **never built**: top-level `qt_build_repo()` does not add `plugins/` | — | N/A | Dead upstream qmake port |

### Missing/partial sources
- C++: none missing. The glob over `src/charts` and `src/chartsqml2` exactly equals the CMake source union, including `QT_FEATURE_opengl` (`glwidget.cpp`, `declarativeopenglrendernode.cpp`) and all chart-type and datetime-axis branches.
- ChartsQml designer files (`src/chartsqml2/designer/`: `ChartViewSpecifics.qml`, `qtcharts.metainfo`, 16 `default/*.qml`, 34 `images/*.png`): missing. Upstream only installs them via `qt_copy_or_install`. Low impact (Qt Design Studio only).
- No shaders or qrc in qtcharts.

### Feature deviations
| feature | CMake default (L/M/W) | Bazel | impact |
|---|---|---|---|
| charts-line/spline/area/scatter/bar/pie/boxplot/candlestick-chart | ON/ON/ON (spline, area, scatter need line) (`src/charts/configure.cmake:18-52`) | all 1 (`OV/qtcharts/BUILD.bazel:103-110`) | matches; no knob |
| charts-datetime-axis | ON if QT_COORD_TYPE is double (default) (`configure.cmake:54-64`) | 1 (:111) | matches the default |
| (uses) QT_FEATURE_opengl | ON on desktop | always compiled, deps opengl and openglwidgets | matches |

### Other deviations
- WIN32 `user32` PUBLIC link (`src/charts/CMakeLists.txt:295-298`) is not in the overlay. It is probably provided transitively by qtbase on MinGW (uncertain).
- `charts_shared` and `chartsqml_shared` are raw `cc_shared_library` (`OV/qtcharts/BUILD.bazel:383-399, 484-507`). They list `@qtbase//:dbus_shared` unconditionally (:387, :489), including on Windows; the macro drops it there.
- `qtcharts-config_p.h` duplicates the public config content under the same include guard (:122-126). Harmless.
- The exports header defines `QT_CHARTS_REMOVED_SINCE` as 0 (:63). Upstream generates a version-aware macro. This only matters for removed_api builds; low.

### Ranked findings
- **High**: none.
- **Medium**: Raw `cc_shared_library` puts `@qtbase//:dbus_shared` on Windows (`OV/qtcharts/BUILD.bazel:387,489`), unlike the macro's select (`QB/qt_internal_library.bzl:262-266`). There may be a Windows/MinGW link issue (uncertain).
- **Low**: WIN32 `user32` is not linked explicitly (`UP/qtcharts/src/charts/CMakeLists.txt:295-298`).
- **Low**: Designer QML/metainfo/images are not shipped (`UP/qtcharts/src/chartsqml2/designer/`).
- **Low**: Features are hard-coded with no knobs (`OV/qtcharts/BUILD.bazel:99-114`), though they match the defaults.


---

