// Milestone 4.7 (F-RT-1 / F-RT-2): unpack the `qt_deploy` tarball into a
// fresh directory outside Bazel's runfiles and run the packaged application.

#include <QtCore/QCoreApplication>
#include <QtCore/QDir>
#include <QtCore/QDirIterator>
#include <QtCore/QFileInfo>
#include <QtCore/QProcess>
#include <QtCore/QProcessEnvironment>
#include <QtCore/QSet>
#include <QtCore/QTemporaryDir>

#include <cstdio>
#include <memory>
#include <string>

#include "rules_cc/cc/runfiles/runfiles.h"

using rules_cc::cc::runfiles::Runfiles;

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);

    // Resolve through the runfiles library: Windows has no runfiles symlink
    // tree, only a manifest, so TEST_SRCDIR-relative paths do not exist there.
    std::string error;
    std::unique_ptr<Runfiles> runfiles(Runfiles::CreateForTest(&error));
    if (!runfiles) {
        std::fprintf(stderr, "FAIL: runfiles: %s\n", error.c_str());
        return 1;
    }
    const QString tarball = QString::fromStdString(
            runfiles->Rlocation("_main/tests/phase4_m7/probe_deploy.tar"));
    if (tarball.isEmpty() || !QFileInfo::exists(tarball)) {
        std::fprintf(stderr, "FAIL: deploy tarball missing: %s\n", qPrintable(tarball));
        return 1;
    }

    QTemporaryDir dir;
    if (!dir.isValid())
        return 1;

    QProcess tar;
#ifdef Q_OS_WIN
    // Use the system bsdtar: an MSYS GNU tar earlier on PATH would parse the
    // drive letter in "C:\..." as a remote host.
    const QString tarExe = qEnvironmentVariable("SystemRoot", QStringLiteral("C:\\Windows")) +
                           QStringLiteral("\\System32\\tar.exe");
#else
    const QString tarExe = QStringLiteral("tar");
#endif
    tar.start(tarExe, {QStringLiteral("xf"), QDir::toNativeSeparators(tarball),
                       QStringLiteral("-C"), QDir::toNativeSeparators(dir.path())});
    if (!tar.waitForFinished(60000) || tar.exitCode() != 0) {
        std::fprintf(stderr, "FAIL: tar: %s\n", tar.readAllStandardError().constData());
        return 1;
    }

#ifdef Q_OS_WIN
    const QString exe = dir.path() + QStringLiteral("/probe_pkg/bin/deploy_probe.exe");
#else
    const QString exe = dir.path() + QStringLiteral("/probe_pkg/bin/deploy_probe");
#endif
    for (const char *p : {"/probe_pkg/bin/qt.conf", "/probe_pkg/translations/qtbase_de.qm",
                          "/probe_pkg/plugins", "/probe_pkg/qml"}) {
        if (!QFileInfo::exists(dir.path() + QLatin1String(p))) {
            std::fprintf(stderr, "FAIL: package is missing %s\n", p);
            return 1;
        }
    }

    // Run with a scrubbed environment so nothing points back into runfiles.
    QProcessEnvironment env;
#ifdef Q_OS_WIN
    // Windows has no RUNPATH: the loader only searches the exe's directory,
    // the system directories and PATH. Point PATH at the DLL directories
    // *inside the unpacked package* (never at Bazel's runfiles) plus System32.
    QSet<QString> dllDirs;
    QDirIterator it(dir.path() + QStringLiteral("/probe_pkg"), {QStringLiteral("*.dll")},
                    QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext())
        dllDirs.insert(QDir::toNativeSeparators(QFileInfo(it.next()).absolutePath()));
    QStringList path(dllDirs.cbegin(), dllDirs.cend());
    const QString systemRoot = qEnvironmentVariable("SystemRoot", QStringLiteral("C:\\Windows"));
    env.insert(QStringLiteral("SystemRoot"), systemRoot);
    path << systemRoot + QStringLiteral("\\System32") << systemRoot;
    env.insert(QStringLiteral("PATH"), path.join(QLatin1Char(';')));
#else
    env.insert(QStringLiteral("PATH"), QStringLiteral("/usr/bin:/bin"));
#endif
    env.insert(QStringLiteral("HOME"), dir.path());
    QProcess probe;
    probe.setProcessEnvironment(env);
    probe.setWorkingDirectory(QDir::rootPath());
    probe.start(exe, {});
    if (!probe.waitForFinished(60000)) {
        std::fprintf(stderr, "FAIL: probe did not finish: %s\n", qPrintable(probe.errorString()));
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
