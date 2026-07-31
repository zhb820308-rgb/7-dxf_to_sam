#ifndef CSSTOOLSETPLUGIN_H
#define CSSTOOLSETPLUGIN_H

#include <QtPlugin>
#include <SAMToolsetGuiInterface.h>

class CSSToolsetPlugin: public QObject, SAMToolsetGuiInterface
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID SAMToolsetGuiPlugin_iid)
    Q_INTERFACES(SAMToolsetGuiInterface)

public:
    CSSToolsetPlugin() = default;
    virtual ~CSSToolsetPlugin() = default;

    virtual void registerToolset() override;
};
#endif // CSSTOOLSETPLUGIN_H
