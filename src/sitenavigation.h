#pragma once
#include <QObject>
#include <QUrl>
#include <QVariantList>
class QMenu;
class QMenuBar;

class SiteNavigation final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString extractionScript READ extractionScript CONSTANT)
public:
    explicit SiteNavigation(QMenuBar *bar, QObject *parent = nullptr);
    QString extractionScript() const;
    Q_INVOKABLE void reset(const QUrl &url, const QString &title, const QString &mode);
    Q_INVOKABLE void setTitle(const QString &title);
    Q_INVOKABLE void update(const QUrl &url, const QVariantMap &snapshot);
signals:
    void linkActivated(const QUrl &url);
private:
    void rebuild();
    void populate(QMenu *menu, const QVariantList &nodes);
    QVariantList sanitize(const QVariantList &nodes, int depth, int &budget) const;
    QMenuBar *m_bar;
    QList<QMenu *> m_menus;
    QUrl m_url;
    QString m_title;
    QString m_mode;
    QVariantList m_nodes;
    quint64 m_revision = 0;
    bool m_rebuildPending = false;
};
