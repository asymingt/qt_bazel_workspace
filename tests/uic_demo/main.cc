#include "ui_demo.h"

#include <QApplication>
#include <QFile>
#include <QString>
#include <QWidget>
#include <cassert>

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    QWidget widget;
    Ui::DemoWidget ui;
    ui.setupUi(&widget);

    assert(ui.actionButton != nullptr);
    assert(ui.actionButton->text() == QStringLiteral("Click Me"));

    QFile qrcFile(QStringLiteral(":/demo/greeting.txt"));
    assert(qrcFile.open(QIODevice::ReadOnly | QIODevice::Text));
    QString qrcText = QString::fromUtf8(qrcFile.readAll()).trimmed();
    assert(qrcText == QStringLiteral("Hello from Qt6 resource!"));

    QFile inlineFile(QStringLiteral(":/inline/greeting.txt"));
    assert(inlineFile.open(QIODevice::ReadOnly | QIODevice::Text));
    QString inlineText = QString::fromUtf8(inlineFile.readAll()).trimmed();
    assert(inlineText == QStringLiteral("Hello from Qt6 resource!"));

    ui.greetingLabel->setText(qrcText);
    assert(ui.greetingLabel->text() == QStringLiteral("Hello from Qt6 resource!"));

    return 0;
}
