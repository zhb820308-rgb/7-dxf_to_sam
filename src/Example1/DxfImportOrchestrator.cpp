#include "DxfImportOrchestrator.h"

#include "ConversionEngine.h"
#include "DxfImportBuildService.h"
#include "DxfImportFeedback.h"
#include "DxfImportFormatting.h"
#include "DxfImportLayers.h"
#include "DxfImportLogger.h"
#include "DxfImportPreflight.h"
#include "DxfImportProgress.h"
#include "DxfImportSession.h"
#include "DxfParser.h"
#include "FeConversionEngine.h"
#include "FeData.h"
#include "PythonFiniteElementBuilder.h"
#include "SamBuilder.h"
#include "SamData.h"

#include <QDebug>
#include <QElapsedTimer>

namespace {

DxfImportOutcome failedImport(
	DxfImportSession& session,
	DxfImportOutcomeStatus status,
	DxfImportErrorCode errorCode,
	const std::string& stage,
	const std::string& detail,
	const QString& warning)
{
	failImport(
		session.logger(), session.errorLogger(), session.importId(),
		session.pathText(), stage, detail, session.elapsed(), warning);
	return DxfImportOutcome::failure(
		status, errorCode, QString::fromStdString(stage), warning);
}

DxfImportOutcome failedBuildImport(
	DxfImportSession& session,
	const ImportBuildResult& buildResult,
	const std::string& stage,
	std::string detail,
	QString warning)
{
	if (buildResult.rollbackAttempted)
	{
		detail += buildResult.rollbackSucceeded
			? " rollback_succeeded=true"
			: " rollback_succeeded=false";
		if (!buildResult.rollbackSucceeded)
		{
			const std::string rollbackError =
				buildResult.rollbackMessage.toLocal8Bit().toStdString();
			detail += " rollback_error=\"" + rollbackError + "\"";
			warning += QStringLiteral("; rollback failed: ") +
				buildResult.rollbackMessage;
		}
	}
	failImport(
		session.logger(), session.errorLogger(), session.importId(),
		session.pathText(), stage, detail, session.elapsed(), warning);
	return DxfImportOutcome::fromBuildFailure(
		buildResult, QString::fromStdString(stage), warning);
}

DxfImportOutcome canceledImport(
	DxfImportSession& session,
	DxfImportProgress& progress,
	const QChar& separator)
{
	progress.close();
	if (session.logger())
	{
		session.logger()->warn(
			"[import={}] canceled stage=\"{}\" progress={}/{} total_elapsed_ms={}",
			session.importId(),
			progress.canceledStage().toLocal8Bit().toStdString(),
			progress.canceledCurrent(), progress.canceledTotal(),
			session.elapsed());
	}
	qWarning().noquote() << QString("[importDxf] IMPORT CANCELED %1 %2 %3/%4")
		.arg(separator)
		.arg(progress.canceledStage())
		.arg(progress.canceledCurrent())
		.arg(progress.canceledTotal());
	session.finish();
	return DxfImportOutcome::failure(
		DxfImportOutcomeStatus::Canceled,
		DxfImportErrorCode::Canceled,
		progress.canceledStage(),
		QStringLiteral("import canceled"));
}

DxfImportOutcome importFiniteElement(
	const DxfImportRequest& request,
	DxfData& dxfData,
	std::size_t outputLimit,
	DxfImportSession& session,
	QElapsedTimer& stageTimer)
{
	stageTimer.restart();
	DxfImportProgress progress(DxfImportProgressMode::FiniteElement);
	FeData feData;
	FeConversionEngine converter;
	converter.setProgressCallback(
		[&progress](const QString& stage, int current, int total) {
			return progress.update(stage, current, total);
		});
	if (!converter.convert(
			dxfData, request.baseX, request.baseY, request.baseZ,
			request.curveTolerance, request.nodeMergeTolerance,
			feData, outputLimit))
	{
		if (feData.errorCode() == DxfImportErrorCode::Canceled)
			return canceledImport(session, progress, QLatin1Char('-'));
		const QString errorCode =
			DxfImportFormatting::errorCodeText(feData.errorCode());
		const QString errorMessage = feData.errorMessage().isEmpty()
			? QStringLiteral("no valid FE nodes to import")
			: feData.errorMessage();
		DxfImportFeedback::showBudgetError(
			feData.errorCode(), request.maxOutputEntities);
		return failedImport(
			session,
			DxfImportOutcomeStatus::ConvertFailed,
			feData.errorCode(),
			"fe_conversion",
			" error_code=" + errorCode.toStdString() +
			" error=\"" + errorMessage.toLocal8Bit().toStdString() + "\"",
			QString("[importDxf] ERROR [%1]: %2")
				.arg(errorCode, errorMessage));
	}

	DxfImportFeedback::showSmallDrawingRecommendation(
		feData.nodes().size() + feData.trusses().size(),
		request.maxOutputEntities);
	dxfData.clear();
	const FeConversionStats stats = feData.stats();
	if (session.logger())
	{
		session.logger()->info(
			"[import={}] fe_conversion_completed nodes={} trusses={}"
			" points_processed={}"
			" merged={} skipped_zero_length={} skipped_dup_truss={}"
			" curve_tolerance={} node_merge_tolerance={} duration_ms={}",
			session.importId(), feData.nodes().size(), feData.trusses().size(),
			stats.pointsProcessed,
			stats.mergedNodes, stats.skippedZeroLength,
			stats.skippedDuplicateTruss, request.curveTolerance,
			request.nodeMergeTolerance, stageTimer.elapsed());
	}

	stageTimer.restart();
	PythonFiniteElementBuilder builder;
	builder.setProgressCallback(
		[&progress](const QString& stage, int current, int total) {
			return progress.update(stage, current, total);
		});
	const ImportBuildResult buildResult = DxfImportBuildService::buildFePart(
		feData, request.modelName, request.partName, builder);
	if (!buildResult.succeeded())
	{
		progress.close();
		const bool canceled =
			buildResult.status == ImportBuildStatus::Canceled;
		const std::string stage =
			DxfImportFormatting::buildStage(buildResult.status);
		std::string detail =
			" error=\"" + buildResult.message.toLocal8Bit().toStdString() + "\"";
		if (!canceled && buildResult.status != ImportBuildStatus::BeginFailed)
		{
			detail = " model=\"" + request.modelName.toLocal8Bit().toStdString() +
				"\" part=\"" + request.partName.toLocal8Bit().toStdString() +
				"\"" + detail;
		}
		const QString warning = canceled
			? QString("[importDxf] IMPORT CANCELED - %1 %2/%3")
				.arg(progress.canceledStage())
				.arg(progress.canceledCurrent())
				.arg(progress.canceledTotal())
			: QString("[importDxf] ERROR: FE build failed, rolling back - %1")
				.arg(buildResult.message);
		return failedBuildImport(
			session, buildResult, stage, detail, warning);
	}

	progress.complete();
	if (session.logger())
	{
		session.logger()->info(
			"[import={}] succeeded mode=FiniteElement model=\"{}\" part=\"{}\""
			" nodes={} trusses={} submitted={} build_duration_ms={}"
			" total_duration_ms={}",
			session.importId(),
			request.modelName.toLocal8Bit().toStdString(),
			request.partName.toLocal8Bit().toStdString(),
			feData.nodes().size(), feData.trusses().size(),
			buildResult.createdCount, stageTimer.elapsed(), session.elapsed());
	}
	session.finish();
	return DxfImportOutcome::success(buildResult.createdCount);
}

DxfImportOutcome importSketch(
	const DxfImportRequest& request,
	DxfData& dxfData,
	const DxfEntityStats& entityStats,
	std::size_t outputLimit,
	DxfImportSession& session,
	QElapsedTimer& stageTimer)
{
	stageTimer.restart();
	SamData samData;
	ConversionEngine converter;
	if (!converter.convert(
			dxfData, request.baseX, request.baseY, request.baseZ,
			request.curveTolerance, samData, outputLimit))
	{
		const QString errorCode =
			DxfImportFormatting::errorCodeText(samData.errorCode());
		const QString errorMessage = samData.errorMessage().isEmpty()
			? QStringLiteral("no valid entities to import")
			: samData.errorMessage();
		DxfImportFeedback::showBudgetError(
			samData.errorCode(), request.maxOutputEntities);
		return failedImport(
			session,
			DxfImportOutcomeStatus::ConvertFailed,
			samData.errorCode(),
			"conversion",
			" error_code=" + errorCode.toStdString() +
			" error=\"" + errorMessage.toLocal8Bit().toStdString() + "\"",
			QString("[importDxf] ERROR [%1]: %2")
				.arg(errorCode, errorMessage));
	}

	DxfImportFeedback::showSmallDrawingRecommendation(
		samData.lines().size() + samData.circles().size(),
		request.maxOutputEntities);
	dxfData.clear();
	if (session.logger())
	{
		session.logger()->info(
			"[import={}] conversion_completed lines={} circles={}"
			" curve_tolerance={} duration_ms={}",
			session.importId(), samData.lines().size(), samData.circles().size(),
			request.curveTolerance, stageTimer.elapsed());
	}
	logConvertedSamData(session.logger(), session.importId(), samData);

	stageTimer.restart();
	DxfImportProgress progress(
		DxfImportProgressMode::Sketch,
		samData.lines().size(), samData.circles().size());
	SamBuilder builder;
	builder.setProgressCallback(
		[&progress](const QString& stage, int current, int total) {
			return progress.update(stage, current, total);
		});
	const ImportBuildResult buildResult =
		DxfImportBuildService::buildSamSketch(samData, builder);
	if (!buildResult.succeeded())
	{
		progress.close();
		const bool canceled =
			buildResult.status == ImportBuildStatus::Canceled;
		const std::string stage =
			DxfImportFormatting::buildStage(buildResult.status);
		std::string detail =
			" error=\"" + buildResult.message.toLocal8Bit().toStdString() + "\"";
		if (buildResult.status != ImportBuildStatus::BeginFailed)
		{
			detail = " sketch=\"" +
				builder.sketchName().toLocal8Bit().toStdString() + "\"" + detail;
		}
		const QString warning = canceled
			? QString("[importDxf] IMPORT CANCELED — %1 %2/%3")
				.arg(progress.canceledStage())
				.arg(progress.canceledCurrent())
				.arg(progress.canceledTotal())
			: QString("[importDxf] ERROR: build failed, rolling back — %1")
				.arg(buildResult.message);
		return failedBuildImport(
			session, buildResult, stage, detail, warning);
	}

	progress.complete();
	qDebug().noquote() << DxfImportFormatting::summaryText(
		buildResult.createdCount, entityStats);
	if (session.logger())
	{
		session.logger()->info(
			"[import={}] succeeded sketch=\"{}\" submitted={}"
			" build_duration_ms={} total_duration_ms={}",
			session.importId(),
			builder.sketchName().toLocal8Bit().toStdString(),
			buildResult.createdCount, stageTimer.elapsed(), session.elapsed());
	}
	session.finish();
	return DxfImportOutcome::success(buildResult.createdCount);
}

} // namespace

DxfImportOutcome runDxfImport(const DxfImportRequest& request)
{
	DxfImportSession session(
		request.filePath, request.baseX, request.baseY, request.baseZ,
		request.curveTolerance, request.maxOutputEntities);

	const DxfImportPreflightResult preflight =
		validateDxfImportRequest(request);
	if (!preflight.valid)
	{
		return failedImport(
			session,
			DxfImportOutcomeStatus::ValidationFailed,
			DxfImportErrorCode::InvalidArgument,
			preflight.stage, preflight.detail, preflight.message);
	}
	const std::set<std::string> ignoredLayers =
		parseIgnoredDxfLayers(request.ignoredLayers);

	QElapsedTimer stageTimer;
	stageTimer.start();
	DxfData dxfData;
	DxfParser parser;
	if (!parser.parseFile(
			request.filePath, dxfData, request.curveTolerance,
			ignoredLayers, preflight.outputLimit))
	{
		const QString errorCode =
			DxfImportFormatting::errorCodeText(dxfData.errorCode());
		const QString errorMessage = dxfData.errorMessage();
		DxfImportFeedback::showBudgetError(
			dxfData.errorCode(), request.maxOutputEntities);
		return failedImport(
			session,
			DxfImportOutcomeStatus::ParseFailed,
			dxfData.errorCode(),
			"parse",
			" error_code=" + errorCode.toStdString() +
			" error=\"" + errorMessage.toLocal8Bit().toStdString() + "\"",
			QString("[importDxf] ERROR [%1]: DXF parse failed - %2")
				.arg(errorCode, errorMessage));
	}

	if (session.logger())
	{
		session.logger()->info(
			"[import={}] parse_completed entities={} points={} lines={} circles={}"
			" arcs={} lw_polylines={} ellipses={} duration_ms={}",
			session.importId(), dxfData.entityCount(), dxfData.points().size(),
			dxfData.lines().size(), dxfData.circles().size(),
			dxfData.arcs().size(), dxfData.lwPolylines().size(),
			dxfData.ellipses().size(), stageTimer.elapsed());
	}
	logRawDxfData(session.logger(), session.importId(), dxfData);
	const DxfEntityStats entityStats = dxfData.entityStats();

	if (preflight.mode == DxfImportMode::FiniteElement)
	{
		return importFiniteElement(
			request, dxfData, preflight.outputLimit, session, stageTimer);
	}
	return importSketch(
		request, dxfData, entityStats,
		preflight.outputLimit, session, stageTimer);
}
