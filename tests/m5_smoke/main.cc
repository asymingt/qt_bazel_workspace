#include <QApplication>
#include <QAudioEngine>
#include <QAudioFormat>
#include <QMediaFormat>
#include <QMediaPlayer>
#include <QQmlEngine>
#include <QSpatialSound>
#include <QTextToSpeech>
#include <QVideoFrameFormat>
#include <QVideoWidget>
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

int main(int argc, char* argv[]) {
#if defined(__x86_64__) || defined(_M_X64)
  if (!hostSupportsAvx2()) {
    std::printf("SKIPPED: Host CPU lacks AVX2 instruction support\n");
    return 0;
  }
#endif

  QApplication app(argc, argv);

  // 1. QtMultimedia core & FFmpeg plugin
  QAudioFormat audioFormat;
  audioFormat.setSampleRate(44100);
  audioFormat.setChannelCount(2);
  audioFormat.setSampleFormat(QAudioFormat::Int16);
  assert(audioFormat.isValid());
  assert(audioFormat.bytesPerFrame() == 4);

  QVideoFrameFormat videoFormat(QSize(640, 480),
                                QVideoFrameFormat::Format_RGBA8888);
  assert(videoFormat.isValid());

  QMediaFormat mediaFormat(QMediaFormat::MPEG4);
  mediaFormat.setVideoCodec(QMediaFormat::VideoCodec::H264);
  assert(mediaFormat.fileFormat() == QMediaFormat::MPEG4);

  QMediaPlayer player;
  assert(player.playbackState() == QMediaPlayer::StoppedState);

  // 2. QtMultimediaWidgets
  QVideoWidget videoWidget;
  assert(videoWidget.videoSink() != nullptr);

  // 3. QtSpatialAudio (ResonanceAudio + Eigen + PFFFT)
  QAudioEngine spatialEngine;
  QSpatialSound spatialSound(&spatialEngine);
  spatialSound.setSize(2.0f);
  assert(spatialSound.size() == 2.0f);

  // 4. QtTextToSpeech (with mock engine plugin)
  QStringList engines = QTextToSpeech::availableEngines();
  assert(engines.contains(QStringLiteral("mock")));
  QTextToSpeech tts(QStringLiteral("mock"));
  assert(tts.engine() == QStringLiteral("mock"));
  assert(tts.state() == QTextToSpeech::Ready);

  // 5. QML engine check
  QQmlEngine qmlEngine;

  return 0;
}
