#include <QCoreApplication>
#include <QtBluetooth/qtbluetooth-config.h>
#include <QtNfc/qtnfc-config.h>
#include <QBluetoothLocalDevice>
#include <QNearFieldManager>
#include <QGeoAreaMonitorSource>
#include <QGeoPositionInfoSource>
#include <QCanBus>
#include <QQmlEngine>
#include <QQmlComponent>
#include <cassert>
#include <cstdio>

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);

#if defined(__linux__)
    static_assert(QT_CONFIG(bluez), "QT_FEATURE_bluez must be enabled on Linux");
    static_assert(QT_CONFIG(neard), "QT_FEATURE_neard must be enabled on Linux");
#elif defined(__APPLE__) || defined(_WIN32)
    static_assert(QT_CONFIG(pcsclite), "QT_FEATURE_pcsclite must be enabled on macOS/Windows");
#endif

    // Verify Bluetooth and NFC classes link and instantiate
    QBluetoothLocalDevice btDevice;
    QNearFieldManager nfcManager;
    (void)btDevice.isValid();
    (void)nfcManager.isSupported();

    // Verify Positioning sources
    const QStringList posSources = QGeoPositionInfoSource::availableSources();
    std::printf("Positioning sources: %s\n", qPrintable(posSources.join(", ")));
    assert(posSources.contains("nmea"));
    // positionpoll is an area-monitor-only plugin ("Position": false).
    const QStringList monitorSources = QGeoAreaMonitorSource::availableSources();
    std::printf("Area monitor sources: %s\n", qPrintable(monitorSources.join(", ")));
    assert(monitorSources.contains("positionpoll"));
#if defined(__linux__)
    assert(posSources.contains("geoclue2"));
#elif defined(__APPLE__)
    assert(posSources.contains("corelocation"));
#endif

    // Verify CAN bus plugins
    const QStringList canPlugins = QCanBus::instance()->plugins();
    std::printf("CAN bus plugins: %s\n", qPrintable(canPlugins.join(", ")));
    assert(canPlugins.contains("virtualcan"));
    assert(canPlugins.contains("passthrucan"));
    assert(canPlugins.contains("peakcan"));
    assert(canPlugins.contains("tinycan"));
#if defined(__linux__)
    assert(canPlugins.contains("socketcan"));
#elif defined(_WIN32)
    assert(canPlugins.contains("systeccan"));
    assert(canPlugins.contains("vectorcan"));
#endif

    // Verify QtSensors QML module foreign types (Sensor reading/properties from C++ QSensor)
    QQmlEngine engine;
    QQmlComponent comp(&engine);
    comp.setData("import QtQuick\nimport QtSensors\nAccelerometer { active: false }\n", QUrl());
    if (comp.isError()) {
        std::fprintf(stderr, "QtSensors QML error: %s\n", qPrintable(comp.errorString()));
        return 1;
    }
    QObject* obj = comp.create();
    assert(obj != nullptr);
    delete obj;

    return 0;
}
