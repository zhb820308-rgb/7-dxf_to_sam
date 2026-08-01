#include <omuArguments.h>
#include <omuPrimNumber.h>
#include <omuPrimType.h>

#include <QCoreApplication>
#include <QDebug>
#include <QElapsedTimer>
#include <QProgressDialog>
#include <QStringList>
#include <set>
#include <string>

#include "DxfImportLogger.h"
#include "DxfImportSession.h"
#include "DxfImportBuildService.h"
#include "DxfImportFeedback.h"
#include "DxfImportFormatting.h"
#include "DxfImportValidation.h"
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
	DxfImportSession importSession(
		filePath, baseX, baseY, baseZ, curveTolerance, maxOutputEntities);
	const auto& logger = importSession.logger();
	const auto& errorLogger = importSession.errorLogger();
	const std::string& importId = importSession.importId();
	const std::string& pathText = importSession.pathText();

	// Validate numeric inputs before parsing. Block expansion may tessellate
	// curves, so invalid tolerances must not reach DxfParser/GeometryUtils.
	const DxfImportValidation::Result validation =
		DxfImportValidation::validate(
			baseX, baseY, baseZ, curveTolerance,
			nodeMergeTolerance, maxOutputEntities);
	if (!validation.valid)
	{
		return failImport(
			logger, errorLogger, importId, pathText,
			"validate_params", validation.detail,
			importSession.elapsed(), validation.message);
	}
	const std::size_t outputLimit = validation.outputLimit;

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
		DxfImportFeedback::showBudgetError(
			dxfData.errorCode(), maxOutputEntities);
		std::string detail = " error_code=" + errorCode.toStdString() +
			" error=\"" + errorMessage.toLocal8Bit().toStdString() + "\"";
		return failImport(logger, errorLogger, importId, pathText,
			"parse", detail, importSession.elapsed(),
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
			importSession.elapsed(),
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
				importSession.elapsed(),
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
			DxfImportFeedback::showBudgetError(
				feData.errorCode(), maxOutputEntities);
			return failImport(logger, errorLogger, importId, pathText,
				"fe_conversion",
				" error_code=" + errorCode.toStdString() +
				" error=\"" + errorMessage.toLocal8Bit().toStdString() + "\"",
				importSession.elapsed(),
				QString("[importDxf] ERROR [%1]: %2")
					.arg(errorCode, errorMessage));
		}
		DxfImportFeedback::showSmallDrawingRecommendation(
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

		const ImportBuildResult buildResult =
			DxfImportBuildService::buildFePart(
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
				stage, detail, importSession.elapsed(), warning);
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
				importSession.elapsed());
		}
		importSession.finish();
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
		DxfImportFeedback::showBudgetError(
			samData.errorCode(), maxOutputEntities);
		return failImport(logger, errorLogger, importId, pathText,
			"conversion",
			" error_code=" + errorCode.toStdString() +
			" error=\"" + errorMessage.toLocal8Bit().toStdString() + "\"",
			importSession.elapsed(),
			QString("[importDxf] ERROR [%1]: %2")
				.arg(errorCode, errorMessage));
	}
	DxfImportFeedback::showSmallDrawingRecommendation(
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

	const ImportBuildResult buildResult =
		DxfImportBuildService::buildSamSketch(samData, builder);
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
				importSession.elapsed());
		}
		qWarning().noquote() << QString("[importDxf] IMPORT CANCELED — %1 %2/%3")
			.arg(canceledStage).arg(canceledCurrent).arg(canceledTotal);
		importSession.finish();
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
			stage, detail, importSession.elapsed(),
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
			importSession.elapsed());
	}
	importSession.finish();
	return new omuPrimNumber(created);
}
