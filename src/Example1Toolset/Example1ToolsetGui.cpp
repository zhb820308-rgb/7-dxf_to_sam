#include <Example1ToolsetGui.h>

#include <rgnGRegionToolsetGui.h>
#include <SAMMenuPane.h>
#include <SAMMenuCommand.h>


#include <cmdKCommandDeliveryRole.h>
#include <omuArguments.h>
#include <omuMethodCall.h>
#include <omuPrimExpr.h>

#include <Example1Form.h>
#include <cmdGCommandDeliveryRole.h>



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
	SAMMenu* testMenu = new SAMMenu(this, tr("&Test"));

	SAMMenuCommand* example1Cmd = new SAMMenuCommand(this, testMenu, tr("&Example1"));
	testMenu->addAction(example1Cmd);
	connect(example1Cmd, SIGNAL(triggered(bool)), this, SLOT(Example1Ui()));
}

void Example1ToolsetGui::createToolboxItems()
{
	// todo
}

void Example1ToolsetGui::Example1Ui()
{
	Example1Form* aExample1Form = new Example1Form(this);
	aExample1Form->onCmdActivate(this, 0, 0);
	qDebug("success");
	cmdGCommandDeliveryRole::Instance().SendCommand("import Example1");
	cmdGCommandDeliveryRole::Instance().SendCommand("Example1.createLine()");


}

