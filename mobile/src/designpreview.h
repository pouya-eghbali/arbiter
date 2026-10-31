#pragma once
#include <QString>
class MobileModel;
class QQmlApplicationEngine;
// Desktop-only, explicitly enabled developer fixture. Never opens a real profile.
void seedDesignPreview(const QString &directory);
void renderDesignPreview(MobileModel &, QQmlApplicationEngine &, const QString &screen,
                         const QString &output);
