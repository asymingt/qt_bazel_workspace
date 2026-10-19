#include <QGuiApplication>
#include <QQmlEngine>
#include <QQmlComponent>
#include <QHelpEngineCore>
#include <QTemporaryDir>
#include <QJsonObject>
#include <QPluginLoader>
#include <cassert>
#include <cstdio>

int main(int argc, char** argv) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QGuiApplication app(argc, argv);

    // 1. Test QtQuick3D.Xr QML import (F-3D-2)
    {
        QQmlEngine engine;
        QQmlComponent comp(&engine);
        comp.setData("import QtQuick\nimport QtQuick3D\nimport QtQuick3D.Xr\nXrOrigin {}\n", QUrl());
        if (comp.isError()) {
            std::fprintf(stderr, "QtQuick3D.Xr error: %s\n", qPrintable(comp.errorString()));
            return 1;
        }
        QObject* obj = comp.create();
        assert(obj != nullptr);
        delete obj;
    }

    // 2. Test RuntimeLoader has assimp importer plugin available (F-3D-3)
    {
        QQmlEngine engine;
        QQmlComponent comp(&engine);
        comp.setData("import QtQuick\nimport QtQuick3D\nimport QtQuick3D.AssetUtils\nRuntimeLoader {}\n", QUrl());
        if (comp.isError()) {
            std::fprintf(stderr, "QtQuick3D.AssetUtils error: %s\n", qPrintable(comp.errorString()));
            return 1;
        }
        QObject* obj = comp.create();
        assert(obj != nullptr);
        delete obj;

        bool foundAssimp = false;
        for (const QStaticPlugin& sp : QPluginLoader::staticPlugins()) {
            const QJsonObject md = sp.metaData();
            const QString iid = md.value("IID").toString();
            const QString cls = md.value("className").toString();
            if (iid.startsWith("org.qt-project.QtQuick3D.AssetImporter") ||
                cls == QLatin1String("AssimpImporterPlugin")) {
                foundAssimp = true;
                break;
            }
        }
        assert(foundAssimp && "AssimpImporterPlugin must be statically linked with :assetutils");
    }

    // 3. Test QHelpEngineCore works out-of-the-box with :help pulling in :qsqlite (F-TOOL-2)
    {
        QTemporaryDir tmpDir;
        assert(tmpDir.isValid());
        const QString colFile = tmpDir.filePath("test.qhc");
        QHelpEngineCore helpEngine(colFile);
        const bool ok = helpEngine.setupData();
        if (!ok) {
            std::fprintf(stderr, "QHelpEngineCore::setupData failed: %s\n", qPrintable(helpEngine.error()));
            return 1;
        }
    }

    return 0;
}
