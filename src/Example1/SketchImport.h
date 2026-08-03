#pragma once

// ============================================================================
// Sketch 路线入口
// ============================================================================

#include "DxfData.h"

#include <cstddef>

class DxfImportSession;
class QElapsedTimer;

/**
 * @brief Sketch 路线入口：DxfData -> SamData -> SAM Sketch。
 *
 * 公共编排器只负责解析和模式分流，所有 Sketch 专属转换、日志和写入都从这里进入。
 */
DxfImportOutcome runSketchImport(
    const DxfImportRequest& request,
    DxfData& dxfData,
    const DxfEntityStats& entityStats,
    std::size_t outputLimit,
    DxfImportSession& session,
    QElapsedTimer& stageTimer);

// ============================================================================
// SAM Sketch 具体 Builder
// ============================================================================
#ifndef SamBuilder_h
#define SamBuilder_h

#include "SketchConversion.h"
#include "DxfImportRuntime.h"
#include <QString>
#include <functional>
#include <vector>

class skcSketch;
class skcGeomFactory;

/**
 * @brief 用 SAM C++ SDK 把 `SamData` 写成 Sketch 的适配器。
 *
 * 这是项目中 SDK 耦合最重的边界之一。它实现 ISamImportBuilder，所以编排层只看见
 * begin/create/commit/rollback；内部才使用 skcSketch、skcGeomFactory、basMdb 等
 * SAM 类型。阅读时先看四个 public 阶段，private 的场景刷新可放到第二遍。
 */
class SamBuilder : public ISamImportBuilder {
public:
    // std::function 可保存普通函数、lambda 或可调用对象。回调返回 false 表示取消。
    using ProgressCallback = std::function<bool(const QString& stage, int current, int total)>;

    SamBuilder();
    ~SamBuilder() override;

    /// @brief Begin an import session. Creates sketch and geometry factory.
    /// @param modelName SAM model name (default "Model-1").
    /// @return Structured begin status and diagnostic message.
    ImportBuildResult beginImport(
        const QString& modelName = QStringLiteral("Model-1")) override;

    /// @brief Insert the sketch into the repository and set up scene display.
    /// @return Structured commit status and created entity count.
    ImportBuildResult commit() override;

    /// @brief Discard the current import without committing.
    /// @return Success or RollbackFailed with a diagnostic message.
    ImportBuildResult rollback() override;

    /// @brief Set a progress callback invoked during geometry creation.
    /// Return false from the callback to cancel the import.
    void setProgressCallback(const ProgressCallback& callback);

    /// @brief Create line entities from DXF line data.
    /// @return Structured create/cancel status and number created.
    ImportBuildResult createLines(
        const std::vector<DxfLine>& lines) override;

    /// @brief Create circle entities from DXF circle data.
    /// @return Structured create/cancel status and number created.
    ImportBuildResult createCircles(
        const std::vector<DxfCircle>& circles) override;

    /// @brief Total number of entities created in the current session.
    int   createdCount() const { return m_createdCount; }
    /// @brief Name of the sketch being built.
    const QString& sketchName() const { return m_sketchName; }
    /// @brief Last error message (empty if no error).
    const QString& lastError() const { return m_lastError; }
    ImportTransactionState transactionState() const { return m_transaction.state(); }

private:
    void buildSketchPath();
    void extendBounds(double x, double y);
    bool reportProgress(const QString& stage, int current, int total);
    bool removePublishedSketch();
    bool isWriting() const;
    void refreshSceneAfterCommit();

    QString m_modelName;
    QString m_sketchName;
    QString m_sketchPath;
    QString m_lastError;

    // SDK 使用裸指针。本类通过 ImportTransaction 明确所有权：提交前拥有并负责清理；
    // 插入 Repository 后所有权语义转交给 SAM，m_sketch 只作为非拥有观察指针使用。
    skcSketch*      m_sketch  = nullptr;
    skcGeomFactory* m_factory = nullptr;
    int m_createdCount = 0;
    bool m_sketchWrapped = false;
    ProgressCallback m_progressCallback;
    ImportTransaction m_transaction;

    // bounding box of imported geometry (for sheet size)
    bool   m_hasBounds = false;
    double m_minX = 0.0, m_minY = 0.0;
    double m_maxX = 0.0, m_maxY = 0.0;
};

#endif
