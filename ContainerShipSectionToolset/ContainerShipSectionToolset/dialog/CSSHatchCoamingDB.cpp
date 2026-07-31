#include "CSSHatchCoamingDB.h"

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

CSSHatchCoamingDB::CSSHatchCoamingDB(CSSForm* form_ptr, CSSParameters* params_ptr):
    SAMDataDialog(form_ptr,
        tr("Create Hatch Coaming"),
        ButtonID::DISMISS | ButtonID::CONTINUE | ButtonID::CANCEL)
{
	cssParams = params_ptr;

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

    double hc_height = cssParams->getHatchCoamingHeight();
    if(hc_height != -1)
    {
		hcHeightLineEdit->setText(QString::number(hc_height));
    }
    double hc_width = cssParams->getHatchCoamingWidth();
    if(hc_width != -1)
    {
		hcWidthLineEdit->setText(QString::number(hc_width));
    }

    /*move(form_ptr->getPrevDialogCenter()
        - QPoint(frameGeometry().width() / 2, frameGeometry().height() / 2));*/
}

CSSHatchCoamingDB::~CSSHatchCoamingDB()
{
    // qDebug() << "CSSHatchCoamingDB::~CSSHatchCoamingDB() called";
}

void CSSHatchCoamingDB::setupUi()
{
	setMinimumSize(1063, 476);

	QHBoxLayout* contentLayout = new QHBoxLayout(contentArea);

	QLabel* imageLabel = new QLabel(contentArea);
	imageLabel->setPixmap(QPixmap(":/resource/image/HatchCoamingDB.png"));
	imageLabel->setAlignment(Qt::AlignCenter);
	contentLayout->addWidget(imageLabel);

	QVBoxLayout* paramLayout = new QVBoxLayout();
	contentLayout->addLayout(paramLayout);

	QGroupBox* paramGroupBox = new QGroupBox(tr("Parameters"), contentArea);
	paramLayout->addWidget(paramGroupBox);

	QFormLayout* formLayout = new QFormLayout(paramGroupBox);

	hcHeightLineEdit = new QLineEdit(paramGroupBox);
	hcHeightLineEdit->setText("700");
	hcHeightLineEdit->setToolTip(cssParams->getHatchCoamingHeightDesc());
	formLayout->addRow(tr("Hatch Coaming Height:"), hcHeightLineEdit);

	hcWidthLineEdit = new QLineEdit(paramGroupBox);
	hcWidthLineEdit->setText("450");
	hcWidthLineEdit->setToolTip(cssParams->getHatchCoamingWidthDesc());
	formLayout->addRow(tr("Hatch Coaming Width:"), hcWidthLineEdit);

}

void CSSHatchCoamingDB::onActionButtonClicked(int click_id)
{
    // qDebug() << "CSSHatchCoamingDB::onActionButtonClicked() called with click_id: " << click_id;
    if(click_id == ID_CLICKED_DISMISS)
    {
        // qDebug() << "CSSHatchCoamingDB::onActionButtonClicked(): ID_CLICKED_DISMISS";

        QString cmd_target = dynamic_cast<const CSSForm*>(getMode())->getCmdPrefix();
        cmdGCommandDeliveryRole::Instance().SendCommand(cmd_target + ".rollbackDraft(4)");

        dynamic_cast<CSSForm*>(getMode())->rollbackMode();
    }
    else
    {
        // qDebug() << "CSSHatchCoamingDB::onActionButtonClicked(): ELSE";
        SAMDataDialog::onActionButtonClicked(click_id); // call onCmd*()
    }
    // qDebug() << "CSSHatchCoamingDB::onActionButtonClicked() completed.";
}

void CSSHatchCoamingDB::onCmdContinue(int click_id)
{
    // qDebug() << "CSSHatchCoamingDB::onCmdContinue() called with click_id: " << click_id;

    QString invalid_style_sheet = "QLineEdit { background: #FFCCCC; }";

    bool is_param_valid = false, is_all_valid = true;
    double hc_height = hcHeightLineEdit->text().toDouble(&is_param_valid);
    if(!is_param_valid || !cssParams->setHatchCoamingHeight(hc_height))
    {
		hcHeightLineEdit->setStyleSheet(invalid_style_sheet);
        is_all_valid = false;
    }
    else
    {
		hcHeightLineEdit->setStyleSheet("");
    }
    double hc_width = hcWidthLineEdit->text().toDouble(&is_param_valid);
    if(!is_param_valid || !cssParams->setHatchCoamingWidth(hc_width))
    {
		hcWidthLineEdit->setStyleSheet(invalid_style_sheet);
        is_all_valid = false;
    }
    else
    {
		hcWidthLineEdit->setStyleSheet("");
    }

    if(!is_all_valid)
    {
        QString error_message = QString("- %1\n- %2")
            .arg(cssParams->getHatchCoamingHeightDesc())
            .arg(cssParams->getHatchCoamingWidthDesc());
        QMessageBox::information(this, tr("Invalid Input"), error_message);
        return;
    }

    // qDebug() << "Hatch Coaming Height: " << hc_height << " , Hatch Coaming Width: " << hc_width;
    // qDebug() << "size: " << size();

    QString cmd_target = dynamic_cast<const CSSForm*>(getMode())->getCmdPrefix();

    omuArguments args;
    args.Put(hc_height, "hatch_coaming_height");
    args.Put(hc_width, "hatch_coaming_width");
    cmdGCommandDeliveryRole::Instance().SendCommand(omuMethodCall(cmd_target, "draftHatchCoaming", args));

    omuArguments fit_args;
    fit_args.Put("hatch_coaming", "step");
    cmdGCommandDeliveryRole::Instance().SendCommand(omuMethodCall(cmd_target, "fitDraftView", fit_args));

    SAMDataDialog::onCmdContinue(click_id); // call getMode()->continueMode()
  
}

void CSSHatchCoamingDB::onCmdCancel(int click_id)
{
  
    SAMDataDialog::onCmdCancel(click_id); // call getMode()->cancel(QObject*, const char*)
   
}
