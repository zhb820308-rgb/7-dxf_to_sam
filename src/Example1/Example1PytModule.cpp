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
	if (!parseDxfFile(filePath, dxfData))
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
	if (!convertToSamData(dxfData, baseX, baseY, baseZ, curveTolerance, samData))
	{
		return failImport(logger, errorLogger, importId, pathText,
			"conversion", "", totalTimer.elapsed(),
			QString("[importDxf] WARNING: no valid entities to import"));
	}

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
	SamBuilder builder;
	int created = buildSamSketch(samData, builder);
	if (created < 0)
	{
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
	dropDxfImportLogger(importId);
	return new omuPrimNumber(created);
}

// ========================================================================
//  DXF import helpers
// ========================================================================

omuPrimitive* Example1PytModule::failImport(
	const std::shared_ptr<spdlog::logger>& logger,
	const std::shared_ptr<spdlog::logger>& errorLogger,
	const std::string& importId,
	const std::string& pathText,
	const std::string& stage,
	const std::string& detail,
	long long elapsedMs,
	const QString& qWarningMsg)
{
	reportImportError(logger, errorLogger, importId, pathText, stage, detail, elapsedMs);
	qWarning() << qWarningMsg;
	dropDxfImportLogger(importId);
	return nullptr;
}

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
