#pragma once
#include <QObject>
#include <QColor>
#include <QTimer>
#include <QLockFile>
#include <memory>
#include "tabs.h"
#include "splitlayout.h"

class Workspaces final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList choices READ choices NOTIFY changed)
    Q_PROPERTY(int count READ count NOTIFY changed)
    Q_PROPERTY(QString currentId READ currentId NOTIFY currentChanged)
    Q_PROPERTY(QString currentName READ currentName NOTIFY changed)
    Q_PROPERTY(QColor currentColor READ currentColor NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
    Q_PROPERTY(QObject *splits READ splits CONSTANT)
public:
    explicit Workspaces(Tabs *tabs, QObject *parent = nullptr);
    Workspaces(Tabs *tabs, const QString &directory, QObject *parent = nullptr);
    ~Workspaces() override;
    QVariantList choices() const;
    int count() const { return m_spaces.size(); }
    QString currentId() const { return m_current; }
    QString currentName() const;
    QColor currentColor() const;
    QString error() const { return m_error; }
    QObject *splits() { return &m_splits; }
    SplitLayout *splitLayout() { return &m_splits; }
    Q_INVOKABLE void activate(const QString &id);
    Q_INVOKABLE void cycle(int direction);
    Q_INVOKABLE QString create(const QString &name, const QColor &color, bool isolated);
    Q_INVOKABLE bool edit(const QString &id, const QString &name, const QColor &color);
    Q_INVOKABLE bool remove(const QString &id);
    Q_INVOKABLE void reorder(const QString &id, int direction);
    Q_INVOKABLE bool moveTab(int row, const QString &id);
    Q_INVOKABLE bool copyTab(int row, const QString &id);
    Q_INVOKABLE int addTab(const QString &id, const QUrl &url, bool activate);
    Q_INVOKABLE void flush();
signals:
    void changed();
    void currentChanged();
private:
    struct Space { QString id; QString name; QColor color; bool isolated; };
    int indexOf(const QString &id) const;
    QString profile(const Space &space) const;
    bool fail(const QString &error);
    void scheduleSave();
    Tabs *m_tabs;
    SplitLayout m_splits;
    QList<Space> m_spaces;
    QString m_current, m_directory, m_error;
    std::unique_ptr<QLockFile> m_lock;
    QTimer m_save;
    bool m_writable = true;
};
