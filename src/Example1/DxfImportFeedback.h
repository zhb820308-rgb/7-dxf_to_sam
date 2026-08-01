#pragma once

#include "DxfImportError.h"

#include <cstddef>

namespace DxfImportFeedback {

void showBudgetError(
	DxfImportErrorCode code,
	int maxOutputEntities);

void showSmallDrawingRecommendation(
	std::size_t outputEntities,
	int maxOutputEntities);

} // namespace DxfImportFeedback
