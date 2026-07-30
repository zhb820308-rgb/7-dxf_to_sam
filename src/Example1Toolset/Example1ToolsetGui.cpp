#include <Example1ToolsetGui.h>

#include <rgnGRegionToolsetGui.h>
#include <SAMMenuPane.h>
#include <SAMMenuCommand.h>
#include <SAMApp.h>
#include <SAMMainWindow.h>

#include <QMenuBar>


#include <cmdKCommandDeliveryRole.h>
#include <omuArguments.h>
#include <omuMethodCall.h>
#include <omuPrimExpr.h>

#include <Example1Form.h>
#include <DxfLogViewerDialog.h>
#include <cmdGCommandDeliveryRole.h>


namespace
{
	QString normalizedMenuText(QString text)
	{
		text.remove('&');
		return text.trimmed();
	}

	QMenu* findMenu(const QList<QAction*>& actions,
		const QString& englishName,
		const QString& chineseName)
	{
		for (QAction* action : actions)
		{
			QMenu* menu = action->menu();
			if (!menu)
				continue;

			const QString title = normalizedMenuText(action->text());
			if (title.compare(englishName, Qt::CaseInsensitive) == 0 ||
				title == chineseName)
			{
				return menu;
			}
		}

		return nullptr;
	}
}


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
	connect(example1Cmd, SIGNAL(triggered(bool)), this, SLOT(onImportDxf()));

	SAMMainWindow* mainWindow = SAMApp::getSAMApp()->getSAMMainWindow();
	QMenuBar* menuBar = mainWindow ? mainWindow->getMenubar() : nullptr;
	QMenu* toolsMenu = menuBar
		? findMenu(menuBar->actions(), QStringLiteral("Tools"), QStringLiteral("工具"))
		: nullptr;
	if (toolsMenu)
	{
		SAMMenuCommand* viewLogsCmd =
			new SAMMenuCommand(this, toolsMenu, tr("DXFimport log"));
		toolsMenu->addAction(viewLogsCmd);
		connect(viewLogsCmd, SIGNAL(triggered(bool)), this, SLOT(onViewDxfLogs()));
	}
	else
	{
		qWarning("Example1Toolset: Tools menu was not found");
	}

	QMenu* fileMenu = menuBar
		? findMenu(menuBar->actions(), QStringLiteral("File"), QStringLiteral("\u6587\u4ef6"))
		: nullptr;
	QMenu* importMenu = fileMenu
		? findMenu(fileMenu->actions(), QStringLiteral("Import"), QStringLiteral("\u5bfc\u5165"))
		: nullptr;

	if (!importMenu)
	{
		qWarning("Example1Toolset: File/Import menu was not found");
		return;
	}

	SAMMenuCommand* importDxfCmd = new SAMMenuCommand(this, importMenu, tr("DXF..."));
	importMenu->addAction(importDxfCmd);
	connect(importDxfCmd, SIGNAL(triggered(bool)), this, SLOT(onImportDxf()));
}

void Example1ToolsetGui::createToolboxItems()
{
	// todo
}

void Example1ToolsetGui::onImportDxf()
{
	Example1Form* aExample1Form = new Example1Form(this);
	aExample1Form->onCmdActivate(this, 0, 0);
}

void Example1ToolsetGui::onViewDxfLogs()
{
	SAMMainWindow* mainWindow = SAMApp::getSAMApp()->getSAMMainWindow();
	DxfLogViewerDialog dialog(mainWindow);
	dialog.exec();
}
