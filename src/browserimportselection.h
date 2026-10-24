#pragma once
#include <QObject>
#include <QVariantList>
class BrowserImporter;
class QWindow;

// Onboarding's multi-browser selection. The importer remains the sole owner of secrets.
class BrowserImportSelection final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList browsers READ browsers NOTIFY changed)
    Q_PROPERTY(QVariantList results READ results NOTIFY changed)
    Q_PROPERTY(int selectedCount READ selectedCount NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(bool done READ done NOTIFY changed)
    Q_PROPERTY(bool hasFailures READ hasFailures NOTIFY changed)
    Q_PROPERTY(QString progress READ progress NOTIFY changed)
public:
    explicit BrowserImportSelection(BrowserImporter *importer,QObject *parent=nullptr);
    QVariantList browsers() const { return m_browsers; }
    QVariantList results() const { return m_results; }
    int selectedCount() const;
    bool busy() const { return m_busy; }
    bool done() const { return m_done; }
    bool hasFailures() const { return !m_failed.isEmpty(); }
    QString progress() const;
    Q_INVOKABLE void refresh(bool preserveSelection=false,bool rescan=true);
    Q_INVOKABLE void toggle(int index,QWindow *window=nullptr);
    Q_INVOKABLE void start();
    Q_INVOKABLE void retryFailed();
    Q_INVOKABLE void cancel();
signals:
    void changed();
    void completed(bool success);
private:
    void begin(const QList<int> &profiles);
    void next();
    void finish();
    BrowserImporter *m_importer;
    QVariantList m_browsers,m_results;
    QList<int> m_queue,m_failed;
    quint64 m_generation=0;
    QString m_accessBrowser;
    int m_current=-1,m_position=0;
    bool m_busy=false,m_done=false,m_cancelled=false;
};
