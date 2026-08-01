#pragma once

#include "DxfData.h"
#include "DxfImportError.h"
#include "ImportBuildResult.h"

#include <QString>

namespace DxfImportFormatting {

QString errorCodeText(DxfImportErrorCode code);
const char* buildStage(ImportBuildStatus status);
QString summaryText(int created, const DxfEntityStats& stats);

} // namespace DxfImportFormatting
