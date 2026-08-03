#pragma once

// ============================================================================
// DxfImportOrchestrator
// ============================================================================

#include "DxfData.h"

/**
 * @brief 执行一次完整 DXF 导入用例。
 *
 * 这是核心主链最值得先读/下断点的函数：校验 → 解析 → 按模式转换 → 构建 → 提交。
 * 它只编排阶段，不亲自实现曲线算法或 SAM 数据库细节。
 */
DxfImportOutcome runDxfImport(const DxfImportRequest& request);

// ============================================================================
// DxfImportUiSupport
// ============================================================================


#include <QProgressDialog>
#include <QString>

#include <cstddef>

/**
 * @file DxfImportApplication.h
 * @brief 导入编排器使用的轻量 UI 支持：提示框、文字格式化和进度对话框。
 *
 * 这三部分都只服务于“把导入过程反馈给用户”，原先拆成三个很小的模块会让
 * Orchestrator 同时包含三个头文件。合并后业务算法仍与 UI 隔离。
 */

namespace DxfImportFeedback {

void showBudgetError(
    DxfImportErrorCode code,
    int maxOutputEntities);

void showSmallDrawingRecommendation(
    std::size_t outputEntities,
    int maxOutputEntities);

} // namespace DxfImportFeedback

namespace DxfImportFormatting {

QString errorCodeText(DxfImportErrorCode code);
const char* buildStage(ImportBuildStatus status);
QString summaryText(int created, const DxfEntityStats& stats);

} // namespace DxfImportFormatting

enum class DxfImportProgressMode
{
    Sketch,
    FiniteElement
};

class DxfImportProgress
{
public:
    DxfImportProgress(
        DxfImportProgressMode mode,
        std::size_t sketchLineCount = 0,
        std::size_t sketchCircleCount = 0);
    ~DxfImportProgress();

    // QProgressDialog 包含窗口资源，不允许意外复制；`= delete` 会让编译器拒绝复制。
    DxfImportProgress(const DxfImportProgress&) = delete;
    DxfImportProgress& operator=(const DxfImportProgress&) = delete;

    bool update(const QString& stage, int current, int total);
    void complete();
    void close();

    const QString& canceledStage() const { return m_canceledStage; }
    int canceledCurrent() const { return m_canceledCurrent; }
    int canceledTotal() const { return m_canceledTotal; }

private:
    DxfImportProgressMode m_mode;
    std::size_t m_sketchLineCount;
    int m_totalSketchEntities;
    QProgressDialog m_dialog;
    QString m_canceledStage;
    int m_canceledCurrent = 0;
    int m_canceledTotal = 0;
    bool m_canceled = false;
    bool m_closed = false;
};
