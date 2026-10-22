#pragma once

#include <QApplication>
#include <QFileOpenEvent>
#include <QUrl>
#include <functional>
#include <utility>

class BrowserApplication : public QApplication
{
public:
    using QApplication::QApplication;
    using UrlHandler = std::function<void(const QUrl &)>;
    void setUrlHandler(UrlHandler handler)
    {
        m_handler = std::move(handler);
        const auto pending = std::exchange(m_pending, {});
        for (const QUrl &url : pending) m_handler(url);
    }
protected:
    bool event(QEvent *event) override
    {
        if (event->type() != QEvent::FileOpen)
            return QApplication::event(event);
        const QUrl url = static_cast<QFileOpenEvent *>(event)->url();
        if (!url.isValid() || url.host().isEmpty()
            || (url.scheme() != QStringLiteral("http") && url.scheme() != QStringLiteral("https")))
            return false;
        if (m_handler) m_handler(url);
        else m_pending.append(url);
        event->accept();
        return true;
    }
private:
    UrlHandler m_handler;
    QList<QUrl> m_pending;
};
