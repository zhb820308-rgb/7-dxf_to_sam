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
	qDebug() << "[importDxf] 阶段 1/4: 开始解析 DXF 文件...";
	DxfData data;
	DxfParser parser;
	if (!parser.parseFile(filePath, data)) {
		qDebug() << "[importDxf] 错误: DXF 文件解析失败! 返回 nullptr";
		return nullptr; // 解析失败，返回 nullptr
	}
	qDebug() << "[importDxf] DXF 解析成功:"
		 << "lines =" << data.lines().size()
		 << ", circles =" << data.circles().size();
	int validCount = 0;

	// ---- 阶段 2: 创建草图 + 几何（不涉及数据库提交）----
	qDebug() << "[importDxf] 阶段 2/4: 创建草图并生成几何...";
	const QString modelName("Model-1");
	const QString sketchName =
		QString("DxfImport_%1")
		.arg(QDateTime::currentMSecsSinceEpoch());
	qDebug() << "[importDxf] 草图名称:" << sketchName;

	// 获取 MDB
	basMdb mdb = basBasis::Instance()->Fetch();
	gmlSketchRepository& sketches = skcKGetSketchRepos(mdb, modelName);
	// 创建草图
	qDebug() << "[importDxf] 创建草图对象...";
	gslMatrix transform;
	skcSketch* sketch = skcCreateSketchWithXYAxis(&transform);
	if (!sketch) {
		qDebug() << "[importDxf] 错误: 创建草图失败! 返回 nullptr";
		return nullptr;
	}
	const uint sketchId = sketches.Size() + 1;
	sketch->SetID(sketchId);
	sketch->DisplayOptions().SetSheetSize(200.0);
	qDebug() << "[importDxf] 草图创建成功, ID:" << sketchId;

	// 创建几何
	qDebug() << "[importDxf] 遍历 DXF 线段, 共" << data.lines().size() << "条...";
	skcGeomFactory geometryFactory(sketch);
	int skipped = 0;
	for (const DxfLine& dxfLine : data.lines()) {
		const DxfPoint& start = dxfLine.start();
		const DxfPoint& end = dxfLine.end();
		if (!start.isValid() || !end.isValid()) {
			++skipped;
			continue;
		}
		if (start.x() == end.x() && start.y() == end.y() && start.z() == end.z()) {
			++skipped;
			continue;
		}
		gslPoint pt1(start.x(), start.y(), start.z());
		gslPoint pt2(end.x(), end.y(), end.z());
		geometryFactory.CreateLine(pt1, pt2, skc_FOREGROUND, false);
		++validCount;
	}
	qDebug() << "[importDxf] 几何生成完成: 有效线段 =" << validCount
		 << ", 跳过 =" << skipped;
	if (validCount == 0) {
		qDebug() << "[importDxf] 没有有效线段, 返回 0";
		return new omuPrimNumber(0); // 没有有效线段，返回 0
	}

	// ---- 阶段 3: 将草图提交到数据库 ----
	qDebug() << "[importDxf] 阶段 3/4: 将草图提交到数据库...";
	gmlSketchWrapper wrapper(sketch);
	sketches.Insert(sketchName, wrapper);
	basBasis::Instance()->Replace(mdb);
	skcUndoRedoStack::Instance().ClearUndoStates();
	qDebug() << "[importDxf] 草图已提交到数据库, 撤销栈已清空";

	// ---- 阶段 4: 场景展示 ----
	qDebug() << "[importDxf] 阶段 4/4: 在场景中显示草图...";
	smgSceneManagerRole& role = smgSceneManagerRole::TheSceneManagerRole();
	const int viewport = role.GetCurrentViewport();
	const omuPrimType sceneType = role.GetSceneManagerName(viewport);
	qDebug() << "[importDxf] 当前视口:" << viewport << "场景类型:" << sceneType.DisplayType();
	if (sceneType.DisplayType() != omu_PART) {
		qDebug() << "[importDxf] 切换到 PART 场景视图";
		role.Map(omu_PART, viewport);
	}
	gcuScene* scene = gcuGetCurrentScene(false);
	if (scene) {
		qDebug() << "[importDxf] 获取当前场景并刷新...";
		scene = gcuGetCurrentScene(true);
		scene->ClearScene();
		skcPDO* sketchPdo = skcKUtils::GetCurrentSketchPDO();
		if (sketchPdo) {
			qDebug() << "[importDxf] 设置草图 PDO 并重建...";
			sketchPdo->SetSketch(sketch);
			sketchPdo->Rebuild();
			skcKToolset::Instance().ShowFrontView();

			QString sketchPath("mdb.models");
			sketchPath.append(omuSquareBracket(modelName));
			sketchPath.append(".sketches");
			sketchPath.append(omuSquareBracket(sketchName));
			sesKSessionState::Instance()->SetPrimaryObjectPath(sketchPath);
			qDebug() << "[importDxf] 草图路径已设置:" << sketchPath;
		} else {
			qDebug() << "[importDxf] 警告: 无法获取当前草图 PDO";
		}
	} else {
		qDebug() << "[importDxf] 提示: 无当前场景, 跳过场景显示";
	}

	qDebug() << "[importDxf] ====== 导入完成, 共" << validCount << "条线段 ======";
	return new omuPrimNumber(validCount);
}
