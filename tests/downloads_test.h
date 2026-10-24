#pragma once
#include <QObject>
class DownloadsTest : public QObject {
    Q_OBJECT
private slots:
    void namesAndReservations();
    void actualTransfers();
};
