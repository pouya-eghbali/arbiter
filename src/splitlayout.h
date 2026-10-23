#pragma once
#include <QObject>
#include <QVariantList>
#include <QVariantMap>
#include <QRectF>
#include <QSizeF>
#include <QHash>
class Tabs;

// Layout contains stable tab IDs, never page objects. Geometry changes do not
// recreate WebEngineViews, navigate them, or move them between profiles.
class SplitLayout final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList panes READ panes NOTIFY changed)
    Q_PROPERTY(QVariantList handles READ handles NOTIFY changed)
    Q_PROPERTY(bool multiple READ multiple NOTIFY changed)
    Q_PROPERTY(int activePane READ activePane NOTIFY changed)
    Q_PROPERTY(qreal minimumWidth READ minimumWidth NOTIFY changed)
    Q_PROPERTY(qreal minimumHeight READ minimumHeight NOTIFY changed)
public:
    explicit SplitLayout(Tabs *tabs, QObject *parent=nullptr);
    QVariantList panes() const;
    QVariantList handles() const;
    bool multiple() const;
    int activePane() const;
    qreal minimumWidth() const;
    qreal minimumHeight() const;
    Q_INVOKABLE void setViewport(qreal width,qreal height);
    Q_INVOKABLE void split(bool horizontal);
    Q_INVOKABLE void activate(int pane);
    Q_INVOKABLE void remove(int pane);
    Q_INVOKABLE void single();
    Q_INVOKABLE void cycle(int direction);
    Q_INVOKABLE bool nextHiddenTab();
    Q_INVOKABLE void resize(int handle,qreal x,qreal y);
    Q_INVOKABLE void swap(int first,int second);
    Q_INVOKABLE QVariantMap geometry(int tab) const;
    QVariantMap snapshot() const;
    void suspend() { m_mutating=true; }
    void restore(const QVariantMap &data);
signals:
    void changed();
    void sessionChanged();
private:
    struct Node { int id=0, tab=0, first=0, second=0; bool horizontal=true; qreal ratio=.5; };
    struct Tree { int root=0, active=0; QHash<int,Node> nodes; };
    Tree &tree();
    const Tree *current() const;
    void synchronize(bool selection);
    int prune(Tree &tree,int node,const QString &workspace);
    int firstLeaf(const Tree &tree,int node) const;
    int row(int tab,const QString &workspace) const;
    QSizeF minimum(const Tree &tree,int node) const;
    void flatten(const Tree &tree,int node,const QRectF &rect,QVariantList &panes,QVariantList &handles) const;
    void publish(bool session=true);
    void ensureGeometry() const;
    mutable bool m_geometryDirty=true;
    mutable QVariantList m_panes,m_handles;
    mutable QHash<int,QVariantMap> m_geometry;
    mutable QHash<int,QSizeF> m_minimum;
    Tabs *m_tabs;
    QHash<QString,Tree> m_trees;
    int m_nextId=1;
    bool m_mutating=false;
    qreal m_width=1280,m_height=820;
};
