#include <QtCore/QObject>
#include <QtQml/QQmlComponent>
#include <QtQml/QQmlEngine>
#include <QtTest/QtTest>

class QmlImportsTest : public QObject {
    Q_OBJECT
private:
    void verifyQmlSnippet(const char *label, const QByteArray &qml) {
        QQmlEngine engine;
        QQmlComponent component(&engine);
        component.setData(qml, QUrl());
        QVERIFY2(!component.isError(),
                 qPrintable(QString::fromLatin1(label) + ": " + component.errorString()));
        std::unique_ptr<QObject> obj(component.create());
        QVERIFY2(obj != nullptr,
                 qPrintable(QString::fromLatin1(label) + ": " + component.errorString()));
    }

private slots:
    void importQtCore() {
        verifyQmlSnippet("QtCore", R"QML(
            import QtCore
            import QtQuick
            Item {
                Settings {
                    property int testVal: 123
                }
            }
        )QML");
    }

    void importQtQuickDialogs() {
        verifyQmlSnippet("QtQuick.Dialogs", R"QML(
            import QtQuick
            import QtQuick.Dialogs
            Item {
                FileDialog { id: fd }
                ColorDialog { id: cd }
                FontDialog { id: ftd }
                MessageDialog { id: md }
            }
        )QML");
    }

    void importQtLabsPlatform() {
        verifyQmlSnippet("Qt.labs.platform", R"QML(
            import QtQuick
            import Qt.labs.platform
            Item {
                Menu {
                    MenuItem { text: "Open" }
                }
            }
        )QML");
    }

    void importQtQuickEffects() {
        verifyQmlSnippet("QtQuick.Effects", R"QML(
            import QtQuick
            import QtQuick.Effects
            Item {
                Rectangle { id: src; width: 50; height: 50; color: "red" }
                MultiEffect {
                    source: src
                    anchors.fill: src
                    blurEnabled: true
                }
            }
        )QML");
    }

    void importQtQuickParticles() {
        verifyQmlSnippet("QtQuick.Particles", R"QML(
            import QtQuick
            import QtQuick.Particles
            ParticleSystem {
                Emitter {
                    width: 10; height: 10
                }
            }
        )QML");
    }

    void importQtLabsQmlModels() {
        verifyQmlSnippet("Qt.labs.qmlmodels", R"QML(
            import QtQuick
            import Qt.labs.qmlmodels
            Item {
                TableModel {
                    TableModelColumn { display: "name" }
                }
            }
        )QML");
    }

    void importQtLabsSettings() {
        verifyQmlSnippet("Qt.labs.settings", R"QML(
            import QtQuick
            import Qt.labs.settings
            Item {
                Settings {
                    property string user: "alice"
                }
            }
        )QML");
    }

    void importQtLabsFolderListModel() {
        verifyQmlSnippet("Qt.labs.folderlistmodel", R"QML(
            import QtQuick
            import Qt.labs.folderlistmodel
            Item {
                FolderListModel {
                    showDirs: true
                }
            }
        )QML");
    }

    void importQtQuickLocalStorage() {
        verifyQmlSnippet("QtQuick.LocalStorage", R"QML(
            import QtQuick
            import QtQuick.LocalStorage
            Item {
                property bool opened: false
                Component.onCompleted: {
                    var db = LocalStorage.openDatabaseSync("Phase4M3TestDB", "1.0", "test", 1024);
                    opened = (db !== null);
                }
            }
        )QML");
    }

    void importQtQmlXmlListModel() {
        verifyQmlSnippet("QtQml.XmlListModel", R"QML(
            import QtQuick
            import QtQml.XmlListModel
            Item {
                XmlListModel {
                    XmlListModelRole { name: "title"; elementName: "title" }
                }
            }
        )QML");
    }

    void importQtQuickVectorImage() {
        verifyQmlSnippet("QtQuick.VectorImage", R"QML(
            import QtQuick
            import QtQuick.VectorImage
            VectorImage {
                width: 64
                height: 64
            }
        )QML");
    }

    void importQtQuickShapesDesignHelpers() {
        verifyQmlSnippet("QtQuick.Shapes.DesignHelpers", R"QML(
            import QtQuick
            import QtQuick.Shapes.DesignHelpers
            RectangleShape {
                width: 64
                height: 64
            }
        )QML");
    }
};

QTEST_MAIN(QmlImportsTest)
#include "qml_imports_test.moc"
