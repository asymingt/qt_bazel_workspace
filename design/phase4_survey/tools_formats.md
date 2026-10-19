# Survey: Bazel vs upstream CMake — qttools, qttranslations, qtlanguageserver, qtsvg, qtimageformats, qt5compat (Qt 6.10.2)

Paths used below:
- `UP/<m>` = `/Users/simmers/Library/Caches/bazel/_bazel_simmers/223ccc89642a35e03547a0f3440b8160/external/<m>+`
- `BZ/<m>` = `/Users/simmers/development/qt_bazel_workspace/modules/<m>/6.10.2/overlay`
- How I checked: I compared sources automatically with a script that lists every `.cpp/.mm/.c` named in upstream `CMakeLists.txt`/`*.cmake` (skipping tests, examples and docs) against the string literals in `BUILD.bazel`. Script: `/Users/simmers/.gemini/jetski/brain/9c9c59dd-a165-4681-8aef-174b110a133e/scratch/srcdiff.py`. I read all non-trivial results by hand. Nothing was built.
- Shared macros: `qt_cc_binary` (`/Users/simmers/development/qt_bazel/qt.bzl:2640`) links `offscreen` plus the native QPA plugin into GUI binaries. `qt_static_plugin` (`qt_internal_library.bzl:282`) builds every plugin **static only** (Q_IMPORT_PLUGIN). None of the plugins below is loadable at runtime.

---

## qttools

**Coverage**: 24 of 27 in-scope CMake targets are present (3 partial). 3 are missing: `assistant`, `qlitehtml`, `QHelpEnginePlugin`.

Not counted:
- N/A: `Linguist` (a CMake-only header module), `QDocCatch*` (qdoc test helpers), `QAxWidgetPlugin`, `QWebViewPlugin`, `QView3DPlugin`.
- Not built upstream: `kmap2qmap`, `qev`.
- Not part of qttools in 6.10: windeployqt, macdeployqt and qtpaths live in **qtbase**.

### Target inventory
| CMake target | kind | gating condition (UP/qttools) | Bazel target | status | notes |
|---|---|---|---|---|---|
| Tools | internal header module + config headers | always (`src/global/CMakeLists.txt:8`) | `:tools_private` (BUILD:73-91) | present | Config comes from `qttools_config.bzl:7-44` |
| UiPlugin | header module | `TARGET Qt::Widgets` (`src/CMakeLists.txt:22`) | `:uiplugin` (BUILD:113) | present | |
| UiTools | module | `TARGET Qt::Widgets` | `:uitools` (BUILD:155) | present | All sources present. `dynamic_deps` includes `dbus_shared` (BUILD:186) on all OSes (unverified on mac/win) |
| Designer | module | `QT_FEATURE_designer` + `QT_FEATURE_process` (`src/designer/src/CMakeLists.txt:11`) | `:designer_lib` (BUILD:513) | present | Shared variant used, so the `QT_BUILD_SHARED_LIBS` source branch applies. All sources present |
| DesignerComponentsPrivate | module | same | `:designercomponents` (BUILD:1041) | present | |
| Help | module | `QT_FEATURE_assistant` → `fullqthelp` (`src/assistant/CMakeLists.txt:4-25`) | `:help` (BUILD:1508) | present | All `fullqthelp` widget sources present. **The `qsqlite` driver is not a dep**, see Other deviations |
| Linguist | CMake-only header module | | — | N/A | Only carries CMake packaging (`Qt6LinguistToolsMacros.cmake`) |
| QQuickWidgetPlugin | designer plugin | `QT_BUILD_SHARED_LIBS AND process` + `TARGET Qt::QuickWidgets` (`src/designer/src/CMakeLists.txt:16`, `plugins/CMakeLists.txt:10`) | `:qquickwidgetplugin` (BUILD:1397) | partial | Static only, and **not linked into `:designer`** (BUILD:2306-2322), so Designer has no QQuickWidget entry |
| QHelpEnginePlugin | help plugin | `TARGET Qt::Help AND TARGET Qt::QmlLSPrivate` (`src/assistant/plugins/CMakeLists.txt:4`) | — | missing | Comment at BUILD:1440 names `:helpplugin`, but no such target exists. It is only useful to qmlls, which @qtdeclarative doesn't build either, so low need |
| QAxWidgetPlugin | designer plugin | `TARGET Qt::AxContainer AND WIN32` | — | N/A | No qtactiveqt module |
| QWebViewPlugin | designer plugin | `TARGET Qt::WebKitWidgets` | — | N/A | |
| QView3DPlugin | designer plugin | not referenced by any parent CMakeLists | — | N/A | |
| lupdate | tool | `QT_FEATURE_linguist` | `:lupdate` (BUILD:1617) | present | Includes the `TARGET Qt::QmlPrivate` branch (`qdeclarative.cpp`) |
| lrelease | tool | linguist | `:lrelease` (BUILD:1685) | present | |
| lconvert | tool | linguist | `:lconvert` (BUILD:1739) | present | |
| lprodump | tool | linguist | `:lprodump` (BUILD:1799) | present | `registry.cpp` is WIN32-only upstream (`lprodump/CMakeLists.txt:60`) but is always compiled here. Guarded in the source, so harmless |
| lrelease-pro / lupdate-pro | tool | linguist | `:lrelease_pro`, `:lupdate_pro` (BUILD:1858, 1883) | present | |
| qhelpgenerator | tool | `QT_FEATURE_assistant` (+Sql, PrintSupport) | `:qhelpgenerator` (BUILD:1944) | present | Upstream static build imports `QMinimalIntegrationPlugin`. Bazel uses `offscreen` + `qsqlite` instead. The generated `qhelpgenerator_plugin_imports_custom.cpp` (`qttools_config.bzl:46`) is never used |
| qdoc | tool | `QT_FEATURE_qdoc` (needs libclang ≥17 + QmlPrivate) (`configure.cmake:43-47`) | `:qdoc` (BUILD:1971) | present | Built against LLVM 21.1.8 from source (`@llvm-project`). `LIBCLANG_VERSION_MAJOR=21` is hard-coded (BUILD:2119) |
| qtattributionsscanner | tool | `commandlineparser` | `:qtattributionsscanner` (BUILD:2142) | present | |
| assistant | app | `QT_FEATURE_assistant` + Sql + PrintSupport | — | **missing** | Plausibly needed by users who browse .qch docs. `QT_FEATURE_assistant -1` (`qttools_config.bzl:27`) |
| qlitehtml / litehtml / gumbo | 3rdparty static lib | assistant only (`src/assistant/CMakeLists.txt:38-61`) | — | missing | Needed only for assistant |
| designer | app | `QT_FEATURE_designer` | `:designer` (BUILD:2306) | partial | No PrintSupport (upstream `designer/CMakeLists.txt:90-93`, so the print action is compiled out at `qdesigner_actions.cpp:54`). No designer plugins linked. Can't load custom-widget plugins at runtime (static Qt plugin model) |
| linguist | app | linguist + process/png/mdiarea/syntaxhighlighter + Quick (`src/linguist/CMakeLists.txt:17-21`) | `:linguist` (BUILD:2481) | partial | Print support forced off with a `-include` hack (`qttools_config.bzl:57-63`, BUILD:2423-2426), so `printout.cpp` is dropped. The upstream QtQuick/Controls static plugin list (`linguist/CMakeLists.txt:69-107`) is not linked; only `quicklayouts`/`quickwidgets` are |
| pixeltool | app | pixeltool feature | `:pixeltool` (BUILD:2602) | present | No macOS bundle plist |
| qdistancefieldgenerator | app | needs Quick | `:qdistancefieldgenerator` (BUILD:2689) | present | |
| qdbus | app | `QT_FEATURE_qdbus` + `dom` | `:qdbus` (BUILD:2499) | present | |
| qdbusviewer | app | qdbus + widgets features | `:qdbusviewer` (BUILD:2567) | present | |
| qtdiag | app | commandlineparser + Gui | `:qtdiag` (BUILD:2614) | present | `NETWORK_DIAG` + OpenGL + Widgets branches all taken |
| qtplugininfo | app | commandlineparser + library | `:qtplugininfo` (BUILD:2637) | present | Static plugin model: it can only inspect external `.so`/`.dll` plugins |
| kmap2qmap | app | dir exists but is never `add_subdirectory`'d (`src/CMakeLists.txt`) | — | N/A | Bazel config still says `QT_FEATURE_kmap2qmap 1` (cosmetic) |
| qev | app | never `add_subdirectory`'d | `:qev` (BUILD:2718) | extra | Harmless addition |

### Missing/partial sources (grouped by condition)
- `assistant` app (all of `src/assistant/assistant/*.cpp`). Includes the UNIX branch `stdinlistener.cpp`, the WIN32 branch `stdinlistener_win.cpp`, the textbrowser branch `helpviewerimpl_qtb.cpp`, and the qlitehtml + litehtml + gumbo 3rdparty sources (about 80 files).
- `src/assistant/plugins/help/qhelpengineplugin.cpp` (gated on QmlLSPrivate).
- `QT_FEATURE_printsupport` branch of linguist: `src/linguist/linguist/printout.cpp` (`linguist/CMakeLists.txt:60-65`). It is deliberately disabled even though qtbase has `QT_FEATURE_printsupport 1` (`BZ/qtbase/qtbase_config.bzl:221`).
- Designer `TARGET Qt::PrintSupport` (link only, no extra sources): not linked.
- N/A plugins: activeqt (5 files), qwebview, view3d.
- Every other CMake source, including the `fullqthelp`, `QT_BUILD_SHARED_LIBS`, QmlPrivate and opengl branches, **is present** (srcdiff found 0 other gaps and 0 unknown extra sources).

### Feature deviations
| feature | CMake default (L/M/W) | Bazel (`qttools_config.bzl`) | impact |
|---|---|---|---|
| assistant | ON/ON/ON (shared build, or the sqlite plugin is available) | -1 (L27) | No Assistant app. qhelpgenerator and QtHelp are still built |
| fullqthelp | ON/ON/ON | 1 (L13) | matches |
| clang / qdoc | ON only if a libclang ≥17 is found; usually OFF in plain desktop builds | 1 / 1 (L28,L30) | qdoc is always built, which pulls a full LLVM/clang build into anything that depends on `:qdoc` (cost, not correctness) |
| clang-rtti | depends on `LLVM_ENABLE_RTTI` | 1 (L29) | **Uncertain**: if the `@llvm-project` overlay builds with `-fno-rtti`, this value is wrong. Only affects qdoc code paths gated on it |
| designer, linguist, pixeltool, distancefieldgenerator, qtattributionsscanner, qtdiag, qtplugininfo, qdbus | ON | 1 | matches |
| kmap2qmap / qev | feature ON, but no target is built | 1 | cosmetic only |
| printsupport (qtbase feature, as seen by linguist) | ON | forced -1 for linguist only | No print in Linguist |
| zstd (Designer `CONDITION QT_FEATURE_zstd`, `src/designer/src/lib/CMakeLists.txt:434`) | ON when libzstd is found | qtbase `QT_FEATURE_zstd -1` (`qtbase_config.bzl:41`) | Link-only in Designer; no visible effect |

### Other deviations
- **Static plugin model**: Designer custom-widget plugins, help plugins and qtplugininfo targets can't be loaded at runtime. Designer won't show QQuickWidget, and third-party Designer plugins can't be used.
- **QtHelp runtime needs QSQLITE**: `:help` (BUILD:1600-1609) doesn't depend on `@qtbase//:qsqlite`. Apps using `QHelpEngine` must link `qsqlite` themselves or the engine fails at runtime. Only `:qhelpgenerator` adds it (BUILD:1958).
- **Translations**: designer and linguist look up `designer_*.qm`/`linguist_*.qm` through `QLibraryInfo::TranslationsPath`. That resolves to `<prefix>/translations` (`qtbase_config.bzl:239-258`), which doesn't exist in runfiles, so the tools run untranslated.
- `COMMON_LOCAL_DEFINES` (BUILD:48-69) adds `QT_NO_EXCEPTIONS` for everything. qdoc and lupdate upstream build with exceptions enabled for clang/tooling code (**uncertain**; worth checking qdoc's `qt_internal_set_exceptions_flags`).
- No XDG desktop files, macOS bundles/Info.plist, or Windows `.rc` files for any app (upstream `*/CMakeLists.txt` APPLE/WIN32 blocks).

### Ranked findings
- **High**: Assistant is missing (`qttools_config.bzl:27` sets assistant -1). Upstream builds it by default on all three desktops (`UP/qttools/configure.cmake:30-34`), and it also needs qlitehtml, litehtml and gumbo (`src/assistant/CMakeLists.txt:38`).
- **Medium**: `:help` lacks the `@qtbase//:qsqlite` dep (BZ/qttools/BUILD.bazel:1600-1609), so QHelpEngine silently fails without it.
- **Medium**: Linguist print support is force-disabled (`qttools_config.bzl:57-63`, BUILD:2423-2426) and `printout.cpp` is dropped, although qtbase provides printsupport.
- **Medium**: Linguist doesn't link the QtQuick/Controls QML plugins that upstream links in static builds (`UP/qttools/src/linguist/linguist/CMakeLists.txt:69-107`), so QML form preview may fail (**unverified**).
- **Medium**: Designer: no PrintSupport and no `:qquickwidgetplugin` linked (BUILD:2306-2322 vs `UP/.../designer/CMakeLists.txt:90`).
- **Low**: qdoc is always on (`qttools_config.bzl:28-30`) and builds LLVM 21 from source. `clang_rtti 1` is unverified.
- **Low**: `QHelpEnginePlugin` is missing but its consumer (qmlls/QmlLSPrivate) isn't built either (`UP/qttools/src/assistant/plugins/CMakeLists.txt:4`).
- **Low**: Unused generated `qhelpgenerator_plugin_imports_custom.cpp` (`qttools_config.bzl:46`). Extra `qev` binary. `kmap2qmap 1` feature with no target.

---

## qttranslations

**Coverage**: 1 of 1 build target (`updateqm`) present, partial. `catalogs.json` is not produced.

### Target inventory
| CMake target | kind | gating condition | Bazel target | status | notes |
|---|---|---|---|---|---|
| updateqm (`qt_add_lrelease`) | custom (.ts → .qm) | `TARGET lupdate AND lconvert` (`src/CMakeLists.txt:4`) | `:translations` / `:qm_files` (BZ/qttranslations/BUILD.bazel:48-59) | partial | All 291 `translations/*.ts` plus 13 generated `*_en.ts` are compiled. The `_en` list (BUILD:23-37) matches upstream's `add_ts_targets` calls plus `qt_en` (`translations/CMakeLists.txt:179-220`) |
| catalogs.json | install file | always (`translations/CMakeLists.txt:224-251`) | — | missing | Used by deployment tooling to pick module catalogs. Low need |
| ts-* / lupdate targets, stage-ts, tika_for_ts | developer targets | | — | N/A | Maintainer workflow only |

### Missing/partial sources
- None. `.ts` coverage matches: upstream `file(GLOB *.ts)` vs Bazel `glob(["translations/*.ts"])`.

### Feature deviations
- None (the module has no `qt_feature`s).

### Other deviations
- **Lookup/installation**: upstream installs to `${INSTALL_TRANSLATIONSDIR}`, where `QLibraryInfo::path(TranslationsPath)` finds the files. Bazel writes to `translations_qm/<stem>.qm` (`qt_tools.bzl:1632`). The smoke test loads them by explicit runfiles path (`tests/m7_smoke/main.cc:107-113`). `QLibraryInfo::TranslationsPath` resolves to `<prefix>/translations` (`BZ/qtbase/qtbase_config.bzl:239-258`, `qt_prfxpath=` empty), so Qt tools and apps that use the standard lookup won't find any `.qm`.
- The `qt_XX.qm` meta-catalogs depend on `qtbase_XX.qm` etc. being in the same directory. They are, because everything lands in one output dir.
- lrelease runs once per file with `-silent`, same as upstream.

### Ranked findings
- **Medium**: `.qm` files can't be found through `QLibraryInfo::TranslationsPath`. Users must pass runfiles paths manually (`qt_tools.bzl:1627-1647`, `qtbase_config.bzl:239`).
- **Low**: `catalogs.json` isn't generated (`UP/qttranslations/translations/CMakeLists.txt:224-251`).

---

## qtlanguageserver

**Coverage**: 2 of 2 in-scope CMake targets present (0 partial).

### Target inventory
| CMake target | kind | gating | Bazel target | status | notes |
|---|---|---|---|---|---|
| JsonRpcPrivate | INTERNAL STATIC module | always (`src/jsonrpc/CMakeLists.txt:8`) | `:jsonrpc` (BZ/qtlanguageserver/BUILD.bazel:94) | present | static, matches |
| LanguageServerPrivate | INTERNAL STATIC module | always (`src/languageserver/CMakeLists.txt:8`) | `:languageserver` (BUILD:152) | present | |

### Missing/partial sources
- None (srcdiff: 0 missing, 0 extra).

### Feature deviations
- None (`configure.cmake` only sets version definitions).

### Other deviations
- Upstream adds `/bigobj` (MSVC) or `-Wa,-mbig-obj` (MinGW) (`src/languageserver/CMakeLists.txt:29-33`). Bazel doesn't. On Windows/MinGW-clang COFF, `qlanguageservergen.cpp` *may* exceed the section limit (**uncertain**: clang usually emits `/bigobj` automatically when needed, GNU as doesn't).
- Bazel's `common_local_defines` spells `QT_NO_AS_CONST=1` (BUILD:44), while the other modules use `QT_NO_QASCONST=1`. That looks like a typo, but it only disables a deprecation helper.
- No consumer in this workspace: qmlls/QmlLSPrivate isn't built by @qtdeclarative (no `qmlls` target in `BZ/qtdeclarative/BUILD.bazel`), so these libraries are effectively unused.

### Ranked findings
- **Low**: Missing big-obj flag for Windows (`UP/qtlanguageserver/src/languageserver/CMakeLists.txt:29-33`).
- **Low**: `QT_NO_AS_CONST` typo (BZ/qtlanguageserver/BUILD.bazel:44).
- **Low**: The module is built but has no consumer (qmlls absent).

---

## qtsvg

**Coverage**: 4 of 4 in-scope CMake targets present (0 partial).

### Target inventory
| CMake target | kind | gating | Bazel target | status | notes |
|---|---|---|---|---|---|
| Svg | module | `xmlstreamreader` + Gui (`src/CMakeLists.txt:4-6`) | `:svg` (BZ/qtsvg/BUILD.bazel:102) | present | The `cssparser` branch is included (qtbase `cssparser 1`). Uses system `@zlib` (= `system_zlib` branch) |
| SvgWidgets | module | Widgets | `:svgwidgets` (BUILD:195) | present | `graphicsview` branch included |
| QSvgPlugin (imageformats) | plugin | `imageformatplugin` | `:qsvg` (BUILD:240) | present (static) | |
| QSvgIconPlugin (iconengines) | plugin | always | `:qsvgicon` (BUILD:266) | present (static) | |

### Missing/partial sources
- None (srcdiff: 0 missing, 0 extra). The stray upstream file `src/svg/qsvgcsshandler.cpp` is correctly unused; `css/` is used.

### Feature deviations
- No qtsvg-specific features. Dependent qtbase features (`cssparser`, `imageformatplugin`, `graphicsview`, compress) are enabled, so the `svg.json`/`qsvgiconengine.json` metadata (compressed `svgz`) is selected as upstream.

### Other deviations
- Plugins are static only, so consumers must depend on `:qsvg`/`:qsvgicon` explicitly to get SVG in QImage/QIcon. Upstream loads them automatically from `plugins/`.
- `qt_attribution.json` licence (XSVG) is reflected (BUILD:15-23).

### Ranked findings
- **Low**: The static plugin model means SVG support in QImage/QIcon must be opted into explicitly (BZ/qtsvg/BUILD.bazel:240, 266). Otherwise matches upstream.

---

## qtimageformats

**Coverage**: 5 of 7 in-scope CMake targets present (0 partial). Missing: `qmacheif`, `qmacjp2` (macOS only).

`qmng`/`qjp2` are N/A (conditional): upstream only builds them when system libmng/jasper are found, which isn't the case by default.

### Target inventory
| CMake target | kind | gating (`UP/qtimageformats/src/plugins/imageformats/CMakeLists.txt`) | Bazel target | status | notes |
|---|---|---|---|---|---|
| QTgaPlugin | plugin | always (L12) | `:qtga` (BZ/qtimageformats/BUILD.bazel:70) | present | |
| QWbmpPlugin | plugin | always (L13) | `:qwbmp` (BUILD:90) | present | |
| QTiffPlugin | plugin | `QT_FEATURE_tiff` (L14) | `:qtiff` (BUILD:124) | present | Uses BCR `@libtiff` 4.7.2 (= `system-tiff` branch) instead of bundled `src/3rdparty/libtiff`. BCR enables LZW/ZIP and also builds `tif_jpeg.c`, a superset of the bundled config (bundled: `JPEG_SUPPORT` undefined, `tiffconf.h:71`) |
| QWebpPlugin | plugin | `QT_FEATURE_webp` (L17) | `:qwebp` (BUILD:141) | present | Uses BCR `@libwebp` 1.6.0 (`system-webp`), which includes demux/mux as `qwebphandler.cpp:6` requires |
| QICNSPlugin | plugin | `QT_FEATURE_regularexpression` (L23) | `:qicns` (BUILD:107) | present | Upstream adds `QT_NO_CAST_FROM_ASCII` (`icns/CMakeLists.txt:14-16`); Bazel doesn't (trivial) |
| QMacHeifPlugin | plugin | `APPLE` (L20) | — | **missing** | HEIF/HEIC via ImageIO on macOS. Plausibly needed by Mac users |
| QMacJp2Plugin | plugin | `APPLE AND NOT jasper` (L32) | — | **missing** | JPEG 2000 via ImageIO on macOS |
| QMngPlugin | plugin | `QT_FEATURE_mng` (system libmng found) | — | N/A (cond.) | Not in BCR; off by default upstream too |
| QJp2Plugin | plugin | `QT_FEATURE_jasper` (system jasper found) | — | N/A (cond.) | Same |
| (dds) | — | not referenced upstream | — | N/A | |

### Missing/partial sources (grouped by condition)
- `APPLE`: `macheif/main.cpp`, `macheif/qmacheifhandler.cpp`, `macjp2/main.cpp`, `macjp2/qmacjp2handler.cpp`, `shared/qiiofhelpers.mm` (+ CoreFoundation/CoreGraphics/ImageIO frameworks, `NS_IIOF_HELPERS=` define).
- `NOT QT_FEATURE_system_tiff` / `NOT system_webp` (bundled libtiff/libwebp incl. the NEON SIMD part): replaced by BCR libs. This is an intended deviation.
- `mng`/`jp2`: conditional, N/A.

### Feature deviations
| feature | CMake default (L/M/W) | Bazel | impact |
|---|---|---|---|
| tiff / system-tiff | ON / ON if a system libtiff is found, else bundled | always the BCR libtiff | Different libtiff version/codecs (superset). Fine |
| webp / system-webp | ON / system if found, else bundled | always the BCR libwebp | Fine |
| mng | OFF unless libmng is found | not built | none |
| jasper | OFF unless found | not built | none, but the macOS fallback `macjp2` is also missing |
| qtimageformats-config.h / _p.h | generated (`CMakeLists.txt:4-10`) | not generated | No source includes it (checked), so no impact |

### Other deviations
- The aggregate `:imageformats` (BUILD:157-167) is described as "all Linux" plugins and is used on macOS too, so Mac users silently lose HEIF and JP2.
- Static plugins only: users must depend on each plugin or on `:imageformats`.

### Ranked findings
- **Medium**: The macOS `qmacheif`/`qmacjp2` plugins are missing (`UP/qtimageformats/src/plugins/imageformats/CMakeLists.txt:20-34`; BZ/qtimageformats/BUILD.bazel:62-64 says "Apple-only", but macOS is in scope).
- **Low**: System libtiff/libwebp from BCR replace the bundled copies (BUILD:123-155). Intended; behaviour slightly differs (libtiff gains JPEG-in-TIFF).
- **Low**: `qicns` lacks `QT_NO_CAST_FROM_ASCII` (`icns/CMakeLists.txt:14`).

---

## qt5compat

**Coverage**: 3 of 3 in-scope CMake targets present (0 partial). `util/unicode/codecs/big5` is a maintainer tool, N/A.

### Target inventory
| CMake target | kind | gating | Bazel target | status | notes |
|---|---|---|---|---|---|
| Core5Compat | module | always (`src/core5/CMakeLists.txt:10`) | `:core5compat` + `:core5compat_shared` (BZ/qt5compat/BUILD.bazel:125, 194) | present | Codec backend is chosen per OS with `select` (BUILD:145-166) |
| qtgraphicaleffectsplugin (Qt5Compat.GraphicalEffects) | QML module + plugin | `TARGET Qt::Gui AND Qt::Quick` (`src/CMakeLists.txt:7`) | `:qtgraphicaleffectsplugin_lib` + `:qtgraphicaleffectsplugin` (BUILD:317, 409) | present | 25 QML files (glob) and 30 prebuilt `.qsb` shaders (glob), same as upstream's lists |
| qtgraphicaleffectsprivate (…GraphicalEffects.private) | QML module + plugin | same | `:qtgraphicaleffectsprivate_lib` + `:qtgraphicaleffectsprivate` (BUILD:215, 283) | present | Depends on `@qtshadertools//:shadertools` as upstream |

### Missing/partial sources (grouped by condition)
- `QT_FEATURE_iconv AND textcodec AND NOT icu`: `codecs/qiconvcodec.cpp` is not built. Upstream only enables it on Linux without ICU. Bazel's qtbase has `QT_FEATURE_icu 1` on Linux (`qtbase_config.bzl:426`), so this is **consistent**.
- Codec branches, checked against qtbase icu = Linux 1 / macOS -1 / Windows -1 (`qtbase_config.bzl:366,396,426`):
  - Linux: `qicucodec.cpp` + ICU deps.
  - macOS: the 7 big codecs.
  - Windows: big codecs + `qwindowscodec.cpp`.
  - All three match the CMake conditions (`src/core5/CMakeLists.txt:58-86`).

### Feature deviations
| feature | CMake default (L/M/W) | Bazel (BUILD:79-83, hand-written header) | impact |
|---|---|---|---|
| textcodec | ON/ON/ON | 1 | match |
| codecs | ON/ON/ON | 1 | match |
| big_codecs | ON/ON/ON | 1 | match (sources compiled only when there is no ICU, as upstream) |
| iconv | L: OFF (ICU present), ON if no ICU; M: OFF (`NOT APPLE`); W: OFF | -1 everywhere | match given Bazel's ICU-on-Linux |
| icu (from qtbase) | L ON if found; M/W usually OFF | L1/M-1/W-1 | consistent |

### Other deviations
- The generated `qmldir` for `Qt5Compat.GraphicalEffects` (BUILD:333-391) omits the `designersupported` line that upstream emits for `DESIGNER_SUPPORTED` (`graphicaleffects5/CMakeLists.txt:39`). This only matters to Qt Design Studio/Creator.
- `qtcore5compatexports.h` and the config headers are hand-written (BUILD:45-100). Upstream uses `NO_GENERATE_CPP_EXPORTS` plus `qcore5global.h`. Functionally equivalent, but `QT_CORE5COMPAT_REMOVED_SINCE` is hard-wired to 0.
- `_COMMON_DEFINES` adds `QT_NO_EXCEPTIONS` and `QT_LEAN_HEADERS=1` (BUILD:25,31). These are not upstream defaults for Core5Compat; low risk.
- Plugins are static only, so QML apps must depend on `:qtgraphicaleffectsplugin`.

### Ranked findings
- **Low**: The `designersupported` line is missing from the GraphicalEffects qmldir (BZ/qt5compat/BUILD.bazel:333 vs `UP/qt5compat/src/imports/graphicaleffects5/CMakeLists.txt:39`).
- **Low**: Hand-written config/export headers (BUILD:45-100) will drift if features change (e.g. building qtbase without ICU on Linux would need `iconv`/`qiconvcodec.cpp` and a header edit).
- **Low**: Extra global defines `QT_NO_EXCEPTIONS`/`QT_LEAN_HEADERS` (BUILD:25-31).
