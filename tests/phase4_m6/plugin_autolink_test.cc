// Milestone 4.6 (F-INF-4, F-DEP-4): a binary that only depends on the Qt
// *library* targets must automatically get the plugins those libraries imply
// (image formats, SQL drivers, TLS backends, multimedia/TTS/positioning/
// sensors/CAN backends), and FFmpeg must be linked as a separate shared
// library (LGPL compliance) rather than statically into Qt.

#include <QCanBus>
#include <QGeoPositionInfoSource>
#include <QGuiApplication>
#include <QImageReader>
#include <QPluginLoader>
#include <QSensor>
#include <QSqlDatabase>
#include <QSslSocket>
#include <QStaticPlugin>
#include <QTextToSpeech>

#include <cstdio>
#include <cstring>
#include <set>
#include <string>

#if defined(__linux__)
#include <link.h>
#endif

static int g_failures = 0;

#define CHECK(cond, ...)                                  \
    do {                                                  \
        if (!(cond)) {                                    \
            std::fprintf(stderr, "FAIL: " __VA_ARGS__);   \
            std::fprintf(stderr, "\n");                   \
            ++g_failures;                                 \
        } else {                                          \
            std::fprintf(stderr, "ok: %s\n", #cond);      \
        }                                                 \
    } while (0)

#if defined(__linux__)
static int find_ffmpeg_cb(struct dl_phdr_info *info, size_t, void *data) {
    if (info->dlpi_name && std::strstr(info->dlpi_name, "ffmpeg_shared")) {
        *static_cast<bool *>(data) = true;
    }
    return 0;
}
#endif

int main(int argc, char **argv) {
    QGuiApplication app(argc, argv);

    std::set<std::string> classNames;
    for (const QStaticPlugin &p : QPluginLoader::staticPlugins()) {
        classNames.insert(p.metaData().value(QStringLiteral("className")).toString().toStdString());
    }

    // Image formats implied by QtGui (+ QtSvg).
    const auto fmts = QImageReader::supportedImageFormats();
    CHECK(fmts.contains("jpeg"), "jpeg image format not auto-linked");
    CHECK(fmts.contains("gif"), "gif image format not auto-linked");
    CHECK(fmts.contains("svg"), "svg image format not auto-linked by @qtsvg//:svg");

    // SQL driver implied by QtSql.
    CHECK(QSqlDatabase::drivers().contains(QStringLiteral("QSQLITE")),
          "QSQLITE driver not auto-linked by @qtbase//:sql");

    // TLS backend implied by QtNetwork.
    CHECK(!QSslSocket::availableBackends().isEmpty(),
          "no TLS backend auto-linked by @qtbase//:network");

    // Multimedia backend implied by QtMultimedia.
    CHECK(classNames.count("QFFmpegMediaPlugin") == 1,
          "QFFmpegMediaPlugin not auto-linked by @qtmultimedia//:multimedia");

    // TTS / positioning / sensors / CAN backends.
    CHECK(!QTextToSpeech::availableEngines().isEmpty(),
          "no TTS engine auto-linked by @qtspeech//:tts");
    CHECK(!QGeoPositionInfoSource::availableSources().isEmpty(),
          "no position plugin auto-linked by @qtpositioning//:positioning");
    CHECK(!QCanBus::instance()->plugins().isEmpty(),
          "no CAN bus plugin auto-linked by @qtserialbus//:serialbus");
    CHECK(!QSensor::sensorTypes().isEmpty(),
          "no sensor backend auto-linked by @qtsensors//:sensors");

#if defined(__linux__)
    // F-DEP-4: FFmpeg must come from its own shared object.
    bool ffmpegShared = false;
    dl_iterate_phdr(find_ffmpeg_cb, &ffmpegShared);
    CHECK(ffmpegShared, "FFmpeg not loaded from libffmpeg_shared.so (statically linked?)");
#endif

    if (g_failures) {
        std::fprintf(stderr, "%d check(s) failed\n", g_failures);
        return 1;
    }
    std::fprintf(stderr, "PASS\n");
    return 0;
}
