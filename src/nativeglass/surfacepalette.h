#pragma once
#include <QObject>
#include <QColor>
#include <QEasingCurve>
#include <QQuickItem>
#include <QtQml/qqmlregistration.h>

// Per-window semantics: native surfaces follow their actual Apple material;
// ordinary pages and settings retain the user's Glacier palette.
class SurfacePalette : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(int revision READ revision NOTIFY changed)
    Q_PROPERTY(bool nativeKeyboardAvailable READ nativeKeyboardAvailable CONSTANT)
    Q_PROPERTY(int keyboardRevision READ keyboardRevision NOTIFY keyboardChanged)
    Q_PROPERTY(int keyboardDuration READ keyboardDuration NOTIFY keyboardChanged)
    Q_PROPERTY(int keyboardEasing READ keyboardEasing NOTIFY keyboardChanged)
    Q_PROPERTY(bool windowSupported READ windowSupported CONSTANT)
public:
    explicit SurfacePalette(QObject *parent = nullptr);
    int revision() const { return m_revision; }
    bool windowSupported() const;
    bool nativeKeyboardAvailable() const;
    int keyboardRevision() const { return m_keyboardRevision; }
    int keyboardDuration() const { return m_keyboardDuration; }
    int keyboardEasing() const { return m_keyboardEasing; }
    void setKeyboardFrame(const QRectF &, int duration, int curve);
    Q_INVOKABLE qreal keyboardInset(QQuickItem *) const;
    Q_INVOKABLE void conceal(QQuickItem *item) const;
    Q_INVOKABLE QWindow *owner(QQuickItem *item) const;
    Q_INVOKABLE bool popupWindow(QWindow *window) const;
    Q_INVOKABLE qreal keyboardTop(QQuickItem *target, qreal y) const;
    Q_INVOKABLE bool active(QQuickItem *item) const;
    Q_INVOKABLE QColor foreground(QQuickItem *item) const;
    static void setForeground(QWindow *, const QColor &);
signals:
    void changed();
    void keyboardChanged();
private:
    int m_revision = 0;
    int m_keyboardRevision = 0, m_keyboardDuration = 250, m_keyboardEasing = QEasingCurve::OutCubic;
    QRectF m_keyboardFrame;
};
