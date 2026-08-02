#pragma once

#include "DxfImportDefaults.h"

#include <QString>

struct DxfImportRequest
{
    QString filePath;
    double baseX = 0.0;
    double baseY = 0.0;
    double baseZ = 0.0;
    double curveTolerance = DxfImportDefaults::kCurveTolerance;
    QString ignoredLayers;
    QString importMode;
    QString modelName;
    QString partName;
    double nodeMergeTolerance = DxfImportDefaults::kNodeMergeTolerance;
    int maxOutputEntities = DxfImportDefaults::kDefaultMaxOutputEntities;
};
