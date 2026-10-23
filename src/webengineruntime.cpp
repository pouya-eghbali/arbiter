#include <QByteArray>
#include <QtGlobal>

// Configure the split development SDK before QApplication / Chromium starts.
// Deployed builds disable this source and use their bundled Qt runtime paths.
namespace {
struct SdkRuntime {
    SdkRuntime() {
        qputenv("QTWEBENGINEPROCESS_PATH", QByteArray(ARBITER_ENGINE_PROCESS));
        qputenv("QTWEBENGINE_RESOURCES_PATH", QByteArray(ARBITER_ENGINE_RESOURCES));
        qputenv("QTWEBENGINE_LOCALES_PATH", QByteArray(ARBITER_ENGINE_LOCALES));
    }
};
const SdkRuntime runtime;
}
