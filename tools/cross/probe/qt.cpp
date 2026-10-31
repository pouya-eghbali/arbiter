#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>
#include <winrt/Windows.Foundation.h>
class Probe : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
};
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    winrt::init_apartment(winrt::apartment_type::single_threaded);
    const winrt::Windows::Foundation::Uri uri(L"https://example.com/");
    Probe probe;
    return uri.AbsoluteUri().empty()
        || probe.metaObject()->className()[0] == '\0'
        || !QFile::exists(":/main.cpp")
        || QJsonDocument(QJsonObject{{"target", "windows-x64"}}).toJson().isEmpty();
}
#include "qt.moc"
