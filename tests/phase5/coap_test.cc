// Phase 5: QtCoap. Message/option API, plus confirmable and non-confirmable
// GETs answered by an in-process CoAP server over UDP.
#include <QtCoap/QCoapClient>
#include <QtCoap/QCoapMessage>
#include <QtCoap/QCoapOption>
#include <QtCoap/QCoapReply>
#include <QtCoap/QCoapRequest>
#include <QtNetwork/QNetworkDatagram>
#include <QtNetwork/QUdpSocket>
#include <QtTest/QtTest>

// Answers each request with "2.05 Content" and a payload of "echo:<Uri-Path>":
// piggybacked on the ACK for confirmable requests (RFC 7252 section 5.2.1), as
// a separate non-confirmable message otherwise (section 5.2.2).
class FakeCoapServer : public QObject {
    Q_OBJECT

public:
    FakeCoapServer() {
        connect(&socket_, &QUdpSocket::readyRead, this, &FakeCoapServer::onReadyRead);
    }

    bool bind() { return socket_.bind(QHostAddress::LocalHost, 0); }
    quint16 port() const { return socket_.localPort(); }

    QStringList requestedPaths;

private:
    void onReadyRead() {
        while (socket_.hasPendingDatagrams()) {
            const QNetworkDatagram datagram = socket_.receiveDatagram();
            const QByteArray in = datagram.data();
            if (in.size() < 4)
                continue;
            const quint8 first = quint8(in.at(0));
            const int type = (first >> 4) & 0x3;
            const int tokenLength = first & 0xf;
            const QByteArray messageId = in.mid(2, 2);
            const QByteArray token = in.mid(4, tokenLength);

            // Walk the options, collecting Uri-Path (11) segments.
            QStringList path;
            qsizetype pos = 4 + tokenLength;
            int optionNumber = 0;
            while (pos < in.size() && quint8(in.at(pos)) != 0xff) {
                const quint8 header = quint8(in.at(pos++));
                int delta = header >> 4;
                int length = header & 0xf;
                auto extended = [&](int nibble) {
                    if (nibble == 13)
                        return 13 + quint8(in.at(pos++));
                    if (nibble == 14) {
                        const int value = (quint8(in.at(pos)) << 8 | quint8(in.at(pos + 1))) + 269;
                        pos += 2;
                        return value;
                    }
                    return nibble;
                };
                delta = extended(delta);
                length = extended(length);
                optionNumber += delta;
                if (optionNumber == 11)
                    path.append(QString::fromUtf8(in.mid(pos, length)));
                pos += length;
            }
            const QString joined = path.join(QLatin1Char('/'));
            requestedPaths.append(joined);

            // A confirmable request gets a piggybacked ACK (same message id);
            // a non-confirmable one a non-confirmable response (new id).
            const bool confirmable = type == 0;
            QByteArray out;
            out += char(0x40 | ((confirmable ? 2 : 1) << 4) | tokenLength);  // Version 1
            out += char(0x45);                                              // 2.05 Content
            out += confirmable ? messageId : QByteArray::fromHex("beef");
            out += token;
            out += char(0xff);
            out += "echo:" + joined.toUtf8();
            socket_.writeDatagram(datagram.makeReply(out));
        }
    }

    QUdpSocket socket_;
};

class CoapTest : public QObject {
    Q_OBJECT

private slots:
    void messageAndOptions() {
        QCoapRequest request(QUrl(QStringLiteral("coap://127.0.0.1/sensors/temperature")),
                             QCoapMessage::Type::NonConfirmable);
        QCOMPARE(request.url().path(), QStringLiteral("/sensors/temperature"));
        QCOMPARE(request.type(), QCoapMessage::Type::NonConfirmable);
        request.addOption(QCoapOption::UriQuery, QByteArrayLiteral("unit=celsius"));
        QVERIFY(request.hasOption(QCoapOption::UriQuery));
        QCOMPARE(request.option(QCoapOption::UriQuery).opaqueValue(),
                 QByteArrayLiteral("unit=celsius"));
        request.setPayload(QByteArrayLiteral("payload"));
        QCOMPARE(request.payload(), QByteArrayLiteral("payload"));

        request.setMessageId(0x1234);
        QCOMPARE(request.messageId(), 0x1234);
        request.setToken(QByteArrayLiteral("tok"));
        QCOMPARE(request.token(), QByteArrayLiteral("tok"));
    }

    void get_data() {
        QTest::addColumn<QCoapMessage::Type>("type");
        QTest::newRow("confirmable") << QCoapMessage::Type::Confirmable;
        QTest::newRow("non-confirmable") << QCoapMessage::Type::NonConfirmable;
    }

    void get() {
        QFETCH(QCoapMessage::Type, type);
        FakeCoapServer server;
        QVERIFY(server.bind());

        QCoapClient client;
        QSignalSpy finished(&client, &QCoapClient::finished);
        const QCoapRequest request(
                QUrl(QStringLiteral("coap://127.0.0.1:%1/sensors/temperature").arg(server.port())),
                type);
        std::unique_ptr<QCoapReply> reply(client.get(request));
        QVERIFY(reply);
        QTRY_VERIFY(reply->isFinished());
        QVERIFY2(!reply->isAborted(), "request was aborted");
        QCOMPARE(reply->errorReceived(), QtCoap::Error::Ok);
        QCOMPARE(reply->responseCode(), QtCoap::ResponseCode::Content);
        QCOMPARE(reply->readAll(), QByteArrayLiteral("echo:sensors/temperature"));
        // QCoapClient::finished is relayed reply -> protocol (worker thread)
        // -> client (this thread) via queued connections, so it can arrive
        // after reply->isFinished() is already true.
        QTRY_COMPARE(finished.size(), 1);
        QCOMPARE(server.requestedPaths, QStringList{QStringLiteral("sensors/temperature")});
    }
};

QTEST_GUILESS_MAIN(CoapTest)
#include "coap_test.moc"
