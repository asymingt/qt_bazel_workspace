# Pre-upstreaming review of `modules/`

Date: 2026-10-19. Scope: every module under `modules/` (49 modules: 39 Qt
submodules, the shared `qt` rules module, and 9 X11/D-Bus helper libraries),
the `qt` rules source as fetched into the Bazel output base, the root
workspace configuration (`MODULE.bazel`, `.bazelrc`, `patches/`), and the
Bazel Central Registry (BCR) contribution rules.

What was verified mechanically:

- Every overlay file hash, patch hash and `strip_prefix` in `source.json`
  matches the files on disk, and every `<version>/MODULE.bazel` is identical
  to its `overlay/MODULE.bazel`.
- Every `license_text` path exists in the fetched upstream tarball (only
  `qtnetworkauth` could not be checked because it has never been fetched here).
- Every `metadata.json` has the required fields and lists its version.

The build itself is in good shape. The problem is that the modules currently
only work *inside this workspace*, and several structural choices will not
survive the BCR presubmit. Findings are grouped by severity.

## 1. Blockers: the modules will not build from the BCR as-is

### 1.1 Hardcoded canonical repository names

`_lbl()` in `qt.bzl` rewrites every `@qtbase//:x` string into
`Label("@@qtbase+//:x")`, and `modules/qtbase/6.10.2/overlay/BUILD.bazel`
references `@@pcre2+//:pcre2` (line 1400), `@@libxau+//:xau` and
`@@libxtrans+//:xtrans` (2895-2896), `@@libexpat+//:expat` (3548) and
`@@libffi+//:libffi` (5637).

Bazel documents the canonical name format as "not an API you should depend on
and subject to change at any time". It already changed once (Bazel 7 used
`~`), it differs under `multiple_version_override` (`qtbase+6.10.2`), and the
BCR runs an incompatible-flags check that is likely to trip on it.

The `@@…+` entries in qtbase exist because those modules are transitive, not
direct, dependencies of qtbase.

**Fix:** declare `pcre2`, `libxau`, `libxtrans`, `libexpat` and `libffi` as
`bazel_dep`s of qtbase and use plain `@name//` labels. In `qt.bzl`, use
`Label("@qtbase//:x")` (resolved against the `qt` module's own repo mapping)
instead of string surgery on canonical names.

### 1.2 Reliance on root-only `single_version_override` patches

`modules/qtbase/6.10.2/overlay/BUILD.bazel` (line 1390) and
`modules/qtmultimedia/6.10.2/overlay/BUILD.bazel` (line 1143) put
`@zlib//:zlib_shared` in `dynamic_deps`. That target only exists because of
`patches/zlib/zlib_shared.patch`, applied by the root `MODULE.bazel`.
Overrides are never part of a registry, so a consumer pulling qtbase from the
BCR gets "no such target".

The same applies, less directly, to:

| Root patch | Why the modules need it |
| --- | --- |
| `patches/rules_cc/runfiles_linkable_more_than_once.patch` | `libcore_shared` links `@rules_cc//cc/runfiles:runfiles` without exporting it. |
| `patches/libexpat/windows.patch` | `wayland-scanner` on Windows execution platforms. |
| `patches/xkbcommon/pregenerated_parser.patch` | bison/m4 do not build on MinGW hosts. |
| `patches/openssl/*.patch`, `patches/ffmpeg/sysctl.patch` | Cross-compiling the TLS and FFmpeg backends. |
| `patches/llvm/*.patch` (13 files) | The toolchain itself. |

Each must land upstream (as a `.bcr.N` version in the BCR) before the Qt
modules can be published, or the modules must stop depending on the patched
behaviour.

### 1.3 No `presubmit.yml` anywhere

The BCR requires one per module version (`find modules -name presubmit.yml`
returns nothing). Decide now what each presubmit can realistically build: with
the default BCR toolchains (GCC on Linux, MSVC on Windows, Apple clang on
macOS) most of these overlays will not compile. The options are a
`bcr_test_module` that registers the hermetic `llvm` toolchain, or Linux-only
tasks.

### 1.4 Toolchain-specific assumptions baked into the overlays

- Windows assumes clang + MinGW + libc++: `mkspecs/win32-clang-g++`,
  `-lmingw32`, GNU-style `-D` flags, and the `win_cxxabi_shim` in the `qt`
  rules `BUILD.bazel`, which reimplements libc++ ABI destructors.
- Linux uses `-Wl,--version-script` and `-Wl,-Bsymbolic-functions`
  (`qt_internal_library.bzl`, around line 403).
- `.bazelrc` carries flags the build depends on:
  `--features=system_include_paths`,
  `--@llvm//config:experimental_stub_libgcc_s=False`, and the per-file
  `-Wno-thread-safety-analysis` suppression. BCR presubmit has no `.bazelrc`.

At minimum, document the required toolchain and gate Windows targets with
`@rules_cc//cc/compiler:clang` selects so MSVC fails clearly instead of
mysteriously.

### 1.5 `bazel_compatibility` is wrong or missing

Twenty-two modules say `>=7.2.1`; twenty-seven say nothing. The README states
Bazel 9 is required (rules_cc 0.2.26 needs Bazel 9's runtime toolchain type),
and the `+` canonical names need Bazel 8 or later. Set `>=9.0.0` consistently,
and give every module a `compatibility_level` (currently only the X11 helpers
and six Qt modules have one).

## 2. Design issues reviewers will push back on

### 2.1 Dependency cycle and over-broad `qt` module

`qt` depends on `qtbase`, `qtdeclarative`, `qtgrpc`, `qtremoteobjects`,
`qtscxml`, `qtshadertools`, `qttools` and `qtwayland`, and every one of those
depends on `qt`. Bazel tolerates module cycles, but community experience
(rules_go and gazelle) is that they are painful. Here it means a hello-world
`qtbase` consumer transitively resolves protobuf 34.1, assimp, ffmpeg,
wayland, and `llvm` plus a from-source LLVM/Clang tree, with MVS forcing those
versions onto them.

None of those eight dependencies is referenced by any label inside the `qt`
tarball. They exist only so that string labels in the macros resolve from the
*caller's* repo mapping (legacy-macro semantics). Move the tool defaults to
`Label()`s and declare only what `qt.bzl` actually loads; drop the rest.

### 2.2 Central 2,900-line dependency table

`_QTBASE_SHARED_MAP` in `qt.bzl` (lines 75-2937) lists the transitive
`*_shared` closure of every target in every Qt module, from qt3d to qtwebview.
This duplicates information each module's `BUILD.bazel` already encodes, must
be edited in a *different repository* whenever any module's dependencies
change, and hardcodes targets in modules `qt` does not depend on. A
`CcSharedLibraryInfo`-collecting aspect, or a per-module exported list, would
remove the duplication. Downstream users have already hit this
(`design/FEEDBACK1.md`, section 3c).

### 2.3 qttools pulls `@llvm-project` for lupdate and qdoc

`modules/qttools/6.10.2/MODULE.bazel` depends on the `llvm` *toolchain*
module and calls its extension without `llvm.version(...)`, so it relies on
the root module having configured that extension. Building `@qttools//...`
compiles Clang from source. Consider a separate target set, or a `libclang`
module dependency, so the main qttools libraries (`uitools`, `designer_lib`,
`help`) stay cheap.

### 2.4 `qt` module patches its own upstream

`modules/qt/6.10.2/patches/` patches asymingt/qt_bazel, which we own. BCR
patches are for fixes that cannot be landed upstream. Cut a 6.10.3 or
`6.10.2.bcr.1` tag of qt_bazel instead. Also rename `qt_plugin_FixMe` before
publishing; it is a public macro used by four modules (qtpositioning,
qtlocation, qtvirtualkeyboard, qtscxml).

### 2.5 Unused `bazel_dep`s

No overlay label references these declared dependencies:

| Module | Unused `bazel_dep` |
| --- | --- |
| qtbase | `libwebp` |
| qtdeclarative | `qtimageformats`, `qtshadertools` |
| qtwayland | `opengl-registry` |

Each one is an extra module forced into consumers' graphs. Remove them.

### 2.6 Platform gating is incomplete

`bazel build @module//...` is what presubmit runs. The X11 helper modules
(dbus, libice, libsm, libxcb-*, xkbcommon-x11) have no
`target_compatible_with` at all, and most Qt overlays have little or none
(qt3d: 0 of 65 rule instances; qtwayland: 0 of 50; qttools: 0 of 67). This
CI never exercises `@qtbase//...` directly (it builds the root `//...`), so
there is no evidence those wildcards succeed on macOS or Windows.

### 2.7 Naming of partial libraries

`libglvnd` exports only `GL/glx.h`; anyone who later needs the real libglvnd
in the BCR will be blocked by the name. Either build the library or fold the
header into qtbase. The xcb-util modules are named `libxcb-util`,
`libxcb-wm`, and so on, while the upstream tarballs are `xcb-util-*`; the BCR
already has `libxcb-keysyms`, so the `libxcb-` prefix is defensible, but pick
one convention and state it. None of the ten non-Qt names exists in the BCR
today (checked 2026-10-19).

## 3. Hermeticity and provenance details

### 3.1 Host tools from `PATH` in rule actions

- `qt_deploy.bzl` shells out to `tar`, `find`, `touch`, `mktemp`, with a
  GNU-tar versus bsdtar branch.
- `qt_tools.bzl` uses `realpath`, `sed`, `cp`, `printf`, `touch`.
- `_tool_env` sets `LC_ALL` by sniffing the tool path for `macos`/`darwin`.

These run in sandboxed `run_shell` actions so they are cache-safe, but
`realpath` and GNU `tar` flags vary across hosts and the path sniffing is
fragile. Prefer `ctx.actions.run` with the tool directly, and a Starlark or
small C++ helper for archiving.

### 3.2 Vendored third-party files inside overlays

| Module | Files | Concern |
| --- | --- | --- |
| qtmultimedia | 32 `pulse/*.h` (PulseAudio, LGPL-2.1+) | No `license()` target, no upstream version noted. |
| qtspeech | `libspeechd.h`, `qtexttospeech_speechd_symbolloader.cpp` | Hand-written API header; no provenance. |
| qtbase | `atspi/*.h`, `moltenvk/mvk_vulkan.h` | Stand-ins for system headers; fine, but say which upstream version they mirror. |
| qtdeclarative | `bazel_prebuilt/yarr/RegExpJitTables.h` | Pregenerated; header comment is good, keep it. |
| qtopcua | `qopcua_openssl_rename_p.h` | Overlay-only workaround; documented. |

Each needs a provenance comment (upstream version and commit), and the LGPL
ones need a license declaration, or BCR reviewers will ask.

### 3.3 dbus defaults

`dbus_config_checks.bzl` bakes Linux-distro runtime paths (`/usr/share`,
`/etc/dbus-1`, `/run/dbus/system_bus_socket`) and the library is Unix-only
with no Windows sources. Fine for its purpose, but gate it to Linux, BSD and
macOS explicitly.

### 3.4 Download URLs

`download.qt.io` is a mirror redirector and is slow; add `mirror_urls`
(for example `mirrors.ocf.berkeley.edu/qt`, `qt.mirror.constant.com`) as is
already done for the freedesktop tarballs. The three GitHub
`archive/refs/tags` URLs (qtcoap, qtmqtt, qtopcua, plus the `qt` module) are
acceptable but trigger the BCR "unstable URL" check.

### 3.5 Housekeeping

- `modules/libglvnd/1.7.0/overlay/MODULE.bazel.lock` is an untracked stray
  file; delete it so the published registry never picks it up.
- Every `metadata.json` lists the maintainer email as `simmers@google.com`.
  Fine if intended, but the BCR pings it, so make it a conscious choice.
- `qtcharts`, `qtcoap`, `qtdatavis3d`, `qtgraphs`, `qtgrpc`, `qthttpserver`,
  `qtlottie`, `qtmqtt`, `qtnetworkauth`, `qtquick3d`, `qtquick3dphysics`,
  `qtquicktimeline` and `qtvirtualkeyboard` are correctly declared
  GPL-3.0-only. Say so in the README, since BCR consumers care.

## 4. Suggested order of attack

1. Fix the canonical-name and `Label()` issues in qt_bazel (1.1, 2.1). This
   also resolves the over-broad `qt` dependency list.
2. Upstream the zlib `zlib_shared` and rules_cc runfiles changes to the BCR as
   `.bcr.N` versions (1.2), then drop the root overrides.
3. Write presubmits for the leaf X11 modules and run a real anonymous-module
   `bazel build @dbus//...`-style test on each platform (1.3, 2.6).
4. Only then start on the Qt modules, qtbase first.

## References

- Bazel modules documentation (canonical names are not an API):
  https://bazel.build/external/module
- BCR contribution rules (presubmit, metadata, patches, URL stability):
  https://github.com/bazelbuild/bazel-central-registry/blob/main/docs/README.md
- Community experience with module cycles (rules_go / gazelle):
  https://github.com/bazelbuild/rules_kotlin/issues/1158
- Downstream consumer feedback already on file: `design/FEEDBACK1.md`
