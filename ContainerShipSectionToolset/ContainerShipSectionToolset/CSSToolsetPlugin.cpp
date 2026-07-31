#include "CSSToolsetPlugin.h"

#include "CSSToolsetGui.h"
#include <SAMApp.h>
#include <SAMMainWindow.h>
#include <SAMModuleGui.h>

void CSSToolsetPlugin::registerToolset()
{
    SAMApp* app_ptr = SAMApp::getSAMApp();
    SAMMainWindow* main_window_ptr = app_ptr->getSAMMainWindow();
    SAMModuleGui* module_gui_ptr = main_window_ptr->getModule("Part");
    if(module_gui_ptr)
    {
        module_gui_ptr->registerToolset(&CSSToolsetGui::Instance(), GUI_IN_MENUBAR | GUI_IN_TOOLBAR);
    }
}
