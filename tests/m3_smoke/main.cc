// Copyright (C) 2026
// SPDX-License-Identifier: Apache-2.0
//
// Phase 2 / Milestone 3 headless smoke test.

#include <QtCore/QCoreApplication>
#include <QtCore/QEventLoop>
#include <QtCore/QTimer>
#include <QtTest/QtTest>

#include <QtCore5Compat/QTextCodec>
#include <QtHttpServer/QHttpServer>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QTcpServer>
#include <QtPositioning/QGeoCoordinate>
#include <QtRemoteObjects/QRemoteObjectHost>
#include <QtScxml/QScxmlStateMachine>
#include <QtSensors/QSensor>
#include <QtSerialBus/QCanBus>
#include <QtSerialPort/QSerialPortInfo>
#include <QtStateMachine/QFinalState>
#include <QtStateMachine/QState>
#include <QtStateMachine/QStateMachine>
#include <QtWebSockets/QWebSocket>
#include <QtWebSockets/QWebSocketServer>

class M3SmokeTest : public QObject {
  Q_OBJECT

 private slots:
  void core5compat() {
    QTextCodec* utf8 = QTextCodec::codecForName("UTF-8");
    QVERIFY(utf8 != nullptr);
    QCOMPARE(utf8->toUnicode(QByteArray("h\xC3\xA9")), QString::fromUtf8("h\xC3\xA9"));
  }

  void websockets_echo() {
    QWebSocketServer server(QStringLiteral("m3"), QWebSocketServer::NonSecureMode);
    QVERIFY(server.listen(QHostAddress::LocalHost));
    QWebSocket* serverSide = nullptr;
    connect(&server, &QWebSocketServer::newConnection, this, [&] {
      serverSide = server.nextPendingConnection();
      connect(serverSide, &QWebSocket::textMessageReceived, serverSide,
              [serverSide](const QString& m) { serverSide->sendTextMessage(m + "!"); });
    });

    QWebSocket client;
    QString received;
    connect(&client, &QWebSocket::connected, &client,
            [&] { client.sendTextMessage(QStringLiteral("ping")); });
    connect(&client, &QWebSocket::textMessageReceived, &client,
            [&](const QString& m) { received = m; });
    client.open(server.serverUrl());
    QTRY_COMPARE_WITH_TIMEOUT(received, QStringLiteral("ping!"), 5000);
  }

  void httpserver_roundtrip() {
    QHttpServer http;
    http.route("/hello", [] { return QStringLiteral("world"); });
    auto* tcp = new QTcpServer(&http);
    QVERIFY(tcp->listen(QHostAddress::LocalHost));
    QVERIFY(http.bind(tcp));

    QNetworkAccessManager nam;
    const QUrl url(QStringLiteral("http://127.0.0.1:%1/hello").arg(tcp->serverPort()));
    QNetworkReply* reply = nam.get(QNetworkRequest(url));
    QTRY_VERIFY_WITH_TIMEOUT(reply->isFinished(), 5000);
    QCOMPARE(reply->error(), QNetworkReply::NoError);
    QCOMPARE(reply->readAll(), QByteArray("world"));
    reply->deleteLater();
  }

  void serialport_enumerates() {
    // Just make sure the platform backend links and runs; the list may be empty.
    const auto ports = QSerialPortInfo::availablePorts();
    QVERIFY(ports.size() >= 0);
    QVERIFY(!QSerialPortInfo::standardBaudRates().isEmpty());
  }

  void serialbus_virtualcan_plugin() {
    const QStringList plugins = QCanBus::instance()->plugins();
    QVERIFY2(plugins.contains(QStringLiteral("virtualcan")), qPrintable(plugins.join(',')));
  }

  void sensors_dummy_backend() {
    const QList<QByteArray> types = QSensor::sensorTypes();
    QVERIFY2(!types.isEmpty(), "dummy sensor backend did not register");
  }

  void positioning_math() {
    const QGeoCoordinate oslo(59.9139, 10.7522);
    const QGeoCoordinate berlin(52.5200, 13.4050);
    const qreal km = oslo.distanceTo(berlin) / 1000.0;
    QVERIFY(km > 800 && km < 900);
  }

  void statemachine_runs() {
    QStateMachine machine;
    auto* s1 = new QState(&machine);
    auto* s2 = new QFinalState(&machine);
    s1->addTransition(s2);
    machine.setInitialState(s1);
    QSignalSpy finished(&machine, &QStateMachine::finished);
    machine.start();
    QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 2000);
  }

  void scxml_dynamic_load() {
    const char kScxml[] =
        "<scxml xmlns='http://www.w3.org/2005/07/scxml' version='1.0' initial='a'>"
        "<state id='a'><transition event='go' target='b'/></state>"
        "<final id='b'/></scxml>";
    QTemporaryFile file;
    QVERIFY(file.open());
    file.write(kScxml);
    file.flush();
    QScopedPointer<QScxmlStateMachine> sm(QScxmlStateMachine::fromFile(file.fileName()));
    QVERIFY2(sm && !sm->parseErrors().size(), "scxml compile failed");
    sm->start();
    QTRY_VERIFY_WITH_TIMEOUT(sm->isRunning(), 2000);
    sm->submitEvent(QStringLiteral("go"));
    QTRY_VERIFY_WITH_TIMEOUT(!sm->isRunning(), 2000);
  }

  void remoteobjects_host() {
    QRemoteObjectHost host(QUrl(QStringLiteral("local:m3smoke")));
    QVERIFY(host.hostUrl().isValid());
  }
};

QTEST_GUILESS_MAIN(M3SmokeTest)
#include "main.moc"
