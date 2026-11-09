#pragma once
#include <QObject>
class DesktopWindowTest : public QObject {
    Q_OBJECT
private slots:
    void webFullscreen();
    void roundedFrameGeometry();
    void frameAndSettingsWhilePageFocused();
};
