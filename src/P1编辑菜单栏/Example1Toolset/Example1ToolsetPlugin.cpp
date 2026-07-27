#include <Example1ToolsetPlugin.h>
#include <Example1ToolsetGui.h>

#include <SAMApp.h>
#include <SAMMainWindow.h>
#include <SAMModuleGui.h>

Example1ToolsetPlugin::Example1ToolsetPlugin()
{

}

Example1ToolsetPlugin::~Example1ToolsetPlugin()
{

}

void Example1ToolsetPlugin::registerToolset()
{
	SAMApp* app = SAMApp::getSAMApp();
	SAMMainWindow* mn = app->getSAMMainWindow();
	if (mn->getModule("Part"))
	{
		mn->getModule("Part")->registerToolset(&Example1ToolsetGui::Instance(), GUI_IN_MENUBAR | GUI_IN_TOOLBAR);
	}
}