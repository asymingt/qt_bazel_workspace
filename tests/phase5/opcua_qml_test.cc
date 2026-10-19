// Phase 5: the QtOpcUa QML module (`import QtOpcUa`) registers its types and
// sees the statically linked open62541 backend.
#include <QtCore/QObject>
#include <QtGui/QGuiApplication>
#include <QtQml/QQmlComponent>
#include <QtQml/QQmlEngine>
#include <QtTest/QtTest>

#include <memory>

class OpcUaQmlTest : public QObject {
    Q_OBJECT

private slots:
    void importAndInstantiate() {
        QQmlEngine engine;
        QQmlComponent component(&engine);
        component.setData(R"QML(
            import QtQml
            import QtOpcUa as QtOpcUa
            QtObject {
                property QtOpcUa.Connection connection: QtOpcUa.Connection {
                    backend: "open62541"
                }
                property QtOpcUa.NodeId nodeId: QtOpcUa.NodeId {
                    identifier: "s=TheAnswer"
                    ns: "Test Namespace"
                }
                property QtOpcUa.ValueNode valueNode: QtOpcUa.ValueNode {
                    connection: connection
                    nodeId: nodeId
                }
            }
        )QML",
                          QUrl());
        QVERIFY2(!component.isError(), qPrintable(component.errorString()));
        std::unique_ptr<QObject> root(component.create());
        QVERIFY2(root, qPrintable(component.errorString()));

        QObject *connection = root->property("connection").value<QObject *>();
        QVERIFY(connection);
        QCOMPARE(connection->property("backend").toString(), QStringLiteral("open62541"));
        QVERIFY(connection->property("availableBackends")
                        .toStringList()
                        .contains(QStringLiteral("open62541")));
        QCOMPARE(connection->property("connected").toBool(), false);

        QObject *nodeId = root->property("nodeId").value<QObject *>();
        QVERIFY(nodeId);
        QCOMPARE(nodeId->property("identifier").toString(), QStringLiteral("s=TheAnswer"));
        QVERIFY(root->property("valueNode").value<QObject *>());
    }
};

QTEST_MAIN(OpcUaQmlTest)
#include "opcua_qml_test.moc"
