#include <Example1ToolsetGui.h>

#include <rgnGRegionToolsetGui.h>
#include <SAMMenuPane.h>
#include <SAMMenuCommand.h>


#include <cmdKCommandDeliveryRole.h>
#include <omuArguments.h>
#include <omuMethodCall.h>
#include <omuPrimExpr.h>



Example1ToolsetGui::Example1ToolsetGui()
	: SAMToolsetGui("Test"),
	omiSingleton<Example1ToolsetGui>()
{
	createMenuItems();
	createToolboxItems();
}

Example1ToolsetGui::~Example1ToolsetGui()
{

}

void Example1ToolsetGui::activate()
{
	// todo
	SAMToolsetGui::activate();
}

void Example1ToolsetGui::deactivate()
{
	// todo
	SAMToolsetGui::deactivate();
}

void Example1ToolsetGui::createMenuItems()
{
	SAMMenu* exampleMenu = new SAMMenu(this, tr("&Test"));
	SAMMenuCommand* exampleCmd = new SAMMenuCommand(this, exampleMenu, tr("&Example"));
	exampleMenu->addAction(exampleCmd);

	connect(exampleCmd, SIGNAL(triggered(bool)), this, SLOT(createUI()));
}

void Example1ToolsetGui::createToolboxItems()
{
	// todo
}

void Example1ToolsetGui::createUI()
{
	qDebug("success");
}
