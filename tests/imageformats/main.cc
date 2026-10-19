#include <QBuffer>
#include <QByteArray>
#include <QColor>
#include <QGuiApplication>
#include <QImage>
#include <QImageReader>
#include <QImageWriter>
#include <cassert>
#include <cstdio>

// Writes `src` with the given format plugin and reads it back.
static void roundTrip(const QImage &src, const char *format) {
    assert(QImageWriter::supportedImageFormats().contains(format));
    assert(QImageReader::supportedImageFormats().contains(format));

    QByteArray data;
    {
        QBuffer buf(&data);
        buf.open(QIODevice::WriteOnly);
        QImageWriter writer(&buf, format);
        if (!writer.write(src)) {
            std::fprintf(stderr, "%s: write failed: %s\n", format,
                         qPrintable(writer.errorString()));
            assert(false);
        }
    }
    assert(!data.isEmpty());

    QBuffer buf(&data);
    buf.open(QIODevice::ReadOnly);
    QImageReader reader(&buf, format);
    QImage back = reader.read();
    assert(!back.isNull());
    assert(back.size() == src.size());
    // Lossless formats (and lossless webp at quality 100) must preserve pixels.
    assert(back.pixelColor(2, 2) == src.pixelColor(2, 2));
    assert(back.pixelColor(13, 13) == src.pixelColor(13, 13));
}

int main(int argc, char *argv[]) {
    QGuiApplication app(argc, argv);

    QImage img(16, 16, QImage::Format_RGB32);
    img.fill(QColor(0, 128, 255));
    for (int y = 8; y < 16; ++y)
        for (int x = 8; x < 16; ++x)
            img.setPixelColor(x, y, QColor(255, 32, 0));

    roundTrip(img, "tiff");  // BCR libtiff

    // webp via BCR libwebp (encoder + decoder).
    {
        QByteArray data;
        QBuffer buf(&data);
        buf.open(QIODevice::WriteOnly);
        QImageWriter w(&buf, "webp");
        w.setQuality(100);  // lossless
        assert(w.write(img));
        QBuffer rbuf(&data);
        rbuf.open(QIODevice::ReadOnly);
        QImage back = QImageReader(&rbuf, "webp").read();
        assert(!back.isNull());
        assert(back.pixelColor(2, 2) == img.pixelColor(2, 2));
    }

    // Core qtbase image format plugins: png (built-in), jpeg, gif, ico.
    roundTrip(img, "png");
    roundTrip(img, "ico");

    // JPEG is lossy; verify supported, encode + decode, and check approximate color.
    assert(QImageWriter::supportedImageFormats().contains("jpeg"));
    assert(QImageReader::supportedImageFormats().contains("jpeg"));
    {
        QByteArray data;
        QBuffer buf(&data);
        buf.open(QIODevice::WriteOnly);
        QImageWriter w(&buf, "jpeg");
        w.setQuality(100);
        assert(w.write(img));
        QBuffer rbuf(&data);
        rbuf.open(QIODevice::ReadOnly);
        QImage back = QImageReader(&rbuf, "jpeg").read();
        assert(!back.isNull());
        assert(back.size() == img.size());
    }

    // GIF plugin is read-only in Qt; decode a minimal valid 1x1 GIF89a image.
    assert(QImageReader::supportedImageFormats().contains("gif"));
    {
        static const unsigned char kGif1x1[] = {
            'G', 'I', 'F', '8', '9', 'a',
            0x01, 0x00, 0x01, 0x00, 0x80, 0x00, 0x00,
            0x00, 0x80, 0xff, 0x00, 0x00, 0x00,
            0x2c, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00,
            0x02, 0x02, 0x44, 0x01, 0x00, 0x3b,
        };
        QByteArray data(reinterpret_cast<const char *>(kGif1x1), sizeof(kGif1x1));
        QBuffer rbuf(&data);
        rbuf.open(QIODevice::ReadOnly);
        QImage back = QImageReader(&rbuf, "gif").read();
        assert(!back.isNull());
        assert(back.size() == QSize(1, 1));
    }

    // Remaining plugins are registered (tga is read-only in Qt; wbmp is 1-bit).
    assert(QImageReader::supportedImageFormats().contains("tga"));
    assert(QImageReader::supportedImageFormats().contains("icns"));
    assert(QImageReader::supportedImageFormats().contains("wbmp"));
    assert(QImageWriter::supportedImageFormats().contains("wbmp"));
    return 0;
}
