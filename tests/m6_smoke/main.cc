#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QWaylandCompositor>
#include <QWaylandIdleInhibitManagerV1>
#include <QWaylandIviApplication>
#include <QWaylandOutput>
#include <QWaylandOutputMode>
#include <QWaylandSeat>
#include <QWaylandSurface>
#include <QWaylandViewporter>
#include <QWaylandWlShell>
#include <QWaylandXdgDecorationManagerV1>
#include <QWaylandXdgOutputManagerV1>
#include <QWaylandXdgShell>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <cassert>

int main(int argc, char* argv[]) {
  QString runtimeDir = QStringLiteral("/tmp/qt-wl-%1").arg(QCoreApplication::applicationPid());
  QDir().mkpath(runtimeDir);
  QFile::setPermissions(runtimeDir, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
  qputenv("XDG_RUNTIME_DIR", runtimeDir.toUtf8());

  QGuiApplication app(argc, argv);

  // 1. Instantiate QWaylandCompositor and register extensions before create()
  QWaylandCompositor compositor;
  compositor.setSocketName(QByteArrayLiteral("qt-m6-smoke-wayland-0"));

  QWaylandXdgShell xdgShell(&compositor);
  QWaylandXdgDecorationManagerV1 xdgDecoration;
  xdgDecoration.setExtensionContainer(&compositor);
  QWaylandXdgOutputManagerV1 xdgOutput(&compositor);
  QWaylandWlShell wlShell(&compositor);
  QWaylandIviApplication iviApp(&compositor);
  QWaylandViewporter viewporter(&compositor);
  QWaylandIdleInhibitManagerV1 idleInhibit(&compositor);

  compositor.create();
  QCoreApplication::sendPostedEvents();
  assert(compositor.isCreated());
  assert(compositor.defaultSeat() != nullptr);

  // 2. Verify extensions initialized when compositor was created
  assert(xdgShell.isInitialized());
  xdgDecoration.initialize();
  assert(xdgDecoration.isInitialized());
  assert(xdgOutput.isInitialized());
  assert(wlShell.isInitialized());
  assert(iviApp.isInitialized());
  assert(viewporter.isInitialized());
  assert(idleInhibit.isInitialized());

  // 3. Output & mode
  QWaylandOutput output(&compositor, nullptr);
  QWaylandOutputMode mode(QSize(1920, 1080), 60000);
  assert(mode.isValid());
  output.addMode(mode, true);
  output.setCurrentMode(mode);
  assert(output.currentMode() == mode);

  // 4. QML imports for QtWayland.Compositor and extensions
  QQmlEngine engine;
  {
    QQmlComponent comp(&engine);
    comp.setData(
        "import QtQuick\n"
        "import QtWayland.Compositor\n"
        "import QtWayland.Compositor.XdgShell\n"
        "import QtWayland.Compositor.WlShell\n"
        "import QtWayland.Compositor.IviApplication\n"
        "import QtWayland.Compositor.PresentationTime\n"
        "import QtWayland.Compositor.QtShell\n"
        "XdgShell {}\n",
        QUrl());
    assert(!comp.isError());
    QObject* obj = comp.create();
    assert(obj != nullptr);
    delete obj;
  }

  return 0;
}
