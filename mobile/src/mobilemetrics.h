#pragma once
#include <QObject>
#include <QTimer>
#include <QtQml/qqmlregistration.h>

class MobileMetrics : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(qreal textScale READ textScale NOTIFY changed)
    Q_PROPERTY(int touchTarget READ touchTarget CONSTANT)
public:
    explicit MobileMetrics(QObject *parent = nullptr);
    qreal textScale() const { return m_scale; }
    int touchTarget() const;
    // Reading textScale in the QML binding (rather than in a C++ invokable)
    // ensures all fonts react when the system preference changes.
signals:
    void changed();
private:
    void refresh();
    QTimer m_timer;
    qreal m_scale = 1;
};
