#include <Example1Toolset.h>

#include <DxfLogViewerDialog.h>
#include <Example1Form.h>

#include <SAMApp.h>
#include <SAMMainWindow.h>
#include <SAMMenuCommand.h>
#include <SAMMenuPane.h>
#include <SAMModuleGui.h>

#include <QMenuBar>

namespace {

QString normalizedMenuText(QString text)
{
    text.remove('&');
    return text.trimmed();
}

QMenu* findMenu(
    const QList<QAction*>& actions,
    const QString& englishName,
    const QString& chineseName)
{
    for (QAction* action : actions)
    {
        QMenu* menu = action->menu();
        if (!menu)
            continue;

        const QString title = normalizedMenuText(action->text());
        if (title.compare(englishName, Qt::CaseInsensitive) == 0
            || title == chineseName)
        {
            return menu;
        }
    }
    return nullptr;
}

} // namespace

Example1ToolsetPlugin::Example1ToolsetPlugin() = default;
Example1ToolsetPlugin::~Example1ToolsetPlugin() = default;

void Example1ToolsetPlugin::registerToolset()
{
    SAMApp* app = SAMApp::getSAMApp();
    SAMMainWindow* mainWindow = app->getSAMMainWindow();

    if (mainWindow->getModule("Part"))
    {
        mainWindow->getModule("Part")->registerToolset(
            &Example1ToolsetGui::Instance(),
            GUI_IN_MENUBAR | GUI_IN_TOOLBAR);
    }
}

Example1ToolsetGui::Example1ToolsetGui()
    : SAMToolsetGui("Test"),
      omiSingleton<Example1ToolsetGui>()
{
    createMenuItems();
    createToolboxItems();
}

Example1ToolsetGui::~Example1ToolsetGui() = default;

void Example1ToolsetGui::activate()
{
    SAMToolsetGui::activate();
}

void Example1ToolsetGui::deactivate()
{
    SAMToolsetGui::deactivate();
}

void Example1ToolsetGui::createMenuItems()
{
    SAMMenu* testMenu = new SAMMenu(this, tr("&Test"));
    SAMMenuCommand* example1Command =
        new SAMMenuCommand(this, testMenu, tr("&Example1"));
    testMenu->addAction(example1Command);
    connect(
        example1Command, SIGNAL(triggered(bool)),
        this, SLOT(onImportDxf()));

    SAMMainWindow* mainWindow = SAMApp::getSAMApp()->getSAMMainWindow();
    QMenuBar* menuBar = mainWindow ? mainWindow->getMenubar() : nullptr;

    QMenu* toolsMenu = menuBar
        ? findMenu(menuBar->actions(), QStringLiteral("Tools"), QStringLiteral("\u5de5\u5177"))
        : nullptr;
    if (toolsMenu)
    {
        SAMMenuCommand* viewLogsCommand =
            new SAMMenuCommand(this, toolsMenu, tr("DXFimport log"));
        toolsMenu->addAction(viewLogsCommand);
        connect(viewLogsCommand, SIGNAL(triggered(bool)),
            this, SLOT(onViewDxfLogs()));
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

    SAMMenuCommand* importDxfCommand =
        new SAMMenuCommand(this, importMenu, tr("DXF..."));
    importMenu->addAction(importDxfCommand);
    connect(importDxfCommand, SIGNAL(triggered(bool)),
        this, SLOT(onImportDxf()));
}

void Example1ToolsetGui::createToolboxItems()
{
}

void Example1ToolsetGui::onImportDxf()
{
    Example1Form* form = new Example1Form(this);
    form->onCmdActivate(this, 0, 0);
}

void Example1ToolsetGui::onViewDxfLogs()
{
    SAMMainWindow* mainWindow = SAMApp::getSAMApp()->getSAMMainWindow();
    DxfLogViewerDialog dialog(mainWindow);
    dialog.exec();
}
