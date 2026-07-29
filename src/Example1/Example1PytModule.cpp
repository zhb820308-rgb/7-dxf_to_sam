#include <omuArguments.h>
#include <omuPrimNumber.h>
#include <omuPrimType.h>

#include <QDateTime>
#include <QDebug>
#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QFileInfoList>
#include <algorithm>
#include <cmath>
#include <memory>
#include <string>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/rotating_file_sink.h>

#include "libdxfrw.h"
#include <gslPoint.h>
#include <gslMatrix.h>

#include <basBasis.h>

#include <gmlSketchRepository.h>
#include <gmlSketchWrapper.h>

#include <skcSketch.h>
#include <skcGeomFactory.h>
#include <skcKReposUtils.h>
#include <skcKToolset.h>
#include <skcKUtils.h>
#include <skcPDO.h>
#include <skcUndoRedoStack.h>
#include <skcUtils.h>
#include <DxfParser.h>
#include <ConversionEngine.h>
#include <SamData.h>
#include <SamBuilder.h>
#include <smgSceneManagerRole.h>

#include <gcuScene.h>
#include <gdyScene.h>

#include <sesKSessionState.h>

#include <QMessageBox>

#include <cmdCWIP.h>
#include <Example1PytModule.h>


static QString omuSquareBracket(const QString& name)
{
	return QString("[%1]").arg(name);
}


static gcuScene* gcuGetCurrentScene(bool force = true)
{
	return static_cast<gcuScene*>(gdyScene::GetCurrentScene(force));
}

static QString dxfLogRootDirectory()
{
	return QCoreApplication::applicationDirPath() + "/logs";
}

static void retainRecentImportLogs(
	const QString& importLogDirectory,
	const QString& currentLogPath)
{
	QDir directory(importLogDirectory);
	QFileInfoList logFiles = directory.entryInfoList(
		QStringList() << "dxf_import_*.log",
		QDir::Files | QDir::NoSymLinks,
		QDir::NoSort);

	std::sort(logFiles.begin(), logFiles.end(),
		[](const QFileInfo& left, const QFileInfo& right) {
			if (left.lastModified() != right.lastModified())
				return left.lastModified() > right.lastModified();
			return left.fileName() > right.fileName();
		});

	const QString currentAbsolutePath = QFileInfo(currentLogPath).absoluteFilePath();
	int retainedCount = 1;
	for (QFileInfoList::const_iterator it = logFiles.constBegin();
		 it != logFiles.constEnd(); ++it)
	{
		if (it->absoluteFilePath() == currentAbsolutePath)
			continue;

		if (retainedCount < 50)
		{
			++retainedCount;
			continue;
		}

		if (!QFile::remove(it->absoluteFilePath()))
		{
			qWarning() << "[importDxf] Failed to remove old import log:"
				<< it->absoluteFilePath();
		}
	}
}

static std::shared_ptr<spdlog::logger> createDxfImportLogger(
	const std::string& importId)
{
	try
	{
		const QString importLogDirectory =
			QDir(dxfLogRootDirectory()).filePath("imports");
		if (!QDir().mkpath(importLogDirectory))
		{
			qWarning() << "[importDxf] Failed to create import log directory:"
				<< importLogDirectory;
			return std::shared_ptr<spdlog::logger>();
		}

		const QString baseFileName = QString("dxf_import_%1")
			.arg(QString::fromStdString(importId));
		QString logPath = QDir(importLogDirectory).filePath(baseFileName + ".log");
		int duplicateIndex = 1;
		while (QFileInfo::exists(logPath))
		{
			logPath = QDir(importLogDirectory).filePath(
				QString("%1_%2.log").arg(baseFileName).arg(duplicateIndex++));
		}

		const std::string nativeLogPath =
			QDir::toNativeSeparators(logPath).toLocal8Bit().toStdString();
		std::shared_ptr<spdlog::sinks::basic_file_sink_mt> sink =
			std::make_shared<spdlog::sinks::basic_file_sink_mt>(
				nativeLogPath, true);
		std::shared_ptr<spdlog::logger> logger =
			std::make_shared<spdlog::logger>(
				"dxf_import_" + importId, sink);
		logger->set_pattern("%Y-%m-%d %H:%M:%S.%e [%l] %v");
		logger->set_level(spdlog::level::trace);
		logger->flush_on(spdlog::level::warn);

		retainRecentImportLogs(importLogDirectory, logPath);
		return logger;
	}
	catch (const spdlog::spdlog_ex& error)
	{
		qWarning() << "[importDxf] Failed to initialize import log:"
			<< error.what();
		return std::shared_ptr<spdlog::logger>();
	}
}

static std::shared_ptr<spdlog::logger> dxfErrorLogger()
{
	static std::shared_ptr<spdlog::logger> logger = []() {
		try
		{
			if (std::shared_ptr<spdlog::logger> existing =
				spdlog::get("dxf_import_errors"))
				return existing;

			const QString logDirectory = dxfLogRootDirectory();
			if (!QDir().mkpath(logDirectory))
			{
				qWarning() << "[importDxf] Failed to create log directory:" << logDirectory;
				return std::shared_ptr<spdlog::logger>();
			}

			const QString logPath =
				QDir(logDirectory).filePath("dxf_import_errors.log");
			const std::string nativeLogPath =
				QDir::toNativeSeparators(logPath).toLocal8Bit().toStdString();

			std::shared_ptr<spdlog::logger> result = spdlog::rotating_logger_mt(
				"dxf_import_errors", nativeLogPath, 10 * 1024 * 1024, 5);
			result->set_pattern("%Y-%m-%d %H:%M:%S.%e [%l] %v");
			result->set_level(spdlog::level::err);
			result->flush_on(spdlog::level::err);
			return result;
		}
		catch (const spdlog::spdlog_ex& error)
		{
			qWarning() << "[importDxf] Failed to initialize error log:"
				<< error.what();
			return std::shared_ptr<spdlog::logger>();
		}
	}();

	return logger;
}

static void logRawDxfData(
	const std::shared_ptr<spdlog::logger>& logger,
	const std::string& importId,
	const DxfData& data)
{
	if (!logger)
		return;

	for (size_t i = 0; i < data.points().size(); ++i)
	{
		const DxfPoint& point = data.points()[i];
		logger->trace(
			"[import={}] raw POINT id={} position=({}, {}, {})",
			importId, point.getId(), point.x(), point.y(), point.z());
	}

	for (size_t i = 0; i < data.lines().size(); ++i)
	{
		const DxfLine& line = data.lines()[i];
		logger->trace(
			"[import={}] raw LINE id={} start=({}, {}, {}) end=({}, {}, {})",
			importId, line.getId(),
			line.start().x(), line.start().y(), line.start().z(),
			line.end().x(), line.end().y(), line.end().z());
	}

	for (size_t i = 0; i < data.circles().size(); ++i)
	{
		const DxfCircle& circle = data.circles()[i];
		logger->trace(
			"[import={}] raw CIRCLE id={} center=({}, {}, {}) radius={}",
			importId, circle.getId(),
			circle.center().x(), circle.center().y(), circle.center().z(),
			circle.radius());
	}

	for (size_t i = 0; i < data.arcs().size(); ++i)
	{
		const DxfArc& arc = data.arcs()[i];
		logger->trace(
			"[import={}] raw ARC id={} center=({}, {}, {}) radius={} start_angle={} end_angle={} ccw={}",
			importId, arc.getId(),
			arc.center().x(), arc.center().y(), arc.center().z(),
			arc.radius(), arc.startAngle(), arc.endAngle(), arc.isCCW());
	}

	for (size_t i = 0; i < data.lwPolylines().size(); ++i)
	{
		const DxfLWPolyline& polyline = data.lwPolylines()[i];
		logger->trace(
			"[import={}] raw LWPOLYLINE id={} vertices={} closed={} const_z={}",
			importId, polyline.getId(), polyline.vertices().size(),
			polyline.isClosed(), polyline.constZ());

		for (size_t vertexIndex = 0;
			 vertexIndex < polyline.vertices().size();
			 ++vertexIndex)
		{
			const DxfPoint& vertex = polyline.vertices()[vertexIndex];
			logger->trace(
				"[import={}] raw LWPOLYLINE id={} VERTEX index={} position=({}, {}, {})",
				importId, polyline.getId(), vertexIndex,
				vertex.x(), vertex.y(), vertex.z());
		}

		for (size_t segmentIndex = 0;
			 segmentIndex < polyline.bulges().size();
			 ++segmentIndex)
		{
			const size_t endIndex = (segmentIndex + 1) % polyline.vertices().size();
			logger->trace(
				"[import={}] raw LWPOLYLINE id={} SEGMENT index={} start_vertex={} end_vertex={} bulge={}",
				importId, polyline.getId(), segmentIndex, segmentIndex, endIndex,
				polyline.bulges()[segmentIndex]);
		}
	}

	for (size_t i = 0; i < data.ellipses().size(); ++i)
	{
		const DxfEllipse& ellipse = data.ellipses()[i];
		logger->trace(
			"[import={}] raw ELLIPSE id={} center=({}, {}, {}) major_axis=({}, {}, {}) ratio={} start_param={} end_param={} ccw={}",
			importId, ellipse.getId(),
			ellipse.center().x(), ellipse.center().y(), ellipse.center().z(),
			ellipse.majorAxisEnd().x(), ellipse.majorAxisEnd().y(),
			ellipse.majorAxisEnd().z(), ellipse.ratio(),
			ellipse.startParam(), ellipse.endParam(), ellipse.isCCW());
	}
}

static void logConvertedSamData(
	const std::shared_ptr<spdlog::logger>& logger,
	const std::string& importId,
	const SamData& data)
{
	if (!logger)
		return;

	for (size_t i = 0; i < data.points().size(); ++i)
	{
		const DxfPoint& point = data.points()[i];
		logger->trace(
			"[import={}] converted POINT id={} position=({}, {}, {})",
			importId, point.getId(), point.x(), point.y(), point.z());
	}

	for (size_t i = 0; i < data.circles().size(); ++i)
	{
		const DxfCircle& circle = data.circles()[i];
		logger->trace(
			"[import={}] converted CIRCLE id={} center=({}, {}, {}) radius={}",
			importId, circle.getId(),
			circle.center().x(), circle.center().y(), circle.center().z(),
			circle.radius());
	}
}

static omuInterfaceObj::methodTable Example1PytModuleMethods[] =
{
	{"calcArea", (omuInterfaceObj::methodFunc)&Example1PytModule::calcArea},

	{"createLine", (omuInterfaceObj::methodFunc)&Example1PytModule::createLine},
	{"importDxf",((omuInterfaceObj::methodFunc)&Example1PytModule::importDxf)},
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

void Example1PytModule::DefineConstants()
{
	// do nothing
}

omuPrimitive* Example1PytModule::calcArea(omuArguments& args)
{
	double length, width;
	args.Begin();
	args.Get(length);
	args.Get(width);
	args.End();

	double area;
	area = length * width;

	return new omuPrimNumber(area);
}

omuPrimitive* Example1PytModule::createLine(omuArguments& args)
{
	args.Begin();
	args.End();

	const QString modelName("Model-1");
	const QString sketchName =
		QString("Example1Sketch_%1")
		.arg(QDateTime::currentMSecsSinceEpoch());

	basMdb mdb = basBasis::Instance()->Fetch();
	gmlSketchRepository& sketches =
		skcKGetSketchRepos(mdb, modelName);

	gslMatrix transform;
	skcSketch* sketch =
		skcCreateSketchWithXYAxis(&transform);

	const uint sketchId = sketches.Size() + 1;
	sketch->SetID(sketchId);
	sketch->DisplayOptions().SetSheetSize(200.0);

	skcGeomFactory geometryFactory(sketch);

	const int lineId = geometryFactory.CreateLine(
		gslPoint(-13.75, 8.75, 0.0),
		gslPoint(17.5, -11.25, 0.0),
		skc_FOREGROUND,
		false);

	gmlSketchWrapper wrapper(sketch);
	sketches.Insert(sketchName, wrapper);
	basBasis::Instance()->Replace(mdb);

	skcUndoRedoStack::Instance().ClearUndoStates();

	smgSceneManagerRole& role =
		smgSceneManagerRole::TheSceneManagerRole();

	const int viewport = role.GetCurrentViewport();
	const omuPrimType sceneType =
		role.GetSceneManagerName(viewport);

	if (sceneType.DisplayType() != omu_PART)
		role.Map(omu_PART, viewport);

	gcuScene* scene = gcuGetCurrentScene(false);
	if (scene)
	{
		scene = gcuGetCurrentScene(true);
		scene->ClearScene();

		skcPDO* sketchPdo =
			skcKUtils::GetCurrentSketchPDO();

		if (sketchPdo)
		{
			sketchPdo->SetSketch(sketch);
			sketchPdo->Rebuild();
			skcKToolset::Instance().ShowFrontView();

			QString sketchPath("mdb.models");
			sketchPath.append(omuSquareBracket(modelName));
			sketchPath.append(".sketches");
			sketchPath.append(omuSquareBracket(sketchName));

			sesKSessionState::Instance()
				->SetPrimaryObjectPath(sketchPath);
		}
	}

	return new omuPrimNumber(lineId);
}

omuPrimitive* Example1PytModule::importDxf(omuArguments& args) {
	QString filePath;
	double baseX = 0.0;
	double baseY = 0.0;
	double baseZ = 0.0;
	args.Begin();
	args.Get(filePath);
	args.Get(baseX);
	args.Get(baseY);
	args.Get(baseZ);
	args.End();

	const std::string importId = QDateTime::currentDateTimeUtc()
		.toString("yyyyMMdd_HHmmss_zzz")
		.toStdString();
	std::shared_ptr<spdlog::logger> logger =
		createDxfImportLogger(importId);
	std::shared_ptr<spdlog::logger> errorLogger = dxfErrorLogger();
	QElapsedTimer totalTimer;
	totalTimer.start();
	QElapsedTimer stageTimer;
	stageTimer.start();

	const std::string pathText = filePath.toLocal8Bit().toStdString();
	if (!logger && errorLogger)
	{
		errorLogger->error(
			"[import={}] import_log_initialization_failed file=\"{}\"",
			importId, pathText);
	}

	if (logger)
	{
		logger->info(
			"[import={}] started file=\"{}\" base=({}, {}, {})",
			importId, pathText, baseX, baseY, baseZ);
	}

	qDebug() << "[importDxf] ====== DXF 导入开始 ======";
	qDebug() << "[importDxf] 文件路径:" << filePath;
	qDebug() << "[importDxf] 基点:" << baseX << baseY << baseZ;

	// ---- 1. 解析 ----
	DxfData dxfData;
	DxfParser parser;
	if (!parser.parseFile(filePath, dxfData)) {
		const std::string errorText =
			dxfData.errorMessage().toLocal8Bit().toStdString();
		if (logger)
		{
			logger->error(
				"[import={}] parse_failed error=\"{}\" elapsed_ms={}",
				importId, errorText,
				totalTimer.elapsed());
			logger->flush();
		}
		if (errorLogger)
		{
			errorLogger->error(
				"[import={}] parse_failed file=\"{}\" error=\"{}\" elapsed_ms={}",
				importId, pathText, errorText, totalTimer.elapsed());
		}
		qDebug() << "[importDxf] 错误: DXF 文件解析失败!" << dxfData.errorMessage();
		return nullptr;
	}

	// ---- 2. 转换 ----
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

	stageTimer.restart();
	SamData samData;
	ConversionEngine engine;
	if (!engine.convert(dxfData, baseX, baseY, baseZ, samData)) {
		if (logger)
		{
			logger->error(
				"[import={}] conversion_failed elapsed_ms={}",
				importId, totalTimer.elapsed());
			logger->flush();
		}
		if (errorLogger)
		{
			errorLogger->error(
				"[import={}] conversion_failed file=\"{}\" elapsed_ms={}",
				importId, pathText, totalTimer.elapsed());
		}
		qDebug() << "[importDxf] 没有有效的图元可导入";
		return new omuPrimNumber(0);
	}

	// ---- 3. SAM对接 ----
	if (logger)
	{
		logger->info(
			"[import={}] conversion_completed points={} lines={} circles={} duration_ms={}",
			importId,
			samData.points().size(),
			samData.lines().size(),
			samData.circles().size(),
			stageTimer.elapsed());
	}
	logConvertedSamData(logger, importId, samData);

	stageTimer.restart();
	SamBuilder builder;
	if (!builder.beginImport()) {
		const std::string errorText =
			builder.lastError().toLocal8Bit().toStdString();
		if (logger)
		{
			logger->error(
				"[import={}] begin_import_failed error=\"{}\" elapsed_ms={}",
				importId, errorText,
				totalTimer.elapsed());
			logger->flush();
		}
		if (errorLogger)
		{
			errorLogger->error(
				"[import={}] begin_import_failed file=\"{}\" error=\"{}\" elapsed_ms={}",
				importId, pathText, errorText, totalTimer.elapsed());
		}
		qDebug() << "[importDxf] 创建草图失败";
		return nullptr;
	}

	int created = 0;
	created += builder.createPoints(samData.points());
	created += builder.createLines(samData.lines());
	created += builder.createCircles(samData.circles());

	if (!builder.commit()) {
		const std::string sketchText =
			builder.sketchName().toLocal8Bit().toStdString();
		const std::string errorText =
			builder.lastError().toLocal8Bit().toStdString();
		if (logger)
		{
			logger->error(
				"[import={}] commit_failed sketch=\"{}\" submitted={} error=\"{}\" elapsed_ms={}",
				importId, sketchText, created, errorText,
				totalTimer.elapsed());
			logger->flush();
		}
		if (errorLogger)
		{
			errorLogger->error(
				"[import={}] commit_failed file=\"{}\" sketch=\"{}\" submitted={} error=\"{}\" elapsed_ms={}",
				importId, pathText, sketchText, created, errorText,
				totalTimer.elapsed());
		}
		qDebug() << "[importDxf] 提交失败，回滚中...";
		builder.rollback();
		return new omuPrimNumber(0);
	}

	qDebug() << "[importDxf] ====== 导入完成, 共" << created << "个图元 ======";
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
	return new omuPrimNumber(created);
}
