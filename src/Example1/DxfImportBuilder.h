#pragma once

#include "DxfData.h"
#include "FeData.h"
#include "ImportBuildResult.h"

#include <QString>

#include <vector>

// Minimal transaction boundaries used by the import coordinator.  The SAM
// adapters implement these interfaces, while tests can inject deterministic
// failures without adding test-only branches to production builders.
class ISamImportBuilder
{
public:
    virtual ~ISamImportBuilder() = default;

    virtual ImportBuildResult beginImport(
        const QString& modelName = QStringLiteral("Model-1")) = 0;
    virtual ImportBuildResult createLines(
        const std::vector<DxfLine>& lines) = 0;
    virtual ImportBuildResult createCircles(
        const std::vector<DxfCircle>& circles) = 0;
    virtual ImportBuildResult commit() = 0;
    virtual ImportBuildResult rollback() = 0;
};

class IFeImportBuilder
{
public:
    virtual ~IFeImportBuilder() = default;

    virtual ImportBuildResult beginImport(
        const QString& modelName, const QString& partName) = 0;
    virtual ImportBuildResult createNodes(
        const std::vector<FeNode>& nodes) = 0;
    virtual ImportBuildResult createTrusses(
        const std::vector<FeTruss>& trusses) = 0;
    virtual ImportBuildResult commit() = 0;
    virtual ImportBuildResult rollback() = 0;
};
