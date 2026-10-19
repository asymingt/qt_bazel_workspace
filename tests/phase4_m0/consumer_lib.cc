#include "tests/phase4_m0/consumer_lib.h"

Q_LOGGING_CATEGORY(testConsumerLog, "test.consumer")

ConsumerObject::ConsumerObject(QObject *parent) : QObject(parent) {
    qCDebug(testConsumerLog) << "ConsumerObject constructed";
}

QString ConsumerObject::greeting() const {
    return QStringLiteral("hello-from-consumer");
}
