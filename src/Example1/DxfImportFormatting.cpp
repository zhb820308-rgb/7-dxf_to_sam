#include "DxfImportFormatting.h"

#include <QStringList>

namespace {

QString boolText(bool value)
{
	return value ? QStringLiteral("true") : QStringLiteral("false");
}

QString splineKindText(const SplineKind& kind)
{
	const QString construction =
		kind.construction == SplineConstruction::ControlBased
		? QStringLiteral("ControlBased")
		: QStringLiteral("FitBased");
	return QStringLiteral("%1|rational=%2|periodic=%3|closed=%4")
		.arg(construction)
		.arg(boolText(kind.rational))
		.arg(boolText(kind.periodic))
		.arg(boolText(kind.closed));
}

} // namespace

namespace DxfImportFormatting {

QString errorCodeText(DxfImportErrorCode code)
{
	switch (code)
	{
	case DxfImportErrorCode::InvalidArgument: return QStringLiteral("INVALID_ARGUMENT");
	case DxfImportErrorCode::ReadFailed: return QStringLiteral("READ_FAILED");
	case DxfImportErrorCode::ExpansionLimit: return QStringLiteral("EXPANSION_LIMIT");
	case DxfImportErrorCode::ConversionLimit: return QStringLiteral("CONVERSION_LIMIT");
	case DxfImportErrorCode::NoSupportedEntities: return QStringLiteral("NO_SUPPORTED_ENTITIES");
	case DxfImportErrorCode::ConversionFailed: return QStringLiteral("CONVERSION_FAILED");
	case DxfImportErrorCode::Canceled: return QStringLiteral("CANCELED");
	case DxfImportErrorCode::BeginFailed: return QStringLiteral("BEGIN_FAILED");
	case DxfImportErrorCode::CreateFailed: return QStringLiteral("CREATE_FAILED");
	case DxfImportErrorCode::CommitFailed: return QStringLiteral("COMMIT_FAILED");
	case DxfImportErrorCode::RollbackFailed: return QStringLiteral("ROLLBACK_FAILED");
	case DxfImportErrorCode::None: break;
	}
	return QStringLiteral("NONE");
}

const char* buildStage(ImportBuildStatus status)
{
	switch (status)
	{
	case ImportBuildStatus::Canceled: return "canceled";
	case ImportBuildStatus::BeginFailed: return "begin_import";
	case ImportBuildStatus::CreateFailed: return "create";
	case ImportBuildStatus::CommitFailed: return "commit";
	case ImportBuildStatus::RollbackFailed: return "rollback";
	case ImportBuildStatus::Success: break;
	}
	return "build";
}

QString summaryText(int created, const DxfEntityStats& stats)
{
	QStringList splineCategories;
	for (const auto& entry : stats.splineKinds)
	{
		if (entry.second == 0)
			continue;
		splineCategories.append(
			QStringLiteral("%1=%2")
			.arg(splineKindText(entry.first))
			.arg(static_cast<qulonglong>(entry.second)));
	}

	return QStringLiteral(
		"[importDxf] Import complete: imported entities=%1; source=%2, accepted=%3, rejected=%4, generated=%5; "
		"types (after layer filter and block expansion): points=%6 "
		"(Sketch ignored, FE standalone nodes), lines=%7, polylines=%8, curves=%9 "
		"(circles=%10, arcs=%11, ellipses=%12, splines=%13; spline categories=[%14])")
		.arg(created)
		.arg(static_cast<qulonglong>(stats.sourceEntities))
		.arg(static_cast<qulonglong>(stats.acceptedEntities))
		.arg(static_cast<qulonglong>(stats.rejectedEntities))
		.arg(static_cast<qulonglong>(stats.generatedEntities))
		.arg(static_cast<qulonglong>(stats.points))
		.arg(static_cast<qulonglong>(stats.lines))
		.arg(static_cast<qulonglong>(stats.lwPolylines))
		.arg(static_cast<qulonglong>(stats.curveCount()))
		.arg(static_cast<qulonglong>(stats.circles))
		.arg(static_cast<qulonglong>(stats.arcs))
		.arg(static_cast<qulonglong>(stats.ellipses))
		.arg(static_cast<qulonglong>(stats.splineCount()))
		.arg(splineCategories.join(QStringLiteral(", ")));
}

} // namespace DxfImportFormatting
