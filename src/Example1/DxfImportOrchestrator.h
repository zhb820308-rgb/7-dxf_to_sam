#pragma once

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

enum class DxfImportOutcomeStatus
{
	Succeeded,
	Failed,
	Canceled
};

struct DxfImportOutcome
{
	DxfImportOutcomeStatus status = DxfImportOutcomeStatus::Failed;
	int createdCount = 0;

	bool succeeded() const
	{
		return status == DxfImportOutcomeStatus::Succeeded;
	}
};

DxfImportOutcome runDxfImport(const DxfImportRequest& request);
