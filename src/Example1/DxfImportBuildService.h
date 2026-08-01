#pragma once

#include "ImportBuildResult.h"

#include <QString>

class FeData;
class PythonFiniteElementBuilder;
class SamBuilder;
class SamData;

namespace DxfImportBuildService {

ImportBuildResult buildSamSketch(
	const SamData& samData,
	SamBuilder& builder);

ImportBuildResult buildFePart(
	FeData& feData,
	const QString& modelName,
	const QString& partName,
	PythonFiniteElementBuilder& builder);

} // namespace DxfImportBuildService
