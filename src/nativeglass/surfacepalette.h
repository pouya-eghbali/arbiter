#pragma once
#include <QObject>
#include <QColor>
#include <QQuickItem>
#include <QtQml/qqmlregistration.h>

// Per-window semantics: native surfaces follow their actual Apple material;
// ordinary pages and settings retain the user's Glacier palette.
class SurfacePalette : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(int revision READ revision NOTIFY changed)
    Q_PROPERTY(bool windowSupported READ windowSupported CONSTANT)
public:
    explicit SurfacePalette(QObject *parent = nullptr);
    int revision() const { return m_revision; }
    bool windowSupported() const;
    Q_INVOKABLE void conceal(QQuickItem *item) const;
    Q_INVOKABLE QWindow *owner(QQuickItem *item) const;
    Q_INVOKABLE bool popupWindow(QWindow *window) const;
    Q_INVOKABLE qreal keyboardTop(QQuickItem *target, qreal y) const;
    Q_INVOKABLE bool active(QQuickItem *item) const;
    Q_INVOKABLE QColor foreground(QQuickItem *item) const;
    static void setForeground(QWindow *, const QColor &);
signals:
    void changed();
private:
    int m_revision = 0;
};
