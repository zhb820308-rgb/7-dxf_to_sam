#include "CSSStringerDB.h"

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

CSSStringerDB::CSSStringerDB(CSSForm* form_ptr, CSSParameters* params_ptr):
    SAMDataDialog(form_ptr,
        tr("Create Stringers"),
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

    QVector<double> defaultDistances = { 1500, 9000 };
    int stringer_cnt = defaultDistances.size();
    if(stringer_cnt == 0)
    {
        stringersSpinBox->setValue(css_params_ptr->getStringerMinimumCnt());
        refreshInputs(css_params_ptr->getStringerMinimumCnt());
    }
    else
    {
        stringersSpinBox->setValue(stringer_cnt);
        refreshInputs(stringer_cnt);

        for(int i = 0; i < stringer_cnt; ++i)
        {
            QLineEdit* line_edit_ptr = qobject_cast<QLineEdit*>(
                stringersListWidget->itemWidget(stringersListWidget->item(i)));
            if(line_edit_ptr)
            {
                line_edit_ptr->setText(QString::number(defaultDistances.at(i)));
            }
        }
    }

    /*move(form_ptr->getPrevDialogCenter()
        - QPoint(frameGeometry().width() / 2, frameGeometry().height() / 2));*/
}

CSSStringerDB::~CSSStringerDB()
{
    // qDebug() << "CSSStringerDB::~CSSStringerDB() called";
}

void CSSStringerDB::setupUi()
{
    setMinimumSize(653, 439);

    QHBoxLayout* contentLayout = new QHBoxLayout(contentArea);

    QLabel* image_label_ptr = new QLabel(contentArea);
    image_label_ptr->setPixmap(QPixmap(":/resource/image/StringerDB.png"));
    image_label_ptr->setAlignment(Qt::AlignCenter);
    contentLayout->addWidget(image_label_ptr);

    QVBoxLayout* paramLayout = new QVBoxLayout();
    contentLayout->addLayout(paramLayout);

    QGroupBox* groupBox = new QGroupBox(tr("Parameters"), contentArea);
    groupBox->setToolTip(css_params_ptr->getStringerDistancesDesc());
    paramLayout->addWidget(groupBox);

    QVBoxLayout* listVLayout = new QVBoxLayout(groupBox);

    QLabel* stringersLabel = new QLabel(tr("Number of Stringers:"), groupBox);
    listVLayout->addWidget(stringersLabel);

    stringersSpinBox = new QSpinBox(groupBox);
    stringersSpinBox->setMinimum(css_params_ptr->getStringerMinimumCnt());
    listVLayout->addWidget(stringersSpinBox);

    QLabel* stringers_list_label_ptr = new QLabel(tr("Distances:"), groupBox);
    listVLayout->addWidget(stringers_list_label_ptr);

    stringersListWidget = new QListWidget(groupBox);
    listVLayout->addWidget(stringersListWidget);

    connect(stringersSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &CSSStringerDB::refreshInputs);
}

void CSSStringerDB::refreshInputs(int new_cnt)
{
    QStringList current_strs;
    for(int i = 0; i < stringersListWidget->count(); ++i)
    {
        QLineEdit* line_edit_ptr = qobject_cast<QLineEdit*>(
            stringersListWidget->itemWidget(stringersListWidget->item(i)));
        if(line_edit_ptr)
        {
            current_strs << line_edit_ptr->text();
        }
    }

    stringersListWidget->clear();

    for(int i = 0; i < new_cnt; ++i)
    {
        QListWidgetItem* item_ptr = new QListWidgetItem;
        stringersListWidget->addItem(item_ptr);

        QLineEdit* line_edit_ptr = new QLineEdit(stringersListWidget);
        if(i < current_strs.length())
        {
            line_edit_ptr->setText(current_strs.at(i));
        }

        stringersListWidget->setItemWidget(item_ptr, line_edit_ptr);
    }
}

void CSSStringerDB::onActionButtonClicked(int click_id)
{
    // qDebug() << "CSSStringerDB::onActionButtonClicked() called with click_id: " << click_id;
    if(click_id == ID_CLICKED_DISMISS)
    {
        // qDebug() << "CSSStringerDB::onActionButtonClicked(): ID_CLICKED_DISMISS";

        QString cmd_target = dynamic_cast<const CSSForm*>(getMode())->getCmdPrefix();
        cmdGCommandDeliveryRole::Instance().SendCommand(cmd_target + ".rollbackDraft(3)");

        dynamic_cast<CSSForm*>(getMode())->rollbackMode();
    }
    else
    {
        SAMDataDialog::onActionButtonClicked(click_id); // call onCmd*()
    }

}

void CSSStringerDB::onCmdContinue(int click_id)
{
    QString invalid_style_sheet = "QLineEdit { background: #FFCCCC; }";

    QVector<double> lw_values;
    bool is_all_valid = true;
    for(int i = 0; i < stringersListWidget->count(); ++i)
    {
        QLineEdit* line_edit_ptr = qobject_cast<QLineEdit*>(
            stringersListWidget->itemWidget(stringersListWidget->item(i)));
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
            tr("Please enter double values for Stringer Distances."));

        return;
    }

    QVector<bool> valid_flags = css_params_ptr->setStringerDistances(lw_values);
    for(int i = 0; i < valid_flags.size(); ++i)
    {
        QLineEdit* line_edit_ptr = qobject_cast<QLineEdit*>(
            stringersListWidget->itemWidget(stringersListWidget->item(i)));
        if(line_edit_ptr && !valid_flags.at(i))
        {
            line_edit_ptr->setStyleSheet(invalid_style_sheet);
            is_all_valid = false;
        }
    }
    if(!is_all_valid)
    {
        QMessageBox::information(this,
            tr("Invalid Input"), css_params_ptr->getStringerDistancesDesc());
        return;
    }

    QString cmd_target = dynamic_cast<const CSSForm*>(getMode())->getCmdPrefix();

    cowListDouble value_list = convertListDouble(lw_values);
    omuArguments args;
    args.Put(value_list, "stringer_distances");
    cmdGCommandDeliveryRole::Instance().SendCommand(omuMethodCall(cmd_target, "draftStringer", args));

    SAMDataDialog::onCmdContinue(click_id); // call getMode()->continueMode()
}

void CSSStringerDB::onCmdCancel(int click_id)
{
    SAMDataDialog::onCmdCancel(click_id); // call getMode()->cancel(QObject*, const char*)
}
