#include "process.h"
#include "spectrumitem.h"

#include <QQmlEngine>
#include <QQmlExtensionPlugin>

class ProcessPlugin : public QQmlExtensionPlugin {
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QQmlExtensionInterface")

  public:
    void registerTypes(const char *uri) override
    {
        qmlRegisterType<Process>(uri, 1, 0, "Process");
        qmlRegisterType<SpectrumItem>(uri, 1, 0, "SpectrumItem");
    }
};

#include "processplugin.moc"
