#pragma once

#include <QAbstractListModel>
#include <QKeySequence>
#include <QVariantMap>

class KeyBindings : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(QVariantMap bindings READ bindings NOTIFY bindingsChanged)
public:
    explicit KeyBindings(QObject *parent = nullptr);
    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    QVariantMap bindings() const;
    Q_INVOKABLE QString assign(int row, int key, int modifiers);
    Q_INVOKABLE QString nativeSequence(const QString &sequence) const;
    Q_INVOKABLE void resetDefaults();
signals:
    void bindingsChanged();
private:
    struct Binding { QString id; QString label; QKeySequence value; QKeySequence fallback; };
    QVector<Binding> m_bindings;
    void save();
};
