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

#if defined(__x86_64__) || defined(_M_X64)
#if defined(_MSC_VER)
#include <intrin.h>
#else
#include <cpuid.h>
#endif

static bool hostSupportsAvx() {
#if defined(_MSC_VER)
    int info[4];
    __cpuid(info, 1);
    if ((info[2] & (1 << 27)) && (info[2] & (1 << 28))) {
        unsigned long long xcr = _xgetbv(0);
        return (xcr & 6) == 6;
    }
    return false;
#else
    unsigned int eax, ebx, ecx, edx;
    if (__get_cpuid(1, &eax, &ebx, &ecx, &edx)) {
        if ((ecx & (1 << 27)) && (ecx & (1 << 28))) {
            unsigned int xcr0_lo, xcr0_hi;
            __asm__ volatile("xgetbv" : "=a"(xcr0_lo), "=d"(xcr0_hi) : "c"(0));
            return (xcr0_lo & 6) == 6;
        }
    }
    return false;
#endif
}

static bool hostSupportsAvx2() {
    if (!hostSupportsAvx()) {
        return false;
    }
#if defined(_MSC_VER)
    int info[4];
    __cpuidex(info, 7, 0);
    return (info[1] & (1 << 5)) != 0;
#else
    unsigned int eax, ebx, ecx, edx;
    if (__get_cpuid_count(7, 0, &eax, &ebx, &ecx, &edx)) {
        return (ebx & (1 << 5)) != 0;
    }
    return false;
#endif
}
#endif

int main(int argc, char** argv) {
#if defined(__x86_64__) || defined(_M_X64)
    if (!hostSupportsAvx2()) {
        std::printf("SKIPPED: Host CPU lacks AVX2 instruction support\n");
        return 0;
    }
#endif

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
