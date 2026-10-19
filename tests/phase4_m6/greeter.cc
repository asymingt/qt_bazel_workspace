#include "tests/phase4_m6/greeter.h"

#include <QtCore/QCoreApplication>

QString m6Greeting() {
    return QStringLiteral("hello from %1").arg(QLatin1StringView(qVersion()));
}
