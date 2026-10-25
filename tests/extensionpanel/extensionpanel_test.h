#pragma once
#include <QObject>
class ExtensionPanelTest : public QObject {
    Q_OBJECT
private slots:
    void extensionPillMode();
    void cookieWarningAppearsBeforeApproval();
    void pageInteractionKeepsExtensionOpen();
};
