#include "CSSOuterHullDB.h"

#include "CSSForm.h"
#include "CSSParameters.h"

#include <sesGSessionState.h>
#include <omuArguments.h>
#include <cmdGCommandDeliveryRole.h>
#include <omuMethodCall.h>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPixmap>
#include <QString>
#include <QVBoxLayout>
#include <QAbstractButton>
#include <QButtonGroup>
#include <QList>
#include <QTooltip>

#include <QDebug>

CSSOuterHullDB::CSSOuterHullDB(CSSForm* form_ptr, CSSParameters* params_ptr):
    SAMDataDialog(form_ptr,
        tr("Create Outer Hull"),
        ButtonID::DISMISS | ButtonID::CONTINUE | ButtonID::CANCEL)
{
    css_params_ptr = params_ptr;

    setupUi();

    QList<QAbstractButton*> buttons = btnGroup->buttons();
    for(QAbstractButton* btn: buttons)
    {
        if(btn->text() == tr("Dismiss"))
        {
            btn->setText(tr("Back..."));
            break;
        }
    }

    double half_breadth = css_params_ptr->getHalfBreadth();
    if(half_breadth != -1)
    {
        half_breadth_le_ptr->setText(QString::number(half_breadth));
    }
    double depth = css_params_ptr->getDepth();
    if(depth != -1)
    {
        depth_le_ptr->setText(QString::number(depth));
    }
    double bilge_radius = css_params_ptr->getBilgeRadius();
    if(bilge_radius != -1)
    {
        bilge_radius_le_ptr->setText(QString::number(bilge_radius));
    }
    double deck_corner_radius = css_params_ptr->getDeckCornerRadius();
    if(deck_corner_radius != -1)
    {
        deck_corner_radius_le_ptr->setText(QString::number(deck_corner_radius));
    }

   /* move(form_ptr->getPrevDialogCenter()
        - QPoint(frameGeometry().width() / 2, frameGeometry().height() / 2));*/
}

CSSOuterHullDB::~CSSOuterHullDB()
{
    // qDebug() << "CSSOuterHullDB::~CSSOuterHullDB() called";
}

void CSSOuterHullDB::setupUi()
{
    setMinimumSize(838, 427);

    QHBoxLayout* content_layout_ptr = new QHBoxLayout(contentArea);

    QLabel* image_label_ptr = new QLabel(contentArea);
    image_label_ptr->setPixmap(QPixmap(":/resource/image/OuterHullDB.png"));
    image_label_ptr->setAlignment(Qt::AlignCenter);
    content_layout_ptr->addWidget(image_label_ptr);

    QVBoxLayout* param_layout_ptr = new QVBoxLayout();
    content_layout_ptr->addLayout(param_layout_ptr);

    QGroupBox* group_box_ptr = new QGroupBox(tr("Parameters"), contentArea);
    param_layout_ptr->addWidget(group_box_ptr);

    QFormLayout* form_layout_ptr = new QFormLayout(group_box_ptr);

    half_breadth_le_ptr = new QLineEdit(group_box_ptr);
	half_breadth_le_ptr->setText("9900");
    half_breadth_le_ptr->setToolTip(css_params_ptr->getHalfBreadthDesc());
    form_layout_ptr->addRow(tr("1/2 Breadth (mm):"), half_breadth_le_ptr);

    depth_le_ptr = new QLineEdit(group_box_ptr);
	depth_le_ptr->setText("9000");
    depth_le_ptr->setToolTip(css_params_ptr->getDepthDesc());
    form_layout_ptr->addRow(tr("Depth (mm):"), depth_le_ptr);

    bilge_radius_le_ptr = new QLineEdit(group_box_ptr);
	bilge_radius_le_ptr->setText("1000");
    bilge_radius_le_ptr->setToolTip(css_params_ptr->getBilgeRadiusDesc());
    form_layout_ptr->addRow(tr("Bilge Radius (mm):"), bilge_radius_le_ptr);

    deck_corner_radius_le_ptr = new QLineEdit(group_box_ptr);
	deck_corner_radius_le_ptr->setText("0");
	deck_corner_radius_le_ptr->setEnabled(false);
    deck_corner_radius_le_ptr->setToolTip(css_params_ptr->getDeckCornerRadiusDesc());
    form_layout_ptr->addRow(tr("Deck Corner Radius (mm):"), deck_corner_radius_le_ptr);
}

void CSSOuterHullDB::onActionButtonClicked(int click_id)
{
    // qDebug() << "CSSOuterHullDB::onActionButtonClicked() called with click_id: " << click_id;
    if(click_id == ID_CLICKED_DISMISS)
    {
        // qDebug() << "CSSCamberDB::onActionButtonClicked(): ID_CLICKED_DISMISS";
        dynamic_cast<CSSForm*>(getMode())->rollbackMode();
    }
    else
    {
        // qDebug() << "CSSCamberDB::onActionButtonClicked(): ELSE";
        SAMDataDialog::onActionButtonClicked(click_id); // call onCmd*()
    }
    // qDebug() << "CSSOuterHullDB::onActionButtonClicked() completed.";
}

void CSSOuterHullDB::onCmdContinue(int click_id)
{
    // qDebug() << "CSSOuterHullDB::onCmdContinue() called with click_id: " << click_id;

    QString invalid_style_sheet = "QLineEdit { background: #FFCCCC; }";

    bool is_param_valid = false, is_all_valid = true;
    double half_breadth = half_breadth_le_ptr->text().toDouble(&is_param_valid);
    if(!is_param_valid || !css_params_ptr->setHalfBreadth(half_breadth))
    {
        half_breadth_le_ptr->setStyleSheet(invalid_style_sheet);
        is_all_valid = false;
    }
    else
    {
        half_breadth_le_ptr->setStyleSheet("");
    }
    double depth = depth_le_ptr->text().toDouble(&is_param_valid);
    if(!is_param_valid || !css_params_ptr->setDepth(depth))
    {
        depth_le_ptr->setStyleSheet(invalid_style_sheet);
        is_all_valid = false;
    }
    else
    {
        depth_le_ptr->setStyleSheet("");
    }
    double bilge_radius = bilge_radius_le_ptr->text().toDouble(&is_param_valid);
    if(!is_param_valid || !css_params_ptr->setBilgeRadius(bilge_radius))
    {
        bilge_radius_le_ptr->setStyleSheet(invalid_style_sheet);
        is_all_valid = false;
    }
    else
    {
        bilge_radius_le_ptr->setStyleSheet("");
    }
    double deck_corner_radius = deck_corner_radius_le_ptr->text().toDouble(&is_param_valid);
    if(!is_param_valid || !css_params_ptr->setDeckCornerRadius(deck_corner_radius))
    {
        deck_corner_radius_le_ptr->setStyleSheet(invalid_style_sheet);
        is_all_valid = false;
    }
    else
    {
        deck_corner_radius_le_ptr->setStyleSheet("");
    }
    if(!is_all_valid)
    {
        QString error_message = QString("- %1\n- %2\n- %3\n- %4")
            .arg(css_params_ptr->getHalfBreadthDesc())
            .arg(css_params_ptr->getDepthDesc())
            .arg(css_params_ptr->getBilgeRadiusDesc())
            .arg(css_params_ptr->getDeckCornerRadiusDesc());
        QMessageBox::information(this, tr("Invalid Input"), error_message);
        return;
    }

    // qDebug() << "Half Breadth: " << half_breadth << " , Depth: " << depth
    //          << " , Bilge Radius: " << bilge_radius << " , Deck Corner Radius: " << deck_corner_radius;
    // qDebug() << "size: " << size();

    QString cmd_target = dynamic_cast<const CSSForm*>(getMode())->getCmdPrefix();

    omuArguments args;
    args.Put(half_breadth, "half_breadth");
    args.Put(depth, "depth");
    args.Put(bilge_radius, "bilge_radius");
    args.Put(deck_corner_radius, "deck_corner_radius");
    cmdGCommandDeliveryRole::Instance().SendCommand(omuMethodCall(cmd_target, "draftOuterHull", args));

    omuArguments fit_args;
    fit_args.Put("outer_hull", "step");
    cmdGCommandDeliveryRole::Instance().SendCommand(omuMethodCall(cmd_target, "fitDraftView", fit_args));

    SAMDataDialog::onCmdContinue(click_id); // call getMode()->continueMode()
    // qDebug() << "CSSOuterHullDB::onCmdContinue() completed.";
}

void CSSOuterHullDB::onCmdCancel(int click_id)
{
    // qDebug() << "CSSOuterHullDB::onCmdCancel() called with click_id: " << click_id;
    SAMDataDialog::onCmdCancel(click_id); // call getMode()->cancel(QObject*, const char*)
    // qDebug() << "CSSOuterHullDB::onCmdCancel() completed.";
}
