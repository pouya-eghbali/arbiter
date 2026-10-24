#include "pageprinter.h"
#include <QBuffer>
#include <QFutureWatcher>
#include <QPainter>
#include <QPdfDocument>
#include <QPrintDialog>
#include <QPrinter>
#include <QtConcurrentRun>
#include <algorithm>

#ifdef Q_OS_MACOS
void abortNativePrintDialog();
int execNativePrintDialog(QPrintDialog *dialog);
#endif

PagePrinter::~PagePrinter() { cancel(); }

void PagePrinter::show(QObject *owner, const QString &title)
{
    if (m_busy || !owner || !m_host || m_host->property("privateCovered").toBool()) return;
    m_error.clear(); emit errorChanged();
    m_owner = owner;
    m_busy = true;
    const int request = ++m_request;
    m_printer = std::make_shared<QPrinter>(QPrinter::HighResolution);
    m_printer->setResolution(300);
    m_printer->setDocName(title.isEmpty() ? tr("Web page") : title);
    auto *dialog = new QPrintDialog(m_printer.get());
    m_dialog = dialog;
    dialog->setWindowTitle(tr("Print page"));
    dialog->setOption(QAbstractPrintDialog::PrintPageRange);
    dialog->setOption(QAbstractPrintDialog::PrintCollateCopies);
    dialog->setOption(QAbstractPrintDialog::PrintShowPageSize);
    dialog->setWindowModality(Qt::WindowModal);
    dialog->winId();
    dialog->windowHandle()->setTransientParent(m_host);
    connect(owner, &QObject::destroyed, this, [this, request] { if (request == m_request) cancel(); });
    // On macOS the native panel is application-modal for a Quick window.
    // Process the result only after its native modal loop has returned; never
    // destroy the dialog/printer from inside the panel's delegate callback.
    QPointer<PagePrinter> guard(this);
    const auto printer = m_printer;
    QMetaObject::invokeMethod(dialog, [this, guard, dialog, printer, request] {
        if (!guard || request != m_request) { delete dialog; return; }
        m_dialogRunning = true;
#ifdef Q_OS_MACOS
        const int result = execNativePrintDialog(dialog);
#else
        const int result = dialog->exec();
#endif
        delete dialog;
        if (!guard) return;
        m_dialogRunning = false;
        if (request != m_request) return;
        m_dialog = nullptr;
        if (result != QDialog::Accepted || !m_owner) { cancel(); return; }
        const auto layout = printer->pageLayout();
        // QML WebEngine accepts standard paper IDs; custom paper is fitted to
        // the printer's selected printable rectangle during final rendering.
        const int pageSize = layout.pageSize().id() == QPageSize::Custom ? QPageSize::A4 : layout.pageSize().id();
        emit pdfRequested(request, pageSize, layout.orientation());
    }, Qt::QueuedConnection);
    emit stateChanged();
}

void PagePrinter::cancelFor(QObject *owner) { if (m_owner == owner) cancel(); }
void PagePrinter::cancel()
{
    // A job already handed to the spooler belongs to the OS print queue.
    if (m_spooling) return;
    ++m_request;
    if (m_dialog && m_dialogRunning) {
#ifdef Q_OS_MACOS
        abortNativePrintDialog();
#else
        m_dialog->reject();
#endif
    }
    m_printer.reset(); m_owner = nullptr;
    if (m_busy) { m_busy = false; emit stateChanged(); }
}
void PagePrinter::fail(const QString &message)
{
    cancel(); m_error = message; emit errorChanged();
}
void PagePrinter::submit(int request, const QByteArray &pdf)
{
    if (request != m_request || !m_busy || m_spooling || !m_owner || !m_printer) return;
    if (pdf.isEmpty()) { fail(tr("The page could not be prepared for printing.")); return; }
    m_spooling = true;
    auto printer = std::move(m_printer);
    auto *watcher = new QFutureWatcher<QString>(this);
    connect(watcher, &QFutureWatcher<QString>::finished, this, [this, watcher] {
        const QString error = watcher->result(); watcher->deleteLater();
        m_spooling = false; cancel();
        if (!error.isEmpty()) { m_error = error; emit errorChanged(); }
    });
    // Rendering/spooling a long document must not freeze the browser UI.
    watcher->setFuture(QtConcurrent::run([pdf, printer] { return printDocument(pdf, *printer); }));
}

QString PagePrinter::printDocument(const QByteArray &pdf, QPrinter &printer)
{
    QBuffer buffer;
    buffer.setData(pdf); buffer.open(QIODevice::ReadOnly);
    QPdfDocument document;
    document.load(&buffer);
    if (document.status() != QPdfDocument::Status::Ready || document.pageCount() < 1)
        return tr("The page could not be prepared for printing.");
    int first = 0, last = document.pageCount() - 1;
    if (printer.printRange() == QPrinter::PageRange) {
        first = std::max(0, printer.fromPage() - 1);
        last = std::min(last, printer.toPage() - 1);
    }
    if (first > last) return tr("The selected page range is outside this document.");
    QList<int> pages;
    const auto ranges = printer.pageRanges();
    for (int page = first; page <= last; ++page) {
        if (printer.printRange() != QPrinter::PageRange || ranges.isEmpty() || ranges.contains(page + 1))
            pages.append(page);
    }
    if (pages.isEmpty()) return tr("The selected page range is outside this document.");
    if (printer.pageOrder() == QPrinter::LastPageFirst) std::reverse(pages.begin(), pages.end());
    const int copies = printer.supportsMultipleCopies() ? 1 : std::max(1, printer.copyCount());
    QList<int> output;
    if (printer.collateCopies()) {
        for (int copy = 0; copy < copies; ++copy) output.append(pages);
    } else {
        for (int page : pages) for (int copy = 0; copy < copies; ++copy) output.append(page);
    }
    QPainter painter;
    if (!painter.begin(&printer)) return tr("Could not start the selected printer.");
    bool started = false;
    for (int page : output) {
        if (started && !printer.newPage()) { painter.end(); return tr("The printer could not continue the job."); }
        started = true;
        const QRect viewport = painter.viewport();
        const QSizeF points = document.pagePointSize(page);
        const QSize renderSize = (points * (printer.resolution() / 72.0)).toSize().scaled(viewport.size(), Qt::KeepAspectRatio);
        const QImage image = document.render(page, renderSize);
        if (image.isNull()) { painter.end(); return tr("A page could not be rendered for printing."); }
        QRect target(QPoint(), renderSize); target.moveCenter(viewport.center());
        painter.drawImage(target, image);
    }
    if (!painter.end() || printer.printerState() == QPrinter::Error)
        return tr("The printer reported an error.");
    return {};
}
