#pragma once

// ============================================================================
// FE 路线入口
// ============================================================================

#include "DxfData.h"

#include <cstddef>

class DxfImportSession;
class QElapsedTimer;

/**
 * @brief FE 路线入口：DxfData -> FeData -> SAM Model/Part。
 *
 * 本接口与 Sketch 路线完全独立，使老师可以从模式分叉点分别追踪两条调用链。
 */
DxfImportOutcome runFiniteElementImport(
    const DxfImportRequest& request,
    DxfData& dxfData,
    std::size_t outputLimit,
    DxfImportSession& session,
    QElapsedTimer& stageTimer);

// ============================================================================
// SAM FE 具体 Builder
// ============================================================================
#ifndef PythonFiniteElementBuilder_h
#define PythonFiniteElementBuilder_h

#include "FeConversion.h"
#include "DxfImportRuntime.h"

#include <QString>
#include <functional>
#include <vector>

struct FeNode;
struct FeTruss;

/**
 * @brief 通过 SAM 内嵌 Python API 创建有限元 Part 的 Builder。
 *
 * C++ 负责批量生成命令字符串并交给 `pytInterpreterRole` 执行，实际顺序是：
 * `mdl.Part()` -> `createNode()` -> `Element(TRUSS)` -> 显示并 fitView。
 * 这里没有直接复用 Sketch 的 C++ API，因为 FE Part 的官方 Python 路径能够正确
 * 更新 SAM Part Manager 和标准 GUI 生命周期。
 */
class PythonFiniteElementBuilder : public IFeImportBuilder {
public:
    using ProgressCallback =
        std::function<bool(const QString& stage, int current, int total)>;

    ~PythonFiniteElementBuilder() override;

    ImportBuildResult beginImport(
        const QString& modelName, const QString& partName) override;
    ImportBuildResult createNodes(
        const std::vector<FeNode>& nodes) override;
    ImportBuildResult createTrusses(
        const std::vector<FeTruss>& trusses) override;
    ImportBuildResult commit() override;
    ImportBuildResult rollback() override;

    void setProgressCallback(const ProgressCallback& callback)
    {
        m_progressCallback = callback;
    }

    // Control how many nodes/trusses are sent in a single Python command.
    // Default: 5000.  Larger batches reduce interpreter round-trips at the
    // cost of longer Python command strings and coarser cancellation
    // granularity.  Set before calling createNodes / createTrusses.
    void setBatchSize(int size) { m_batchSize = size; }
    int batchSize() const { return m_batchSize; }

    const QString& lastError() const { return m_lastError; }
    int createdNodeCount() const { return m_createdNodeCount; }
    int createdTrussCount() const { return m_createdTrussCount; }
    ImportTransactionState transactionState() const { return m_transaction.state(); }

private:
    bool runCommand(const QString& command, const QString& stage);
    bool reportProgress(const QString& stage, int current, int total);
    bool partExists(bool* querySucceeded = nullptr) const;
    bool removeOwnedPart();
    bool isWriting() const;
    static QString pythonStringLiteral(const QString& value);

    QString m_modelName;
    QString m_partName;
    QString m_lastError;
    // 形如 mdb.models['Model-1'].parts['Part-1'] 的 Python 表达式。
    QString m_partExpression;
    int m_createdNodeCount = 0;
    int m_createdTrussCount = 0;
    int m_batchSize = 5000;
    ProgressCallback m_progressCallback;
    ImportTransaction m_transaction;
};

#endif // PythonFiniteElementBuilder_h
