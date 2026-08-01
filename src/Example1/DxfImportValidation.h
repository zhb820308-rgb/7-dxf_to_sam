#pragma once

#include <QString>

#include <cstddef>
#include <string>

namespace DxfImportValidation {

constexpr std::size_t kSmallDrawingEntityLimit = 100000;
constexpr int kLargeDrawingEntityLimit = 500000;

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
