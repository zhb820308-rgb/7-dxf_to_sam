#include "CSSPostprocessDB.h"

#include "CSSForm.h"
#include "CSSParameters.h"

#include <omuArguments.h>
#include <cmdGCommandDeliveryRole.h>
#include <omuMethodCall.h>
#include <QAbstractButton>
#include <QButtonGroup>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGroupBox>
#include <QCheckBox>
#include <QComboBox>
#include <QSpinBox>
#include <QLabel>
#include <QFrame>
#include <QLineEdit>
#include <QList>
#include <QPushButton>
#include <QMessageBox>
#include <QString>
#include <QFileDialog>
#include <QDir>

#include <QDebug>

CSSPostprocessDB::CSSPostprocessDB(CSSForm* form_ptr, CSSParameters* params_ptr):
    SAMDataDialog(form_ptr,
        tr("End Container Ship Section"),
        ButtonID::DISMISS | ButtonID::OK | ButtonID::CANCEL)
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

    mirrorBox->setChecked(cssParams->needMirrorSection());
    extrudeBox->setChecked(cssParams->needExtrudeSection());
    toggleExtrudeControls();
    double extrude_direction = cssParams->getExtrudeDirection();
    if(!qFuzzyIsNull(extrude_direction))
    {
        extrudeDx->setText(QString::number(extrude_direction));
    }
    int extrude_cnt = cssParams->getExtrudeCnt();
    if(extrude_cnt != -1)
    {
        extrudeTimes->setValue(extrude_cnt);
    }
    refineBox->setChecked(cssParams->needRefineSection());
    stiffenerCheckBox->setChecked(cssParams->needStiffener());
    QString file_path = cssParams->getExportFilePath();
    if(!file_path.isEmpty())
    {
        filePathEdit->setText(file_path);
    }

    /*move(form_ptr->getPrevDialogCenter()
        - QPoint(frameGeometry().width() / 2, frameGeometry().height() / 2));*/
}

CSSPostprocessDB::~CSSPostprocessDB()
{
    // qDebug() << "CSSPostprocessDB::~CSSPostprocessDB() called";
}

void CSSPostprocessDB::setupUi()
{
    setMinimumSize(565, 564);

    QVBoxLayout* content_layout_ptr = new QVBoxLayout(contentArea);

    QGroupBox* meshGroup = new QGroupBox(tr("Mesh Options"), contentArea);
    content_layout_ptr->addWidget(meshGroup);

    QVBoxLayout* mesh_layout_ptr = new QVBoxLayout(meshGroup);

    mirrorBox = new QCheckBox(tr("Mirror Section"));
    mesh_layout_ptr->addWidget(mirrorBox);

    QFrame* separate_line_ptr = new QFrame(meshGroup);
    separate_line_ptr->setFrameShape(QFrame::HLine);
    separate_line_ptr->setFrameShadow(QFrame::Sunken);
    mesh_layout_ptr->addWidget(separate_line_ptr);

    extrudeBox = new QCheckBox(tr("Extrude Section"));
    connect(extrudeBox, &QCheckBox::toggled, this, &CSSPostprocessDB::toggleExtrudeControls);
    mesh_layout_ptr->addWidget(extrudeBox);

    QLabel* extrudeDirectionLabel= new QLabel(tr("Extrude Direction (dx):"));
	extrudeDirectionLabel->setObjectName("extrude_direction_label");
    mesh_layout_ptr->addWidget(extrudeDirectionLabel);

    extrudeDx = new QLineEdit();
	extrudeDx->setText("800");
    mesh_layout_ptr->addWidget(extrudeDx);

    QLabel* extrude_cnt_label_ptr = new QLabel(tr("Number of Times:"));
    extrude_cnt_label_ptr->setObjectName("extrude_cnt_label");
    mesh_layout_ptr->addWidget(extrude_cnt_label_ptr);

    extrudeTimes = new QSpinBox();
    extrudeTimes->setRange(1, 100);
	extrudeTimes->setValue(10);
    mesh_layout_ptr->addWidget(extrudeTimes);

    refineBox = new QCheckBox(tr("Auto Refine Section"));
    mesh_layout_ptr->addWidget(refineBox);

    stiffenerCheckBox = new QCheckBox(tr("Add Stiffener"));
	stiffenerCheckBox->setChecked(false);
    connect(stiffenerCheckBox, &QCheckBox::toggled, this, &CSSPostprocessDB::toggleExtrudeControls);
    mesh_layout_ptr->addWidget(stiffenerCheckBox);

    QHBoxLayout* stiffener_layout_ptr = new QHBoxLayout();
    mesh_layout_ptr->addLayout(stiffener_layout_ptr);

    stiffenerProfileBox = new SAMComboBox();
    stiffenerProfileBox->addItem("Profile-1");
    stiffener_layout_ptr->addWidget(stiffenerProfileBox);

    stiffenerMaterialBox = new SAMComboBox();
    stiffenerMaterialBox->addItem("Material-1");
    stiffener_layout_ptr->addWidget(stiffenerMaterialBox);

    QGroupBox* file_gb_ptr = new QGroupBox(tr("Export Parameters"), contentArea);
    content_layout_ptr->addWidget(file_gb_ptr);

    QHBoxLayout* file_layout_ptr = new QHBoxLayout(file_gb_ptr);

    filePathEdit = new QLineEdit(contentArea);
    filePathEdit->setPlaceholderText(tr("Save to file..."));
    filePathEdit->setMinimumWidth(400);
    // filePathEdit->setAcceptDrops(true);
    file_layout_ptr->addWidget(filePathEdit);

    QPushButton* browse_btn_ptr = new QPushButton(tr("Browse..."), contentArea);
    connect(browse_btn_ptr, &QPushButton::clicked, this, &CSSPostprocessDB::openFileDialog);
    file_layout_ptr->addWidget(browse_btn_ptr);
}

void CSSPostprocessDB::onActionButtonClicked(int click_id)
{
    // qDebug() << "CSSPostprocessDB::onActionButtonClicked() called with click_id: " << click_id;
    if(click_id == ID_CLICKED_DISMISS)
    {
        // qDebug() << "CSSPostprocessDB::onActionButtonClicked(): ID_CLICKED_DISMISS";

        QString cmd_target = dynamic_cast<const CSSForm*>(getMode())->getCmdPrefix();
        cmdGCommandDeliveryRole::Instance().SendCommand(cmd_target + ".rollbackDraft(5)");

        omuArguments fit_args;
        fit_args.Put("stringer", "step");
        cmdGCommandDeliveryRole::Instance().SendCommand(
            omuMethodCall(cmd_target, "fitDraftView", fit_args));

        dynamic_cast<CSSForm*>(getMode())->rollbackMode();
    }
    else
    {
        // qDebug() << "CSSPostprocessDB::onActionButtonClicked(): ELSE";
        SAMDataDialog::onActionButtonClicked(click_id); // call onCmd*()
    }
    // qDebug() << "CSSPostprocessDB::onActionButtonClicked() completed.";
}

void CSSPostprocessDB::onCmdOk(int click_id)
{
    // qDebug() << "CSSPostprocessDB::onCmdOk() called with click_id: " << click_id;

    QString invalid_style_sheet = "{ background: #FFCCCC; }";

    cssParams->setMirrorSection(mirrorBox->isChecked());
    cssParams->setExtrudeSection(extrudeBox->isChecked());
    bool is_param_valid = false, is_all_valid = true;
    double extrude_direction = extrudeDx->text().toDouble(&is_param_valid);
    if(!is_param_valid || !cssParams->setExtrudeDirection(extrude_direction))
    {
        extrudeDx->setStyleSheet("QLineEdit " + invalid_style_sheet);
        is_all_valid = false;
    }
    else
    {
        extrudeDx->setStyleSheet("");
    }
    int extrude_cnt = extrudeTimes->value();
    if(!cssParams->setExtrudeCnt(extrude_cnt))
    {
        extrudeTimes->setStyleSheet("QSpinBox " + invalid_style_sheet);
        is_all_valid = false;
    }
    else
    {
        extrudeTimes->setStyleSheet("");
    }
    if(!is_all_valid)
    {
        QMessageBox::information(this,
            tr("Invalid Input"),
            tr("Please correct the highlighted fields."));
        return;
    }
    cssParams->setRefineSection(refineBox->isChecked());
    cssParams->setNeedStiffener(stiffenerCheckBox->isChecked());
    if(stiffenerCheckBox->isChecked() &&
        (stiffenerProfileBox->currentText().isEmpty() ||
        stiffenerMaterialBox->currentText().isEmpty()))
    {
        stiffenerProfileBox->setStyleSheet("QComboBox " + invalid_style_sheet);
        stiffenerMaterialBox->setStyleSheet("QComboBox " + invalid_style_sheet);
        QMessageBox::information(this,
            tr("Invalid Stiffener Parameters"),
            tr("Please create a profile and material for the stiffener."));
        return;
    }
    else
    {
        stiffenerProfileBox->setStyleSheet("");
        stiffenerMaterialBox->setStyleSheet("");
    }

    QString file_path = filePathEdit->text().trimmed();
    if(!file_path.isEmpty())
    {
        if(!cssParams->setExportFilePath(file_path))
        {
            filePathEdit->setStyleSheet("QLineEdit " + invalid_style_sheet);
            QMessageBox::information(this,
                tr("Invalid File Path"),
                tr("The specified file path is invalid or cannot be set."));
            return;
        }
        else
        {
            filePathEdit->setStyleSheet("");
        }
        cssParams->exportToFile(file_path);
    }

    // qDebug() << "need_mirror_section: " << cssParams->needMirrorSection()
    //          << ", need_extrude_section: " << cssParams->needExtrudeSection()
    //          << ", extrude_direction: " << cssParams->getExtrudeDirection()
    //          << ", extrude_cnt: " << cssParams->getExtrudeCnt()
    //          << ", need_refine_section: " << cssParams->needRefineSection()
    //          << ", need_stiffener: " << cssParams->needStiffener()
    //          << ", file_path: " << file_path;
    // qDebug() << "size: " << size();

    QString cmd_target = dynamic_cast<const CSSForm*>(getMode())->getCmdPrefix();
    cmdGCommandDeliveryRole::Instance().SendCommand(cmd_target + ".draftToNodesAndBeams()");
    if(cssParams->needMirrorSection())
    {
        cmdGCommandDeliveryRole::Instance().SendCommand(cmd_target + ".mirrorSection()");
    }
    if(cssParams->needExtrudeSection())
    {
        omuArguments args;
        args.Put(cssParams->getExtrudeDirection(), "extrude_direction");
        args.Put(cssParams->getExtrudeCnt(), "extrude_cnt");
        cmdGCommandDeliveryRole::Instance().SendCommand(omuMethodCall(cmd_target, "extrudeSection", args));

        if(cssParams->needRefineSection())
        {
            cmdGCommandDeliveryRole::Instance().SendCommand(cmd_target + ".refineSection()");
        }
        if(cssParams->needStiffener())
        {
            omuArguments args;
            args.Put(cssParams->needMirrorSection(), "need_mirror_section");
            args.Put(stiffenerProfileBox->currentText(), "profile_name");
            args.Put(stiffenerMaterialBox->currentText(), "material_name");
            cmdGCommandDeliveryRole::Instance().SendCommand(omuMethodCall(cmd_target, "addStiffener", args));
        }
    }

    SAMDataDialog::onCmdOk(click_id); // call getMode()->commit()
    // qDebug() << "CSSPostprocessDB::onCmdOk() completed.";
}

void CSSPostprocessDB::onCmdCancel(int click_id)
{
    // qDebug() << "CSSPostprocessDB::onCmdCancel() called with click_id: " << click_id;
    SAMDataDialog::onCmdCancel(click_id); // call getMode()->cancel(QObject*, const char*)
    // qDebug() << "CSSPostprocessDB::onCmdCancel() completed.";
}

void CSSPostprocessDB::toggleExtrudeControls()
{
    bool is_extrude_enabled = extrudeBox->isChecked();
    bool is_stiffener_enabled = is_extrude_enabled && stiffenerCheckBox->isChecked();
    findChild<QLabel*>("extrude_direction_label")->setEnabled(is_extrude_enabled);
    extrudeDx->setEnabled(is_extrude_enabled);
    findChild<QLabel*>("extrude_cnt_label")->setEnabled(is_extrude_enabled);
    extrudeTimes->setEnabled(is_extrude_enabled);
    refineBox->setEnabled(is_extrude_enabled);
    stiffenerCheckBox->setEnabled(is_extrude_enabled);
    stiffenerProfileBox->setEnabled(is_stiffener_enabled);
    stiffenerMaterialBox->setEnabled(is_stiffener_enabled);
}

void CSSPostprocessDB::openFileDialog()
{
    QString path = QFileDialog::getSaveFileName(this,
        tr("Save File"),
        QDir::toNativeSeparators(QFileInfo(filePathEdit->text().trimmed()).absolutePath()),
        tr("SAM Section Parameter Files (*.samsp)"));
    if(!path.isEmpty())
    {
        filePathEdit->setText(path);
    }
}
