#include "CSSInnerHullDB.h"

#include "CSSForm.h"
#include "CSSParameters.h"

#include <sesGSessionState.h>
#include <omuArguments.h>
#include <cmdGCommandDeliveryRole.h>
#include <omuMethodCall.h>
#include <QAbstractButton>
#include <QButtonGroup>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QList>
#include <QMessageBox>
#include <QPixmap>
#include <QString>
#include <QVBoxLayout>
#include <QTooltip>

#include <QDebug>

CSSInnerHullDB::CSSInnerHullDB(CSSForm* form_ptr, CSSParameters* params_ptr):
    SAMDataDialog(form_ptr,
        tr("Create Inner Hull"),
        ButtonID::DISMISS | ButtonID::CONTINUE | ButtonID::CANCEL)
{
    css_params_ptr = params_ptr;

    setupUi();

    QList<QAbstractButton*> buttons = btnGroup->buttons();
    for(QAbstractButton* btn: buttons)
    {
        if(btn->text() == tr("Dismiss"))
        {
            btn->setText(tr("Rollback..."));
            break;
        }
    }

    double inner_hull_height = css_params_ptr->getInnerHullHeight();
    if(inner_hull_height != -1)
    {
        innerHullHeightEdit->setText(QString::number(inner_hull_height));
    }
    double inner_hull_distance = css_params_ptr->getInnerHullDistance();
    if(inner_hull_distance != -1)
    {
        innerHullDistanceEdit->setText(QString::number(inner_hull_distance));
    }

    /*move(form_ptr->getPrevDialogCenter()
        - QPoint(frameGeometry().width() / 2, frameGeometry().height() / 2));*/
}

CSSInnerHullDB::~CSSInnerHullDB()
{
    // qDebug() << "CSSInnerHullDB::~CSSInnerHullDB() called";
}

void CSSInnerHullDB::setupUi()
{
    setMinimumSize(851, 434);

    QHBoxLayout* content_layout_ptr = new QHBoxLayout(contentArea);

    QLabel* image_label_ptr = new QLabel(contentArea);
    image_label_ptr->setPixmap(QPixmap(":/resource/image/InnerHullDB.png"));
    image_label_ptr->setAlignment(Qt::AlignCenter);
    content_layout_ptr->addWidget(image_label_ptr);

    QVBoxLayout* param_layout_ptr = new QVBoxLayout();
    content_layout_ptr->addLayout(param_layout_ptr);

    QGroupBox* group_box_ptr = new QGroupBox(tr("Parameters"), contentArea);
    param_layout_ptr->addWidget(group_box_ptr);

    QFormLayout* form_layout_ptr = new QFormLayout(group_box_ptr);

    innerHullHeightEdit = new QLineEdit(group_box_ptr);
	innerHullHeightEdit->setText("1500");
    innerHullHeightEdit->setToolTip(css_params_ptr->getInnerHullHeightDesc());
    form_layout_ptr->addRow(tr("Inner Hull Height (mm):"), innerHullHeightEdit);

    innerHullDistanceEdit = new QLineEdit(group_box_ptr);
	innerHullDistanceEdit->setText("8000");
    innerHullDistanceEdit->setToolTip(css_params_ptr->getInnerHullDistanceDesc());
    form_layout_ptr->addRow(tr("Inner Hull Distance (mm):"), innerHullDistanceEdit);
}

void CSSInnerHullDB::onActionButtonClicked(int click_id)
{
    // qDebug() << "CSSInnerHullDB::onActionButtonClicked() called with click_id: " << click_id;
    if(click_id == ID_CLICKED_DISMISS)
    {
        // qDebug() << "CSSInnerHullDB::onActionButtonClicked(): ID_CLICKED_DISMISS";

        QString cmd_target = dynamic_cast<const CSSForm*>(getMode())->getCmdPrefix();
        cmdGCommandDeliveryRole::Instance().SendCommand(cmd_target + ".rollbackDraft(1)");

        dynamic_cast<CSSForm*>(getMode())->rollbackMode();
    }
    else
    {
        // qDebug() << "CSSInnerHullDB::onActionButtonClicked(): ELSE";
        SAMDataDialog::onActionButtonClicked(click_id); // call onCmd*()
    }
    // qDebug() << "CSSInnerHullDB::onActionButtonClicked() completed.";
}

void CSSInnerHullDB::onCmdContinue(int click_id)
{
    // qDebug() << "CSSInnerHullDB::onCmdContinue() called with click_id: " << click_id;

    QString invalid_style_sheet = "QLineEdit { background: #FFCCCC; }";

    bool is_param_valid = false, is_all_valid = true;
    double inner_hull_height = innerHullHeightEdit->text().toDouble(&is_param_valid);
    if(!is_param_valid || !css_params_ptr->setInnerHullHeight(inner_hull_height))
    {
        innerHullHeightEdit->setStyleSheet(invalid_style_sheet);
        is_all_valid = false;
    }
    else
    {
        innerHullHeightEdit->setStyleSheet("");
    }
    double inner_hull_distance = innerHullDistanceEdit->text().toDouble(&is_param_valid);
    if(!is_param_valid || !css_params_ptr->setInnerHullDistance(inner_hull_distance))
    {
        innerHullDistanceEdit->setStyleSheet(invalid_style_sheet);
        is_all_valid = false;
    }
    else
    {
        innerHullDistanceEdit->setStyleSheet("");
    }
    if(!is_all_valid)
    {
        QString error_message = QString("- %1\n- %2")
            .arg(css_params_ptr->getInnerHullHeightDesc())
            .arg(css_params_ptr->getInnerHullDistanceDesc());
        QMessageBox::information(this, tr("Invalid Input"), error_message);
        return;
    }

    // qDebug() << "Inner Hull Height: " << inner_hull_height
    //          << " , Inner Hull Distance: " << inner_hull_distance;
    // qDebug() << "size: " << size();

    QString cmd_target = dynamic_cast<const CSSForm*>(getMode())->getCmdPrefix();

    omuArguments args;
    args.Put(inner_hull_height, "inner_hull_height");
    args.Put(inner_hull_distance, "inner_hull_distance");
    cmdGCommandDeliveryRole::Instance().SendCommand(omuMethodCall(cmd_target, "draftInnerHull", args));

    SAMDataDialog::onCmdContinue(click_id); // call getMode()->continueMode()
    // qDebug() << "CSSInnerHullDB::onCmdContinue() completed.";
}

void CSSInnerHullDB::onCmdCancel(int click_id)
{
    // qDebug() << "CSSInnerHullDB::onCmdCancel() called with click_id: " << click_id;
    SAMDataDialog::onCmdCancel(click_id); // call getMode()->cancel(QObject*, const char*)
    // qDebug() << "CSSInnerHullDB::onCmdCancel() completed.";
}
