#include "tests/moc_test/counter.h"

#include <QCoreApplication>
#include <QObject>
#include <cassert>

class LocalReceiver : public QObject {
    Q_OBJECT

public:
    explicit LocalReceiver(QObject *parent = nullptr) : QObject(parent), lastReceived(0) {}
    int lastReceived;

public slots:
    void onValueChanged(int v) { lastReceived = v; }
};

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    Counter a;
    Counter b;
    LocalReceiver r;

    QObject::connect(&a, &Counter::valueChanged, &b, &Counter::setValue);
    QObject::connect(&b, &Counter::valueChanged, &r, &LocalReceiver::onValueChanged);

    a.setValue(42);
    assert(b.value() == 42);
    assert(r.lastReceived == 42);
    return 0;
}

#include "main.moc"
