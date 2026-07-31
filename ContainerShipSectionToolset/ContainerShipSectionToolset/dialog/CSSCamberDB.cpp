#include "CSSCamberDB.h"

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

CSSCamberDB::CSSCamberDB(CSSForm* form_ptr, CSSParameters* params_ptr):
    SAMDataDialog(form_ptr,
        tr("Create Camber Without Knuckle"),
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

    double camber_height = css_params_ptr->getCamberHeight();
    if(camber_height != -1)
    {
        camber_height_le_ptr->setText(QString::number(camber_height));
    }
    double deck_distance = css_params_ptr->getDeckDistance();
    if(deck_distance != -1)
    {
        deck_distance_le_ptr->setText(QString::number(deck_distance));
    }

   /* move(form_ptr->getPrevDialogCenter()
        - QPoint(frameGeometry().width() / 2, frameGeometry().height() / 2));*/
}

CSSCamberDB::~CSSCamberDB()
{
   
}

void CSSCamberDB::setupUi()
{
    setMinimumSize(1054, 435);

    QHBoxLayout* content_layout_ptr = new QHBoxLayout(contentArea);

    QLabel* image_label_ptr = new QLabel(contentArea);
    image_label_ptr->setPixmap(QPixmap(":/resource/image/CamberDB.png"));
    image_label_ptr->setAlignment(Qt::AlignCenter);
    content_layout_ptr->addWidget(image_label_ptr);

    QVBoxLayout* param_layout_ptr = new QVBoxLayout();
    content_layout_ptr->addLayout(param_layout_ptr);

    QGroupBox* group_box_ptr = new QGroupBox(tr("Parameters"), contentArea);
    param_layout_ptr->addWidget(group_box_ptr);

    QFormLayout* form_layout_ptr = new QFormLayout(group_box_ptr);

    camber_height_le_ptr = new QLineEdit(group_box_ptr);
	camber_height_le_ptr->setText("0");
	camber_height_le_ptr->setEnabled(false);
    camber_height_le_ptr->setToolTip(css_params_ptr->getCamberHeightDesc());
    form_layout_ptr->addRow(tr("Camber Height (mm):"), camber_height_le_ptr);

    deck_distance_le_ptr = new QLineEdit(group_box_ptr);
	deck_distance_le_ptr->setText("9900");
    deck_distance_le_ptr->setToolTip(css_params_ptr->getDeckDistanceDesc());
    form_layout_ptr->addRow(tr("Distance from CL to Deck Start Position (mm):"), deck_distance_le_ptr);
}

void CSSCamberDB::onActionButtonClicked(int click_id)
{
    if(click_id == ID_CLICKED_DISMISS)
    {
        QString cmd_target = dynamic_cast<const CSSForm*>(getMode())->getCmdPrefix();
        cmdGCommandDeliveryRole::Instance().SendCommand(cmd_target + ".rollbackDraft(0)");

        dynamic_cast<CSSForm*>(getMode())->rollbackMode();
    }
    else
    {
        SAMDataDialog::onActionButtonClicked(click_id); // call onCmd*()
    }
    
}

void CSSCamberDB::onCmdContinue(int click_id)
{
   
    QString invalid_style_sheet = "QLineEdit { background: #FFCCCC; }";

    bool is_param_valid = false, is_all_valid = true;
    double camber_height = camber_height_le_ptr->text().toDouble(&is_param_valid);
    if(!is_param_valid || !css_params_ptr->setCamberHeight(camber_height))
    {
        camber_height_le_ptr->setStyleSheet(invalid_style_sheet);
        is_all_valid = false;
    }
    else
    {
        camber_height_le_ptr->setStyleSheet("");
    }
    double deck_distance = deck_distance_le_ptr->text().toDouble(&is_param_valid);
    if(!is_param_valid || !css_params_ptr->setDeckDistance(deck_distance))
    {
        deck_distance_le_ptr->setStyleSheet(invalid_style_sheet);
        is_all_valid = false;
    }
    else
    {
        deck_distance_le_ptr->setStyleSheet("");
    }
    if(!is_all_valid)
    {
        QString error_message = QString("- %1\n- %2")
            .arg(css_params_ptr->getCamberHeightDesc())
            .arg(css_params_ptr->getDeckDistanceDesc());
        QMessageBox::information(this, tr("Invalid Input"), error_message);
        return;
    }

    QString cmd_target = dynamic_cast<const CSSForm*>(getMode())->getCmdPrefix();

    omuArguments args;
    args.Put(camber_height, "camber_height");
    args.Put(deck_distance, "deck_distance");
    cmdGCommandDeliveryRole::Instance().SendCommand(omuMethodCall(cmd_target, "draftCamber", args));

    SAMDataDialog::onCmdContinue(click_id); // call getMode()->continueMode()
 
}

void CSSCamberDB::onCmdCancel(int click_id)
{
    SAMDataDialog::onCmdCancel(click_id); // call getMode()->cancel(QObject*, const char*)
  
}
