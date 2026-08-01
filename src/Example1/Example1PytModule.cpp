#include <omuArguments.h>
#include <omuPrimNumber.h>
#include <omuPrimType.h>

#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QMessageBox>
#include <QElapsedTimer>
#include <QProgressDialog>
#include <QStringList>
#include <cmath>
#include <limits>
#include <set>
#include <string>

#include "DxfImportLogger.h"
#include "DxfImportFormatting.h"
#include "DxfImportError.h"
#include "DxfParser.h"
#include "ConversionEngine.h"
#include "FeConversionEngine.h"
#include "FeData.h"
#include "PythonFiniteElementBuilder.h"
#include "SamData.h"
#include "SamBuilder.h"

#include <Example1PytModule.h>


static omuInterfaceObj::methodTable Example1PytModuleMethods[] =
{
	{"importDxf", ((omuInterfaceObj::methodFunc)&Example1PytModule::importDxf)},
	{0, 0}
};

Example1PytModule::Example1PytModule()
	: pyoModule("Example1", Example1PytModuleMethods, pyoModule::NO_IMPORT)
{
	// The first argument must match the project output library name
}


Example1PytModule::~Example1PytModule()
{
	// do nothing
}



// ========================================================================
//  DXF import helpers
// ========================================================================

constexpr std::size_t kSmallDrawingEntityLimit = 100000;
constexpr int kLargeDrawingEntityLimit = 500000;

static bool isBudgetError(DxfImportErrorCode code)
{
	return code == DxfImportErrorCode::ExpansionLimit ||
		code == DxfImportErrorCode::ConversionLimit;
}

static void showBudgetErrorDialog(DxfImportErrorCode code,
	int maxOutputEntities)
{
	if (!isBudgetError(code))
		return;
	if (maxOutputEntities < 0)
	{
		QMessageBox::warning(
			nullptr,
			QStringLiteral("DXF Import - Expansion Safety Limit"),
			QStringLiteral(
				"The drawing exceeded an INSERT expansion safety limit.\n\n"
				"Unlimited mode only removes the final output limit. "
				"Please simplify the block structure or reduce the array size."));
		return;
	}
	if (maxOutputEntities <= static_cast<int>(kSmallDrawingEntityLimit))
	{
		QMessageBox::warning(
			nullptr,
			QStringLiteral("DXF Import - Drawing Too Large"),
			QStringLiteral(
				"The drawing exceeds the small drawing limit of 100,000 entities.\n\n"
				"Please reopen the DXF Import dialog and select Large drawing."));
		return;
	}

	QMessageBox::warning(
		nullptr,
		QStringLiteral("DXF Import - Drawing Too Large"),
		QStringLiteral(
			"The drawing still exceeds the selected large drawing limit of %1 entities.\n\n"
			"Increase the curve tolerance or simplify the drawing before importing.")
			.arg(maxOutputEntities));
}

static void showSmallDrawingRecommendation(std::size_t outputEntities,
	int maxOutputEntities)
{
	if (maxOutputEntities == static_cast<int>(kSmallDrawingEntityLimit) ||
		outputEntities > kSmallDrawingEntityLimit)
	{
		return;
	}

	QMessageBox::information(
		nullptr,
		QStringLiteral("DXF Import Recommendation"),
		QStringLiteral(
			"The converted result contains %1 entities, which is within the small drawing limit.\n\n"
			"For lower memory usage, consider selecting Small drawing next time.")
			.arg(static_cast<qulonglong>(outputEntities)));
}

template <typename Builder>
static ImportBuildResult rollbackAfterFailure(
	Builder& builder, ImportBuildResult failure, int previouslyCreated = 0)
{
	failure.createdCount += previouslyCreated;
	const ImportBuildResult cleanup = builder.rollback();
	if (cleanup.succeeded())
		return failure;

	const QString combined = failure.message.isEmpty()
		? cleanup.message
		: failure.message + QStringLiteral("; ") + cleanup.message;
	return ImportBuildResult::failure(
		ImportBuildStatus::RollbackFailed, combined, failure.createdCount);
}

ImportBuildResult Example1PytModule::buildSamSketch(
	const SamData& samData,
	SamBuilder& builder)
{
	ImportBuildResult result = builder.beginImport();
	if (!result.succeeded())
		return result;

	int created = 0;
	result = builder.createLines(samData.lines());
	if (!result.succeeded())
		return rollbackAfterFailure(builder, result);
	created += result.createdCount;

	result = builder.createCircles(samData.circles());
	if (!result.succeeded())
		return rollbackAfterFailure(builder, result, created);
	created += result.createdCount;

	result = builder.commit();
	if (!result.succeeded())
		return rollbackAfterFailure(builder, result);

	return ImportBuildResult::success(created);
}

ImportBuildResult Example1PytModule::buildFePart(
	FeData& feData,
	const QString& modelName,
	const QString& partName,
	PythonFiniteElementBuilder& builder)
{
	ImportBuildResult result = builder.beginImport(modelName, partName);
	if (!result.succeeded())
		return result;

	result = builder.createNodes(feData.nodes());
	if (!result.succeeded())
		return rollbackAfterFailure(builder, result);
	const int nodeCount = result.createdCount;

	result = builder.createTrusses(feData.trusses());
	if (!result.succeeded())
		return rollbackAfterFailure(builder, result, nodeCount);
	const int trussCount = result.createdCount;

	result = builder.commit();
	if (!result.succeeded())
		return rollbackAfterFailure(builder, result);

	return ImportBuildResult::success(nodeCount + trussCount);
}

void Example1PytModule::DefineConstants()
{
	// do nothing
}

omuPrimitive* Example1PytModule::importDxf(omuArguments& args)
{
	// [1/8] Parse arguments
	QString filePath;
	double baseX = 0.0;
	double baseY = 0.0;
	double baseZ = 0.0;
	double curveTolerance = ConversionEngine::defaultBulgeTolerance();
	QString ignoreLayersStr;
	QString importModeStr;
	QString modelName;
	QString partName;
	double nodeMergeTolerance = FeConversionEngine::defaultNodeMergeTolerance();
	int maxOutputEntities = 100000;
	args.Begin();
	args.Get(filePath);
	args.Get(baseX);
	args.Get(baseY);
	args.Get(baseZ);
	args.Optional();
	args.Get(curveTolerance, "curveTolerance");
	args.Get(ignoreLayersStr, "ignoreLayers");
	args.Get(importModeStr, "importMode");
	args.Get(modelName, "modelName");
	args.Get(partName, "partName");
	args.Get(nodeMergeTolerance, "nodeMergeTolerance");
	args.Get(maxOutputEntities, "maxOutputEntities");
	args.End();

	// Parse ignored layers
	std::set<std::string> ignoredLayers;
	if (!ignoreLayersStr.isEmpty()) {
		QStringList parts = ignoreLayersStr.split(',', QString::SkipEmptyParts);
		for (const QString& part : parts) {
			std::string layer = part.trimmed().toStdString();
			if (!layer.empty()) {
				ignoredLayers.insert(layer);
			}
		}
	}

	// [2/8] Initialize logging
	const std::string importId = QDateTime::currentDateTimeUtc()
		.toString("yyyyMMdd_HHmmss_zzz")
		.toStdString();
	std::shared_ptr<spdlog::logger> logger =
		createDxfImportLogger(importId);
	std::shared_ptr<spdlog::logger> errorLogger = dxfErrorLogger();
	QElapsedTimer totalTimer;
	totalTimer.start();

	// Log paths as UTF-8 so diagnostics preserve the original Unicode path.
	// libdxfrw receives its separately adapted narrow path in DxfInputFile.
	const std::string pathText = filePath.toUtf8().toStdString();
	if (!logger && errorLogger)
	{
		errorLogger->error(
			"[import={}] import_log_initialization_failed file=\"{}\"",
			importId, pathText);
	}
	if (!logger && !errorLogger)
	{
		qWarning() << "[importDxf] ERROR: log system unavailable for import"
			<< QString::fromStdString(importId);
	}

	if (logger)
	{
		logger->info(
			"[import={}] started file=\"{}\" base=({}, {}, {}) curve_tolerance={} max_output_entities={}",
			importId, pathText, baseX, baseY, baseZ, curveTolerance,
			maxOutputEntities);
	}

	// Validate numeric inputs before parsing. Block expansion may tessellate
	// curves, so invalid tolerances must not reach DxfParser/GeometryUtils.
	if (!std::isfinite(baseX) ||
		!std::isfinite(baseY) ||
		!std::isfinite(baseZ))
	{
		return failImport(logger, errorLogger, importId, pathText,
			"validate_params", " invalid_base_coordinates",
			totalTimer.elapsed(),
			QStringLiteral(
				"[importDxf] ERROR: base coordinates must be finite"));
	}
	if (!std::isfinite(curveTolerance) || curveTolerance <= 0.0)
	{
		return failImport(logger, errorLogger, importId, pathText,
			"validate_params", " invalid_curveTolerance",
			totalTimer.elapsed(),
			QString("[importDxf] ERROR: invalid curveTolerance %1")
				.arg(curveTolerance));
	}
	if (!std::isfinite(nodeMergeTolerance) || nodeMergeTolerance < 0.0)
	{
		return failImport(logger, errorLogger, importId, pathText,
			"validate_params", " invalid_nodeMergeTolerance",
			totalTimer.elapsed(),
			QString("[importDxf] ERROR: invalid nodeMergeTolerance %1")
				.arg(nodeMergeTolerance));
	}
	if (maxOutputEntities != static_cast<int>(kSmallDrawingEntityLimit) &&
		maxOutputEntities != kLargeDrawingEntityLimit &&
		maxOutputEntities != -1)
	{
		return failImport(logger, errorLogger, importId, pathText,
			"validate_params", " error_code=INVALID_ARGUMENT invalid_maxOutputEntities",
			totalTimer.elapsed(),
			QStringLiteral("[importDxf] ERROR [INVALID_ARGUMENT]: invalid maxOutputEntities"));
	}
	const std::size_t outputLimit = maxOutputEntities < 0
		? std::numeric_limits<std::size_t>::max()
		: static_cast<std::size_t>(maxOutputEntities);

	// [3/8] Stage 1: Parse DXF file
	QElapsedTimer stageTimer;
	stageTimer.start();
	DxfData dxfData;
	DxfParser parser;
	if (!parser.parseFile(filePath, dxfData, curveTolerance, ignoredLayers,
		outputLimit))
	{
		const QString errorCode = DxfImportFormatting::errorCodeText(
			dxfData.errorCode());
		const QString errorMessage = dxfData.errorMessage();
		showBudgetErrorDialog(dxfData.errorCode(), maxOutputEntities);
		std::string detail = " error_code=" + errorCode.toStdString() +
			" error=\"" + errorMessage.toLocal8Bit().toStdString() + "\"";
		return failImport(logger, errorLogger, importId, pathText,
			"parse", detail, totalTimer.elapsed(),
			QString("[importDxf] ERROR [%1]: DXF parse failed - %2")
				.arg(errorCode, errorMessage));
	}

	// [4/8] Log parse results
	if (logger)
	{
		logger->info(
			"[import={}] parse_completed entities={} points={} lines={} circles={} arcs={} lw_polylines={} ellipses={} duration_ms={}",
			importId,
			dxfData.entityCount(),
			dxfData.points().size(),
			dxfData.lines().size(),
			dxfData.circles().size(),
			dxfData.arcs().size(),
			dxfData.lwPolylines().size(),
			dxfData.ellipses().size(),
			stageTimer.elapsed());
	}
	logRawDxfData(logger, importId, dxfData);
	const DxfEntityStats entityStats = dxfData.entityStats();

	// [5/8] Select the import mode. Empty mode remains backward-compatible
	// with the original Sketch behavior.
	const bool isFeMode = importModeStr.compare(
		QStringLiteral("FiniteElement"), Qt::CaseInsensitive) == 0;
	if (!importModeStr.isEmpty() && !isFeMode &&
		importModeStr.compare(QStringLiteral("Sketch"), Qt::CaseInsensitive) != 0)
	{
		return failImport(logger, errorLogger, importId, pathText,
			"validate_mode",
			" mode=\"" + importModeStr.toLocal8Bit().toStdString() + "\"",
			totalTimer.elapsed(),
			QString("[importDxf] ERROR: unsupported importMode '%1'")
				.arg(importModeStr));
	}

	if (isFeMode)
	{
		if (modelName.isEmpty() || partName.isEmpty())
		{
			const QString missingName = modelName.isEmpty()
				? QStringLiteral("modelName")
				: QStringLiteral("partName");
			return failImport(logger, errorLogger, importId, pathText,
				"validate_params",
				" missing=\"" + missingName.toLocal8Bit().toStdString() + "\"",
				totalTimer.elapsed(),
				QString("[importDxf] ERROR: FE mode requires '%1'")
					.arg(missingName));
		}
		stageTimer.restart();
		FeData feData;
		FeConversionEngine feConverter;
		if (!feConverter.convert(dxfData, baseX, baseY, baseZ,
			curveTolerance, nodeMergeTolerance, feData,
			outputLimit))
		{
			const QString errorCode = DxfImportFormatting::errorCodeText(
				feData.errorCode());
			const QString errorMessage = feData.errorMessage().isEmpty()
				? QStringLiteral("no valid FE nodes to import")
				: feData.errorMessage();
			showBudgetErrorDialog(feData.errorCode(), maxOutputEntities);
			return failImport(logger, errorLogger, importId, pathText,
				"fe_conversion",
				" error_code=" + errorCode.toStdString() +
				" error=\"" + errorMessage.toLocal8Bit().toStdString() + "\"",
				totalTimer.elapsed(),
				QString("[importDxf] ERROR [%1]: %2")
					.arg(errorCode, errorMessage));
		}
		showSmallDrawingRecommendation(
			feData.nodes().size() + feData.trusses().size(),
			maxOutputEntities);
		dxfData.clear();

		const FeConversionStats feStats = feData.stats();
		if (logger)
		{
			logger->info(
				"[import={}] fe_conversion_completed nodes={} trusses={}"
				" merged={} skipped_zero_length={} skipped_dup_truss={}"
				" curve_tolerance={} node_merge_tolerance={} duration_ms={}",
				importId,
				feData.nodes().size(),
				feData.trusses().size(),
				feStats.mergedNodes,
				feStats.skippedZeroLength,
				feStats.skippedDuplicateTruss,
				curveTolerance,
				nodeMergeTolerance,
				stageTimer.elapsed());
		}

		stageTimer.restart();
		QProgressDialog progressDialog(
			QStringLiteral("Importing DXF as FE Part..."),
			QStringLiteral("Cancel"), 0, 100);
		progressDialog.setWindowTitle(QStringLiteral("DXF Import (FE)"));
		progressDialog.setWindowModality(Qt::ApplicationModal);
		progressDialog.setMinimumDuration(0);
		progressDialog.show();
		QCoreApplication::processEvents();

		PythonFiniteElementBuilder builder;
		QString canceledStage;
		int canceledCurrent = 0;
		int canceledTotal = 0;
		builder.setProgressCallback(
			[&](const QString& stage, int current, int total) -> bool {
				int value = stage == QStringLiteral("Creating FE nodes") ? 20 : 60;
				if (total > 0)
				{
					const int offset =
						stage == QStringLiteral("Creating FE nodes") ? 20 : 60;
					value = offset + static_cast<int>(40.0 * current / total);
				}
				progressDialog.setLabelText(
					QStringLiteral("%1: %2 / %3").arg(stage).arg(current).arg(total));
				progressDialog.setValue(value);
				QCoreApplication::processEvents();
				if (!progressDialog.wasCanceled())
					return true;

				canceledStage = stage;
				canceledCurrent = current;
				canceledTotal = total;
				return false;
			});

		const ImportBuildResult buildResult = buildFePart(
			feData, modelName, partName, builder);
		if (!buildResult.succeeded())
		{
			progressDialog.close();
			const bool canceled =
				buildResult.status == ImportBuildStatus::Canceled;
			const std::string stage = DxfImportFormatting::buildStage(
				buildResult.status);
			std::string detail =
				" error=\"" + buildResult.message.toLocal8Bit().toStdString() + "\"";
			if (!canceled &&
				buildResult.status != ImportBuildStatus::BeginFailed)
			{
				detail = " model=\"" + modelName.toLocal8Bit().toStdString() +
					"\" part=\"" + partName.toLocal8Bit().toStdString() +
					"\"" + detail;
			}
			const QString warning = canceled
				? QString("[importDxf] IMPORT CANCELED - %1 %2/%3")
					.arg(canceledStage).arg(canceledCurrent).arg(canceledTotal)
				: QString("[importDxf] ERROR: FE build failed, rolling back - %1")
					.arg(buildResult.message);
			return failImport(logger, errorLogger, importId, pathText,
				stage, detail, totalTimer.elapsed(), warning);
		}
		const int created = buildResult.createdCount;

		progressDialog.setValue(100);
		if (logger)
		{
			logger->info(
				"[import={}] succeeded mode=FiniteElement"
				" model=\"{}\" part=\"{}\" nodes={} trusses={}"
				" submitted={} build_duration_ms={} total_duration_ms={}",
				importId,
				modelName.toLocal8Bit().toStdString(),
				partName.toLocal8Bit().toStdString(),
				feData.nodes().size(),
				feData.trusses().size(),
				created,
				stageTimer.elapsed(),
				totalTimer.elapsed());
			logger->flush();
		}
		dropDxfImportLogger(importId);
		return new omuPrimNumber(created);
	}

	// [5/8] Stage 2: Coordinate conversion (incl. tessellation)
	stageTimer.restart();
	SamData samData;
	ConversionEngine convEngine;
	if (!convEngine.convert(dxfData, baseX, baseY, baseZ, curveTolerance,
		samData, outputLimit))
	{
		const QString errorCode = DxfImportFormatting::errorCodeText(
			samData.errorCode());
		const QString errorMessage = samData.errorMessage().isEmpty()
			? QStringLiteral("no valid entities to import")
			: samData.errorMessage();
		showBudgetErrorDialog(samData.errorCode(), maxOutputEntities);
		return failImport(logger, errorLogger, importId, pathText,
			"conversion",
			" error_code=" + errorCode.toStdString() +
			" error=\"" + errorMessage.toLocal8Bit().toStdString() + "\"",
			totalTimer.elapsed(),
			QString("[importDxf] ERROR [%1]: %2")
				.arg(errorCode, errorMessage));
	}
	showSmallDrawingRecommendation(
		samData.lines().size() + samData.circles().size(),
		maxOutputEntities);

	// Release parsed data to reduce peak memory (DxfData no longer needed after conversion)
	dxfData.clear();

	// [6/8] Log conversion results
	if (logger)
	{
		logger->info(
			"[import={}] conversion_completed lines={} circles={} curve_tolerance={} duration_ms={}",
			importId,
			samData.lines().size(),
			samData.circles().size(),
			curveTolerance,
			stageTimer.elapsed());
	}
	logConvertedSamData(logger, importId, samData);

	// [7/8] Stage 3: Build SAM sketch and commit
	stageTimer.restart();

	QProgressDialog progressDialog(
		QStringLiteral("Importing DXF..."),
		QStringLiteral("Cancel"),
		0, 100);
	progressDialog.setWindowTitle(QStringLiteral("DXF Import"));
	progressDialog.setWindowModality(Qt::ApplicationModal);
	progressDialog.setMinimumDuration(0);
	progressDialog.show();
	QCoreApplication::processEvents();

	SamBuilder builder;

	QString canceledStage;
	int canceledCurrent = 0, canceledTotal = 0;
	const int totalBuildEntities = static_cast<int>(
		samData.lines().size() + samData.circles().size());
	builder.setProgressCallback(
		[&](const QString& stage, int current, int total) -> bool {
			int value = 20;
			if (stage == QStringLiteral("Creating lines")) {
				if (totalBuildEntities > 0)
					value = 20 + static_cast<int>(
						79.0 * current / totalBuildEntities);
			} else if (stage == QStringLiteral("Creating circles")) {
				if (totalBuildEntities > 0)
					value = 20 + static_cast<int>(
						79.0 * (samData.lines().size() + current) /
						totalBuildEntities);
			} else if (stage == QStringLiteral("Finalizing import")) {
				value = 99;
			}

			progressDialog.setLabelText(
				QStringLiteral("%1: %2 / %3").arg(stage).arg(current).arg(total));
			progressDialog.setValue(value);
			QCoreApplication::processEvents();

			if (!progressDialog.wasCanceled())
				return true;

			canceledStage = stage;
			canceledCurrent = current;
			canceledTotal = total;
			return false;
		});

	const ImportBuildResult buildResult = buildSamSketch(samData, builder);
	if (buildResult.status == ImportBuildStatus::Canceled)
	{
		progressDialog.close();
		if (logger)
		{
			logger->warn(
				"[import={}] canceled stage=\"{}\" progress={}/{} total_elapsed_ms={}",
				importId,
				canceledStage.toLocal8Bit().toStdString(),
				canceledCurrent,
				canceledTotal,
				totalTimer.elapsed());
			logger->flush();
		}
		qWarning().noquote() << QString("[importDxf] IMPORT CANCELED — %1 %2/%3")
			.arg(canceledStage).arg(canceledCurrent).arg(canceledTotal);
		dropDxfImportLogger(importId);
		return nullptr;
	}

	if (!buildResult.succeeded())
	{
		const std::string stage = DxfImportFormatting::buildStage(
			buildResult.status);
		std::string detail =
			" error=\"" + buildResult.message.toLocal8Bit().toStdString() + "\"";
		if (buildResult.status != ImportBuildStatus::BeginFailed)
			detail = " sketch=\"" +
				builder.sketchName().toLocal8Bit().toStdString() + "\"" + detail;
		return failImport(logger, errorLogger, importId, pathText,
			stage, detail, totalTimer.elapsed(),
			QString("[importDxf] ERROR: build failed, rolling back — %1")
				.arg(buildResult.message));
	}

	const int created = buildResult.createdCount;
	// [8/8] Success
	progressDialog.setValue(100);
	qDebug().noquote() << DxfImportFormatting::summaryText(
		created, entityStats);
	if (logger)
	{
		logger->info(
			"[import={}] succeeded sketch=\"{}\" submitted={} build_duration_ms={} total_duration_ms={}",
			importId,
			builder.sketchName().toLocal8Bit().toStdString(),
			created,
			stageTimer.elapsed(),
			totalTimer.elapsed());
		logger->flush();
	}
	dropDxfImportLogger(importId);
	return new omuPrimNumber(created);
}
