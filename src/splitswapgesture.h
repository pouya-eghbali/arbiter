#pragma once
#include <QQuickItem>
#include <QVariantList>

// Intercept the modifier gesture before WebEngine receives a mouse-down, so
// swapping never selects text, drags a link, or clicks through into the page.
class SplitSwapGesture : public QQuickItem {
    Q_OBJECT
    Q_PROPERTY(QVariantList panes READ panes WRITE setPanes)
public:
    explicit SplitSwapGesture(QQuickItem *parent=nullptr);
    ~SplitSwapGesture() override;
    QVariantList panes() const { return m_panes; }
    void setPanes(const QVariantList &panes);
signals:
    void panePressed(int pane);
    void started(int pane,QPointF position);
    void moved(QPointF position);
    void finished(bool commit,QPointF position);
protected:
    bool eventFilter(QObject *watched,QEvent *event) override;
private:
    int paneAt(const QPointF &point) const;
    void cancel();
    QVariantList m_panes;
    QPointF m_origin,m_position;
    int m_source=0;
    bool m_dragging=false,m_cancelled=false;
};
