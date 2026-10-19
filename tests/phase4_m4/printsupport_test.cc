#include <QtCore/QFile>
#include <QtCore/QTemporaryDir>
#include <QtGui/QPainter>
#include <QtPrintSupport/QPageSetupDialog>
#include <QtPrintSupport/QPrintDialog>
#include <QtPrintSupport/QPrintPreviewDialog>
#include <QtPrintSupport/QPrintPreviewWidget>
#include <QtPrintSupport/QPrinter>
#include <QtPrintSupport/QPrinterInfo>
#include <QtTest/QtTest>

class PrintSupportTest : public QObject {
  Q_OBJECT

 private slots:
  void testPdfPrinting() {
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    const QString pdfPath = tempDir.filePath(QStringLiteral("output.pdf"));

    QPrinter printer(QPrinter::HighResolution);
    printer.setOutputFormat(QPrinter::PdfFormat);
    printer.setOutputFileName(pdfPath);

    QPainter painter;
    QVERIFY(painter.begin(&printer));
    painter.drawText(100, 100, QStringLiteral("Phase 4 Milestone 4.4 PrintSupport Test"));
    QVERIFY(painter.end());

    QFile pdfFile(pdfPath);
    QVERIFY(pdfFile.exists());
    QVERIFY(pdfFile.open(QIODevice::ReadOnly));
    const QByteArray header = pdfFile.read(5);
    QCOMPARE(header, QByteArray("%PDF-"));
  }

  void testPrintPreviewWidget() {
    QPrinter printer(QPrinter::ScreenResolution);
    printer.setOutputFormat(QPrinter::PdfFormat);
    QPrintPreviewWidget preview(&printer);
    QCOMPARE(preview.currentPage(), 1);
  }
};

QTEST_MAIN(PrintSupportTest)
#include "printsupport_test.moc"
