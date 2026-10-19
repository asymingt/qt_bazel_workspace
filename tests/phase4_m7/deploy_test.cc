// Milestone 4.7 (F-RT-1 / F-RT-2): unpack the `qt_deploy` tarball into a
// fresh directory outside Bazel's runfiles and run the packaged application.

#include <QtCore/QCoreApplication>
#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtCore/QProcess>
#include <QtCore/QProcessEnvironment>
#include <QtCore/QTemporaryDir>

#include <cstdio>

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);

    const QByteArray srcdir = qgetenv("TEST_SRCDIR");
    const QString tarball = QString::fromLocal8Bit(srcdir) +
                            QStringLiteral("/_main/tests/phase4_m7/probe_deploy.tar");
    if (!QFileInfo::exists(tarball)) {
        std::fprintf(stderr, "FAIL: deploy tarball missing: %s\n", qPrintable(tarball));
        return 1;
    }

    QTemporaryDir dir;
    if (!dir.isValid())
        return 1;

    QProcess tar;
    tar.start(QStringLiteral("tar"), {QStringLiteral("xf"), tarball, QStringLiteral("-C"), dir.path()});
    if (!tar.waitForFinished(60000) || tar.exitCode() != 0) {
        std::fprintf(stderr, "FAIL: tar: %s\n", tar.readAllStandardError().constData());
        return 1;
    }

    const QString exe = dir.path() + QStringLiteral("/probe_pkg/bin/deploy_probe");
    for (const char *p : {"/probe_pkg/bin/qt.conf", "/probe_pkg/translations/qtbase_de.qm",
                          "/probe_pkg/plugins", "/probe_pkg/qml"}) {
        if (!QFileInfo::exists(dir.path() + QLatin1String(p))) {
            std::fprintf(stderr, "FAIL: package is missing %s\n", p);
            return 1;
        }
    }

    // Run with a scrubbed environment so nothing points back into runfiles.
    QProcessEnvironment env;
    env.insert(QStringLiteral("PATH"), QStringLiteral("/usr/bin:/bin"));
    env.insert(QStringLiteral("HOME"), dir.path());
    QProcess probe;
    probe.setProcessEnvironment(env);
    probe.setWorkingDirectory(QDir::rootPath());
    probe.start(exe, {});
    if (!probe.waitForFinished(60000)) {
        std::fprintf(stderr, "FAIL: probe did not finish\n");
        return 1;
    }
    std::fprintf(stderr, "%s%s", probe.readAllStandardOutput().constData(),
                 probe.readAllStandardError().constData());
    if (probe.exitStatus() != QProcess::NormalExit || probe.exitCode() != 0) {
        std::fprintf(stderr, "FAIL: deployed probe exited with %d\n", probe.exitCode());
        return 1;
    }
    std::fprintf(stderr, "PASS\n");
    return 0;
}
