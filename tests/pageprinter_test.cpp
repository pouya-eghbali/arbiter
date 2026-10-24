#include "../src/pageprinter.h"
#include <QtTest>
#include <QBuffer>
#include <QPainter>
#include <QPdfDocument>
#include <QPdfWriter>
#include <QPrinter>
#include <QTemporaryDir>
#ifdef Q_OS_MACOS
QString checkNativePrintAppearance();
#endif

class PagePrinterTest : public QObject {
    Q_OBJECT
    QByteArray fixture() {
        QByteArray bytes;
        QBuffer buffer(&bytes); buffer.open(QIODevice::WriteOnly);
        {
            QPdfWriter writer(&buffer); writer.setResolution(72);
            QPainter painter(&writer);
            for (int page = 0; page < 3; ++page) {
                if (page) writer.newPage();
                painter.fillRect(painter.viewport(), QList<QColor>{Qt::red, Qt::green, Qt::blue}[page]);
            }
        }
        return bytes;
    }
private slots:
    void nativeAppearance() {
#ifdef Q_OS_MACOS
        if (QGuiApplication::platformName() != "cocoa") QSKIP("Native appearance requires the macOS desktop backend");
        const QString error = checkNativePrintAppearance();
        QVERIFY2(error.isEmpty(), qPrintable(error));
#else
        QSKIP("macOS native print-panel regression");
#endif
    }
    void disjointPageRanges() {
        QTemporaryDir directory; QVERIFY(directory.isValid());
        QPrinter printer; printer.setResolution(72);
        printer.setOutputFormat(QPrinter::PdfFormat);
        printer.setOutputFileName(directory.filePath("ranges.pdf"));
        QPageRanges ranges; ranges.addPage(1); ranges.addPage(3);
        printer.setPageRanges(ranges); printer.setPrintRange(QPrinter::PageRange);
        QCOMPARE(PagePrinter::printDocument(fixture(), printer), QString());
        QPdfDocument result; QCOMPARE(result.load(printer.outputFileName()), QPdfDocument::Error::None);
        QCOMPARE(result.pageCount(), 2);
        QVERIFY(result.render(0, QSize(100,140)).pixelColor(50,70).red() > 240);
        QVERIFY(result.render(1, QSize(100,140)).pixelColor(50,70).blue() > 240);
    }
    void rangeAndReverseOrder() {
        QTemporaryDir directory; QVERIFY(directory.isValid());
        const QString path = directory.filePath("printed.pdf");
        QPrinter printer(QPrinter::HighResolution);
        printer.setResolution(150);
        printer.setOutputFormat(QPrinter::PdfFormat); printer.setOutputFileName(path);
        printer.setFromTo(2, 3); printer.setPrintRange(QPrinter::PageRange);
        printer.setPageOrder(QPrinter::LastPageFirst);
        QCOMPARE(PagePrinter::printDocument(fixture(), printer), QString());
        QPdfDocument result; QCOMPARE(result.load(path), QPdfDocument::Error::None);
        QCOMPARE(result.pageCount(), 2);
        const auto first = result.render(0, QSize(100, 140)).pixelColor(50, 70);
        const auto second = result.render(1, QSize(100, 140)).pixelColor(50, 70);
        QVERIFY(first.blue() > 240 && first.red() < 10);
        QVERIFY(second.green() > 240 && second.blue() < 10);
    }
    void rejectsBadDocumentsAndRanges() {
        QTemporaryDir directory; QVERIFY(directory.isValid());
        QPrinter printer; printer.setOutputFormat(QPrinter::PdfFormat);
        printer.setOutputFileName(directory.filePath("invalid.pdf"));
        QVERIFY(!PagePrinter::printDocument("not a PDF", printer).isEmpty());
        QVERIFY(!QFile::exists(printer.outputFileName()));
        printer.setFromTo(8, 10); printer.setPrintRange(QPrinter::PageRange);
        QVERIFY(!PagePrinter::printDocument(fixture(), printer).isEmpty());
        QVERIFY(!QFile::exists(printer.outputFileName()));
    }
    void staleCallbacksCannotPrint() {
        PagePrinter controller;
        controller.submit(123, fixture());
        QVERIFY(!controller.busy());
        QVERIFY(controller.error().isEmpty());
        controller.cancel(); controller.submit(123, fixture());
        QVERIFY(!controller.busy());
    }
};
QTEST_MAIN(PagePrinterTest)
#include "pageprinter_test.moc"
