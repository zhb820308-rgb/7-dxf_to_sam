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
	//第一个参数与项目输出的库名称相同
}


Example1PytModule::~Example1PytModule()
{
	// do nothing
}



// ========================================================================
//  DXF import helpers
// ========================================================================

int Example1PytModule::buildSamSketch(const SamData& samData, SamBuilder& builder)
{
	if (!builder.beginImport())
	{
		return -1;
	}

	int created = 0;
	created += builder.createPoints(samData.points());
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
	// ① 参数解析
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
	if (!ignoredLayers.empty()) {
		qDebug() << "[importDxf] 忽略图层:" << ignoreLayersStr;
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

	qDebug() << "[importDxf] ====== DXF 导入开始 ======";
	qDebug() << "[importDxf] 文件路径:" << filePath;
	qDebug() << "[importDxf] 基点:" << baseX << baseY << baseZ;
	qDebug() << "[importDxf] 曲线离散容差:" << curveTolerance;

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

	// ⑤ 阶段2：坐标转换（含离散化）
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
			"[import={}] conversion_completed points={} lines={} circles={} curve_tolerance={} duration_ms={}",
			importId,
			samData.points().size(),
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
	qDebug() << "[importDxf] ====== 导入完成, 共" << created << "个图元 =====";
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
