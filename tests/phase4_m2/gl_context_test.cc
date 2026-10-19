#include <QtCore/QObject>
#include <QtCore/QPluginLoader>
#include <QtCore/QProcess>
#include <QtGui/QGuiApplication>
#include <QtGui/QOffscreenSurface>
#include <QtGui/QOpenGLContext>
#include <QtGui/QOpenGLFunctions>
#include <QtGui/private/qtgui-config_p.h>
#include <QtGui/qtguiglobal.h>
#if QT_CONFIG(vulkan)
#include <QtGui/QVulkanInstance>
#include <QtGui/QVulkanWindow>
#endif
#include <QtOpenGLWidgets/QOpenGLWidget>
#include <QtTest/QtTest>

class GlContextTest : public QObject {
  Q_OBJECT

 private slots:
  void testStaticPluginsAndFeatures() {
#if defined(__linux__)
    // F-LNX-1: XCB GLX and EGL integration plugins must be enabled in qtgui-config
    // and statically linked into every GUI binary alongside xcb and wayland.
#if !QT_CONFIG(xcb_glx_plugin)
    QFAIL("QT_FEATURE_xcb_glx_plugin is not enabled!");
#endif
#if !QT_CONFIG(xcb_glx)
    QFAIL("QT_FEATURE_xcb_glx is not enabled!");
#endif
#if !QT_CONFIG(xcb_egl_plugin)
    QFAIL("QT_FEATURE_xcb_egl_plugin is not enabled!");
#endif
#if !QT_CONFIG(egl_x11)
    QFAIL("QT_FEATURE_egl_x11 is not enabled!");
#endif

    bool hasGlxPlugin = false;
    bool hasEglPlugin = false;
    bool hasWaylandPlugin = false;
    bool hasXcbPlugin = false;
    for (const QStaticPlugin& sp : QPluginLoader::staticPlugins()) {
      const QString cls =
          sp.metaData().value(QStringLiteral("className")).toString();
      if (cls == QStringLiteral("QXcbGlxIntegrationPlugin")) {
        hasGlxPlugin = true;
      } else if (cls == QStringLiteral("QXcbEglIntegrationPlugin")) {
        hasEglPlugin = true;
      } else if (cls == QStringLiteral("QWaylandIntegrationPlugin")) {
        hasWaylandPlugin = true;
      } else if (cls == QStringLiteral("QXcbIntegrationPlugin")) {
        hasXcbPlugin = true;
      }
    }
    QVERIFY2(hasXcbPlugin, "QXcbIntegrationPlugin not auto-linked");
    QVERIFY2(hasGlxPlugin, "QXcbGlxIntegrationPlugin not auto-linked");
    QVERIFY2(hasEglPlugin, "QXcbEglIntegrationPlugin not auto-linked");
    QVERIFY2(hasWaylandPlugin, "QWaylandIntegrationPlugin not auto-linked");
#endif

    // F-LNX-5: Vulkan must be enabled across platforms.
#if !QT_CONFIG(vulkan)
    QFAIL("QT_FEATURE_vulkan is not enabled!");
#else
    QVulkanInstance vkInst;
    // Even if a system lacks a runtime Vulkan ICD in the test container,
    // QVulkanInstance must be constructible and linkable.
    Q_UNUSED(vkInst);
#endif
  }

  void testXcbOpenGLContextUnderXvfb() {
#if defined(__linux__)
    // If running in offscreen mode and Xvfb is installed, spawn self under Xvfb
    // with QT_QPA_PLATFORM=xcb to verify real X11 GLX context creation.
    if (QGuiApplication::platformName() == QStringLiteral("offscreen")) {
      QString xvfbPath = QStandardPaths::findExecutable(QStringLiteral("Xvfb"));
      if (xvfbPath.isEmpty()) {
        QSKIP("Xvfb not available on this host");
      }
      QProcess xvfb;
      xvfb.start(xvfbPath, {QStringLiteral(":99"), QStringLiteral("-screen"),
                            QStringLiteral("0"), QStringLiteral("640x480x24"),
                            QStringLiteral("-nolisten"), QStringLiteral("tcp")});
      if (!xvfb.waitForStarted(3000)) {
        QSKIP("Could not start Xvfb :99");
      }
      QTest::qWait(200);

      QProcess child;
      QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
      env.insert(QStringLiteral("DISPLAY"), QStringLiteral(":99"));
      env.insert(QStringLiteral("QT_QPA_PLATFORM"), QStringLiteral("xcb"));
      env.insert(QStringLiteral("LIBGL_ALWAYS_SOFTWARE"), QStringLiteral("1"));
      child.setProcessEnvironment(env);
      child.start(QCoreApplication::applicationFilePath(),
                  {QStringLiteral("testXcbOpenGLContextUnderXvfb")});
      bool finished = child.waitForFinished(15000);
      xvfb.terminate();
      xvfb.waitForFinished(2000);
      QVERIFY2(finished, "Child XCB OpenGL test timed out");
      if (child.exitCode() != 0) {
        qWarning().noquote() << child.readAllStandardOutput();
        qWarning().noquote() << child.readAllStandardError();
      }
      QCOMPARE(child.exitCode(), 0);
      return;
    }

    QCOMPARE(QGuiApplication::platformName(), QStringLiteral("xcb"));
    QOpenGLContext ctx;
    QVERIFY2(ctx.create(), "Failed to create QOpenGLContext on xcb platform");

    QOffscreenSurface surface;
    surface.setFormat(ctx.format());
    surface.create();
    QVERIFY(surface.isValid());
    QVERIFY(ctx.makeCurrent(&surface));

    QOpenGLFunctions* f = ctx.functions();
    QVERIFY(f != nullptr);
    const char* vendor =
        reinterpret_cast<const char*>(f->glGetString(GL_VENDOR));
    QVERIFY(vendor != nullptr);
    ctx.doneCurrent();

    QOpenGLWidget widget;
    widget.resize(128, 128);
    widget.show();
    QVERIFY(QTest::qWaitForWindowExposed(&widget));
    QVERIFY(widget.isValid());
#endif
  }
};

QTEST_MAIN(GlContextTest)
#include "gl_context_test.moc"
