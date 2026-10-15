// Copyright (C) 2026
// SPDX-License-Identifier: Apache-2.0
//
// Phase 2 / Milestone 3 QML / GUI smoke test (runs on the offscreen QPA).

#include <QtCore/QTemporaryDir>
#include <QtGui/QGuiApplication>
#include <QtGui/QImage>
#include <QtQml/QQmlComponent>
#include <QtQml/QQmlEngine>
#include <QtQuick/QQuickItem>
#include <QtTest/QtTest>
#include <QtWidgets/QApplication>

#include <QtCharts/QChart>
#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtWebChannel/QWebChannel>

class M3QuickSmokeTest : public QObject {
  Q_OBJECT

 private slots:
  void charts_widget_renders() {
    auto* series = new QLineSeries();
    series->append(0, 0);
    series->append(1, 1);
    auto* chart = new QChart();
    chart->addSeries(series);
    chart->createDefaultAxes();
    QChartView view(chart);
    view.resize(200, 120);
    const QImage img = view.grab().toImage();
    QVERIFY(!img.isNull());
    // QChartView applies its own minimum size, so only check we got a
    // non-trivial, non-uniform rendering (axes + line on a background).
    QVERIFY(img.width() >= 200 && img.height() >= 120);
    bool nonUniform = false;
    const QRgb first = img.pixel(0, 0);
    for (int y = 0; y < img.height() && !nonUniform; y += 4)
      for (int x = 0; x < img.width(); x += 4)
        if (img.pixel(x, y) != first) { nonUniform = true; break; }
    QVERIFY(nonUniform);
  }

  void webchannel_constructs() {
    QWebChannel channel;
    QObject obj;
    channel.registerObject(QStringLiteral("obj"), &obj);
    QVERIFY(channel.registeredObjects().contains(QStringLiteral("obj")));
  }

  void qml_imports_resolve() {
    QQmlEngine engine;
    const char kQml[] =
        "import QtQuick\n"
        "import QtCharts\n"
        "import QtPositioning\n"
        "import QtLocation\n"
        "import Qt.labs.lottieqt\n"
        "import QtQuick.Timeline\n"
        "import QtQml.StateMachine as DSM\n"
        "import QtWebChannel\n"
        "Item {\n"
        "  property var coord: QtPositioning.coordinate(59.9, 10.7)\n"
        "  Plugin { id: p; name: 'itemsoverlay' }\n"
        "  Map { plugin: p; width: 10; height: 10 }\n"
        "  ChartView { width: 10; height: 10 }\n"
        "  Timeline { enabled: false }\n"
        "  WebChannel {}\n"
        "  DSM.StateMachine { running: false; DSM.State { id: s0 } initialState: s0 }\n"
        "}\n";
    QQmlComponent component(&engine);
    component.setData(QByteArray(kQml), QUrl(QStringLiteral("qrc:/m3_quick_smoke.qml")));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    QScopedPointer<QObject> root(component.create());
    QVERIFY2(root, qPrintable(component.errorString()));
    QVERIFY(root->property("coord").isValid());
  }
};

int main(int argc, char** argv) {
  qputenv("QT_QPA_PLATFORM", "offscreen");
  QApplication app(argc, argv);
  M3QuickSmokeTest tc;
  QTEST_SET_MAIN_SOURCE_PATH
  return QTest::qExec(&tc, argc, argv);
}

#include "quick_main.moc"
