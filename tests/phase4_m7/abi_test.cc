// Milestone 4.7 ABI checks (F-ABI-4, F-INF-10).
//
//  * F-ABI-4: Qt's ELF symbols carry the `Qt_6` version node, as upstream's
//    version script does, so binaries linked against official Qt builds and
//    against this build resolve the same versioned symbols.
//  * F-INF-10: `QT_<MODULE>_REMOVED_SINCE` is honoured, so API that was
//    removed from the headers is still exported from the library through
//    compat/removed_api.cpp (binary compatibility with older Qt 6 apps).

#include <QtCore/QCoreApplication>
#include <QtCore/QtGlobal>

#include <cstdio>

#if defined(__linux__)
#include <dlfcn.h>
#endif

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    int failures = 0;

#if defined(__linux__)
    // F-ABI-4: versioned symbol lookup.
    void *versioned = dlvsym(RTLD_DEFAULT, "qVersion", "Qt_6");
    if (!versioned) {
        std::fprintf(stderr, "FAIL: qVersion@@Qt_6 not found (no ELF symbol versioning)\n");
        ++failures;
    } else {
        std::fprintf(stderr, "ok: qVersion@@Qt_6\n");
    }

    // F-INF-10: `int QMetaType::id() const` was removed from the headers in
    // Qt 6.1 (QT_CORE_REMOVED_SINCE(6, 1)) but must remain exported.
    void *removed = dlsym(RTLD_DEFAULT, "_ZNK9QMetaType2idEv");
    if (!removed) {
        std::fprintf(stderr, "FAIL: QMetaType::id() const (removed-since-6.1 API) not exported\n");
        ++failures;
    } else {
        std::fprintf(stderr, "ok: QMetaType::id() const exported\n");
    }

    // QCryptographicHash::hash(const QByteArray &, Algorithm), removed in 6.3.
    void *removed63 = dlsym(RTLD_DEFAULT, "_ZN18QCryptographicHash4hashERK10QByteArrayNS_9AlgorithmE");
    if (!removed63) {
        std::fprintf(stderr, "FAIL: QCryptographicHash::hash(const QByteArray&, Algorithm) not exported\n");
        ++failures;
    } else {
        std::fprintf(stderr, "ok: QCryptographicHash::hash(QByteArray) exported\n");
    }
#endif

    if (failures) {
        std::fprintf(stderr, "%d check(s) failed\n", failures);
        return 1;
    }
    std::fprintf(stderr, "PASS\n");
    return 0;
}
