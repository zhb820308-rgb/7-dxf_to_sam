#pragma once

#include "DxfImportOutcome.h"

#include <QString>

struct DxfImportRequest
{
	QString filePath;
	double baseX = 0.0;
	double baseY = 0.0;
	double baseZ = 0.0;
	double curveTolerance = 0.0;
	QString ignoredLayers;
	QString importMode;
	QString modelName;
	QString partName;
	double nodeMergeTolerance = 0.0;
	int maxOutputEntities = 100000;
};

DxfImportOutcome runDxfImport(const DxfImportRequest& request);
