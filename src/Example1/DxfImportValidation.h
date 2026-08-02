#pragma once

#include "DxfImportDefaults.h"

#include <QString>

#include <cstddef>
#include <string>

namespace DxfImportValidation {

constexpr std::size_t kSmallDrawingEntityLimit =
	DxfImportDefaults::kSmallDrawingEntityLimit;
constexpr int kLargeDrawingEntityLimit =
	DxfImportDefaults::kLargeDrawingEntityLimit;
constexpr double kMinimumCurveTolerance =
	DxfImportDefaults::kMinimumCurveTolerance;
constexpr double kMaximumCurveTolerance =
	DxfImportDefaults::kMaximumCurveTolerance;

struct Result
{
	bool valid = false;
	std::size_t outputLimit = 0;
	std::string detail;
	QString message;
};

Result validate(
	double baseX,
	double baseY,
	double baseZ,
	double curveTolerance,
	double nodeMergeTolerance,
	int maxOutputEntities);

} // namespace DxfImportValidation
