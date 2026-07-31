#include <omuArguments.h>
#include <omuPrimNumber.h>
#include <omuPrimType.h>

#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QElapsedTimer>
#include <QProgressDialog>
#include <QStringList>
#include <cmath>
#include <set>
#include <string>

#include "DxfImportLogger.h"
#include "DxfParser.h"
#include "ConversionEngine.h"
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

static QString boolText(bool value)
{
	return value ? QStringLiteral("true") : QStringLiteral("false");
}

static QString splineKindText(const SplineKind& kind)
{
	const QString construction = kind.construction == SplineConstruction::ControlBased
		? QStringLiteral("ControlBased")
		: QStringLiteral("FitBased");
	return QStringLiteral("%1|rational=%2|periodic=%3|closed=%4")
		.arg(construction)
		.arg(boolText(kind.rational))
		.arg(boolText(kind.periodic))
		.arg(boolText(kind.closed));
}

static QString importSummaryText(int created, const DxfEntityStats& stats)
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
		"[importDxf] Import complete: imported entities=%1; source (after layer filter and block expansion): "
		"lines=%2, polylines=%3, curves=%4 (circles=%5, arcs=%6, ellipses=%7, splines=%8; spline categories=[%9])")
		.arg(created)
		.arg(static_cast<qulonglong>(stats.lines))
		.arg(static_cast<qulonglong>(stats.lwPolylines))
		.arg(static_cast<qulonglong>(stats.curveCount()))
		.arg(static_cast<qulonglong>(stats.circles))
		.arg(static_cast<qulonglong>(stats.arcs))
		.arg(static_cast<qulonglong>(stats.ellipses))
		.arg(static_cast<qulonglong>(stats.splineCount()))
		.arg(splineCategories.join(QStringLiteral(", ")));
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
	args.Begin();
	args.Get(filePath);
	args.Get(baseX);
	args.Get(baseY);
	args.Get(baseZ);
	args.Optional();
	args.Get(curveTolerance, "curveTolerance");
	args.Get(ignoreLayersStr, "ignoreLayers");
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

	// [3/8] Stage 1: Parse DXF file
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

	// [4/8] Log parse results
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

	// [5/8] Stage 2: Coordinate conversion (incl. tessellation)
	stageTimer.restart();
	SamData samData;
	ConversionEngine convEngine;
	if (!convEngine.convert(dxfData, baseX, baseY, baseZ, curveTolerance, samData))
	{
		return failImport(logger, errorLogger, importId, pathText,
			"conversion", "", totalTimer.elapsed(),
			QString("[importDxf] WARNING: no valid entities to import"));
	}

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

	// [8/8] Success
	progressDialog.setValue(100);
	qDebug().noquote() << importSummaryText(created, entityStats);
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
