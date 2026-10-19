// Milestone 4.6 (F-INF-9): a consumer cc_shared_library built with
// `qt_cc_shared_library` links Qt dynamically (no private copy of QtCore), so
// the binary and the library share one QCoreApplication instance.

#include "tests/phase4_m6/greeter.h"

#include <QtCore/QCoreApplication>

#include <cstdio>

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    const QString g = m6Greeting();
    if (!g.startsWith(QStringLiteral("hello from 6."))) {
        std::fprintf(stderr, "FAIL: unexpected greeting '%s'\n", qPrintable(g));
        return 1;
    }
    std::fprintf(stderr, "PASS: %s\n", qPrintable(g));
    return 0;
}
