#include "tests/phase4_m0/consumer_lib.h"

#include <QCoreApplication>
#include <cassert>

extern "C" int non_qt_zlib_runfiles_helper();

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    ConsumerObject obj;
    qCInfo(testConsumerLog) << obj.greeting();
    assert(obj.greeting() == QStringLiteral("hello-from-consumer"));
    assert(non_qt_zlib_runfiles_helper() == 42);
    return 0;
}
