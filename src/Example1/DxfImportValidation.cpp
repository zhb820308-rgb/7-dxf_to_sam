#include "DxfImportValidation.h"

#include <cmath>
#include <limits>

namespace DxfImportValidation {

Result validate(
	double baseX,
	double baseY,
	double baseZ,
	double curveTolerance,
	double nodeMergeTolerance,
	int maxOutputEntities)
{
	if (!std::isfinite(baseX)
		|| !std::isfinite(baseY)
		|| !std::isfinite(baseZ))
	{
		return Result{
			false,
			0,
			" invalid_base_coordinates",
			QStringLiteral(
				"[importDxf] ERROR: base coordinates must be finite")};
	}
	if (!std::isfinite(curveTolerance) || curveTolerance <= 0.0)
	{
		return Result{
			false,
			0,
			" invalid_curveTolerance",
			QString("[importDxf] ERROR: invalid curveTolerance %1")
				.arg(curveTolerance)};
	}
	if (!std::isfinite(nodeMergeTolerance) || nodeMergeTolerance < 0.0)
	{
		return Result{
			false,
			0,
			" invalid_nodeMergeTolerance",
			QString("[importDxf] ERROR: invalid nodeMergeTolerance %1")
				.arg(nodeMergeTolerance)};
	}
	if (maxOutputEntities != static_cast<int>(kSmallDrawingEntityLimit)
		&& maxOutputEntities != kLargeDrawingEntityLimit
		&& maxOutputEntities != -1)
	{
		return Result{
			false,
			0,
			" error_code=INVALID_ARGUMENT invalid_maxOutputEntities",
			QStringLiteral(
				"[importDxf] ERROR [INVALID_ARGUMENT]: invalid maxOutputEntities")};
	}

	const std::size_t outputLimit = maxOutputEntities < 0
		? std::numeric_limits<std::size_t>::max()
		: static_cast<std::size_t>(maxOutputEntities);
	return Result{true, outputLimit, std::string(), QString()};
}

} // namespace DxfImportValidation
