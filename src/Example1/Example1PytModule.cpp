#include <omuArguments.h>
#include <omuPrimNumber.h>
#include <omuPrimType.h>

#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QElapsedTimer>
#include <QProgressDialog>
#include <cmath>
#include <set>
#include <string>

#include "DxfImportLogger.h"
#include "DxfParser.h"
#include "ConversionEngine.h"
#include "SamData.h"
#include "SamBuilder.h"
#include "FeConversionEngine.h"
#include "FeData.h"
#include "PythonFiniteElementBuilder.h"

#include <Example1PytModule.h>


static omuInterfaceObj::methodTable Example1PytModuleMethods[] =
{
	{"importDxf", ((omuInterfaceObj::methodFunc)&Example1PytModule::importDxf)},

	{0, 0}
};

Example1PytModule::Example1PytModule()
	: pyoModule("Example1", Example1PytModuleMethods, pyoModule::NO_IMPORT)
{
	//第一个参数与项目输出的库名称相同
}


Example1PytModule::~Example1PytModule()
{
	// do nothing
}



int Example1PytModule::buildSamSketch(const SamData& samData, SamBuilder& builder)
{
	if (!builder.beginImport())
	{
		return -1;
	}

	int created = 0;
	created += builder.createLines(samData.lines());
	created += builder.createCircles(samData.circles());

	if (!builder.commit())
	{
		builder.rollback();
		return -1;
	}

	return created;
}
int Example1PytModule::buildFePart(FeData& feData,
	const QString& modelName, const QString& partName,
	PythonFiniteElementBuilder& builder)
{
	if (!builder.beginImport(modelName, partName))
	{
		return -1;
	}

	const int nodeCount = builder.createNodes(feData.nodes());
	if (nodeCount < 0)
	{
		builder.rollback();
		return -1;
	}

	const int trussCount = builder.createTrusses(feData.trusses());
	if (trussCount < 0)
	{
		builder.rollback();
		return -1;
	}

	if (!builder.commit())
	{
		builder.rollback();
		return -1;
	}

	return nodeCount + trussCount;
}

void Example1PytModule::DefineConstants()
{
	// do nothing
}

omuPrimitive* Example1PytModule::importDxf(omuArguments& args)
{
	// ① 参数解析
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

	// ② 日志初始化
	const std::string importId = QDateTime::currentDateTimeUtc()
		.toString("yyyyMMdd_HHmmss_zzz")
		.toStdString();
	std::shared_ptr<spdlog::logger> logger =
		createDxfImportLogger(importId);
	std::shared_ptr<spdlog::logger> errorLogger = dxfErrorLogger();
	QElapsedTimer totalTimer;
	totalTimer.start();

	const std::string pathText = filePath.toLocal8Bit().toStdString();
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
			"[import={}] started file=\"{}\" base=({}, {}, {}) curve_tolerance={}",
			importId, pathText, baseX, baseY, baseZ, curveTolerance);
	}

	// ③ 阶段1：解析 DXF 文件
	QElapsedTimer stageTimer;
	stageTimer.start();
	DxfData dxfData;
	DxfParser parser;
	if (!parser.parseFile(filePath, dxfData, curveTolerance, ignoredLayers))
	{
		std::string detail = " error=\"" + dxfData.errorMessage().toLocal8Bit().toStdString() + "\"";
		return failImport(logger, errorLogger, importId, pathText,
			"parse", detail, totalTimer.elapsed(),
			QString("[importDxf] ERROR: DXF parse failed — %1").arg(dxfData.errorMessage()));
	}

	// ④ 解析完成日志
	if (logger)
	{
		logger->info(
			"[import={}] parse_completed points={} lines={} circles={} arcs={} lw_polylines={} ellipses={} duration_ms={}",
			importId,
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

	// ⑤ mode branching
	const bool isFeMode = (importModeStr.compare(
		QStringLiteral("FiniteElement"), Qt::CaseInsensitive) == 0);
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
		// FE mode: validate required parameters
		if (modelName.isEmpty())
		{
			return failImport(logger, errorLogger, importId, pathText,
				"validate_params", " modelName is empty",
				totalTimer.elapsed(),
				QString("[importDxf] ERROR: FE mode requires 'modelName'"));
		}
		if (partName.isEmpty())
		{
			return failImport(logger, errorLogger, importId, pathText,
				"validate_params", " partName is empty",
				totalTimer.elapsed(),
				QString("[importDxf] ERROR: FE mode requires 'partName'"));
		}
		if (!std::isfinite(nodeMergeTolerance) || nodeMergeTolerance < 0.0)
		{
			return failImport(logger, errorLogger, importId, pathText,
				"validate_params", " invalid nodeMergeTolerance",
				totalTimer.elapsed(),
				QString("[importDxf] ERROR: invalid nodeMergeTolerance %1")
					.arg(nodeMergeTolerance));
		}

		// FE conversion
		stageTimer.restart();
		FeData feData;
		FeConversionEngine feConverter;
		if (!feConverter.convert(dxfData, baseX, baseY, baseZ,
			curveTolerance, nodeMergeTolerance, feData))
		{
			return failImport(logger, errorLogger, importId, pathText,
				"fe_conversion", "", totalTimer.elapsed(),
				QString("[importDxf] WARNING: no valid FE nodes to import"));
		}
		dxfData.clear();

		const FeConversionStats& feStats = feData.stats();
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
		// FE build
		stageTimer.restart();

		QProgressDialog feProgressDialog(
			QStringLiteral("Importing DXF as FE Part..."),
			QStringLiteral("Cancel"), 0, 100);
		feProgressDialog.setWindowTitle(QStringLiteral("DXF Import (FE)"));
		feProgressDialog.setWindowModality(Qt::ApplicationModal);
		feProgressDialog.setMinimumDuration(0);
		feProgressDialog.show();
		QCoreApplication::processEvents();

		PythonFiniteElementBuilder builder;

		QString canceledStage;
		int canceledCurrent = 0, canceledTotal = 0;
		builder.setProgressCallback(
			[&](const QString& stage, int current, int total) -> bool {
				int value = 20;
				if (stage == QStringLiteral("Creating FE nodes")) {
					if (total > 0)
						value = 20 + static_cast<int>(40.0 * current / total);
					else
						value = 60;
				} else {
					if (total > 0)
						value = 60 + static_cast<int>(40.0 * current / total);
					else
						value = 100;
				}
				feProgressDialog.setLabelText(
					QStringLiteral("%1: %2 / %3").arg(stage).arg(current).arg(total));
				feProgressDialog.setValue(value);
				QCoreApplication::processEvents();
				if (!feProgressDialog.wasCanceled()) return true;
				canceledStage = stage;
				canceledCurrent = current;
				canceledTotal = total;
				return false;
			});

		const int feCreated = buildFePart(feData, modelName, partName, builder);
		if (feCreated < 0)
		{
			bool isBeginImport = (builder.lastError().contains(
				QStringLiteral("already exists")) ||
				builder.lastError().startsWith(
					QStringLiteral("beginImport")));
			bool isCanceled = (builder.lastError() ==
				QStringLiteral("import canceled by user"));
			std::string feStage = isBeginImport ? "begin_import"
				: isCanceled ? "canceled" : "commit";
			std::string feDetail = " error=\"" +
				builder.lastError().toLocal8Bit().toStdString() + "\"";
			QString qWarningMsg;
			if (isBeginImport)
			{
				qWarningMsg = QString(
					"[importDxf] ERROR: FE beginImport failed — %1")
					.arg(builder.lastError());
			}
			else if (isCanceled)
			{
				qWarningMsg = QString(
					"[importDxf] IMPORT CANCELED — %1 %2/%3")
					.arg(canceledStage).arg(canceledCurrent).arg(canceledTotal);
			}
			else
			{
				feDetail = " model=\"" + modelName.toLocal8Bit().toStdString()
					+ "\" part=\"" + partName.toLocal8Bit().toStdString()
					+ "\"" + feDetail;
				qWarningMsg = QString(
					"[importDxf] ERROR: FE commit failed, rolling back — %1")
					.arg(builder.lastError());
			}
			return failImport(logger, errorLogger, importId, pathText,
				feStage, feDetail, totalTimer.elapsed(), qWarningMsg);
		}

		// FE success
		feProgressDialog.setValue(100);
		const int nodeCnt = static_cast<int>(feData.nodes().size());
		const int trussCnt = static_cast<int>(feData.trusses().size());
		if (logger)
		{
			logger->info(
				"[import={}] succeeded mode=FiniteElement"
				" model=\"{}\" part=\"{}\""
				" nodes={} trusses={}"
				" merged_nodes={} skipped_zero_length={} skipped_dup_truss={}"
				" build_duration_ms={} total_duration_ms={}",
				importId,
				modelName.toLocal8Bit().toStdString(),
				partName.toLocal8Bit().toStdString(),
				nodeCnt,
				trussCnt,
				feStats.mergedNodes,
				feStats.skippedZeroLength,
				feStats.skippedDuplicateTruss,
				stageTimer.elapsed(),
				totalTimer.elapsed());
			logger->flush();
		}
		dropDxfImportLogger(importId);
		return new omuPrimNumber(feCreated);
	}

	// (Sketch mode continues below)
	// ⑤ 阶段2 (Sketch)：坐标转换（含离散化）
	stageTimer.restart();
	SamData samData;
	ConversionEngine convEngine;
	if (!convEngine.convert(dxfData, baseX, baseY, baseZ, curveTolerance, samData))
	{
		return failImport(logger, errorLogger, importId, pathText,
			"conversion", "", totalTimer.elapsed(),
			QString("[importDxf] WARNING: no valid entities to import"));
	}

	// 释放解析数据，降低峰值内存（转换完成后 DxfData 不再需要）
	dxfData.clear();

	// ⑥ 转换完成日志
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

	// ⑦ 阶段3：构建 SAM 草图并提交
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
	builder.setProgressCallback(
		[&](const QString& stage, int current, int total) -> bool {
			int value = 20;
			if (stage == QStringLiteral("Creating lines")) {
				if (total > 0)
					value = 20 + static_cast<int>(80.0 * current / total);
				else
					value = 100;  // empty dataset → done
			} else if (stage == QStringLiteral("Creating circles")) {
				value = 100;
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

	int created = buildSamSketch(samData, builder);
	if (created < 0)
	{
		bool isBeginImport = (builder.lastError() == QString("failed to create sketch"));
		bool isCanceled = (builder.lastError() == QString("import canceled by user"));
		std::string stage = isBeginImport ? "begin_import" : isCanceled ? "canceled" : "commit";
		std::string detail = " error=\"" + builder.lastError().toLocal8Bit().toStdString() + "\"";
		QString qWarningMsg;
		if (isBeginImport)
		{
			qWarningMsg = QString("[importDxf] ERROR: failed to create sketch — %1").arg(builder.lastError());
		}
		else if (isCanceled)
		{
			qWarningMsg = QString("[importDxf] IMPORT CANCELED — %1 %2/%3")
				.arg(canceledStage).arg(canceledCurrent).arg(canceledTotal);
		}
		else
		{
			detail = " sketch=\"" + builder.sketchName().toLocal8Bit().toStdString() + "\"" + detail;
			qWarningMsg = QString("[importDxf] ERROR: commit failed, rolling back — %1").arg(builder.lastError());
		}
		return failImport(logger, errorLogger, importId, pathText,
			stage, detail, totalTimer.elapsed(), qWarningMsg);
	}

	// ⑧ 成功
	progressDialog.setValue(100);
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
