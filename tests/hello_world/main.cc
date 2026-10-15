#include <QApplication>
#include <QPushButton>
#include <QString>
#include <QTimer>

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    QPushButton button(QStringLiteral("Hello, Modular Qt 6.10.2!"));
    button.resize(240, 80);
    button.show();
#ifdef HELLO_WORLD_AUTOQUIT
    // Set by hello_world_test so the test terminates; the interactive binary
    // stays open until the window is closed.
    QTimer::singleShot(10, &app, &QCoreApplication::quit);
#endif
    return app.exec();
}
