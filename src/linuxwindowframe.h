#pragma once
#include <QObject>
#include <QPointer>
#include <QQuickWindow>

// Keep client-drawn shadows outside Wayland's usable window geometry.
class LinuxWindowFrame : public QObject {
    Q_OBJECT
    Q_PROPERTY(QQuickWindow *targetWindow READ window WRITE setWindow NOTIFY windowChanged)
    Q_PROPERTY(int shadowMargin READ shadowMargin WRITE setShadowMargin NOTIFY shadowMarginChanged)
public:
    using QObject::QObject;
    QQuickWindow *window() const { return m_window; }
    int shadowMargin() const { return m_margin; }
    void setWindow(QQuickWindow *window);
    void setShadowMargin(int margin);
signals:
    void windowChanged();
    void shadowMarginChanged();
protected:
    bool eventFilter(QObject *object, QEvent *event) override;
private:
    void updateFrame();
    QPointer<QQuickWindow> m_window;
    int m_margin = 0;
};
