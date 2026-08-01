#pragma once

#include <QString>

#include <string>

enum class DxfImportMode
{
	Sketch,
	FiniteElement
};

struct DxfImportModeResult
{
	bool valid = false;
	DxfImportMode mode = DxfImportMode::Sketch;
	const char* stage = "validate_mode";
	std::string detail;
	QString message;
};

DxfImportModeResult selectDxfImportMode(
	const QString& importMode,
	const QString& modelName,
	const QString& partName);
