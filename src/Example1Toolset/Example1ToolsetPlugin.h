#ifndef Example1ToolsetPlugin_h
#define Example1ToolsetPlugin_h

#include <QtPlugin>
#include <SAMToolsetGuiInterface.h>



/// @brief Qt plugin entry point for the Example1 toolset GUI.
///
/// Registers the DXF import and log viewer tools with the SAM application
/// framework via the SAMToolsetGuiInterface.
class Example1ToolsetPlugin : public QObject, SAMToolsetGuiInterface
{
	Q_OBJECT
	Q_PLUGIN_METADATA(IID SAMToolsetGuiPlugin_iid)
	Q_INTERFACES(SAMToolsetGuiInterface)
public:
	Example1ToolsetPlugin();
	virtual ~Example1ToolsetPlugin();

public:
	/// @brief Register the toolset GUI with the SAM framework.
	virtual void registerToolset();
};


#endif
