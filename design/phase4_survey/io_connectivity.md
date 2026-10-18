# Bazel vs CMake survey: IO / connectivity modules (Qt 6.10.2)

Modules covered: qtgrpc, qtremoteobjects, qtscxml, qtserialport, qtserialbus, qtsensors, qtpositioning, qtlocation, qtconnectivity.

Path aliases:
- `UP/<m>` = `/Users/simmers/Library/Caches/bazel/_bazel_simmers/223ccc89642a35e03547a0f3440b8160/external/<m>+/`
- `OV/<m>` = `/Users/simmers/development/qt_bazel_workspace/modules/<m>/6.10.2/overlay/`
- `QB` = `/Users/simmers/development/qt_bazel/`

## Notes that apply to every module

- **Plugins are static only.** `qt_static_plugin` (`QB/qt_internal_library.bzl:282`) builds an alwayslink `_impl` library and a generated `Q_IMPORT_PLUGIN` stub. It does not build a loadable `.so`/`.dll`. CMake builds dynamic plugins, and `QFactoryLoader` finds them at runtime under `plugins/<type>/`. Under Bazel, a binary only gets a backend (CAN, sensors, position, geoservices, scxmldatamodel) if it lists the plugin target in `deps`. Nothing selects a default per platform, except the QPA plugin (`QB/qt.bzl:2072`).
- **The `_QTBASE_SHARED_MAP` entries for plugins are incomplete** (`QB/qt.bzl:391-398, 441-535, 1755-1812`). The map has the module libraries, the QML plugins, `qtvirtualcanbus`, `qtsensors_dummy`, `qtsensors_generic`, `qtgeoservices_osm` and `qtgeoservices_itemsoverlay`. It does not have `qtsocketcanbus`, `qtpassthrucanbus`, `qtpeakcanbus`, `qttinycanbus`, `qtsensors_iio_sensor_proxy`, `qtposition_*`, `qtgeoservices_{esri,mapbox,nokia}` or `qscxmlecmascriptdatamodel`. If a binary depends on one of those plugins without also depending on its module library, the module gets linked in statically, so the binary may hold two copies of the module (an ODR risk).
- **Generated qmldir files differ from CMake.** `qt_qml_module` (`QB/qt.bzl:2289-2296`) always writes `optional plugin`, even where CMake uses `NO_PLUGIN_OPTIONAL` (qtlocation, qtpositioning). It also never writes `linktarget` or `depends` lines, e.g. CMake's `DEPENDENCIES QtQuick/auto QtPositioning/auto` for QtLocation. qmllint and qmlimportscanner tooling sees less dependency information. Runtime impact is low.
- **The user-facing codegen rules differ from CMake's.** CMake's `qt_add_protobuf`/`qt_add_grpc`/`qt6_add_repc_*`/`qt6_add_statecharts` map to `qt_protobuf_gen`/`qt_grpc_gen` (`QB/qt_protobuf.bzl`), `qt_rep` (`QB/qt_tools.bzl:1499-1558`) and `qt_scxml` (`QB/qt_tools.bzl:1560-1625`). Gaps are noted per module.

---

## qtgrpc
**Coverage**: 12 of 12 in-scope CMake targets present (0 partial).

### Target inventory
| CMake target | kind | gating condition | Bazel target | status | notes |
|---|---|---|---|---|---|
| Protobuf | module | always | `:protobuf` (+`_shared`) `OV/qtgrpc/BUILD.bazel:201` | present | srcs match `UP/qtgrpc/src/protobuf/CMakeLists.txt` |
| ProtobufWellKnownTypes | protobuf module | `protobuf_wellknowntypes` and qtprotobufgen available | `:protobufwellknowntypes` L280 | present | protos come from `@protobuf//:well_known_type_protos`; CMake uses an explicit list of 10 TYPES (uncertain whether the sets match) |
| ProtobufQtCoreTypes | protobuf module | `protobuf_qtcoretypes` | `:protobufqtcoretypes` L360 | present | |
| ProtobufQtGuiTypes | protobuf module | `TARGET Qt6::Gui AND protobuf_qtguitypes` | `:protobufqtguitypes` L441 | present | |
| ProtobufQuick + `protobufquickplugin` | qml module + plugin | `protobufquick` (Qt::Quick) | `:protobufquick` L515, `:protobufquickplugin` L569 | present | CMake PUBLIC_LIBRARIES Qt::Quick; Bazel depends only on qml (low impact) |
| Grpc | module | `grpc` (Network + http) | `:grpc` L626 | present | srcs match |
| GrpcQuick + `grpcquickplugin` | qml module + plugin | `grpcquick` | `:grpcquick` L723, `:grpcquickplugin` L825 | present | QmlNetwork is unconditional in Bazel; CMake only adds it with `qml_ssl` |
| QtProtocCommon | static lib (tool helper) | qtprotobufgen or qtgrpcgen | `:qtprotoccommon` L70 | present | |
| qtprotobufgen | tool | `qtprotobufgen` | `:qtprotobufgen` L108 | present | |
| qtgrpcgen | tool | `qtgrpcgen` | `:qtgrpcgen` L140 | present | includes the server printer |

(The 12 count includes the two QML plugins; `:protoc` is an alias to `@protobuf//:protoc`, L103.)

### Missing/partial sources
- None. Every `SOURCES` entry in protobuf, grpc, wellknown, the qttypes modules and grpcquick is accounted for. `src/protobufquick` contains only a header.

### Feature deviations (`OV/qtgrpc/qtgrpc_config.bzl:7-45`)
| feature | CMake default (L/M/W) | Bazel | impact |
|---|---|---|---|
| grpc, grpcquick | 1/1/1 | 1 | none |
| qtprotobufgen, qtgrpcgen | 1 when protobuf is found | 1 | none |
| protobuf_qtcoretypes / qtguitypes / wellknowntypes | 1 | 1 | none |
| protobufquick | 1 | 1 | none |
| protobuf_unsafe_registry | -1 | -1 | none |

### Other deviations
- `qt_protobuf_gen` (`QB/qt_protobuf.bzl:21-134`) has no QML mode. CMake's `qt_add_protobuf(... QML QML_URI ...)` also creates a QML module with type registration; Bazel can only pass `extra_opts` and never runs qmltyperegistrar on the output. `qt_grpc_gen` does have `qml = True` for gRPC clients (L221-226).

### Ranked findings
- **Medium**: `qt_protobuf_gen` cannot produce QML-registered message types the way `qt_add_protobuf(QML)` does (`QB/qt_protobuf.bzl:112-134`).
- **Low**: The well-known-types proto set is taken from `@protobuf//:well_known_type_protos` instead of CMake's 10-type list (`OV/qtgrpc/BUILD.bazel:250-258` vs `UP/qtgrpc/src/wellknown/CMakeLists.txt`). Uncertain whether extra protos are included.
- **Low**: The GrpcQuick → QmlNetwork dependency is unconditional (`OV/qtgrpc/BUILD.bazel:738-753`).

---

## qtremoteobjects
**Coverage**: 5 of 5 in-scope CMake targets present (0 partial).

### Target inventory
| CMake target | kind | gating condition | Bazel target | status | notes |
|---|---|---|---|---|---|
| RepParser | header module | always | `:repparser_headers` + `:repparser_grammar` (qlalr) `OV/qtremoteobjects/BUILD.bazel:32-54` | present | |
| RemoteObjects | module | `localserver` | `:remoteobjects`, `:remoteobjects_shared` L132/L182 | present | srcs match (QNX sources are N/A) |
| RemoteObjectsQml | qml module | TARGET Qt::Quick | `:remoteobjectsqml` L217 | present | |
| declarative_remoteobjects | qml plugin | with RemoteObjectsQml | generated by `qt_qml_module` | present | |
| repc | tool | `commandlineparser` | `:repc` L56 | present | `RO_INSTALL_HEADERS` is set to an odd value (L78), but repc sources never use it |

### Missing/partial sources
- `CONDITION QNX` (qconnection_qnx_*): N/A.

### Feature deviations
| feature | CMake default (L/M/W) | Bazel | impact |
|---|---|---|---|
| use_ham | -1 (QNX only) | no config header; `QT_NO_USE_HAM=1` local define (L171) | none. No source includes `qtremoteobjects-config.h`. |

### Other deviations
- `qt_rep` supports modes `replica|source|merged` (`QB/qt_tools.bzl:1534-1547`). It does not support repc's header→.rep mode (`qt6_reps_from_headers`).

### Ranked findings
- **Low**: `qt_rep` lacks the header-to-rep mode (`QB/qt_tools.bzl:1499-1558`).
- **Low**: The qmldir has no `depends`/`linktarget` lines (`QB/qt.bzl:2289-2296`).

---

## qtscxml
**Coverage**: 9 of 9 in-scope CMake targets present (0 partial).

### Target inventory
| CMake target | kind | gating | Bazel target | status | notes |
|---|---|---|---|---|---|
| ScxmlGlobalPrivate | internal header module | always | `:scxmlglobal_headers` + config `OV/qtscxml/BUILD.bazel:34-53` | present | |
| Scxml | module | `scxml` | `:scxml`, `:scxml_shared` L102/L133 | present | srcs match |
| qscxmlc | tool | `scxml AND commandlineparser` | `:qscxmlc` L158 (+templates qrc L146) | present | |
| ScxmlQml | qml module | `scxml_qml` | `:scxmlqml` L227 | present | |
| declarative_scxml | qml plugin | with ScxmlQml | generated by `qt_qml_module` | present | |
| QScxmlEcmaScriptDataModelPlugin | plugin (`scxmldatamodel`) | `scxml_ecmascriptdatamodel` | `:qscxmlecmascriptdatamodel` L269 (`qt_plugin_FixMe`) | present | static only; ECMAScript datamodel charts need it listed in deps explicitly |
| StateMachine | module | `statemachine` | `:statemachine` L338 | present | includes `qeventtransition` gui/ sources |
| StateMachineQml | qml module | `statemachine_qml` | `:statemachineqml` L426 | present | |
| qtqmlstatemachine | qml plugin | with StateMachineQml | generated by `qt_qml_module` | present | |

### Missing/partial sources
- None.

### Feature deviations (`OV/qtscxml/BUILD.bazel:37-44`)
| feature | CMake (L/M/W) | Bazel | impact |
|---|---|---|---|
| scxml, scxml_qml, scxml_ecmascriptdatamodel, statemachine, statemachine_qml, qeventtransition | 1/1/1 | 1 | none |

### Other deviations
- The ECMAScript plugin uses the `qt_plugin_FixMe` macro (`QB/qt.bzl:2369`); the name suggests a temporary wrapper around `qt_static_plugin`.
- `QScxmlDataModel::createScxmlDataModel("ecmascript")` loads the plugin through `QFactoryLoader`. Under Bazel it must be in `deps`; otherwise ECMAScript-datamodel charts fail at runtime.

### Ranked findings
- **Medium**: The ECMAScript datamodel plugin is not pulled in automatically with `:scxml`/`:scxmlqml` (`OV/qtscxml/BUILD.bazel:269-288`). It is a silent runtime failure.
- **Low**: The plugin is missing from `_QTBASE_SHARED_MAP` (`QB/qt.bzl:647-680`).

---

## qtserialport
**Coverage**: 1 of 1 in-scope CMake targets present (0 partial).

### Target inventory
| CMake target | kind | gating | Bazel target | status | notes |
|---|---|---|---|---|---|
| SerialPort | module | always | `:serialport`, `:serialport_shared` `OV/qtserialport/BUILD.bazel:100-155` | present | |

### Missing/partial sources
- `WIN32`, `UNIX`, `MACOS` and `UNIX AND NOT FREEBSD AND NOT MACOS` branches all match (L106-121). The FREEBSD branch is N/A.
- `CONDITION QT_FEATURE_libudev AND UNIX` (define `LINK_LIBUDEV`, link libudev) is not applied. `qserialportinfo_unix.cpp:303,356` then loads libudev at runtime via `QLibrary`, and falls back to sysfs.

### Feature deviations
| feature | CMake (L/M/W) | Bazel | impact |
|---|---|---|---|
| ntddmodm | -/-/1 when the header test passes | not defined, so the GUID is defined locally (`qserialportinfo_win.cpp:27`) | none in practice |
| libudev (qtbase) | 1 on Linux if found | runtime dlopen | low |

### Ranked findings
- **Low**: No direct libudev link on Linux (`OV/qtserialport/BUILD.bazel:106-110`). Behaviour is the same if libudev.so.1 is present at runtime.

---

## qtserialbus
**Coverage**: 7 of 9 in-scope CMake targets present (0 partial).

### Target inventory
| CMake target | kind | gating | Bazel target | status | notes |
|---|---|---|---|---|---|
| SerialBus | module | always | `:serialbus`, `:serialbus_shared` `OV/qtserialbus/BUILD.bazel:128-197` | present | Modbus TCP and RTU (serialport) included |
| VirtualCanBusPlugin | plugin canbus | always | `:qtvirtualcanbus` L199 | present | |
| SocketCanBusPlugin | plugin | `socketcan` (Linux) | `:qtsocketcanbus` L221 (linux-only) | present | |
| PassThruCanBusPlugin | plugin | `library` | `:qtpassthrucanbus` L248 | present | |
| PeakCanBusPlugin | plugin | `library` | `:qtpeakcanbus` L276 | present | |
| TinyCanBusPlugin | plugin | `library` | `:qttinycanbus` L301 | present | |
| SystecCanBusPlugin | plugin | `library AND WIN32` | none | **missing** | Windows-only SYS TEC hardware; niche |
| VectorCanBusPlugin | plugin | `library AND WIN32 AND NOT WINRT` | none | **missing** | Windows-only Vector XL hardware; plausible in automotive setups |
| canbusutil | app | commandlineparser, desktop | `:canbusutil` L340 | present | **links no CAN plugins** (see findings) |

### Missing/partial sources
- `CONDITION QT_FEATURE_modbus_serialport`: present (L148-149).
- Plugin sources all match.

### Feature deviations (`OV/qtserialbus/BUILD.bazel:69-99`)
| feature | CMake (L/M/W) | Bazel | impact |
|---|---|---|---|
| socketcan | 1 (if headers found)/-1/-1 | `__linux__` → 1 | none |
| socketcan_fd | 1 (newer kernel)/-1/-1 | `__linux__` → 1 | none (assumes sysroot headers have `canfd_frame`) |
| modbus_serialport | 1/1/1 | 1 | none |

### Ranked findings
- **Medium**: `canbusutil` (`OV/qtserialbus/BUILD.bazel:340-360`) depends only on `:serialbus`. Plugins are static, so `QCanBus::plugins()` is empty and the tool cannot open any device. Add the CAN plugin targets to its deps.
- **Medium**: The `systeccan` and `vectorcan` Windows plugins are missing (`UP/qtserialbus/src/plugins/canbus/CMakeLists.txt:13-17`). Both dlopen the vendor DLLs and need no SDK at build time, so they are cheap to add.
- **Low**: CAN plugins other than virtualcan are missing from `_QTBASE_SHARED_MAP` (`QB/qt.bzl:391-393`).

---

## qtsensors
**Coverage**: 5 of 6 in-scope CMake targets present (0 partial). Bazel also builds `dummy`, which CMake only builds when it is listed in `SENSORS_PLUGINS`.

### Target inventory
| CMake target | kind | gating | Bazel target | status | notes |
|---|---|---|---|---|---|
| Sensors | module | always | `:sensors`, `:sensors_shared` `OV/qtsensors/BUILD.bazel:117-163` | present | srcs via glob |
| SensorsQuick | qml module | TARGET Qt::Quick | `:sensorsquick` L177 | present | |
| SensorsQuick default plugin | qml plugin | with SensorsQuick | `:sensorsquickplugin` L253 | present | class name `QtSensorsQuickPlugin` is Bazel-chosen (uncertain whether it matches CMake's auto-generated name; it is self-consistent) |
| genericSensorPlugin | plugin sensors | always | `:qtsensors_generic` L269 | present | includes `GENERICROTATIONSENSOR` |
| IIOSensorProxySensorPlugin | plugin | `LINUX AND TARGET Qt::DBus` | `:qtsensors_iio_sensor_proxy` L332 (linux) | present | CMake links `rt`; harmless difference |
| WinRtSensorPlugin | plugin | `winrt_sensors` (WIN32 + compile test) | none | **missing** | the only Windows sensors backend; MinGW header availability is uncertain |
| dummySensorPlugin | plugin | opt-in via `SENSORS_PLUGINS` | `:qtsensors_dummy` L293 | present (extra) | |
| sensorfw / android / ios | plugin | disabled / mobile | none | N/A | |

### Missing/partial sources
- None for the present targets.

### Feature deviations
| feature | CMake (L/M/W) | Bazel | impact |
|---|---|---|---|
| sensorfw | -1 (the plugin build is commented out upstream) | none | none |
| winrt_sensors | -/-/1 if the test passes | absent | no sensors on Windows |

### Other deviations
- **The `sensorsquick` QML registration uses library labels as `foreign_types`** (`OV/qtsensors/BUILD.bazel:209-213`: `@qtbase//:core`, `@qtdeclarative//:qml`, `:sensors`). Other modules pass `*_moc_hdrs` targets, e.g. `OV/qtgrpc/BUILD.bazel:539-543`. `_qt_qml_type_registration_impl` (`QB/qt_tools.bzl:841-845`) falls back to `target.files`, which hands library files to `--foreign-types`. Foreign metatypes (QSensor and others) are probably not resolved, so `plugins.qmltypes` is incomplete. Uncertain: check the build log for qmltyperegistrar warnings.

### Ranked findings
- **Medium**: The `sensorsquick` `foreign_types` labels are wrong (`OV/qtsensors/BUILD.bazel:209-213`, `QB/qt_tools.bzl:841-845`).
- **Medium**: The WinRT sensors plugin is missing, so Windows has no sensor backend (`UP/qtsensors/src/plugins/sensors/CMakeLists.txt:16-18`).
- **Low**: No backend is auto-linked; users must list `:qtsensors_generic`/`:qtsensors_iio_sensor_proxy` themselves.

---

## qtpositioning
**Coverage**: 9 of 11 in-scope CMake targets present (0 partial).

### Target inventory
| CMake target | kind | gating | Bazel target | status | notes |
|---|---|---|---|---|---|
| Bundled_Poly2Tri | 3rdparty static | always | `:poly2tri` `OV/qtpositioning/BUILD.bazel:47` | present | CMake hides these symbols; Bazel re-exports them via `exports_filter` (L218-222) |
| Bundled_Clipper | 3rdparty static | always | `:clipper` L70 | present | same |
| Bundled_Clip2Tri | 3rdparty static | always | `:clip2tri` L79 | present | same |
| Positioning | module | always | `:positioning`, `:positioning_shared` L152/L213 | present | srcs match |
| PositioningQuick | qml module | TARGET Qt::Quick | `:positioningquick` L263 | present | |
| positioningquickplugin | qml plugin (custom source) | with PositioningQuick | `:positioningquickplugin` L331 | present | CMake `NO_PLUGIN_OPTIONAL`; Bazel qmldir says `optional` |
| QGeoPositionInfoSourceFactoryPollPlugin | plugin position | always | `:qtposition_positionpoll` L348 | present | |
| …NmeaPlugin | plugin | TARGET Qt::Network (+SerialPort) | `:qtposition_nmea` L370 | present | `QT_NMEA_PLUGIN_HAS_SERIALPORT` set |
| …Geoclue2Plugin | plugin | DBus and Linux/BSD | `:qtposition_geoclue2` L423 (linux) | present | |
| …CLPlugin (corelocation) | plugin | `IOS OR MACOS` | none | **missing** | the only macOS backend; `createDefaultSource()` returns null on macOS. Users would need it. |
| …WinRTPlugin | plugin | `winrt_geolocation` (WIN32 + compile test) | none | **missing** | the only Windows backend; uncertain whether llvm-mingw has `windows.devices.geolocation.h` |
| …GypsyPlugin | plugin | `gypsy` (libgypsy + gconf) | none | N/A | off by default; obsolete |
| …AndroidPlugin | plugin | ANDROID | none | N/A | |

### Missing/partial sources
- `CONDITION WIN32` only sets NO_UNITY for `qclipperutils.cpp`. N/A.

### Feature deviations
| feature | CMake (L/M/W) | Bazel | impact |
|---|---|---|---|
| gypsy | -1 unless libs are found | no `qtpositioning-config.h` | none (no source consumes it) |
| winrt_geolocation | -/-/1 if the test passes | absent | no Windows positioning |

### Ranked findings
- **High**: The CoreLocation plugin is missing, so positioning does not work on macOS (`UP/qtpositioning/src/plugins/position/CMakeLists.txt:16-18`; `OV/qtpositioning/BUILD.bazel:344-451` has no `corelocation`). It needs `.mm` sources and `-framework CoreLocation -framework Foundation`.
- **Medium**: The WinRT geolocation plugin is missing on Windows (`UP/qtpositioning/src/plugins/position/winrt/CMakeLists.txt`). MinGW feasibility is uncertain.
- **Low**: The 3rdparty clip2tri/clipper/poly2tri symbols are exported from `libpositioning_shared` (`OV/qtpositioning/BUILD.bazel:218-222`).
- **Low**: The qmldir marks `positioningquickplugin` as `optional`, while CMake uses `NO_PLUGIN_OPTIONAL` (`QB/qt.bzl:2290-2291`).

---

## qtlocation
**Coverage**: 4 of 4 in-scope CMake targets present (0 partial). Bazel also builds 3 plugins that CMake disables.

### Target inventory
| CMake target | kind | gating | Bazel target | status | notes |
|---|---|---|---|---|---|
| Location (module + qml module, URI QtLocation, MapView.qml) | module+qml | always | `:location`, `:location_shared` `OV/qtlocation/BUILD.bazel:180-336` | present | the scripted diff of all `.cpp` files in `src/location/CMakeLists.txt` against the overlay shows no differences |
| declarative_location | qml plugin | always | generated by `qt_qml_module` (L295-296) | present | CMake `NO_PLUGIN_OPTIONAL`; Bazel qmldir says `optional` |
| QGeoServiceProviderFactoryItemsOverlayPlugin | plugin geoservices | always | `:qtgeoservices_itemsoverlay` L342 | present | |
| QGeoServiceProviderFactoryOsmPlugin | plugin | `geoservices_osm` (TRUE) | `:qtgeoservices_osm` L363 | present | |
| GeoServiceProviderFactoryEsriPlugin | plugin | `geoservices_esri` (**FALSE**) | `:qtgeoservices_esri` L415 | extra (CMake N/A) | |
| QGeoServiceProviderFactoryMapboxPlugin | plugin | `geoservices_mapbox` (**FALSE**) | `:qtgeoservices_mapbox` L466 | extra | |
| QGeoServiceProviderFactoryNokiaPlugin | plugin | `geoservices_nokia` (**FALSE**) | `:qtgeoservices_nokia` L517 | extra | |

### Missing/partial sources
- None.

### Feature deviations (`UP/qtlocation/src/configure.cmake`)
| feature | CMake (L/M/W) | Bazel | impact |
|---|---|---|---|
| geoservices_osm | 1 | plugin built | none |
| geoservices_esri / mapbox / nokia | -1 (hard `CONDITION FALSE`) | plugins built | Low. They are unsupported upstream and the HERE/Nokia API is defunct; harmless unless linked. |

### Ranked findings
- **Low**: Builds esri, mapbox and nokia, which upstream disables (`OV/qtlocation/BUILD.bazel:409-585`).
- **Low**: The `esri`/`mapbox`/`nokia` plugins are missing from `_QTBASE_SHARED_MAP` (`QB/qt.bzl:470-533`).
- **Low**: The qmldir marks the plugin `optional`, while CMake uses NO_PLUGIN_OPTIONAL, and it omits `depends QtQuick/QtPositioning`.

---

## qtconnectivity
**Coverage**: 2 of 4 in-scope CMake targets present (2 partial: Bluetooth, Nfc). The Connectivity header module is folded into per-module config headers; sdpscanner is missing.

### Target inventory
| CMake target | kind | gating | Bazel target | status | notes |
|---|---|---|---|---|---|
| Connectivity | internal header module (bluetooth/nfc feature config) | always | none (per-module config headers instead) | N/A / folded | no public header includes `QtConnectivity/*` |
| Bluetooth | module | `bluetooth` | `:bluetooth`, `:bluetooth_shared` `OV/qtconnectivity/BUILD.bazel:135-284` | **partial** | macOS backend present. **Linux and Windows use the dummy backend.** |
| Nfc | module | `nfc` | `:nfc`, `:nfc_shared` L344-390 | **partial** | **generic (no-op) backend on every OS** |
| sdpscanner | app (libexec) | `bluez` (needs libbluetooth) | none | missing | used by BlueZ classic SDP discovery (`qbluetoothservicediscoveryagent_bluez.cpp`); only relevant once BlueZ is enabled |

### Missing/partial sources
- **Bluetooth, `QT_FEATURE_bluez` (Linux):** none of these are built: the 27 `bluez/*.cpp` files plus `qbluetooth*_bluez.cpp` and `qbluetoothsocket_bluezdbus.cpp` (`UP/qtconnectivity/src/bluetooth/CMakeLists.txt:60-100`).
- **`QT_FEATURE_bluez_le`:** `lecmaccalculator.cpp`, `qleadvertiser_bluez*.cpp`, `qlowenergycontroller_bluez*.cpp` (L102-117). `CONFIG_LINUX_CRYPTO_API` is missing too.
- **`QT_FEATURE_winrt_bt` (Windows):** 9 `*_winrt.cpp` files and the `runtimeobject`/`user32` link (the `elseif(QT_FEATURE_winrt_bt)` block).
- **Nfc, `QT_FEATURE_pcsclite`:** `qnearfieldmanager_pcsc.cpp`, `qnearfieldtarget_pcsc.cpp`, `qapduutils.cpp`, `pcsc/*.cpp`, `ndef/qnfctagtype4ndeffsm.cpp`. Links `winscard` on Windows, `PCSC.framework` on macOS, libpcsclite on Linux (`UP/qtconnectivity/src/nfc/CMakeLists.txt:72-91`, `UP/qtconnectivity/cmake/FindPCSCLITE.cmake`).
- **Nfc, `QT_FEATURE_neard` (Linux, DBus):** `qnearfieldmanager_neard.cpp`, `qnearfieldtarget_neard_p.cpp`, `neard/neard_helper.cpp`, plus 4 DBus interface XMLs (L93-110).
- The macOS Bluetooth branch matches CMake (`OV/qtconnectivity/BUILD.bazel:164-190`). Bazel additionally links CoreBluetooth, which is harmless.

### Feature deviations (`OV/qtconnectivity/BUILD.bazel:37-54, 296-311`)
| feature | CMake default (L/M/W) | Bazel | impact |
|---|---|---|---|
| bluetooth / nfc | 1/1/1 | implicit | none |
| bluez | **1** when pkg-config `bluez` and the `bluetooth/bluetooth.h` test succeed (typical desktop) / -1 / -1 | -1 | **Linux QtBluetooth is non-functional.** The module does not link libbluetooth; it needs only DBus plus kernel socket defs (`bluez_data_p.h`), so the backend is feasible hermetically. |
| bluez_le | 1 (L, with bluez) | -1 | no BLE on Linux |
| linux_crypto_api | 1 (L) | -1 | no BLE signed writes |
| winrt_bt | -/-/**1** when the WRL/WinRT header test passes (MSVC; MinGW uncertain) | -1 | Windows QtBluetooth is non-functional |
| pcsclite | 1 (L if libpcsclite) / **1** (M: PCSC.framework) / **1** (W: winscard) | -1 | **no NFC on macOS/Windows**, even though it needs no external deps there |
| neard | 1 (L, DBus and no pcsclite) | -1 | no NFC on Linux |

### Other deviations
- `bluetooth_headers` stages the macOS private headers on every OS (L122-133). Harmless.

### Ranked findings
- **High**: Linux Bluetooth is the dummy backend; `bluez`, `bluez_le` and `linux_crypto_api` are all -1 (`OV/qtconnectivity/BUILD.bazel:37-54, 191-200`). It can be enabled with only `@qtbase//:dbus`.
- **High** (if Windows BT matters): Windows Bluetooth is the dummy backend; `winrt_bt` is -1 (`OV/qtconnectivity/BUILD.bazel:52`). MinGW WinRT header availability is uncertain.
- **Medium**: NFC uses the generic no-op backend everywhere (`OV/qtconnectivity/BUILD.bazel:296-303, 354`). PC/SC needs only system frameworks/libs on macOS and Windows; neard needs only DBus on Linux.
- **Low**: The `sdpscanner` helper is missing (`UP/qtconnectivity/src/tools/sdpscanner/CMakeLists.txt`). It matters only once BlueZ is enabled, and it needs libbluetooth.

---

## Cross-module ranked summary
1. **High**: qtconnectivity: Bluetooth is a dummy backend on Linux (BlueZ) and Windows (WinRT).
2. **High**: qtpositioning: the macOS CoreLocation plugin is missing.
3. **Medium**: qtconnectivity: NFC is a no-op everywhere; PC/SC on macOS/Windows and neard on Linux are cheap to enable.
4. **Medium**: qtserialbus: `canbusutil` links no CAN plugins; `systeccan`/`vectorcan` (Windows) are missing.
5. **Medium**: qtsensors: `foreign_types` points at libraries instead of moc metatypes; the WinRT sensors plugin (Windows) is missing.
6. **Medium**: qtpositioning: WinRT geolocation (Windows) is missing.
7. **Medium**: all plugin modules (scxml ECMAScript datamodel, CAN, sensors, position, geoservices): static plugins are never auto-linked, so backends fail silently unless listed in deps. `_QTBASE_SHARED_MAP` lacks most plugin entries.
8. **Medium**: qtgrpc: `qt_protobuf_gen` has no QML mode.
9. **Low**: qtlocation builds the disabled esri/mapbox/nokia plugins; qmldir files lack `depends`/`linktarget` and ignore NO_PLUGIN_OPTIONAL; positioning 3rdparty symbols are exported; serialport uses runtime-dlopen udev.
