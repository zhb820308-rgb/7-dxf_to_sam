#include <omuArguments.h>
#include <omuPrimNumber.h>
#include <omuPrimType.h>

#include <QDateTime>
#include <QDebug>
#include <cmath>
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
#include <smgSceneManagerRole.h>

#include <gcuScene.h>
#include <gdyScene.h>

#include <sesKSessionState.h>

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
	args.Begin();
	args.Get(filePath);
	args.End();

	qDebug() << "[importDxf] ====== DXF 导入开始 ======";
	qDebug() << "[importDxf] 文件路径:" << filePath;

	// ---- 阶段 1: DXF 解析 ----
	DxfData dxfData;
	DxfParser parser;
	if (!parser.parseFile(filePath, dxfData)) {
		qDebug() << "[importDxf] 错误: DXF 文件解析失败!";
		return nullptr;
	}
	qDebug() << "[importDxf] DXF 解析成功:"
			 << "points=" << dxfData.points().size()
			 << "lines="  << dxfData.lines().size()
			 << "circles="<< dxfData.circles().size();

	// ---- 阶段 2: 转换引擎（基点平移 + 过滤无效图元）----
	SamData samData;
	ConversionEngine engine;
	if (!engine.convert(dxfData, 0.0, 0.0, 0.0, samData)) {
		qDebug() << "[importDxf] 错误: 没有有效的图元可导入!";
		return new omuPrimNumber(0);
	}
	qDebug() << "[importDxf] 转换完成:"
			 << "points=" << samData.points().size()
			 << "lines="  << samData.lines().size()
			 << "circles="<< samData.circles().size();

	// ---- 阶段 3: 创建草图 ----
	const QString modelName("Model-1");
	const QString sketchName =
		QString("DxfImport_%1")
		.arg(QDateTime::currentMSecsSinceEpoch());
	qDebug() << "[importDxf] 草图名称:" << sketchName;

	basMdb mdb = basBasis::Instance()->Fetch();
	gmlSketchRepository& sketches = skcKGetSketchRepos(mdb, modelName);

	gslMatrix transform;
	skcSketch* sketch = skcCreateSketchWithXYAxis(&transform);
	if (!sketch) {
		qDebug() << "[importDxf] 错误: 创建草图失败!";
		return nullptr;
	}
	const uint sketchId = sketches.Size() + 1;
	sketch->SetID(sketchId);
	sketch->DisplayOptions().SetSheetSize(200.0);

	// ---- 阶段 4: 在草图中创建几何图元 ----
	skcGeomFactory geometryFactory(sketch);
	int createdCount = 0;

	// 4a. 创建点
	for (const DxfPoint& pt : samData.points()) {
		// skcGeomFactory::CreatePoint 接口待确认
		// geometryFactory.CreatePoint(gslPoint(pt.x(), pt.y(), pt.z()));
		++createdCount;
		qDebug() << "[importDxf] 创建点:"
				 << pt.x() << pt.y() << pt.z();
	}

	// 4b. 创建直线
	for (const DxfLine& line : samData.lines()) {
		gslPoint p1(line.start().x(), line.start().y(), line.start().z());
		gslPoint p2(line.end().x(),   line.end().y(),   line.end().z());
		geometryFactory.CreateLine(p1, p2, skc_FOREGROUND, false);
		++createdCount;
	}

	// 4c. 创建圆
	for (const DxfCircle& circle : samData.circles()) {
		// skcGeomFactory::CreateCircle 接口待确认
		qDebug() << "[importDxf] 创建圆: 圆心="
				 << circle.center().x() << circle.center().y() << circle.center().z()
				 << "半径=" << circle.radius();
		++createdCount;
	}

	qDebug() << "[importDxf] 几何创建完成, 共" << createdCount << "个图元";

	// ---- 阶段 5: 提交到数据库 ----
	gmlSketchWrapper wrapper(sketch);
	sketches.Insert(sketchName, wrapper);
	basBasis::Instance()->Replace(mdb);
	skcUndoRedoStack::Instance().ClearUndoStates();

	// ---- 阶段 6: 场景展示 ----
	smgSceneManagerRole& role = smgSceneManagerRole::TheSceneManagerRole();
	const int viewport = role.GetCurrentViewport();
	const omuPrimType sceneType = role.GetSceneManagerName(viewport);
	if (sceneType.DisplayType() != omu_PART)
		role.Map(omu_PART, viewport);

	gcuScene* scene = gcuGetCurrentScene(false);
	if (scene) {
		scene = gcuGetCurrentScene(true);
		scene->ClearScene();
		skcPDO* sketchPdo = skcKUtils::GetCurrentSketchPDO();
		if (sketchPdo) {
			sketchPdo->SetSketch(sketch);
			sketchPdo->Rebuild();
			skcKToolset::Instance().ShowFrontView();
			QString sketchPath("mdb.models");
			sketchPath.append(omuSquareBracket(modelName));
			sketchPath.append(".sketches");
			sketchPath.append(omuSquareBracket(sketchName));
			sesKSessionState::Instance()->SetPrimaryObjectPath(sketchPath);
		}
	}

	qDebug() << "[importDxf] ====== 导入完成, 共创建" << createdCount << "个图元 ======";
	return new omuPrimNumber(createdCount);
}
