#include <QtCore/QObject>
#include <QtGui/QGuiApplication>
#include <QtQml/QQmlComponent>
#include <QtQml/QQmlEngine>
#include <QtQuickControls2/QQuickStyle>
#include <QtTest/QtTest>

class QmlControlsDefaultStyleTest : public QObject {
    Q_OBJECT
private slots:
    void defaultStyleLoadsApplicationWindow() {
        qunsetenv("QT_QUICK_CONTROLS_STYLE");
        const QString styleName = QQuickStyle::name();
#if defined(Q_OS_LINUX)
        QCOMPARE(styleName, QStringLiteral("Fusion"));
#elif defined(Q_OS_MACOS)
        QCOMPARE(styleName, QStringLiteral("macOS"));
#elif defined(Q_OS_WIN)
        QCOMPARE(styleName, QStringLiteral("Windows"));
#endif

        QQmlEngine engine;
        QQmlComponent component(&engine);
        component.setData(R"QML(
            import QtQuick
            import QtQuick.Controls
            ApplicationWindow {
                width: 200
                height: 200
                Button { text: "Click" }
                CheckBox { text: "Check" }
                Slider { value: 0.5 }
                TextField { placeholderText: "Type" }
            }
        )QML", QUrl());
        QVERIFY2(!component.isError(), qPrintable(component.errorString()));
        std::unique_ptr<QObject> obj(component.create());
        QVERIFY2(obj != nullptr, qPrintable(component.errorString()));
    }

    void explicitStylesLoad() {
        const QStringList styles = {
            QStringLiteral("Basic"),
            QStringLiteral("Fusion"),
            QStringLiteral("Material"),
            QStringLiteral("Universal"),
            QStringLiteral("Imagine"),
            QStringLiteral("FluentWinUI3"),
        };
        for (const QString &style : styles) {
            QQmlEngine engine;
            QQmlComponent component(&engine);
            const QByteArray qml = QStringLiteral(R"QML(
                import QtQuick
                import QtQuick.Controls.%1
                Button { text: "%1" }
            )QML").arg(style).toUtf8();
            component.setData(qml, QUrl());
            QVERIFY2(!component.isError(), qPrintable(style + ": " + component.errorString()));
            std::unique_ptr<QObject> obj(component.create());
            QVERIFY2(obj != nullptr, qPrintable(style + ": " + component.errorString()));
        }
    }
};

QTEST_MAIN(QmlControlsDefaultStyleTest)
#include "qml_controls_default_style_test.moc"
