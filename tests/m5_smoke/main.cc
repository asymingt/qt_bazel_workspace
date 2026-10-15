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

int main(int argc, char* argv[]) {
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
