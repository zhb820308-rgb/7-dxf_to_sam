#include <omuArguments.h>
#include <omuPrimNumber.h>
#include <omuPrimType.h>

#include <QDateTime>
#include <QDebug>
#include <QElapsedTimer>
#include <QCoreApplication>
#include <QProgressDialog>
#include <cmath>

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

bool Example1PytModule::parseDxfFile(const QString& filePath, DxfData& outData)
{
	DxfParser parser;
	return parser.parseFile(filePath, outData);
}

bool Example1PytModule::convertToSamData(const DxfData& dxfData, double baseX, double baseY,
                                          double baseZ, double tolerance, SamData& outData)
{
	ConversionEngine engine;
	return engine.convert(dxfData, baseX, baseY, baseZ, tolerance, outData);
}

int Example1PytModule::buildSamSketch(const SamData& samData, SamBuilder& builder)
{
	if (!builder.beginImport())
	{
		return -1;
	}

	int created = 0;
	int stepCreated = builder.createPoints(samData.points());
	if (stepCreated < 0)
	{
		builder.rollback();
		return -1;
	}
	created += stepCreated;

	stepCreated = builder.createLines(samData.lines());
	if (stepCreated < 0)
	{
		builder.rollback();
		return -1;
	}
	created += stepCreated;

	stepCreated = builder.createCircles(samData.circles());
	if (stepCreated < 0)
	{
		builder.rollback();
		return -1;
	}
	created += stepCreated;

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
	args.Begin();
	args.Get(filePath);
	args.Get(baseX);
	args.Get(baseY);
	args.Get(baseZ);
	args.Optional();
	args.Get(curveTolerance, "curveTolerance");
	args.End();

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

	QProgressDialog progressDialog(
		QStringLiteral("Importing DXF..."),
		QStringLiteral("Cancel"),
		0,
		100);
	progressDialog.setWindowTitle(QStringLiteral("DXF Import"));
	progressDialog.setWindowModality(Qt::ApplicationModal);
	progressDialog.setMinimumDuration(0);
	progressDialog.setAutoClose(false);
	progressDialog.setAutoReset(false);

	auto updateProgress = [&](int value, const QString& label) -> bool {
		progressDialog.setLabelText(label);
		progressDialog.setValue(value);
		QCoreApplication::processEvents();
		return !progressDialog.wasCanceled();
	};

	auto cancelImport = [&](const QString& stage, int current, int total)
		-> omuPrimitive* {
		if (logger)
		{
			logger->warn(
				"[import={}] canceled stage=\"{}\" progress={}/{} total_elapsed_ms={}",
				importId,
				stage.toLocal8Bit().toStdString(),
				current,
				total,
				totalTimer.elapsed());
			logger->flush();
		}
		qWarning() << "[importDxf] canceled by user during"
		           << stage << current << "/" << total;
		dropDxfImportLogger(importId);
		return nullptr;
	};

	progressDialog.show();
	if (!updateProgress(0, QStringLiteral("Importing DXF file (0-10%)")))
		return cancelImport(QStringLiteral("import"), 0, 10);

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
	if (!updateProgress(10, QStringLiteral("Parsing DXF (10-20%)")))
		return cancelImport(QStringLiteral("parse"), 10, 20);

	DxfData dxfData;
	if (!parseDxfFile(filePath, dxfData))
	{
		std::string detail = " error=\"" + dxfData.errorMessage().toLocal8Bit().toStdString() + "\"";
		return failImport(logger, errorLogger, importId, pathText,
			"parse", detail, totalTimer.elapsed(),
			QString("[importDxf] ERROR: DXF parse failed — %1").arg(dxfData.errorMessage()));
	}

	// ④ 解析完成日志
	if (!updateProgress(20, QStringLiteral("DXF parsed (20%)")))
		return cancelImport(QStringLiteral("parse"), 20, 20);

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
	if (!convertToSamData(dxfData, baseX, baseY, baseZ, curveTolerance, samData))
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
	if (!updateProgress(20, QStringLiteral("Creating lines (20-100%)")))
		return cancelImport(QStringLiteral("create_lines"), 20, 100);

	// ⑦ 阶段3：构建 SAM 草图并提交
	stageTimer.restart();
	SamBuilder builder;
	const int lineTotal = static_cast<int>(samData.lines().size());

	QString canceledStage;
	int canceledCurrent = 0;
	int canceledTotal = lineTotal;
	builder.setProgressCallback(
		[&](const QString& stage, int current, int total) -> bool {
			int value = 20;
			if (stage == QStringLiteral("Creating lines") && total > 0)
			{
				value = 20 + static_cast<int>(
					(static_cast<long long>(current) * 80) / total);
			}
			else if (stage == QStringLiteral("Creating circles"))
			{
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
		if (builder.lastError() == QStringLiteral("import canceled by user"))
		{
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
			qWarning() << "[importDxf] canceled by user during" << canceledStage
			           << canceledCurrent << "/" << canceledTotal;
			dropDxfImportLogger(importId);
			return nullptr;
		}

		bool isBeginImport = (builder.lastError() == QString("failed to create sketch"));
		std::string stage = isBeginImport ? "begin_import" : "commit";
		std::string detail = " error=\"" + builder.lastError().toLocal8Bit().toStdString() + "\"";
		QString qWarningMsg;
		if (isBeginImport)
		{
			qWarningMsg = QString("[importDxf] ERROR: failed to create sketch — %1").arg(builder.lastError());
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
	progressDialog.setValue(100);
	progressDialog.close();
	dropDxfImportLogger(importId);
	return new omuPrimNumber(created);
}
