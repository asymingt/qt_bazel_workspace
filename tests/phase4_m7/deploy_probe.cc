// Runs from inside a `qt_deploy` package (see deploy_test.cc). Verifies that
// qt.conf makes QLibraryInfo resolve the package's prefix layout and that the
// deployed .qm catalogs load from TranslationsPath (F-RT-1, F-TOOL-4).

#include <QtCore/QCoreApplication>
#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtCore/QLibraryInfo>
#include <QtCore/QTranslator>

#include <cstdio>

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    const QString appDir = QCoreApplication::applicationDirPath();
    const QString expected = QDir::cleanPath(appDir + QStringLiteral("/../translations"));
    const QString actual = QDir::cleanPath(QLibraryInfo::path(QLibraryInfo::TranslationsPath));
    const QString prefix = QDir::cleanPath(QLibraryInfo::path(QLibraryInfo::PrefixPath));

    std::printf("appDir=%s\nprefix=%s\ntranslations=%s\n",
                qPrintable(appDir), qPrintable(prefix), qPrintable(actual));

    int failures = 0;
    if (QFileInfo(actual).canonicalFilePath() != QFileInfo(expected).canonicalFilePath()) {
        std::fprintf(stderr, "FAIL: TranslationsPath %s != %s\n", qPrintable(actual), qPrintable(expected));
        ++failures;
    }
    QTranslator tr;
    if (!tr.load(QStringLiteral("qtbase_de"), actual)) {
        std::fprintf(stderr, "FAIL: could not load qtbase_de.qm from %s\n", qPrintable(actual));
        ++failures;
    }
    return failures ? 1 : 0;
}
