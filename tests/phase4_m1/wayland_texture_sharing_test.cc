#include <QtCore/QFile>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonObject>
#include <QtCore/QObject>
#include <QtCore/QPluginLoader>
#include <QtTest/QtTest>

class WaylandTextureSharingTest : public QObject {
  Q_OBJECT

 private slots:
  void testTextureSharingQmldirAndPluginMetadata() {
    QVERIFY(QFile::exists(QStringLiteral(
        ":/qt-project.org/imports/QtWayland/Client/TextureSharing/qmldir")));
    QVERIFY(QFile::exists(QStringLiteral(
        ":/qt-project.org/imports/QtWayland/Compositor/TextureSharingExtension/qmldir")));

    bool foundClientPlugin = false;
    bool foundCompositorPlugin = false;
    for (const QStaticPlugin& sp : QPluginLoader::staticPlugins()) {
      const QJsonObject meta = sp.metaData();
      const QString className = meta.value(QStringLiteral("className")).toString();
      QJsonArray uris = meta.value(QStringLiteral("uri")).toArray();
      if (uris.isEmpty()) {
        uris = meta.value(QStringLiteral("URI")).toArray();
      }
      if (className == QStringLiteral("QWaylandTextureSharingPlugin")) {
        foundClientPlugin = true;
        QVERIFY(!uris.isEmpty());
        QCOMPARE(uris.first().toString(),
                 QStringLiteral("QtWayland.Client.TextureSharing"));
      } else if (className == QStringLiteral("QWaylandTextureSharingExtensionPlugin")) {
        foundCompositorPlugin = true;
        QVERIFY(!uris.isEmpty());
        QCOMPARE(uris.first().toString(),
                 QStringLiteral("QtWayland.Compositor.TextureSharingExtension"));
      }
    }
    QVERIFY(foundClientPlugin);
    QVERIFY(foundCompositorPlugin);
  }
};

QTEST_MAIN(WaylandTextureSharingTest)
#include "wayland_texture_sharing_test.moc"
