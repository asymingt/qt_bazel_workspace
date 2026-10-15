#include <QtCore/QObject>
#include <QtGui/QColor>
#include <QtGui/QImage>
#include <QtQml/QJSEngine>
#include <QtQml/QQmlComponent>
#include <QtQml/QQmlEngine>
#include <QtQuick/QQuickItem>
#include <QtQuick/QQuickRenderControl>
#include <QtQuick/QQuickWindow>
#include <QtQuickControls2/QQuickStyle>
#include <QtTest/QTest>

class DeclarativeQuickTest : public QObject {
    Q_OBJECT

private slots:
    void testJsEngine() {
        QJSEngine engine;
        QJSValue fn = engine.evaluate(QStringLiteral("(function(a, b) { return a * b + 7; })"));
        QVERIFY(fn.isCallable());
        QJSValue result = fn.call({QJSValue(5), QJSValue(7)});
        QCOMPARE(result.toInt(), 42);

        // Test RegExp (exercising YARR JIT / interpreter)
        QJSValue regexMatch = engine.evaluate(QStringLiteral("('hello_qt6_10_2'.match(/qt(\\d+)_(\\d+)/))[1]"));
        QCOMPARE(regexMatch.toString(), QStringLiteral("6"));
    }

    void testQmlBindingsAndSignals() {
        QQmlEngine engine;
        QQmlComponent component(&engine);
        component.setData(
            "import QtQml\n"
            "QtObject {\n"
            "    property int width: 12\n"
            "    property int height: 4\n"
            "    property int area: width * height\n"
            "    property int signalCount: 0\n"
            "    onAreaChanged: signalCount += 1\n"
            "}\n",
            QUrl());
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));

        std::unique_ptr<QObject> obj(component.create());
        QVERIFY(obj != nullptr);
        QCOMPARE(obj->property("area").toInt(), 48);
        QCOMPARE(obj->property("signalCount").toInt(), 1);

        obj->setProperty("width", 20);
        QCOMPARE(obj->property("area").toInt(), 80);
        QCOMPARE(obj->property("signalCount").toInt(), 2);
    }

    void testQuickItemsAndLayouts() {
        QQmlEngine engine;
        QQmlComponent component(&engine);
        component.setData(
            "import QtQuick\n"
            "import QtQuick.Layouts\n"
            "Item {\n"
            "    width: 200\n"
            "    height: 100\n"
            "    RowLayout {\n"
            "        objectName: \"row\"\n"
            "        anchors.fill: parent\n"
            "        spacing: 10\n"
            "        Rectangle {\n"
            "            objectName: \"rect1\"\n"
            "            color: \"#ff0000\"\n"
            "            Layout.fillWidth: true\n"
            "            Layout.fillHeight: true\n"
            "        }\n"
            "        Rectangle {\n"
            "            objectName: \"rect2\"\n"
            "            color: \"#00ff00\"\n"
            "            Layout.preferredWidth: 50\n"
            "            Layout.fillHeight: true\n"
            "        }\n"
            "    }\n"
            "}\n",
            QUrl());
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));

        std::unique_ptr<QObject> rootObj(component.create());
        auto *rootItem = qobject_cast<QQuickItem *>(rootObj.get());
        QVERIFY(rootItem != nullptr);
        QCOMPARE(rootItem->width(), 200.0);
        QCOMPARE(rootItem->height(), 100.0);

        auto *rect2 = rootItem->findChild<QQuickItem *>(QStringLiteral("rect2"));
        QVERIFY(rect2 != nullptr);
        QCOMPARE(rect2->property("color").value<QColor>(), QColor(0, 255, 0));
    }

    void testQuickControlsBasic() {
        QQuickStyle::setStyle(QStringLiteral("Basic"));
        QQmlEngine engine;
        QQmlComponent component(&engine);
        component.setData(
            "import QtQuick\n"
            "import QtQuick.Controls.Basic\n"
            "Button {\n"
            "    text: \"Click Me\"\n"
            "    checkable: true\n"
            "    checked: true\n"
            "}\n",
            QUrl());
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));

        std::unique_ptr<QObject> button(component.create());
        QVERIFY(button != nullptr);
        QCOMPARE(button->property("text").toString(), QStringLiteral("Click Me"));
        QCOMPARE(button->property("checked").toBool(), true);
    }
};

QTEST_MAIN(DeclarativeQuickTest)
#include "main.moc"
