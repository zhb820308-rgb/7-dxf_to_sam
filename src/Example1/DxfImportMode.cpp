#include "DxfImportMode.h"

DxfImportModeResult selectDxfImportMode(
	const QString& importMode,
	const QString& modelName,
	const QString& partName)
{
	const bool finiteElement = importMode.compare(
		QStringLiteral("FiniteElement"), Qt::CaseInsensitive) == 0;
	const bool sketch = importMode.isEmpty() || importMode.compare(
		QStringLiteral("Sketch"), Qt::CaseInsensitive) == 0;
	if (!sketch && !finiteElement)
	{
		return DxfImportModeResult{
			false,
			DxfImportMode::Sketch,
			"validate_mode",
			" mode=\"" + importMode.toLocal8Bit().toStdString() + "\"",
			QString("[importDxf] ERROR: unsupported importMode '%1'")
				.arg(importMode)};
	}

	if (!finiteElement)
	{
		return DxfImportModeResult{
			true, DxfImportMode::Sketch, "validate_mode",
			std::string(), QString()};
	}

	if (modelName.isEmpty() || partName.isEmpty())
	{
		const QString missingName = modelName.isEmpty()
			? QStringLiteral("modelName")
			: QStringLiteral("partName");
		return DxfImportModeResult{
			false,
			DxfImportMode::FiniteElement,
			"validate_params",
			" missing=\"" + missingName.toLocal8Bit().toStdString() + "\"",
			QString("[importDxf] ERROR: FE mode requires '%1'")
				.arg(missingName)};
	}

	return DxfImportModeResult{
		true, DxfImportMode::FiniteElement, "validate_mode",
		std::string(), QString()};
}
