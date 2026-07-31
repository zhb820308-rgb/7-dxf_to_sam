#include "CSSForm.h"

#include "CSSPreprocessDB.h"
#include "CSSOuterHullDB.h"
#include "CSSCamberDB.h"
#include "CSSInnerHullDB.h"
#include "CSSGriderDB.h"
#include "CSSStringerDB.h"
#include "CSSHatchCoamingDB.h"
#include "CSSPostprocessDB.h"
#include "CSSParameters.h"

#include <sesGSessionState.h>
#include <cmdCWIP.h>
#include <cmdGCommandDeliveryRole.h>
#include <omuArguments.h>
#include <omuMethodCall.h>
#include <QMessageBox>
#include <QDebug>

CSSForm::CSSForm(SAMGuiObjectManager* owner_ptr): SAMForm(owner_ptr)
{
    css_params_ptr = new CSSParameters(this);
    // setModal(true);
}

CSSForm::~CSSForm()
{
    // qDebug() << "CSSForm::~CSSForm() called";
}

QString CSSForm::getCmdPrefix() const
{
    return cmd_target;
}

QPoint CSSForm::getPrevDialogCenter() const
{
    return prev_dialog_center;
}

void CSSForm::setDialogCenter()
{
    prev_dialog_center.setX(getCurrentDialog()->x() + getCurrentDialog()->frameGeometry().width() / 2);
    prev_dialog_center.setY(getCurrentDialog()->y() + getCurrentDialog()->frameGeometry().height() / 2);
}

void CSSForm::activate()
{
    // qDebug() << "CSSForm::activate() called";
    // TODO: 第一个对话框前处理工作

    const sesGVpContext& context = sesGSessionState::Instance()->ConstGetVpContext();
    cmd_target = "mdb.models['" + context.ModelName() + "'].parts['" + context.PartName() + "']";

    cmdGCommandDeliveryRole::Instance().SendCommand("import ContainerShipSection");

    SAMForm::activate(); // call getFirstDialog()

    // TODO: 第一个对话框后处理工作
    // qDebug() << "CSSForm::activate() completed.";
}

int CSSForm::rollbackMode()
{
    // qDebug() << "CSSForm::rollbackMode() called";
    SAMDialog* current_dialog_ptr = getCurrentDialog();
    if(!current_dialog_ptr)
    {
        return 0;
    }
    setDialogCenter();
    SAMDialog* previos_dialog_ptr = getPreviousDialog(current_dialog_ptr);
    if(!previos_dialog_ptr)
    {
        return 0;
    }
    current_dialog_ptr->hide();
    currentDb = previos_dialog_ptr;
    previos_dialog_ptr->show();
    // qDebug() << "CSSForm::rollbackMode() completed.";
    return 1;
}

int CSSForm::continueMode()
{
    // qDebug() << "CSSForm::continueMode() called";
    // TODO: 下一个对话框前处理工作
    setDialogCenter();
    int result = SAMForm::continueMode(); // call getNextDialog()
    // TODO: 下一个对话框后处理工作
    // qDebug() << "CSSForm::continueMode() completed.";
    return result;
}

int CSSForm::commit()
{
    // qDebug() << "CSSForm::commit() called";
    // TODO: 提交前处理工作
    int result = SAMForm::commit();
    // TODO: 提交后处理工作
    // qDebug() << "CSSForm::commit(): completed.";
    return result;
}

void CSSForm::cancel(QObject* target_ptr, const char* message_ptr)
{
    // qDebug() << "CSSForm::cancel() called";
    // TODO: 取消前目标消息处理工作

    cmdGCommandDeliveryRole::Instance().SendCommand(cmd_target + ".clearDraft()");

    SAMForm::cancel(target_ptr, message_ptr);
    // TODO: 取消后目标消息处理工作
    // qDebug() << "CSSForm::cancel() completed.";
}

int CSSForm::onCmdActivate(QObject* qobject_ptr, uint uint_val, void* void_ptr)
{
    // qDebug() << "CSSForm::onCmdActivate() called";
    // TODO: 模式激活前处理工作

    const sesGVpContext& context = sesGSessionState::Instance()->ConstGetVpContext();
    QString model_name = context.ModelName();
    if(model_name.isEmpty())
    {
        QMessageBox::information(nullptr,
            tr("Error"),
            tr("Please create a model first!"));
        return 0;
    }
    QString part_name = context.PartName();
    if(part_name.isEmpty())
    {
        QMessageBox::information(nullptr,
            tr("Error"),
            tr("Please create a part first!"));
        return 0;
    }

    return SAMForm::onCmdActivate(qobject_ptr, uint_val, void_ptr); // call activate()
}

SAMDialog* CSSForm::getFirstDialog()
{
	const sesGVpContext& ctx = sesGSessionState::Instance()->ConstGetVpContext();
	QString currentPartName = ctx.PartName();
	if (currentPartName.isEmpty())
	{
		QMessageBox::information(new CSSPreprocessDB(this, css_params_ptr), tr("Error"), tr("Please create a part first!"));
		return 0;
	}
	else
		return new CSSPreprocessDB(this, css_params_ptr);
}

SAMDialog* CSSForm::getPreviousDialog(SAMDialog* current_dialog_ptr)
{
    // qDebug() << "CSSForm::getPreviousDialog() called";
    if(dynamic_cast<CSSPostprocessDB*>(current_dialog_ptr))
    {
        return new CSSHatchCoamingDB(this, css_params_ptr);
    }
    else if(dynamic_cast<CSSHatchCoamingDB*>(current_dialog_ptr))
    {
        return new CSSStringerDB(this, css_params_ptr);
    }
    else if(dynamic_cast<CSSStringerDB*>(current_dialog_ptr))
    {
        return new CSSGriderDB(this, css_params_ptr);
    }
    else if(dynamic_cast<CSSGriderDB*>(current_dialog_ptr))
    {
        return new CSSInnerHullDB(this, css_params_ptr);
    }
    else if(dynamic_cast<CSSInnerHullDB*>(current_dialog_ptr))
    {
        return new CSSCamberDB(this, css_params_ptr);
    }
    else if(dynamic_cast<CSSCamberDB*>(current_dialog_ptr))
    {
        return new CSSOuterHullDB(this, css_params_ptr);
    }
    else if(dynamic_cast<CSSOuterHullDB*>(current_dialog_ptr))
    {
        return new CSSPreprocessDB(this, css_params_ptr);
    }
    return nullptr;
}

SAMDialog* CSSForm::getNextDialog(SAMDialog* current_dialog_ptr)
{
    // qDebug() << "CSSForm::getNextDialog() called";
    // SAMForm::getNextDialog(previous_dialog_ptr); // maybe useless
    if(dynamic_cast<CSSPreprocessDB*>(current_dialog_ptr))
    {
        return new CSSOuterHullDB(this, css_params_ptr);
    }
    else if(dynamic_cast<CSSOuterHullDB*>(current_dialog_ptr))
    {
        return new CSSCamberDB(this, css_params_ptr);
    }
    else if(dynamic_cast<CSSCamberDB*>(current_dialog_ptr))
    {
        return new CSSInnerHullDB(this, css_params_ptr);
    }
    else if(dynamic_cast<CSSInnerHullDB*>(current_dialog_ptr))
    {
        return new CSSGriderDB(this, css_params_ptr);
    }
    else if(dynamic_cast<CSSGriderDB*>(current_dialog_ptr))
    {
        return new CSSStringerDB(this, css_params_ptr);
    }
    else if(dynamic_cast<CSSStringerDB*>(current_dialog_ptr))
    {
        return new CSSHatchCoamingDB(this, css_params_ptr);
    }
    else if(dynamic_cast<CSSHatchCoamingDB*>(current_dialog_ptr))
    {
        return new CSSPostprocessDB(this, css_params_ptr);
    }
    return nullptr;
}
