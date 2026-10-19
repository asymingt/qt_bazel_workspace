#include <QtCore/QPluginLoader>
#include <QtCore/QProcess>
#include <QtCore/QSet>
#include <QtTest/QtTest>

#include "rules_cc/cc/runfiles/runfiles.h"

using rules_cc::cc::runfiles::Runfiles;

class QmlToolsAndDebugTest : public QObject {
    Q_OBJECT
private slots:
    void qmlDebugAndLottiePluginsRegistered() {
        QSet<QString> classes;
        for (const QStaticPlugin &plugin : QPluginLoader::staticPlugins()) {
            classes.insert(plugin.metaData().value(QStringLiteral("className")).toString());
        }
        const QStringList expected = {
            QStringLiteral("QQmlDebuggerServiceFactory"),
            QStringLiteral("QQmlInspectorServiceFactory"),
            QStringLiteral("QLocalClientConnectionFactory"),
            QStringLiteral("QDebugMessageServiceFactory"),
            QStringLiteral("QQmlNativeDebugConnectorFactory"),
            QStringLiteral("QQmlNativeDebugServiceFactory"),
            QStringLiteral("QQmlPreviewServiceFactory"),
            QStringLiteral("QQmlProfilerServiceFactory"),
            QStringLiteral("QQuickProfilerAdapterFactory"),
            QStringLiteral("QQmlDebugServerFactory"),
            QStringLiteral("QTcpServerConnectionFactory"),
            QStringLiteral("QLottieVectorImagePlugin"),
        };
        for (const QString &cls : expected) {
            QVERIFY2(classes.contains(cls), qPrintable("Missing static plugin class: " + cls));
        }
    }

    void developerToolsExecute() {
        std::string error;
        std::unique_ptr<Runfiles> runfiles(Runfiles::CreateForTest(&error));
        QVERIFY2(runfiles != nullptr, error.c_str());

        const QList<QPair<QString, QStringList>> tools = {
            {QStringLiteral("qtdeclarative+/qmllint"), {QStringLiteral("--version")}},
            {QStringLiteral("qtdeclarative+/qmlformat"), {QStringLiteral("--version")}},
            {QStringLiteral("qtdeclarative+/qmlimportscanner"), {QStringLiteral("-help")}},
            {QStringLiteral("qtdeclarative+/qmlls"), {QStringLiteral("--version")}},
            {QStringLiteral("qtdeclarative+/qml_tool"), {QStringLiteral("--version")}},
            {QStringLiteral("qtdeclarative+/svgtoqml"), {QStringLiteral("--version")}},
            {QStringLiteral("qtlottie+/lottietoqml"), {QStringLiteral("--version")}},
        };

        for (const auto &entry : tools) {
            std::string path = runfiles->Rlocation(entry.first.toStdString());
            QVERIFY2(!path.empty() && QFileInfo::exists(QString::fromStdString(path)),
                     qPrintable("Missing tool binary: " + entry.first));
            QProcess proc;
            proc.start(QString::fromStdString(path), entry.second);
            QVERIFY2(proc.waitForFinished(10000), qPrintable("Tool timed out: " + entry.first));
            QCOMPARE(proc.exitStatus(), QProcess::NormalExit);
        }
    }
};

QTEST_MAIN(QmlToolsAndDebugTest)
#include "qml_tools_and_debug_test.moc"
