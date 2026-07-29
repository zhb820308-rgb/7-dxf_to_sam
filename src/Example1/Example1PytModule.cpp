#include <omuArguments.h>
#include <omuPrimNumber.h>
#include <omuPrimType.h>

#include <QDateTime>
#include <QDebug>
#include <QElapsedTimer>
#include <cmath>

#include "DxfImportLogger.h"
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
	QElapsedTimer stageTimer;
	stageTimer.start();
	DxfData dxfData;
	DxfParser parser;
	if (!parser.parseFile(filePath, dxfData)) {
		std::string detail = " error=\"" + dxfData.errorMessage().toLocal8Bit().toStdString() + "\"";
		reportImportError(logger, errorLogger, importId, pathText,
			"parse", detail, totalTimer.elapsed());
		qWarning() << "[importDxf] ERROR: DXF parse failed —" << dxfData.errorMessage();
		dropDxfImportLogger(importId);
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
		reportImportError(logger, errorLogger, importId, pathText,
			"conversion", "", totalTimer.elapsed());
		qWarning() << "[importDxf] WARNING: no valid entities to import";
		dropDxfImportLogger(importId);
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
		std::string detail = " error=\"" + builder.lastError().toLocal8Bit().toStdString() + "\"";
		reportImportError(logger, errorLogger, importId, pathText,
			"begin_import", detail, totalTimer.elapsed());
		qWarning() << "[importDxf] ERROR: failed to create sketch —" << builder.lastError();
		dropDxfImportLogger(importId);
		return nullptr;
	}

	int created = 0;
	created += builder.createPoints(samData.points());
	created += builder.createLines(samData.lines());
	created += builder.createCircles(samData.circles());

	if (!builder.commit()) {
		std::string detail = " sketch=\"" + builder.sketchName().toLocal8Bit().toStdString()
			+ "\" submitted=" + std::to_string(created)
			+ " error=\"" + builder.lastError().toLocal8Bit().toStdString() + "\"";
		reportImportError(logger, errorLogger, importId, pathText,
			"commit", detail, totalTimer.elapsed());
		qWarning() << "[importDxf] ERROR: commit failed, rolling back —" << builder.lastError();
		builder.rollback();
		dropDxfImportLogger(importId);
		return new omuPrimNumber(0);
	}

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
