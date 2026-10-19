# PHASE 5: IoT Protocol Modules — Qt MQTT, Qt CoAP, Qt OPC UA (Qt 6.10.2)

## 1. Executive Summary

Phases 1–4 cover every open-source Qt 6.10 module on the
[Qt 6.10 module list](https://wiki.qt.io/Qt_6.10_Modules) except six. Phase 5 adds the three
IoT/industrial protocol add-ons, which are small, self-contained and widely used:

| Module | Upstream repository | What it adds |
|---|---|---|
| **Qt MQTT** | `qt/qtmqtt` | `QMqttClient` (MQTT 3.1, 3.1.1, 5.0) over TCP, TLS and WebSockets |
| **Qt CoAP** | `qt/qtcoap` | `QCoapClient` (RFC 7252) over UDP and DTLS |
| **Qt OPC UA** | `qt/qtopcua` | `QOpcUaClient`/`QOpcUaProvider`, the bundled **open62541** backend, GDS/x509 helpers and the `QtOpcUa` QML module |

The other three missing modules are deliberately **out of scope**:

- **qtwebengine** (and Qt PDF, which lives in the same repository): Chromium, built by GN and
  Ninja, which needs MSVC on Windows. It is a separate project.
- **qtactiveqt**: Windows COM/ActiveX, built mainly for MSVC.
- **qtquickeffectmaker**: a desktop design tool, not a library.

Phase 5 also makes the build **warning-free**. It fixes the remaining `qmltyperegistrar`
foreign-type errors and silences the `-Wthread-safety` false positives that our toolchain
(but not upstream) enables (Milestone 5.0).

The goal is the same as Phase 4: **match a default upstream CMake build** of each module on
Linux, macOS and Windows, and **prove it with runtime tests** that exercise the protocol end to
end against in-process peers, not just that the code compiles.

---

## 2. Scope & Sources

**Platforms**: the Phase 3/4 matrix. That is Linux (x86_64/aarch64, glibc 2.34), macOS
(x86_64/arm64) and Windows (x86_64/arm64, MinGW-UCRT clang/lld), on 6 native hosts and 30
host→target cross pairs.

**Sources**: Qt does not publish `*-everywhere-src` tarballs for these three repositories under
`official_releases/qt/6.10/6.10.2/submodules/` or `additional_libraries/`. Each module's
`source.json` therefore pins GitHub's archive of the `v6.10.2` tag
(`https://github.com/qt/<module>/archive/refs/tags/v6.10.2.tar.gz`, `strip_prefix
<module>-6.10.2`). Neither repository has git submodules: open62541 is vendored as a single-file
amalgamation in `qtopcua/src/3rdparty/open62541`.

**Licenses**:

| Module | License kinds |
|---|---|
| qtmqtt, qtcoap | GPL-3.0-only (commercial or GPL, the same as qtnetworkauth) |
| qtopcua | LGPL-3.0-only, GPL-2.0-only, GPL-3.0-only |
| open62541 | MPL-2.0 (plus CC0/CC-BY-SA/BSD/Apache/MIT parts; `:open62541_license`) |

---

## 3. Module Design

All three follow the Phase 4 overlay style used by qtwebsockets and qtnetworkauth:

- `qt_export_header` for `qt<module>exports.h`. This keeps the upstream `REMOVED_SINCE` and
  `INLINE_SINCE` semantics, plus the Windows always-inline rule from `f4f86ed`.
- `qt_sync_headers` for `<QtModule/...>`, `<QtModule/private/...>` and the umbrella headers.
- `qt_internal_library(create_shared = False, moc_unity = True)` for the static library, plus a
  hand-written `cc_shared_library` (`<name>_shared`).

None of the three generates a `qt<module>-config.h`. Upstream features become plain defines or
source-list choices.

### 3.1 qtmqtt (`@qtmqtt`)

| Target | Contents |
|---|---|
| `:mqtt`, `:mqtt_shared` | All of `src/mqtt`, plus `transportLayers/` (the WebSocket and secure-WebSocket transports) |

- Upstream builds the WebSocket transports, and defines `QT_MQTT_WITH_WEBSOCKETS`, whenever
  `Qt::WebSockets` exists. It always does here, so `@qtwebsockets//:websockets` is a public
  dependency.
- The transport sources `#include "moc_*_p.cpp"`. `moc_gen` detects this and leaves those files
  out of `mocs_compilation.cpp`.
- No QML module: in 6.10 the QML MQTT client exists only as example code.

### 3.2 qtcoap (`@qtcoap`)

| Target | Contents |
|---|---|
| `:coap`, `:coap_shared` | All of `src/coap` |

- DTLS (`QT_CONFIG(dtls)`) comes from QtNetwork, which has `QT_FEATURE_dtls 1` with the
  hermetic OpenSSL.

### 3.3 qtopcua (`@qtopcua`)

| Target | Upstream equivalent | Notes |
|---|---|---|
| `:open62541` | `src/3rdparty/open62541` (`INPUT_open62541=qt`) | C99 amalgamation. `UA_ENABLE_ENCRYPTION_OPENSSL` is a **public** define, because `open62541.h` derives `UA_ENABLE_ENCRYPTION`, and with it the config struct layout, from that macro. Built with `-w`, since upstream also silences its warnings. Windows links `iphlpapi` and `ws2_32`. |
| `:opcua`, `:opcua_shared` | `Qt6OpcUa` | `client/`, `core/` and `compat/removed_api.cpp`. Namespace 0 id names are on (`QT_FEATURE_ns0idnames`). `-fconstexpr-steps=2000000` (upstream's Clang branch) lets moc's metaobject for the ~20k-value `Namespace0` enum compile. |
| `:opcua_x509` | the `QT_FEATURE_gds` part of `Qt6OpcUa` | `QOpcUaGdsClient`, `QOpcUaKeyPair` and `QOpcUaX509*`, which link the hermetic `@openssl//:crypto` (`QT_LINKED_OPENSSL`). Not built on macOS, matching upstream's `NOT APPLE`. See §4.2 for why this is split out. |
| `:open62541_backend` | `QOpen62541Plugin` (plugin type `opcua`) | `qt_static_plugin`, which `QOpcUaProvider` finds through `QFactoryLoader`'s static instances. |
| `:opcua_plugins` | — | All backend plugins (currently only open62541), for consumers. |
| `:declarative_opcua`, `:declarative_opcuaplugin`, `:declarative_opcua_shared` | `DeclarativeOpcua` (`import QtOpcUa`) | `past_major_versions` 1 and 5, `depends QtQuick`. `foreign_types = [":opcua_moc_hdrs"]` so the `QML_FOREIGN` QtOpcUa types resolve. |

**Feature mapping** (upstream `src/opcua/configure.cmake`):

| Upstream feature | Upstream default | Here |
|---|---|---|
| `open62541` (bundled) | on | on (`:open62541`) |
| `system-open62541` | off | off |
| `open62541-security` | on if OpenSSL is found | **on**: the hermetic OpenSSL is always available |
| `gds` | on if OpenSSL 3, not on Apple | on for Linux and Windows (`:opcua_x509`), off on macOS |
| `ns0idnames` | on | on |
| `datatypecodegenerator` (`qopcuaxmldatatypes2cpp`) | on | **not built** (a host code generator; §6) |
| `internalgenerator` | off | off |
| uacpp backend | commercial SDK only | not applicable |

---

## 4. Cross-Cutting Decisions

### 4.1 Shared-library resolution (`QT_SHARED_LIBS`)

`qt_cc_binary` and `qt_cc_test` map Qt `deps` to `dynamic_deps` through the
`_QTBASE_SHARED_MAP` table in `asymingt/qt_bazel`'s `qt.bzl`. The new modules are not in that
table yet. Consumers still link correctly, because QtCore, QtNetwork and the rest resolve to
their shared libraries and the new module links statically. For the LGPL shared layout, the
Phase 5 tests list `@qtmqtt//:mqtt_shared` and the other new shared libraries in `dynamic_deps`
explicitly.

**Follow-up**: add the `@qtmqtt`, `@qtcoap` and `@qtopcua` entries to `_QTBASE_SHARED_MAP` in
the next `qt_bazel` release, then drop the explicit `dynamic_deps` from `tests/phase5`.

### 4.2 OpenSSL linkage

qtbase links the hermetic OpenSSL **statically into binaries**, through the `qopensslbackend`
static TLS plugin. It never puts OpenSSL into a Qt shared library. Phase 5 keeps that rule:

- `:open62541` (inside the static backend plugin) and `:opcua_x509` both link `@openssl//:crypto`
  statically, so a binary ends up with one copy alongside the TLS plugin's.
- `opcua_shared` contains no OpenSSL. Two alternatives were tried and rejected:
  1. *Exporting `@openssl//:crypto` from `opcua_shared`* (`exports_filter`, the pattern
     `core_shared` uses for glib and ICU). A static archive only contributes the objects the
     shared library references, so `libssl` in the binary then failed to link against the
     missing parts of libcrypto.
  2. *Runtime `dlopen` of libcrypto*, as upstream does by default. This needs a system OpenSSL
     at runtime, which Windows does not provide, and is not hermetic.
- QtOpcUa's `openssl_symbols.cpp` defines global `q_<function>` wrappers, such as
  `q_X509_free` and `q_PEM_read_bio_PUBKEY`. These have the same names as the ones in qtbase's
  `qsslsocket_openssl_symbols.cpp`; upstream they never meet because they live in different
  shared objects. The overlay header `src/opcua/x509/qopcua_openssl_rename_p.h`, generated from
  the `DEFINEFUNC` list, renames QtOpcUa's 70 wrappers to `qopcua_q_*`. It is force-included
  (`-include`) only into `:opcua_x509`, so no upstream source is patched.

### 4.3 Local registry caching

Bazel keeps local-registry (`file://`) module files in the server's memory once a module has
been resolved. After editing anything under `modules/<name>/` (overlay or `source.json`), run
`bazel shutdown` before the next build. Otherwise Bazel keeps using the previously fetched
overlay.

---

## 5. Milestones

### Milestone 5.0: Warning-free build (prerequisite)

- `qmltyperegistrar` foreign types. Several QML modules passed the `cc_library` targets
  (`@qtbase//:core`, ...) as `foreign_types` instead of their `*_moc_hdrs` metatype targets, so
  `qmltyperegistrar` tried to parse `libcore.a` as JSON ("Failed to parse JSON: 7 illegal
  number"). The resulting `.qmltypes` files lacked the foreign types: wrong prototypes, missing
  exports, and therefore broken qmllint, qmlcachegen and QML language server support. Fixed in
  qt5compat (two modules), qtcharts (`chartsqml`), qtwebchannel (`webchannelquick`) and
  qtwebsockets. `QtQml.StateMachine` (qtscxml) was fixed in `4052351`.
- `-Wthread-safety`. The `@llvm` toolchain enables it by default; upstream Qt does not.
  Clang's per-function analysis cannot follow locks returned by `qt_scoped_lock()` or taken in
  VulkanMemoryAllocator's `Lock()`/`Unlock()` wrappers, so Qt produces only false positives
  (`qguiapplication.cpp`, `qopenglcontext.cpp`, `vk_mem_alloc.h`). `.bazelrc` adds
  `-Wno-thread-safety-analysis` for `external/qt*+/` sources, through `--per_file_copt` and
  `--host_per_file_copt` (tools). Consumers' own code keeps the check.

### Milestone 5.1: qtmqtt

`@qtmqtt//:mqtt` and `:mqtt_shared`, with the WebSocket transports. **Test**: `mqtt_test`.

### Milestone 5.2: qtcoap

`@qtcoap//:coap` and `:coap_shared`. **Test**: `coap_test`.

### Milestone 5.3: qtopcua core and open62541 backend

`:open62541`, `:opcua`, `:opcua_shared`, `:opcua_x509`, `:open62541_backend` and
`:opcua_plugins`. **Test**: `opcua_test`.

### Milestone 5.4: QtOpcUa QML module

`:declarative_opcua`, `:declarative_opcuaplugin` and `:declarative_opcua_shared`.
**Test**: `opcua_qml_test`.

### Milestone 5.5: CI

All 36 PR jobs green (6 native, 30 cross), with no compiler or `qmltyperegistrar` warnings in
the Qt build.

---

## 6. Verification Strategy

Every test in `tests/phase5` is a QtTest (`qt_cc_test`) that talks to a real peer **in the same
process** over loopback. They need no network access and no external broker or server.

| Test | What it proves |
|---|---|
| `mqtt_test` | Topic filter and topic name validation and `+`/`#` matching. A full **CONNECT → SUBSCRIBE → PUBLISH → deliver → DISCONNECT** round trip (MQTT 3.1.1) against a minimal broker built on `QTcpServer`, checking the packets the broker saw. |
| `coap_test` | The `QCoapRequest`/`QCoapMessage`/`QCoapOption` API. **Confirmable and non-confirmable GETs** against a `QUdpSocket` CoAP server that parses the request (token, message id, Uri-Path options) and answers with a piggybacked ACK or a separate NON response containing 2.05 Content. |
| `opcua_test` | `QOpcUaProvider` finds the statically linked **open62541** backend. Namespace 0 id names (`ns0idnames`). The backend reports the `Basic256Sha256` policy (open62541 built with OpenSSL). **requestEndpoints → connect → read Value and DisplayName → write → read back → disconnect** against an **in-process open62541 server**, with one writable `Int32` node. On Linux and Windows, **RSA key generation and a PEM round trip** through `QOpcUaKeyPair` (GDS/x509 with the linked OpenSSL). |
| `opcua_qml_test` | `import QtOpcUa` loads. `Connection`, `NodeId` and `ValueNode` instantiate, and `Connection.availableBackends` contains `open62541`. |

`opcua_test` expects, and checks, open62541's "skip verifying ApplicationURI" warning for the
certificate-less client. Every other warning would show up in the test log.

---

## 7. Not Done / Follow-ups

1. **`_QTBASE_SHARED_MAP` entries** for the new modules in `asymingt/qt_bazel` (§4.1).
2. **`qopcuaxmldatatypes2cpp`** (`QT_FEATURE_datatypecodegenerator`) and the
   `qt_opcua_generate_datatypes` CMake helper. Add these as a host tool and a Bazel rule if a
   consumer needs generated OPC UA data types.
3. **OPC UA over an encrypted channel** in tests. This needs a test PKI (CA, server and client
   certificates). The current test only proves that encryption support is compiled in.
4. **MQTT over WebSockets** in tests. The transport is built but only exercised at link time.
5. **BCR upstreaming** of the three modules once `qt_bazel` and the other Qt modules are
   published there.
