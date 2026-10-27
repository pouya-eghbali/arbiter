#include <QByteArray>
#include <QDir>
#include <QtGlobal>

// Configure the split development SDK before QApplication / Chromium starts.
// Deployed builds disable this source and use their bundled Qt runtime paths.
namespace {
struct SdkRuntime {
    SdkRuntime() {
        qputenv("QTWEBENGINEPROCESS_PATH", QByteArray(ARBITER_ENGINE_PROCESS));
        qputenv("QTWEBENGINE_RESOURCES_PATH", QByteArray(ARBITER_ENGINE_RESOURCES));
        qputenv("QTWEBENGINE_LOCALES_PATH", QByteArray(ARBITER_ENGINE_LOCALES));
        // All QML engines in an SDK build must resolve the patched module,
        // including test fixtures that create their own QQmlEngine.
        QByteArray imports(ARBITER_WEBENGINE_QML_PATH);
        const auto existing = qgetenv("QML_IMPORT_PATH");
        if (!existing.isEmpty()) {
            imports += QDir::listSeparator().toLatin1();
            imports += existing;
        }
        qputenv("QML_IMPORT_PATH", imports);
    }
};
const SdkRuntime runtime;
}
