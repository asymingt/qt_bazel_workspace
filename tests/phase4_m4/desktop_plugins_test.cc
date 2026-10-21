#include <QtCore/QPluginLoader>
#include <QtGui/QAccessible>
#include <QtGui/private/qtgui-config_p.h>
#include <QtNetwork/QNetworkInformation>
#include <QtNetwork/QSslSocket>
#include <QtSql/QSqlDatabase>
#include <QtTest/QtTest>
#include <QtWidgets/QStyleFactory>
#include <QtWidgets/private/qtwidgets-config_p.h>

class DesktopPluginsTest : public QObject {
  Q_OBJECT

 private slots:
  void testWidgetStyles() {
    const QStringList styles = QStyleFactory::keys();
    QVERIFY(styles.contains(QStringLiteral("Fusion"), Qt::CaseInsensitive));
    QVERIFY(styles.contains(QStringLiteral("Windows"), Qt::CaseInsensitive));
#if defined(Q_OS_MACOS)
    QVERIFY(QT_CONFIG(style_mac));
    QVERIFY(styles.contains(QStringLiteral("macOS"), Qt::CaseInsensitive));
#elif defined(Q_OS_WIN)
    QVERIFY(QT_CONFIG(style_windowsvista));
    QVERIFY(QT_CONFIG(style_windows11));
    QVERIFY(styles.contains(QStringLiteral("windowsvista"), Qt::CaseInsensitive));
#endif
  }

  void testLinuxDesktopIntegration() {
#if defined(Q_OS_LINUX)
    // Verify AT-SPI accessibility bridge is compiled into QtGui on Linux (F-LNX-4).
    QVERIFY(QT_CONFIG(accessibility_atspi_bridge));

    // Verify compose, ibus, and xdg-desktop-portal static plugins are registered.
    const QList<QStaticPlugin> plugins = QPluginLoader::staticPlugins();
    bool hasCompose = false;
    bool hasIbus = false;
    bool hasXdgDesktopPortal = false;
    for (const QStaticPlugin& plugin : plugins) {
      const QJsonObject meta = plugin.metaData();
      const QString className = meta.value(QStringLiteral("className")).toString();
      if (className == QStringLiteral("QComposePlatformInputContextPlugin")) {
        hasCompose = true;
      } else if (className == QStringLiteral("QIbusPlatformInputContextPlugin")) {
        hasIbus = true;
      } else if (className == QStringLiteral("QXdgDesktopPortalThemePlugin")) {
        hasXdgDesktopPortal = true;
      }
    }
    QVERIFY2(hasCompose, "QComposePlatformInputContextPlugin not registered");
    QVERIFY2(hasIbus, "QIbusPlatformInputContextPlugin not registered");
    QVERIFY2(hasXdgDesktopPortal, "QXdgDesktopPortalThemePlugin not registered");
#endif
  }

  void testNetworkInformationBackends() {
    const QStringList backends = QNetworkInformation::availableBackends();
    QVERIFY2(!backends.isEmpty(), "Expected at least one QNetworkInformation backend");
#if defined(Q_OS_LINUX)
    QVERIFY(backends.contains(QStringLiteral("glib")) ||
            backends.contains(QStringLiteral("networkmanager")));
#elif defined(Q_OS_MACOS)
    QVERIFY(backends.contains(QStringLiteral("applenetworkinformation")));
#elif defined(Q_OS_WIN)
    QVERIFY(backends.contains(QStringLiteral("networklistmanager")));
#endif
  }

  void testTlsBackends() {
    const QList<QString> tlsBackends = QSslSocket::availableBackends();
    QVERIFY(tlsBackends.contains(QStringLiteral("openssl")));
    QVERIFY(tlsBackends.contains(QStringLiteral("cert-only")));
#if defined(Q_OS_MACOS)
    QVERIFY(tlsBackends.contains(QStringLiteral("securetransport")));
#elif defined(Q_OS_WIN)
    QVERIFY(tlsBackends.contains(QStringLiteral("schannel")));
#endif
  }

  void testSqlDriversAndTracing() {
    const QStringList drivers = QSqlDatabase::drivers();
    QVERIFY(drivers.contains(QStringLiteral("QSQLITE")));
#if defined(Q_OS_WIN)
    QVERIFY(drivers.contains(QStringLiteral("QODBC")));
#endif

    const QList<QStaticPlugin> plugins = QPluginLoader::staticPlugins();
    bool hasCtf = false;
    for (const QStaticPlugin& plugin : plugins) {
      if (plugin.metaData().value(QStringLiteral("className")).toString() ==
          QStringLiteral("QCtfTracePlugin")) {
        hasCtf = true;
      }
    }
    QVERIFY2(hasCtf, "QCtfTracePlugin not registered");
  }
};

QTEST_MAIN(DesktopPluginsTest)
#include "desktop_plugins_test.moc"
