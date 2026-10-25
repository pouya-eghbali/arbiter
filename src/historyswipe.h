#pragma once
#include <QObject>
#include <QPointer>
#include <QQuickItem>
#include <QTimer>
#include <QtQml/qqmlregistration.h>

// Receives only browser-process compositor reports from our patched WebEngine.
class HistorySwipe : public QObject {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QQuickItem *view READ view WRITE setView NOTIFY viewChanged)
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY enabledChanged)
    Q_PROPERTY(bool canGoBack READ canGoBack WRITE setCanGoBack NOTIFY availabilityChanged)
    Q_PROPERTY(bool canGoForward READ canGoForward WRITE setCanGoForward NOTIFY availabilityChanged)
    Q_PROPERTY(bool active READ active NOTIFY changed)
    Q_PROPERTY(int direction READ direction NOTIFY changed)
    Q_PROPERTY(double progress READ progress NOTIFY changed)
public:
    explicit HistorySwipe(QObject *parent=nullptr);
    ~HistorySwipe() override;
    QQuickItem *view() const { return m_view; }
    void setView(QQuickItem *view);
    bool enabled() const { return m_enabled; }
    void setEnabled(bool enabled);
    bool canGoBack() const { return m_back; }
    bool canGoForward() const { return m_forward; }
    void setCanGoBack(bool value);
    void setCanGoForward(bool value);
    bool active() const { return m_tracking && m_direction != 0; }
    int direction() const { return m_direction; } // -1 Back, +1 Forward
    double progress() const;
    Q_INVOKABLE void cancel();
    Q_INVOKABLE void beginGesture(bool eligible);
    Q_INVOKABLE void scrollUpdate(double dx,double dy,bool consumed);
    Q_INVOKABLE void overscroll(double dx,double dy,bool allowed);
    Q_INVOKABLE void endGesture();
signals:
    void viewChanged();
    void enabledChanged();
    void availabilityChanged();
    void changed();
    void triggered(int direction);
private:
    double threshold() const;
    void updateFeedback();
    QPointer<QQuickItem> m_view;
    QTimer m_timeout;
    bool m_enabled=true,m_back=false,m_forward=false,m_tracking=false,m_updated=false;
    int m_direction=0,m_boundaryDirection=0;
    double m_x=0,m_y=0,m_totalX=0,m_totalY=0,m_reverse=0;
};
