#include <omuArguments.h>
#include <omuPrimNumber.h>
#include <omuPrimType.h>

#include <QDateTime>
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

	// ---- 阶段 1: DXF 解析 ----
	DxfData data;
	DxfParser parser;
	if (!parser.parseFile(filePath, data)) {
		return nullptr; // 解析失败，返回 nullptr
	}
	int validCount = 0;

	// ---- 阶段 2: 创建草图 + 几何（不涉及数据库提交）----
	const QString modelName("Model-1");
	const QString sketchName =
		QString("DxfImport_%1")
		.arg(QDateTime::currentMSecsSinceEpoch());
	// 获取 MDB
	basMdb mdb = basBasis::Instance()->Fetch();
	gmlSketchRepository& sketches = skcKGetSketchRepos(mdb, modelName);
	// 创建草图
	gslMatrix transform;
	skcSketch* sketch = skcCreateSketchWithXYAxis(&transform);
	if (!sketch) return nullptr;
	const uint sketchId = sketches.Size() + 1;
	sketch->SetID(sketchId);
	sketch->DisplayOptions().SetSheetSize(200.0);
	// 创建几何
	skcGeomFactory geometryFactory(sketch);
	for (const DxfLine& dxfLine : data.lines) {
		const Point3D& start = dxfLine.start;
		const Point3D& end = dxfLine.end;
		if (!std::isfinite(start.x) || !std::isfinite(start.y) || !std::isfinite(start.z) ||
		    !std::isfinite(end.x)   || !std::isfinite(end.y)   || !std::isfinite(end.z))
			continue;
		if (start.x == end.x && start.y == end.y && start.z == end.z)
			continue;
		gslPoint pt1(start.x, start.y, start.z);
		gslPoint pt2(end.x, end.y, end.z);
		geometryFactory.CreateLine(pt1, pt2, skc_FOREGROUND, false);
		++validCount;
	}
	if (validCount == 0) {
		return new omuPrimNumber(0); // 没有有效线段，返回 0
	}

	// ---- 阶段 3: 将草图提交到数据库 ----
	gmlSketchWrapper wrapper(sketch);
	sketches.Insert(sketchName, wrapper);
	basBasis::Instance()->Replace(mdb);
	skcUndoRedoStack::Instance().ClearUndoStates();

	// ---- 阶段 4: 场景展示 ----
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
	return new omuPrimNumber(validCount);
}
