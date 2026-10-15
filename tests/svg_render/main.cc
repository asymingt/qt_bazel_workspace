#include <QBuffer>
#include <QByteArray>
#include <QColor>
#include <QGuiApplication>
#include <QImage>
#include <QImageReader>
#include <QPainter>
#include <QSvgRenderer>
#include <cassert>

static const char kSvg[] =
    "<svg xmlns='http://www.w3.org/2000/svg' width='16' height='16'>"
    "<rect x='0' y='0' width='16' height='16' fill='#ff0000'/></svg>";

int main(int argc, char *argv[]) {
    QGuiApplication app(argc, argv);

    // Direct use of the QtSvg library.
    QSvgRenderer renderer{QByteArray(kSvg)};
    assert(renderer.isValid());
    QImage img(16, 16, QImage::Format_ARGB32);
    img.fill(Qt::transparent);
    {
        QPainter p(&img);
        renderer.render(&p);
    }
    assert(img.pixelColor(8, 8) == QColor(255, 0, 0));

    // The statically linked imageformats/qsvg plugin must be registered.
    assert(QImageReader::supportedImageFormats().contains("svg"));
    QByteArray data(kSvg);
    QBuffer buf(&data);
    buf.open(QIODevice::ReadOnly);
    QImageReader reader(&buf, "svg");
    QImage viaPlugin = reader.read();
    assert(!viaPlugin.isNull());
    assert(viaPlugin.pixelColor(8, 8) == QColor(255, 0, 0));
    return 0;
}
