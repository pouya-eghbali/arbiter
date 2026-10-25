#pragma once
#include <QObject>
#include <QSet>
#include <QUrl>

// Normal windows share remembered host rules; private windows use a local copy.
class SiteAudio final : public QObject {
    Q_OBJECT
    Q_PROPERTY(int revision READ revision NOTIFY changed)
public:
    explicit SiteAudio(bool persistent = true, QObject *parent = nullptr);
    int revision() const { return m_revision; }
    Q_INVOKABLE static QString siteHost(const QUrl &url);
    Q_INVOKABLE bool isMuted(const QUrl &url) const;
    Q_INVOKABLE bool setMuted(const QUrl &url, bool muted);
signals:
    void changed();
private:
    QSet<QString> m_muted;
    bool m_persistent;
    int m_revision = 0;
};
