#include "CSSGriderDB.h"

#include "CSSForm.h"
#include "CSSParameters.h"
#include "FormCalcUtils.h"

#include <cowListDouble.h>
#include <sesGSessionState.h>
#include <omuArguments.h>
#include <cmdGCommandDeliveryRole.h>
#include <omuMethodCall.h>
#include <QAbstractButton>
#include <QButtonGroup>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QList>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QPixmap>
#include <QSpinBox>
#include <QString>
#include <QStringList>
#include <QVBoxLayout>
#include <QVector>
#include <QTooltip>

#include <QDebug>

CSSGriderDB::CSSGriderDB(CSSForm* form_ptr, CSSParameters* params_ptr):
    SAMDataDialog(form_ptr,
        tr("Create Griders"),
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

	QVector<double> defaultDistances = { 0, 1800, 4800, 8000 };
    int grider_cnt = defaultDistances.size();
    if(grider_cnt == 0)
    {
        griderSpinBox->setValue(cssParams->getGriderMinimumCnt());
        refreshInputs(cssParams->getGriderMinimumCnt());
    }
    else
    {
        griderSpinBox->setValue(grider_cnt);
        refreshInputs(grider_cnt);

        for(int i = 0; i < grider_cnt; ++i)
        {
            QLineEdit* line_edit_ptr = qobject_cast<QLineEdit*>(
                griderListWidget->itemWidget(griderListWidget->item(i)));
            if(line_edit_ptr)
            {
                line_edit_ptr->setText(QString::number(defaultDistances.at(i)));
            }
        }
    }

    /*move(form_ptr->getPrevDialogCenter()
        - QPoint(frameGeometry().width() / 2, frameGeometry().height() / 2));*/
}

CSSGriderDB::~CSSGriderDB()
{
    // qDebug() << "CSSGriderDB::~CSSGriderDB() called";
}

void CSSGriderDB::setupUi()
{
    setMinimumSize(653, 439);

	QHBoxLayout* contentLayout = new QHBoxLayout(contentArea);

	QLabel* imageLabel = new QLabel(contentArea);
	imageLabel->setPixmap(QPixmap(":/resource/image/GriderDB.png"));
	imageLabel->setAlignment(Qt::AlignCenter);
	contentLayout->addWidget(imageLabel);

	QVBoxLayout* paramLayout = new QVBoxLayout();
	contentLayout->addLayout(paramLayout);

	QGroupBox* paramGroupBox = new QGroupBox(tr("Parameters"), contentArea);
	paramGroupBox->setToolTip(cssParams->getGriderDistancesDesc());
	paramLayout->addWidget(paramGroupBox);

	QVBoxLayout* listLayout = new QVBoxLayout(paramGroupBox);

	QLabel* griderCountLabel = new QLabel(tr("Number of Griders:"), paramGroupBox);
	listLayout->addWidget(griderCountLabel);

	griderSpinBox = new QSpinBox(paramGroupBox);
	griderSpinBox->setMinimum(cssParams->getGriderMinimumCnt());
	listLayout->addWidget(griderSpinBox);

	QLabel* distancesLabel = new QLabel(tr("Distances:"), paramGroupBox);
	listLayout->addWidget(distancesLabel);

	griderListWidget = new QListWidget(paramGroupBox);
	listLayout->addWidget(griderListWidget);

	connect(griderSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
		this, &CSSGriderDB::refreshInputs);
}

void CSSGriderDB::refreshInputs(int new_cnt)
{
    QStringList current_strs;
    for(int i = 0; i < griderListWidget->count(); ++i)
    {
        QLineEdit* line_edit_ptr
            = qobject_cast<QLineEdit*>(griderListWidget->itemWidget(griderListWidget->item(i)));
        if(line_edit_ptr)
        {
            current_strs << line_edit_ptr->text();
        }
    }

    griderListWidget->clear();

    for(int i = 0; i < new_cnt; ++i)
    {
        QListWidgetItem* item_ptr = new QListWidgetItem;
        griderListWidget->addItem(item_ptr);

        QLineEdit* line_edit_ptr = new QLineEdit(griderListWidget);
        if(i < current_strs.length())
        {
            line_edit_ptr->setText(current_strs.at(i));
        }

        griderListWidget->setItemWidget(item_ptr, line_edit_ptr);
    }
}

void CSSGriderDB::onActionButtonClicked(int click_id)
{
    if(click_id == ID_CLICKED_DISMISS)
    {
        QString cmd_target = dynamic_cast<const CSSForm*>(getMode())->getCmdPrefix();
        cmdGCommandDeliveryRole::Instance().SendCommand(cmd_target + ".rollbackDraft(2)");

        dynamic_cast<CSSForm*>(getMode())->rollbackMode();
    }
    else
    {
        SAMDataDialog::onActionButtonClicked(click_id); // call onCmd*()
    }
}

void CSSGriderDB::onCmdContinue(int click_id)
{
    QString invalid_style_sheet = "QLineEdit { background: #FFCCCC; }";

    QVector<double> lw_values;
    bool is_all_valid = true;
    for(int i = 0; i < griderListWidget->count(); ++i)
    {
        QLineEdit* line_edit_ptr = qobject_cast<QLineEdit*>(
            griderListWidget->itemWidget(griderListWidget->item(i)));
        if(line_edit_ptr)
        {
            bool is_value_valid = false;
            double distance = line_edit_ptr->text().toDouble(&is_value_valid);
            if(!is_value_valid)
            {
                line_edit_ptr->setStyleSheet(invalid_style_sheet);
                is_all_valid = false;
            }
            else
            {
                line_edit_ptr->setStyleSheet("");
                lw_values.append(distance);
            }
        }
    }
    if(!is_all_valid)
    {
        QMessageBox::information(this,
            tr("Invalid Input"),
            tr("Please enter double values for Grider Distances."));
        return;
    }

    QVector<bool> valid_flags = cssParams->setGriderDistances(lw_values);
    for(int i = 0; i < valid_flags.size(); ++i)
    {
        QLineEdit* line_edit_ptr = qobject_cast<QLineEdit*>(
            griderListWidget->itemWidget(griderListWidget->item(i)));
        if(line_edit_ptr && !valid_flags.at(i))
        {
            line_edit_ptr->setStyleSheet(invalid_style_sheet);
            is_all_valid = false;
        }
    }
    if(!is_all_valid)
    {
        QMessageBox::information(this,
            tr("Invalid Input"), cssParams->getGriderDistancesDesc());
        return;
    }

    QString cmd_target = dynamic_cast<const CSSForm*>(getMode())->getCmdPrefix();

    cowListDouble value_list = convertListDouble(lw_values);
    omuArguments args;
    args.Put(value_list, "grider_distances");
    cmdGCommandDeliveryRole::Instance().SendCommand(omuMethodCall(cmd_target, "draftGrider", args));

    SAMDataDialog::onCmdContinue(click_id); // call getMode()->continueMode()
}

void CSSGriderDB::onCmdCancel(int click_id)
{
    SAMDataDialog::onCmdCancel(click_id); // call getMode()->cancel(QObject*, const char*)
}
