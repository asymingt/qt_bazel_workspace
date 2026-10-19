#include <QCoreApplication>
#include <QGuiApplication>
#include <QMediaDevices>
#include <QAudioDevice>
#include <QCameraDevice>
#include <QTextToSpeech>
#include <QtMultimedia/qtmultimedia-config.h>
#include <QtTextToSpeech/qttexttospeech-config.h>
#include <cassert>
#include <cstdio>

int main(int argc, char** argv) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QGuiApplication app(argc, argv);

#if defined(__linux__)
    // Verify Linux PulseAudio and V4L2 features are enabled in QtMultimedia
    static_assert(QT_CONFIG(pulseaudio), "QT_FEATURE_pulseaudio must be enabled on Linux");
    static_assert(QT_CONFIG(speechd), "QT_FEATURE_speechd must be enabled on Linux");
#elif defined(__APPLE__)
    static_assert(QT_CONFIG(avfoundation), "QT_FEATURE_avfoundation must be enabled on macOS");
#elif defined(_WIN32)
    static_assert(QT_CONFIG(native_windows_backend), "QT_FEATURE_native_windows_backend must be enabled on Windows");
#endif

    // Query audio & video devices (exercises QPulseAudioDevices / QAVFVideoDevices / V4L2 camera devices without crashing)
    const auto audioInputs = QMediaDevices::audioInputs();
    const auto audioOutputs = QMediaDevices::audioOutputs();
    const auto videoInputs = QMediaDevices::videoInputs();
    std::printf("Audio inputs: %lld, Audio outputs: %lld, Video inputs: %lld\n",
                static_cast<long long>(audioInputs.size()),
                static_cast<long long>(audioOutputs.size()),
                static_cast<long long>(videoInputs.size()));

    // Check TTS engines
    const QStringList ttsEngines = QTextToSpeech::availableEngines();
    std::printf("TTS engines: %s\n", qPrintable(ttsEngines.join(", ")));
    assert(ttsEngines.contains("mock"));

    return 0;
}
