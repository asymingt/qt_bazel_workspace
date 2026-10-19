// Phase 5: QtMqtt. Topic validation/matching, plus a full CONNECT ->
// SUBSCRIBE -> PUBLISH -> deliver round trip against an in-process broker.
#include <QtMqtt/QMqttClient>
#include <QtMqtt/QMqttSubscription>
#include <QtMqtt/QMqttTopicFilter>
#include <QtMqtt/QMqttTopicName>
#include <QtNetwork/QTcpServer>
#include <QtNetwork/QTcpSocket>
#include <QtTest/QtTest>

// Just enough of an MQTT 3.1.1 broker for one client: acknowledges CONNECT,
// SUBSCRIBE (granting QoS 0) and PINGREQ, and delivers every QoS 0 PUBLISH back
// to the sender (which is also the only subscriber).
class FakeBroker : public QObject {
    Q_OBJECT

public:
    FakeBroker() {
        connect(&server_, &QTcpServer::newConnection, this, &FakeBroker::onNewConnection);
    }

    bool listen() { return server_.listen(QHostAddress::LocalHost, 0); }
    quint16 port() const { return server_.serverPort(); }

    // Control packet types received, in order (1 = CONNECT, 3 = PUBLISH, ...).
    QList<int> packetTypes;

private:
    void onNewConnection() {
        while (QTcpSocket *socket = server_.nextPendingConnection()) {
            connect(socket, &QTcpSocket::readyRead, this, [this, socket] { onReadyRead(socket); });
        }
    }

    void onReadyRead(QTcpSocket *socket) {
        buffer_ += socket->readAll();
        for (;;) {
            // Fixed header: packet type/flags byte, then a varint remaining length.
            qsizetype pos = 1;
            quint32 length = 0;
            int shift = 0;
            quint8 byte = 0;
            do {
                if (pos >= buffer_.size())
                    return;
                byte = quint8(buffer_.at(pos++));
                length |= quint32(byte & 0x7f) << shift;
                shift += 7;
            } while (byte & 0x80);
            if (buffer_.size() < pos + qsizetype(length))
                return;
            const QByteArray packet = buffer_.left(pos + length);
            const QByteArray body = buffer_.mid(pos, length);
            buffer_.remove(0, pos + length);
            handle(socket, packet, body);
        }
    }

    void handle(QTcpSocket *socket, const QByteArray &packet, const QByteArray &body) {
        const int type = quint8(packet.at(0)) >> 4;
        packetTypes.append(type);
        switch (type) {
        case 1:  // CONNECT -> CONNACK (session not present, accepted)
            socket->write(QByteArray::fromHex("20020000"));
            break;
        case 3:  // PUBLISH (QoS 0) -> deliver it straight back
            socket->write(packet);
            break;
        case 8: {  // SUBSCRIBE -> SUBACK echoing the packet id, granting QoS 0
            QByteArray ack = QByteArray::fromHex("9003");
            ack += body.left(2);
            ack += char(0);
            socket->write(ack);
            break;
        }
        case 12:  // PINGREQ -> PINGRESP
            socket->write(QByteArray::fromHex("d000"));
            break;
        case 14:  // DISCONNECT
            socket->disconnectFromHost();
            break;
        default:
            break;
        }
    }

    QTcpServer server_;
    QByteArray buffer_;
};

class MqttTest : public QObject {
    Q_OBJECT

private slots:
    void topicValidationAndMatching() {
        QVERIFY(QMqttTopicFilter(QStringLiteral("sensors/+/temperature")).isValid());
        QVERIFY(QMqttTopicFilter(QStringLiteral("sensors/#")).isValid());
        QVERIFY(!QMqttTopicFilter(QStringLiteral("sensors/#/temperature")).isValid());
        QVERIFY(!QMqttTopicName(QStringLiteral("sensors/+/temperature")).isValid());

        const QMqttTopicName kitchen(QStringLiteral("sensors/kitchen/temperature"));
        QVERIFY(kitchen.isValid());
        QCOMPARE(kitchen.levelCount(), 3);
        QVERIFY(QMqttTopicFilter(QStringLiteral("sensors/+/temperature")).match(kitchen));
        QVERIFY(QMqttTopicFilter(QStringLiteral("sensors/#")).match(kitchen));
        QVERIFY(!QMqttTopicFilter(QStringLiteral("sensors/+/humidity")).match(kitchen));
    }

    void publishSubscribeRoundTrip() {
        FakeBroker broker;
        QVERIFY(broker.listen());

        QMqttClient client;
        client.setHostname(QStringLiteral("127.0.0.1"));
        client.setPort(broker.port());
        client.setClientId(QStringLiteral("phase5-mqtt-test"));
        client.setProtocolVersion(QMqttClient::MQTT_3_1_1);
        client.connectToHost();
        QTRY_COMPARE(client.state(), QMqttClient::Connected);
        QCOMPARE(client.error(), QMqttClient::NoError);

        QMqttSubscription *subscription =
                client.subscribe(QMqttTopicFilter(QStringLiteral("sensors/+/temperature")), 0);
        QVERIFY(subscription);
        QTRY_COMPARE(subscription->state(), QMqttSubscription::Subscribed);

        QSignalSpy received(subscription, &QMqttSubscription::messageReceived);
        QVERIFY(client.publish(QMqttTopicName(QStringLiteral("sensors/kitchen/temperature")),
                               QByteArrayLiteral("21.5"), 0) != -1);
        QTRY_COMPARE(received.size(), 1);
        const auto message = received.at(0).at(0).value<QMqttMessage>();
        QCOMPARE(message.topic().name(), QStringLiteral("sensors/kitchen/temperature"));
        QCOMPARE(message.payload(), QByteArrayLiteral("21.5"));

        client.disconnectFromHost();
        QTRY_COMPARE(client.state(), QMqttClient::Disconnected);
        QCOMPARE(broker.packetTypes.first(), 1);  // CONNECT
        QVERIFY(broker.packetTypes.contains(8));  // SUBSCRIBE
        QVERIFY(broker.packetTypes.contains(3));  // PUBLISH
    }
};

QTEST_GUILESS_MAIN(MqttTest)
#include "mqtt_test.moc"
