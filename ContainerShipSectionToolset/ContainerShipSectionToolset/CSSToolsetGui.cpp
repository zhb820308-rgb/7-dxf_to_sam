#include "CSSToolsetGui.h"

#include "CSSForm.h"

#include <SAMMenuCommand.h>
#include <SAMMenuPane.h>
#include <SAMToolBar.h>
#include <QToolButton>

CSSToolsetGui::CSSToolsetGui():
    SAMToolsetGui("ContainerShipSection"),
    omiSingleton<CSSToolsetGui>()
{
    createMenuItems();
    createToolboxItems();
}

CSSToolsetGui::~CSSToolsetGui()
{ }

void CSSToolsetGui::activate()
{
    SAMToolsetGui::activate();
}

void CSSToolsetGui::deactivate()
{
    SAMToolsetGui::deactivate();
}

void CSSToolsetGui::createMenuItems()
{
    SAMMenu* css_menu_ptr = new SAMMenu(this, tr("&ShipSection"));
    SAMMenuCommand* css_action_ptr = new SAMMenuCommand(this, css_menu_ptr, tr("&ContainerShip"));
    css_menu_ptr->addAction(css_action_ptr);
    connect(css_action_ptr, &QAction::triggered, this, &CSSToolsetGui::showCSSUi);
}

void CSSToolsetGui::createToolboxItems()
{
    SAMToolBar* css_toolbar_ptr = new SAMToolBar(this, tr("Section"));

    QToolButton* css_toolbtn_ptr = new QToolButton(css_toolbar_ptr);
    css_toolbtn_ptr->setIcon(QIcon(":/resource/image/css_icon.png"));
    css_toolbar_ptr->addWidget(css_toolbtn_ptr);

    connect(css_toolbtn_ptr, &QToolButton::clicked, this, &CSSToolsetGui::showCSSUi);
}

void CSSToolsetGui::showCSSUi()
{
    CSSForm* css_form_ptr = new CSSForm(this);
    css_form_ptr->onCmdActivate(this, 0, nullptr);
}
