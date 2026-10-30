#pragma once
#include <QByteArray>
#include <QMutex>
#include <QStringList>
#include <QUrl>
class AdBlockEngine {
  public:
    explicit AdBlockEngine(const QByteArray &rules);
    ~AdBlockEngine();
    bool valid() const {
        return m_engine;
    }
    bool blocks(const QUrl &url, const QUrl &source, const QByteArray &type, const QByteArray &method) const;
    QStringList cosmetic(const QUrl &url, const QStringList &classes, const QStringList &ids) const;

  private:
    void *m_engine = nullptr;
    mutable QMutex m_mutex;
};
