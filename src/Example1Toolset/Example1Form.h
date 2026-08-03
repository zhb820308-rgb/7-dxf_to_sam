#pragma once

#include <SAMCreateEditForm.h>
#include <SAMDataDialog.h>

class QLineEdit;
class SAMGuiObjectManager;
class Example1DB;

/**
 * @file Example1Form.h
 * @brief SAM Form 适配层，与 Toolset 单例模板隔离。
 *
 * 本文件不能包含 Example1Toolset.h。这样调用 cmdGCommandDeliveryRole 的编译单元
 * 看不到 omiSingleton.T，只会链接 SAMGui.dll 导出的真实命令投递单例。
 */
class Example1Form : public SAMForm
{
    Q_OBJECT
    friend class Example1DB;

public:
    explicit Example1Form(SAMGuiObjectManager* owner);
    ~Example1Form() override;

protected:
    SAMDialog* getFirstDialog() override;
};

/// 早期“长 × 宽”教学对话框；不属于当前 DXF 导入主流程。
class Example1DB : public SAMDataDialog
{
    Q_OBJECT

public:
    explicit Example1DB(Example1Form* form);
    ~Example1DB() override;

private slots:
    void onCmdOk(int id) override;

private:
    QLineEdit* m_lengthEdit;
    QLineEdit* m_widthEdit;
};
