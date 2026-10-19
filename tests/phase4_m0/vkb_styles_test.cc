#include <QCoreApplication>
#include <QFile>
#include <QPluginLoader>
#include <cassert>
#include <cstdio>

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);

    // F-VKB-2: Both QtQuickVirtualKeyboardStylesBuiltinPlugin and
    // QtQuickVirtualKeyboardStylesPlugin must be registered as static plugins
    // without symbol collisions, and the builtin style QRC resources must be
    // initialized under their upstream names (`virtualkeyboard_default_style`
    // and `virtualkeyboard_retro_style`).
    const QObjectList plugins = QPluginLoader::staticInstances();
    bool foundBuiltinStyles = false;
    bool foundStyles = false;
    for (QObject *p : plugins) {
        const QByteArray className = p->metaObject()->className();
        if (className == "QtQuickVirtualKeyboardStylesBuiltinPlugin") {
            foundBuiltinStyles = true;
        } else if (className == "QtQuickVirtualKeyboardStylesPlugin") {
            foundStyles = true;
        }
    }
    assert(foundBuiltinStyles);
    assert(foundStyles);

    assert(QFile::exists(QStringLiteral(
        ":/qt-project.org/imports/QtQuick/VirtualKeyboard/Styles/Builtin/default/style.qml")));
    assert(QFile::exists(QStringLiteral(
        ":/qt-project.org/imports/QtQuick/VirtualKeyboard/Styles/Builtin/retro/style.qml")));
    return 0;
}
