#pragma once
#include <QByteArray>

// A dedicated locked page keeps the session key out of ordinary swap. Copies
// are only made for the existing short-lived, explicitly wiped crypto paths.
class LockedKey final {
public:
    ~LockedKey();
    LockedKey() = default;
    LockedKey(const LockedKey &) = delete;
    LockedKey &operator=(const LockedKey &) = delete;
    bool set(const QByteArray &key);
    bool present() const { return m_present; }
    QByteArray copy() const;
    void clear();
private:
    void *m_page = nullptr;
    size_t m_size = 0;
    bool m_present = false;
};
