// Phase 5: QtOpcUa. Backend discovery, namespace 0 id names, the
// open62541 backend end to end (endpoints, connect, read, write) against an
// in-process open62541 server, and the OpenSSL-backed x509 helpers.
#include <QtNetwork/QTcpServer>
#include <QtOpcUa/QOpcUaClient>
#include <QtOpcUa/QOpcUaEndpointDescription>
#include <QtOpcUa/QOpcUaNode>
#include <QtOpcUa/QOpcUaProvider>
#if !defined(Q_OS_MACOS)
#include <QtOpcUa/QOpcUaKeyPair>
#endif
#include <QtTest/QtTest>

#include <atomic>
#include <memory>
#include <thread>

#include <open62541.h>

namespace {

constexpr char kNodeId[] = "ns=1;s=TheAnswer";

// An open62541 server with one writable Int32 variable (ns=1;s=TheAnswer = 42),
// serviced on a background thread.
class TestServer {
public:
    ~TestServer() { stop(); }

    bool start(quint16 port) {
        UA_ServerConfig config;
        memset(&config, 0, sizeof(config));
        config.logging = UA_Log_Stdout_new(UA_LOGLEVEL_WARNING);
        if (UA_ServerConfig_setMinimal(&config, port, nullptr) != UA_STATUSCODE_GOOD)
            return false;
        // Listen on, and advertise, the loopback address the client connects
        // to (the default advertises the host name).
        UA_Array_delete(config.serverUrls, config.serverUrlsSize, &UA_TYPES[UA_TYPES_STRING]);
        config.serverUrls = static_cast<UA_String *>(UA_Array_new(1, &UA_TYPES[UA_TYPES_STRING]));
        config.serverUrlsSize = 1;
        config.serverUrls[0] =
                UA_String_fromChars(QStringLiteral("opc.tcp://127.0.0.1:%1").arg(port).toUtf8());
        server_ = UA_Server_newWithConfig(&config);
        if (!server_)
            return false;

        UA_VariableAttributes attributes = UA_VariableAttributes_default;
        UA_Int32 value = 42;
        UA_Variant_setScalar(&attributes.value, &value, &UA_TYPES[UA_TYPES_INT32]);
        attributes.displayName =
                UA_LOCALIZEDTEXT(const_cast<char *>("en-US"), const_cast<char *>("TheAnswer"));
        attributes.accessLevel = UA_ACCESSLEVELMASK_READ | UA_ACCESSLEVELMASK_WRITE;
        const UA_StatusCode added = UA_Server_addVariableNode(
                server_, UA_NODEID_STRING(1, const_cast<char *>("TheAnswer")),
                UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER),
                UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES),
                UA_QUALIFIEDNAME(1, const_cast<char *>("TheAnswer")),
                UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE), attributes, nullptr,
                nullptr);
        if (added != UA_STATUSCODE_GOOD || UA_Server_run_startup(server_) != UA_STATUSCODE_GOOD)
            return false;
        thread_ = std::thread([this] {
            while (running_)
                UA_Server_run_iterate(server_, true);
        });
        return true;
    }

    void stop() {
        if (thread_.joinable()) {
            running_ = false;
            thread_.join();
            UA_Server_run_shutdown(server_);
        }
        if (server_) {
            UA_Server_delete(server_);
            server_ = nullptr;
        }
    }

private:
    UA_Server *server_ = nullptr;
    std::thread thread_;
    std::atomic<bool> running_{true};
};

quint16 freeTcpPort() {
    QTcpServer probe;
    if (!probe.listen(QHostAddress::LocalHost, 0))
        return 0;
    return probe.serverPort();
}

}  // namespace

class OpcUaTest : public QObject {
    Q_OBJECT

private slots:
    void backendIsRegistered() {
        QVERIFY2(QOpcUaProvider::availableBackends().contains(QStringLiteral("open62541")),
                 qPrintable(QOpcUaProvider::availableBackends().join(QLatin1Char(','))));
    }

    void namespace0Names() {
        QCOMPARE(QOpcUa::namespace0Id(QOpcUa::NodeIds::Namespace0::ObjectsFolder),
                 QStringLiteral("ns=0;i=85"));
        QCOMPARE(QOpcUa::namespace0IdName(QOpcUa::NodeIds::Namespace0::ObjectsFolder),
                 QStringLiteral("ObjectsFolder"));
    }

    void securityPoliciesIncludeEncryption() {
        QOpcUaProvider provider;
        std::unique_ptr<QOpcUaClient> client(provider.createClient(QStringLiteral("open62541")));
        QVERIFY(client);
        // Only listed when open62541 is built with UA_ENABLE_ENCRYPTION_OPENSSL.
        QVERIFY(client->supportedSecurityPolicies().contains(
                QStringLiteral("http://opcfoundation.org/UA/SecurityPolicy#Basic256Sha256")));
    }

    void connectReadWrite() {
        const quint16 port = freeTcpPort();
        QVERIFY(port != 0);
        TestServer server;
        QVERIFY(server.start(port));

        QOpcUaProvider provider;
        std::unique_ptr<QOpcUaClient> client(provider.createClient(QStringLiteral("open62541")));
        QVERIFY(client);

        // open62541 is built with encryption, so each connection notes that the
        // (certificate-less) test client skips ApplicationURI verification.
        const QRegularExpression noCertificate(
                QStringLiteral("skip verifying ApplicationURI for the SecurityPolicy"));
        QTest::ignoreMessage(QtWarningMsg, noCertificate);
        QTest::ignoreMessage(QtWarningMsg, noCertificate);

        QSignalSpy endpointsSpy(client.get(), &QOpcUaClient::endpointsRequestFinished);
        client->requestEndpoints(QUrl(QStringLiteral("opc.tcp://127.0.0.1:%1").arg(port)));
        QTRY_COMPARE_WITH_TIMEOUT(endpointsSpy.size(), 1, 10000);
        const auto endpoints = endpointsSpy.at(0).at(0).value<QList<QOpcUaEndpointDescription>>();
        QVERIFY(!endpoints.isEmpty());
        QOpcUaEndpointDescription endpoint;
        for (const auto &candidate : endpoints) {
            if (candidate.securityMode() == QOpcUaEndpointDescription::MessageSecurityMode::None)
                endpoint = candidate;
        }
        QCOMPARE(endpoint.securityMode(), QOpcUaEndpointDescription::MessageSecurityMode::None);

        client->connectToEndpoint(endpoint);
        QTRY_COMPARE_WITH_TIMEOUT(client->state(), QOpcUaClient::Connected, 10000);

        std::unique_ptr<QOpcUaNode> node(client->node(QString::fromLatin1(kNodeId)));
        QVERIFY(node);
        QSignalSpy readSpy(node.get(), &QOpcUaNode::attributeRead);
        node->readAttributes(QOpcUa::NodeAttribute::Value | QOpcUa::NodeAttribute::DisplayName);
        QTRY_COMPARE_WITH_TIMEOUT(readSpy.size(), 1, 10000);
        QCOMPARE(node->attribute(QOpcUa::NodeAttribute::Value).toInt(), 42);
        QCOMPARE(node->attribute(QOpcUa::NodeAttribute::DisplayName)
                         .value<QOpcUaLocalizedText>()
                         .text(),
                 QStringLiteral("TheAnswer"));

        QSignalSpy writeSpy(node.get(), &QOpcUaNode::attributeWritten);
        node->writeAttribute(QOpcUa::NodeAttribute::Value, 7, QOpcUa::Types::Int32);
        QTRY_COMPARE_WITH_TIMEOUT(writeSpy.size(), 1, 10000);
        QCOMPARE(writeSpy.at(0).at(1).value<QOpcUa::UaStatusCode>(), QOpcUa::UaStatusCode::Good);

        readSpy.clear();
        node->readAttributes(QOpcUa::NodeAttribute::Value);
        QTRY_COMPARE_WITH_TIMEOUT(readSpy.size(), 1, 10000);
        QCOMPARE(node->attribute(QOpcUa::NodeAttribute::Value).toInt(), 7);

        node.reset();
        client->disconnectFromEndpoint();
        QTRY_COMPARE_WITH_TIMEOUT(client->state(), QOpcUaClient::Disconnected, 10000);
    }

#if !defined(Q_OS_MACOS)
    // QT_FEATURE_gds / x509: key generation goes through the linked OpenSSL.
    void x509KeyPair() {
        QOpcUaKeyPair key;
        QVERIFY(!key.hasPrivateKey());
        QCOMPARE(key.type(), QOpcUaKeyPair::KeyType::Empty);
        key.generateRsaKey(QOpcUaKeyPair::RsaKeyStrength::Bits1024);
        QVERIFY(key.hasPrivateKey());
        QCOMPARE(key.type(), QOpcUaKeyPair::KeyType::Rsa);
        const QByteArray pem = key.publicKeyToByteArray();
        QVERIFY2(pem.startsWith("-----BEGIN PUBLIC KEY-----"), pem.constData());

        // Round-trip the private key through PEM.
        QOpcUaKeyPair loaded;
        QVERIFY(loaded.loadFromPemData(
                key.privateKeyToByteArray(QOpcUaKeyPair::Cipher::Unencrypted, QString())));
        QVERIFY(loaded.hasPrivateKey());
        QCOMPARE(loaded.publicKeyToByteArray(), pem);
    }
#endif
};

QTEST_GUILESS_MAIN(OpcUaTest)
#include "opcua_test.moc"
