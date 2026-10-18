# Survey: Bazel vs CMake for qtwayland and qtvirtualkeyboard (Qt 6.10.2)

Paths used below:
- `UP_WL` = `/Users/simmers/Library/Caches/bazel/_bazel_simmers/223ccc89642a35e03547a0f3440b8160/external/qtwayland+`
- `UP_VKB` = `.../external/qtvirtualkeyboard+`
- `BZ_WL` = `/Users/simmers/development/qt_bazel_workspace/modules/qtwayland/6.10.2/overlay/BUILD.bazel`
- `BZ_VKB` = `/Users/simmers/development/qt_bazel_workspace/modules/qtvirtualkeyboard/6.10.2/overlay/BUILD.bazel`
- `QTBASE_CFG` = `/Users/simmers/development/qt_bazel_workspace/modules/qtbase/6.10.2/overlay/qtbase_config.bzl`
- `QT_BZL` = `/Users/simmers/development/qt_bazel/qt.bzl`, `QIL` = `/Users/simmers/development/qt_bazel/qt_internal_library.bzl`

Method: I only read and grepped files; I didn't run bazel. Bazel output-base artifacts exist only for
`@qtvirtualkeyboard//:virtualkeyboard` (core). None exist for the styles, plugin or wayland targets, so
my link and runtime claims about those are inferred from the code. They are not confirmed.

> In Qt 6.10, **QtWaylandClient, the `wayland` QPA plugin, client shells (xdg/wl/fullscreen),
> the bradient decoration, client hw-integration plugins, `qtwaylandscanner` and WaylandGlobal all moved to
> qtbase**. They are out of scope here; the overlay correctly uses `@qtbase//:wayland_client`,
> `@qtbase//:qtwaylandscanner` and `@qtbase//:src/3rdparty/wayland/...` XMLs. qtwayland now holds only the
> compositor, its QML modules, compositor hw-integration plugins, qt-shell/ivi-shell client shells, the
> adwaita decoration, and texture-sharing.

---

## qtwayland

**Coverage**: 18 of 21 in-scope CMake targets present (2 of those partial). 3 missing. 3 N/A.

### Target inventory

| CMake target | kind | gating condition | Bazel target | status | notes |
|---|---|---|---|---|---|
| WaylandClientFeaturesPrivate | module (header-only, internal) | `wayland_client` | `:wayland_client_features_private` (BZ_WL:137-156) | present | config_p.h lacks `wayland_decoration_adwaita` (unused in C++) |
| WaylandCompositor | module | `wayland_server` (NOT WIN32, wayland, waylandscanner) | `:wayland_compositor` (+`_shared`) (BZ_WL:257-497) | present | all 73 .cpp match |
| QtWayland.Compositor (qml on WaylandCompositor, PLUGIN_TARGET qwaylandcompositorplugin) | qml module + plugin | `TARGET Qt::Qml` | `qml_args` on `:wayland_compositor` + `:qwaylandcompositorplugin` (BZ_WL:455-533) | present | qmldir hand-written, has `depends QtQuick`, past major 1 |
| WaylandEglCompositorHwIntegrationPrivate | internal module | `wayland_egl` | `:wayland_egl_compositor_hw_integration_private` (BZ_WL:539-612) | present | `QT_EGL_NO_X11` unconditional; CMake uses `NOT egl_x11`, and qtbase `egl_x11=-1` (QTBASE_CFG:686), so they agree |
| WaylandCompositorXdgShell | qml module | `wayland_compositor_quick` | `:wayland_compositor_xdgshell` + plugin (BZ_WL:1070-1170) | present | |
| WaylandCompositorWLShell | qml module | same | `:wayland_compositor_wlshell` + plugin (BZ_WL:968-1068) | present | |
| WaylandCompositorIviapplication | qml module | same | `:wayland_compositor_iviapplication` + plugin (BZ_WL:618-718) | present | |
| WaylandCompositorQtShell | qml module | same | `:wayland_compositor_qtshell` + plugin (BZ_WL:822-966) | present | qt-shell-unstable-v1 server scanned |
| WaylandCompositorPresentationTime | qml module | same | `:wayland_compositor_presentationtime` + plugin (BZ_WL:720-820) | present | |
| WaylandTextureSharing (URI QtWayland.Client.TextureSharing) | qml module (manual plugin) | opengl AND Quick AND WaylandClient AND WaylandCompositor | `:wayland_texture_sharing_plugin` (BZ_WL:1183-1242) | **partial** | plain `qt_static_plugin`: no qmldir, no `-Muri`, no QML module registration |
| WaylandTextureSharingExtension (URI QtWayland.Compositor.TextureSharingExtension) | qml module (manual plugin) | above + `wayland_compositor_quick` | `:wayland_texture_sharing_extension_plugin` (BZ_WL:1244-1287) | **partial** | same issue |
| QWaylandQtShellIntegrationPlugin | plugin (wayland-shell-integration) | `wayland_client_qt_shell` | `:qwayland_qt_shell_integration_plugin` (BZ_WL:1401-1453) | present | |
| QWaylandIviShellIntegrationPlugin | plugin | `wayland_client_ivi_shell` | `:qwayland_ivi_shell_integration_plugin` (BZ_WL:1342-1399) | present | |
| QWaylandAdwaitaDecorationPlugin | plugin (wayland-decoration-client) | NOT WIN32, wayland_client, DBus, Svg | `:qwayland_adwaita_decoration_plugin` (BZ_WL:1293-1340) | present | no explicit `target_compatible_with` |
| QWaylandEglClientBufferIntegrationPlugin | plugin (wayland-graphics-integration-server) | `wayland_egl` | `:qwayland_egl_compositor_buffer_plugin` (BZ_WL:1455-1504) | present | |
| QWaylandEglStreamClientBufferIntegrationPlugin | plugin | `wayland_egl` | `:qwayland_eglstream_compositor_buffer_plugin` (BZ_WL:1630-1697) | present | |
| DrmEglServerBufferIntegrationPlugin | plugin | `wayland_drm_egl_server_buffer` | `:qwayland_drm_egl_compositor_buffer_plugin` (BZ_WL:1567-1628) | present | |
| ShmServerBufferIntegrationPlugin | plugin | `wayland_shm_emulation_server_buffer` | `:qwayland_shm_emulation_compositor_buffer_plugin` (BZ_WL:1506-1565) | present | |
| QWaylandDmabufClientBufferIntegrationPlugin (linux-dmabuf-unstable-v1) | plugin | `wayland_dmabuf_client_buffer` (EGL + libdrm compile test; ON on typical Linux) | — | **missing** | Plausibly needed: compositors serving modern Mesa/GPU clients using zwp_linux_dmabuf |
| DmaBufServerBufferIntegrationPlugin | plugin | `wayland_dmabuf_server_buffer` (compile test; usually ON on Linux) | — | **missing** | qtbase sets `-1` (QTBASE_CFG ~L659). Niche |
| VulkanServerBufferIntegrationPlugin | plugin | `wayland_vulkan_server_buffer` (needs vulkan) | — | **missing** | qtbase `vulkan=-1` (QTBASE_CFG:516). Niche |
| QWaylandBrcmClientBufferIntegrationPlugin | plugin | `wayland_brcm` (eglfs_brcm, RPi) | — | N/A | off by default |
| LibHybrisEglServerBufferIntegrationPlugin | plugin | libhybris test | — | N/A | hybris only |
| Vsp2HardwareLayerIntegrationPlugin | plugin | `wayland_layer_integration_vsp2` | — | N/A | Renesas only |

There are no `qt_internal_add_tool`, `qt_internal_add_app` or `qt_internal_add_3rdparty_library` targets in qtwayland 6.10, and the module has no `tools/` directory.

### Missing/partial sources (grouped by condition)

- **WaylandCompositor**: I extracted all 73 `.cpp` from `UP_WL/src/compositor/CMakeLists.txt`, including every
  `qt_internal_extend_target` branch (`opengl`, `wayland_compositor_quick`, `wayland_datadevice`, `im`,
  `draganddrop`, `opengl AND wayland_compositor_quick`). They match BZ_WL:259-332 exactly: 0 missing, 0 extra.
  Bazel compiles every branch unconditionally, which is correct for Linux defaults (all 1 in QTBASE_CFG).
- Shader resources (7 files, prefix `/qt-project.org/wayland/compositor`): match (BZ_WL:226-255).
- QML extension modules, client shells, adwaita, egl/eglstream/drm-egl/shm plugins: sources match.
- Missing plugin sources (whole targets): `src/hardwareintegration/compositor/linux-dmabuf-unstable-v1/{linuxdmabuf,linuxdmabufclientbufferintegration}.cpp`,
  `dmabuf-server/dmabufserverbufferintegration.cpp`, `vulkan-server/{vulkanserverbufferintegration,vulkanwrapper}.cpp`.

### Feature deviations

Compositor and client features: `UP_WL/src/{compositor,client}/configure.cmake`. Wayland hw features: qtbase
`src/gui/configure.cmake:1528-1606`. Bazel values: QTBASE_CFG (Linux block ~L653-668) and BZ_WL:78-125.

| feature | CMake default (L/M/W) | Bazel (Linux) | impact |
|---|---|---|---|
| wayland_server | 1 / 0* / 0 | 1 (also re-defined in public `qtwaylandcompositor-config.h`, BZ_WL:107; upstream PRIVATE) | Harmless duplicate. *macOS 0 in practice |
| wayland_compositor_quick (PUBLIC) | 1 / – / – | 1 | ok |
| wayland_dmabuf_client_buffer | 1 if EGL+libdrm test passes | **undefined** (not in BZ_WL:114-125) | No C++ uses it; only gates the missing linux-dmabuf plugin |
| wayland_layer_integration_vsp2 | 0 | undefined | none |
| wayland_client_ivi_shell / qt_shell | 1 | 1 (BZ_WL:94-95) | ok |
| wayland_decoration_adwaita | 1 (L, if DBus+Svg) / – / 0 | undefined (not in BZ_WL:88-99) | Not referenced by `QT_CONFIG`; plugin built anyway |
| wayland_egl | 1 | 1 | ok |
| wayland_drm_egl_server_buffer | 1 (test) | 1 | ok |
| wayland_shm_emulation_server_buffer | 1 | 1 | ok |
| wayland_dmabuf_server_buffer | usually 1 on Linux | **-1** | dmabuf-server plugin absent |
| wayland_vulkan_server_buffer | 1 if vulkan + test | **-1** (vulkan=-1) | vulkan-server plugin absent |
| wayland_brcm / libhybris | 0 | -1 | ok |
| egl_extension_platform_wayland | 1 if EGL 1.5 test | -1 | qtbase concern (uncertain) |

### Other deviations

- **Protocol coverage**: the 17 XMLs for the compositor module (BZ_WL:167-185) match CMake exactly. The
  qt-shell (server and client), ivi-application/controller, shm-emulation, drm-egl and wl-eglstream XMLs are all
  scanned. Two differences:
  - CMake also scans `server-buffer-extension.xml` locally in the shm/drm-egl/dmabuf/vulkan plugins. Bazel uses
    the module's `QtWaylandCompositor/private/qwayland-server-server-buffer-extension.h` instead. That works.
  - The client `WaylandTextureSharing` scans `server-buffer-extension.xml` in CMake but not in Bazel
    (BZ_WL:1176-1181). The generated code needs `qt_server_buffer_interface` from `@qtbase//:wayland_client`
    (qtbase BUILD ~L4651). Probably OK but unverified.
- **Not a static QML module (texture-sharing)**: the CMake versions are `qt_internal_add_qml_module` (URI,
  qmldir resource, `-Muri` moc option in static builds). Bazel builds a bare `qt_static_plugin` (QIL:282-418), so
  the QML imports likely can't be resolved (uncertain; not built).
- **Plugin form**: all plugins are static only (`Q_IMPORT_PLUGIN` auto-generated); there are no loadable `.so`.
  The factory loaders still find linked static plugins.
- **Platform gating**: only the protocol rules carry `target_compatible_with = linux` (`qt_wayland.bzl:142,181`).
  Other targets rely on transitive incompatibility. Every target lists `x11_shared`/`dbus_shared` in
  `dynamic_deps` even where CMake doesn't link them.
- `INTERFACE_AUTOMOC_MACRO_NAMES` (`Q_COMPOSITOR_DECLARE_QUICK_EXTENSION_*`) has no Bazel equivalent for
  downstream users. Low.

### Ranked findings

**High**
- (none that break the default Linux compositor path; the module and all QML extensions match CMake.)

**Medium**
- Missing `linux-dmabuf-unstable-v1` compositor plugin, which upstream enables by default on Linux with EGL+libdrm (`UP_WL/src/plugins/hardwareintegration/compositor/linux-dmabuf-unstable-v1/CMakeLists.txt`).
- Texture-sharing QML modules built as bare static plugins with no qmldir/URI (BZ_WL:1183-1287). QML import likely fails.

**Low**
- `dmabuf-server`/`vulkan-server` plugins missing; qtbase forces both features off (QTBASE_CFG ~L659-661, L516).
- Compositor config headers omit `wayland_dmabuf_client_buffer`/`vsp2` and duplicate `wayland_server` publicly (BZ_WL:101-125). Client-features config omits `wayland_decoration_adwaita` (BZ_WL:88-99). None are referenced by `QT_CONFIG`.
- Texture-sharing client doesn't scan `server-buffer-extension.xml` locally (BZ_WL:1176-1181); unverified.
- Adwaita, client-features and texture-sharing-extension have no explicit Linux `target_compatible_with`.

---

## qtvirtualkeyboard

**Coverage**: 14 of 15 in-scope CMake targets present (5 of those partial). 1 missing (Hunspell). Cerence, MyScript, Example-HWR and BundledHunspell are N/A.

### Target inventory

| CMake target | kind | gating condition | Bazel target | status | notes |
|---|---|---|---|---|---|
| VirtualKeyboard (URI QtQuick.VirtualKeyboard.Core) | module + qml | always | `:virtualkeyboard`, `:virtualkeyboard_shared` (BZ_VKB:159-236) | present | 29 .cpp match |
| VirtualKeyboardQml (URI QtQuick.VirtualKeyboard, PLUGIN qtvkbplugin) | module + qml | always | `:virtualkeyboardqml` + `:qtvkbplugin` (BZ_VKB:648-677) | **partial** | qmldir lacks `import` of Core/Layouts/Components, `depends`, PAST_MAJOR_VERSIONS 2 1 |
| VirtualKeyboardSettings | qml module | always | `:virtualkeyboardsettings` (BZ_VKB:242-266) | present | no past majors |
| qtvkbcomponentsplugin | qml module | always | `:qtvkbcomponents` (BZ_VKB:272-288) | present | 31 QML match |
| qtvkblayoutsplugin | qml module + resources | `vkb_layouts` | `:qtvkblayouts` (BZ_VKB:294-316) | present | layout set differs, see Other |
| qtvkbstylesplugin | qml module, manual plugin (image provider) | always | `:qtvkbstyles` (BZ_VKB:357-393), `generate_plugin=False` | **partial** | plugin never imported, so `qtvkbsvg` provider is missing; no import of Styles.Builtin |
| qtvkbbuiltinstylesplugin | qml module, manual plugin + 2 resources | NOT `vkb_no_builtin_style` | `:qtvkbbuiltinstyles` (BZ_VKB:322-355) | **partial** | `Q_INIT_RESOURCE` name mismatch |
| qtvkbpluginsplugin | qml module (imports only) | always | `:qtvkbplugins` (BZ_VKB:626-642) | **partial** | qmldir lacks imports of IM plugin modules |
| QVirtualKeyboardPlugin (platforminputcontexts) | plugin | always | `:qtvirtualkeyboardplugin` via `qt_plugin_FixMe` (BZ_VKB:679-697) | **partial** | Windows: no `imm32` (`plugin.cpp:37` `ImmDisableIME`) |
| qtvkbhangulplugin | qml module | `hangul` | `:qtvkbhangul` (BZ_VKB:399-426) | present | |
| qtvkbthaiplugin | qml module | `thai` | `:qtvkbthai` (BZ_VKB:428-449) | present | built without hunspell |
| qtvkbopenwnnplugin + BundledOpenwnn | qml module + 3rdparty | `openwnn` | `:qtvkbopenwnn` + `:openwnn` (BZ_VKB:451-514) | present | 20 sources match |
| qtvkbpinyinplugin + BundledPinyin | qml + 3rdparty + dict | `pinyin AND NOT cerence_xt9` | `:qtvkbpinyin` + `:pinyin` (BZ_VKB:516-569) | present | 16 sources and dict path match |
| qtvkbtcimeplugin + BundledTcime | qml + 3rdparty + dicts | `tcime` | `:qtvkbtcime` + `:tcime` (BZ_VKB:571-624) | present | 6 sources and 3 dicts match |
| HunspellInputMethod + qtvkbhunspellplugin | module + qml | `hunspell` = system_hunspell OR 3rdparty | — | **missing** | Commonly ON on Linux with libhunspell. Gives prediction/spell-check. Plausibly wanted |
| BundledHunspell | 3rdparty | `3rdparty_hunspell` | — | N/A | no sources in tree, so upstream is OFF too |
| Cerence (hwr/xt9/common) | plugins/libs | `cerence_sdk` | — | N/A | proprietary |
| qtvkbmyscriptplugin | plugin | `myscript` | — | N/A | proprietary |
| qtvkbexample(hwr)plugin | plugin | `example_hwr` (AUTODETECT FALSE) | — | N/A | demo |

### Missing/partial sources (grouped by condition)

- Core: unconditional and `vkb_desktop` (`desktopinputpanel.cpp`, `inputview.cpp`) are present (BZ_VKB:161-190). `vkb_record_trace_input` (`unipentrace.cpp`) is correctly off.
- Settings style-define branches are collapsed to `"default"` (BZ_VKB:255), which matches the default.
- Styles (7 QML/JS) and Builtin resources (56 files) match.
- OpenWNN, Pinyin and TCIme 3rdparty: glob equals CMake lists.
- Missing: `src/plugins/hunspell/module/*.cpp` (4 files).

### Feature deviations

Source: `UP_VKB/src/virtualkeyboard/configure.cmake` vs BZ_VKB:57-103 (one header for all platforms).

| feature | CMake default (L/M/W) | Bazel | impact |
|---|---|---|---|
| vkb_desktop / vkb_layouts / vkb_default_style | 1 | 1 | ok |
| vkb_retro_style / no_builtin / record_trace / arrow_keynav / sensitive_debug | 0 | -1 | ok |
| cerence_* / myscript / example_hwr | 0 | -1 | ok |
| system_hunspell | 1 if Hunspell found (L common, M Homebrew, W rare) | **-1** | No Hunspell module |
| hunspell | = system_hunspell | **-1** | No prediction; Thai without hunspell |
| 3rdparty_hunspell | 0 | -1 | ok |
| openwnn/pinyin/tcime/zhuyin/cangjie/hangul/thai | 1 | 1 | ok |
| vkb_lang_* (44) | 1 (zh_HK 0) | **not defined** | No C++ uses; you can't trim languages |

### Other deviations

- **qmldir lacks `import`/`depends`** (QT_BZL:2289-2296):
  - The Plugins module should import each IM plugin. Layouts do `Qt.createQmlObject('import QtQuick.VirtualKeyboard.Plugins; PinyinInputMethod {}')` (`UP_VKB/src/layouts/zh_CN/main.qml:11`), so **the CJK/Thai IMs are likely unreachable**.
  - `QtQuick.VirtualKeyboard` should import Core/Layouts/Components. `Styles` should import `Styles.Builtin`.
- **No PAST_MAJOR_VERSIONS**: `qt_qml_module` (QT_BZL:2247-2321) lacks the parameter, so `import ... 2.x` fails.
- **Manual plugins with `generate_plugin=False`** (BZ_VKB:344,367): compiled into `alwayslink` libs (QIL:251)
  without `QT_STATICPLUGIN`/`Q_IMPORT_PLUGIN`. Expected effects, none of them built yet:
  1. `initializeEngine` never runs, so `image://qtvkbsvg` is missing and the retro style breaks (`retro/style.qml:845,1014,1109`).
  2. `Q_INIT_RESOURCE(virtualkeyboard_default_style/retro_style)` (`qtquickvirtualkeyboardbstylesbuiltinplugin.cpp:17-18`) doesn't match Bazel rcc names `vkb_*_style_qrc` (QT_BZL:2232). That gives undefined symbols at link.
  3. There is a possible duplicate extern "C" `qt_plugin_instance` from two non-static plugin mocs.
- **Layouts**: Bazel globs 265 files versus CMake's 195 core files. The extras are 49 handwriting layouts (gated at runtime by `VirtualKeyboardFeatures.Handwriting`, `components/Keyboard.qml:1777`) plus the CJK/Thai layouts that CMake ships in plugin resources. Same qrc prefix, so this is functionally equivalent.
- `qt_plugin_FixMe` (QT_BZL:2369) is a placeholder name. qmldir marks all plugins `optional`, while CMake uses `NO_PLUGIN_OPTIONAL`.

### Ranked findings

**High**
- Plugins qmldir lacks IM imports, so zh/ja/ko/th input likely breaks (QT_BZL:2289-2296; BZ_VKB:626-642; `layouts/zh_CN/main.qml:11`).
- Builtin styles: `Q_INIT_RESOURCE` names don't match `vkb_*_style_qrc`, likely a link error; also possible duplicate `qt_plugin_instance` (BZ_VKB:322-393; QT_BZL:2232; QIL:251). Unverified.
- `QtQuick.VirtualKeyboard` qmldir doesn't re-export Core/Layouts/Components (BZ_VKB:648-677).

**Medium**
- No PAST_MAJOR_VERSIONS on VKB QML modules (QT_BZL:2247-2321).
- Styles plugin never imported, so the `qtvkbsvg` provider is missing and the retro style breaks (BZ_VKB:357-393).
- Hunspell module/plugin missing (BZ_VKB:88-90).
- Windows: plugin lacks `imm32` (BZ_VKB:679-697; `src/plugin/CMakeLists.txt` WIN32 branch).

**Low**
- Extra handwriting layouts; CJK layouts bundled in core (BZ_VKB:294-302).
- `vkb_lang_*` undefined (BZ_VKB:77-103).
- `qt_plugin_FixMe` placeholder name (BZ_VKB:679).
