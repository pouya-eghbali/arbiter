#pragma once

#include <QSettings>
#include <memory>

// Portable TOML preferences. System files provide per-key defaults; writes
// merge into the user's file under a lock and commit atomically.
class Settings {
public:
    Settings();
    ~Settings();
    QVariant value(const QString &key,const QVariant &fallback=QVariant()) const;
    bool contains(const QString &key) const;
    void setValue(const QString &key,const QVariant &value);
    void remove(const QString &key);
    void clear();
    void sync();
    QSettings::Status status() const;
    QString fileName() const;
    static QString configFile();
    static QStringList systemFiles();
    static QString initialize();
private:
    struct Impl;
    std::unique_ptr<Impl> m;
    void update(const QString &key,const QVariant &value,bool remove,bool clear);
};
