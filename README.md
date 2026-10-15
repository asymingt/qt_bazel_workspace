# Qt 6 for Bazel — from source, hermetic, cross-compiled

This repository is a **staging area for a set of Bazel modules that build Qt 6
from source** with a hermetic LLVM toolchain. It holds a local Bazel registry
(`bazel_registry.json` + `modules/`) with one module per Qt submodule, the
smoke tests that exercise them, and the CI that proves they build everywhere.
The goal is to upstream these modules to the
[Bazel Central Registry](https://registry.bazel.build); until then you can use
them directly from here.

## Why build Qt from source?

The existing Bazel integrations download the official prebuilt Qt binaries.
That is convenient, but it quietly breaks the guarantees Bazel is supposed to
give you:

- **Prebuilts assume GCC and libstdc++.** The official Linux binaries are
  compiled with GCC against libstdc++. Link them from a Clang/libc++ toolchain
  (as any hermetic LLVM setup does) and you are mixing two C++ standard
  libraries and two ABIs in one process — at best a wall of warnings, at worst
  `std::string`/`std::function` layout mismatches that surface as crashes at
  runtime rather than errors at build time.
- **Prebuilts are not self-contained.** They dynamically load a long tail of
  system libraries at *runtime* — `libxcb-*`, `libxkbcommon`, `libGL`,
  `fontconfig`, `libdbus`, `libpulse`, `libgstreamer`, and so on — which must
  be installed via `apt` on every machine that builds *and* every machine that
  runs the result. Which versions you get depends on the host image, not on
  Bazel, so builds are no longer reproducible and remote execution is a
  gamble.
- **Cross-compilation is off the table.** A downloaded SDK is for one
  OS/architecture; targeting another means a different download, a different
  sysroot and, on macOS and Windows, a non-hermetic native SDK.

Building Qt — and its X11/D-Bus/multimedia dependencies — from source inside
the action graph with one hermetic Clang toolchain fixes all three: a single
ABI end to end, nothing read from `/usr` at build or run time, and
`--platforms=...` as the only difference between a Linux, macOS or Windows
binary.

## Try it now

Point Bazel (**9.0 or newer** — `rules_cc` ≥ 0.2.21, which the hermetic LLVM
toolchain depends on, needs Bazel 9's C++ runtime toolchain type) at the GitHub
Pages mirror of this registry, ahead of the BCR, in your `.bazelrc`:

```
common --registry=https://asymingt.github.io/qt_bazel_workspace/
common --registry=https://bcr.bazel.build
```

Then depend on Qt like any other module:

```starlark
# MODULE.bazel
bazel_dep(name = "llvm", version = "0.8.24")   # hermetic toolchain
bazel_dep(name = "qt", version = "6.10.2")     # rules
bazel_dep(name = "qtbase", version = "6.10.2")
bazel_dep(name = "qtdeclarative", version = "6.10.2")
register_toolchains("@llvm//toolchain:all")
```

```starlark
# BUILD.bazel
load("@qt//:qt.bzl", "qt_cc_binary")

qt_cc_binary(
    name = "app",
    srcs = ["main.cc"],
    deps = ["@qtbase//:widgets", "@qtdeclarative//:quick"],
)
```

Cross-compile the same target for any supported platform with nothing but a
flag:

```
bazel build //:app --platforms=@llvm//platforms:macos_arm64
bazel build //:app --platforms=@llvm//platforms:windows_amd64
bazel build //:app --platforms=@llvm//platforms:linux_arm64_gnu.2.39
```

## What's in the box

36 Qt 6.10 modules, all building on Linux, macOS and Windows for both x86_64
and arm64:

| Area | Modules |
| --- | --- |
| Core | `qtbase` (Core, Gui, Widgets, Network, Sql, Dbus, OpenGL, PrintSupport, Test, Concurrent, Xml), `qt5compat`, `qttools`, `qttranslations` |
| QML / Quick | `qtdeclarative`, `qtshadertools`, `qtsvg`, `qtimageformats`, `qtlanguageserver`, `qtquicktimeline`, `qtlottie`, `qtvirtualkeyboard` |
| 3D / Visualization | `qtquick3d`, `qtquick3dphysics`, `qt3d`, `qtgraphs`, `qtdatavis3d`, `qtcharts` |
| Multimedia | `qtmultimedia` (FFmpeg, spatial audio), `qtspeech`, `qtwebview` |
| Networking / IPC | `qtwebsockets`, `qtwebchannel`, `qthttpserver`, `qtnetworkauth`, `qtgrpc`, `qtremoteobjects` |
| Devices / Sensors | `qtserialport`, `qtserialbus`, `qtsensors`, `qtpositioning`, `qtlocation`, `qtconnectivity` |
| Platform | `qtwayland`, `qtscxml`, plus the shared [`qt`](https://github.com/asymingt/qt_bazel) rules module |

Plus a handful of small X11/D-Bus dependencies that are not (yet) in the BCR.

## How this differs from other Bazel + Qt projects

The user-facing API (`qt_cc_library`, `qt_cc_binary`, `qt_ui_library`,
`qt_resource`, `qt_qml_module`) is deliberately modelled on
[Vertexwahn/rules_qt6](https://github.com/Vertexwahn/rules_qt6) and its
ancestors ([justbuchanan/bazel_rules_qt](https://github.com/justbuchanan/bazel_rules_qt),
[bbreslauer/qt-bazel-example](https://github.com/bbreslauer/qt-bazel-example)),
so existing projects can switch by changing a `load()` path. Under the hood it
is a different thing:

- **Qt is built, not downloaded.** Nearly every existing project
  (`rules_qt6`, `aabtop/rules_qt`, `jheaff1/rules_qt`, ...) wraps a prebuilt or
  system Qt. Here every Qt library, plugin and code generator (`moc`, `rcc`,
  `uic`, `qsb`, `qmltyperegistrar`, ...) is compiled in the Bazel action graph
  from hash-pinned upstream source tarballs. No SDK download, no system Qt.
- **Hermetic and cross-compilable by construction.** Builds use the
  [hermetic LLVM toolchain](https://github.com/hermeticbuild/hermetic-llvm):
  no Xcode, Visual Studio or Windows SDK. One Linux host can produce
  Linux/macOS/Windows binaries for x86_64 and arm64, including the native
  `xcb`/`wayland`, `cocoa` and `windows` QPA plugins.
- **One bzlmod module per Qt submodule.** Unlike monolithic from-source
  efforts ([kklochkov/rules_qt](https://github.com/kklochkov/rules_qt),
  [OpenROAD/qt_bazel_prebuilts](https://github.com/The-OpenROAD-Project/qt_bazel_prebuilts)),
  Qt is split along its own submodule boundaries with the Starlark factored
  into a shared `qt` module, so modules version and depend on each other
  through a registry exactly like any other BCR dependency.
- **Zero patches to Qt.** Every module is the pristine upstream tarball plus a
  registry overlay (`BUILD.bazel`, `MODULE.bazel`, a config `.bzl`). Adding a
  Qt submodule means writing a `BUILD.bazel`, not forking Qt.

## Layout

| Path | Purpose |
| --- | --- |
| `bazel_registry.json`, `modules/` | The Bazel registry, published to GitHub Pages on every merge (`.github/workflows/ci.yaml`). |
| `tests/` | Smoke tests per Qt area, run natively and cross-compiled by the PR matrix (`.github/workflows/pull_request.yaml`). |
| `patches/` | Patches for third-party BCR modules (`llvm`, `ffmpeg`, `openssl`) consumed via `single_version_override`. |
| `.bazelrc` | `--config=<os>_<arch>` / `--config=host_<os>_<arch>` presets for the six supported platforms. |
