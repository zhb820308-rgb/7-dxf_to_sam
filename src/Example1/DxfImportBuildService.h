#pragma once

#include "DxfImportBuilder.h"
#include "ImportBuildResult.h"

#include <QString>

class FeData;
class SamData;

namespace DxfImportBuildService {

ImportBuildResult buildSamSketch(
	const SamData& samData,
	ISamImportBuilder& builder);

ImportBuildResult buildFePart(
	FeData& feData,
	const QString& modelName,
	const QString& partName,
	IFeImportBuilder& builder);

} // namespace DxfImportBuildService
