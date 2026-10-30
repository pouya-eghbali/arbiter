#pragma once
#include <QAbstractListModel>
#include <QJsonArray>
#include <QJsonObject>
class MobileTab : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantMap record READ record NOTIFY changed)
  public:
    using QObject::QObject;
    QJsonObject value;
    QVariantMap record() const {
        return value.toVariantMap();
    }
  signals:
    void changed();
};
class MobileTabs : public QAbstractListModel {
    Q_OBJECT
  public:
    using QAbstractListModel::QAbstractListModel;
    int rowCount(const QModelIndex &parent = {}) const override {
        return parent.isValid() ? 0 : m_tabs.size();
    }
    QVariant data(const QModelIndex &index, int role) const override {
        if (!index.isValid() || index.row() >= m_tabs.size() || role != Qt::UserRole)
            return {};
        return QVariant::fromValue(m_tabs[index.row()]);
    }
    QHash<int, QByteArray> roleNames() const override {
        return {{Qt::UserRole, "tab"}};
    }
    void synchronize(const QJsonArray &records) {
        QSet<QString> ids;
        for (const auto &v : records)
            ids.insert(v.toObject().value("id").toString());
        for (int i = m_tabs.size() - 1; i >= 0; --i)
            if (!ids.contains(m_tabs[i]->value.value("id").toString())) {
                beginRemoveRows({}, i, i);
                auto t = m_tabs.takeAt(i);
                endRemoveRows();
                t->deleteLater();
            }
        for (const auto &v : records) {
            auto record = v.toObject();
            MobileTab *current = nullptr;
            for (auto t : m_tabs)
                if (t->value.value("id") == record.value("id")) {
                    current = t;
                    break;
                }
            if (!current) {
                beginInsertRows({}, m_tabs.size(), m_tabs.size());
                current = new MobileTab(this);
                current->value = record;
                m_tabs.append(current);
                endInsertRows();
            } else if (current->value != record) {
                current->value = record;
                emit current->changed();
            }
        }
    }

  private:
    QList<MobileTab *> m_tabs;
};
