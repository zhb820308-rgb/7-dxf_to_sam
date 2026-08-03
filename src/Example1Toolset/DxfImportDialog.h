#pragma once

#include <SAMDataDialog.h>
#include <QStringList>

class QComboBox;
class QDoubleValidator;
class QLineEdit;
class QPushButton;
class Example1Form;
class MultiSelectComboBox;

/**
 * @file DxfImportDialog.h
 * @brief DXF 参数窗口，以及只读取 DXF 图层名的轻量辅助接口。
 */
class Example1DXFImportDialog : public SAMDataDialog
{
    Q_OBJECT

public:
    explicit Example1DXFImportDialog(Example1Form* form);
    ~Example1DXFImportDialog() override;

private slots:
    void onCmdOk(int id) override;
    void onBrowse();
    void onDxfPathEditingFinished();
    void onImportModeChanged(int index);
    void onDrawingSizeChanged(int index);

private:
    QLineEdit* m_dxfPathEdit;
    QLineEdit* m_baseXEdit;
    QLineEdit* m_baseYEdit;
    QLineEdit* m_baseZEdit;
    QLineEdit* m_toleranceEdit;
    QComboBox* m_drawingSizeCombo;
    QLineEdit* m_modelNameEdit;
    QLineEdit* m_partNameEdit;
    MultiSelectComboBox* m_ignoreLayersCombo;
    QComboBox* m_importModeCombo;
    QPushButton* m_browseButton;
    QDoubleValidator* m_doubleValidator;
    QDoubleValidator* m_toleranceValidator;
    static const int BROWSE_BUTTON_ID = 2;
};

bool collectDxfLayers(const QString& filePath, QStringList& layers);
