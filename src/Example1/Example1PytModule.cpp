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

// 将 bulge 圆弧段离散为采样点列表（首尾包含原始端点）
static std::vector<Point3D> tessellateBulgeArc(
	const Point3D& p0, const Point3D& p1,
	double bulge, double tolerance)
{
	std::vector<Point3D> result;
	result.push_back(p0);

	// bulge 约等于 0  → 直线，仅返回两端点
	if (std::abs(bulge) < 1e-12) {
		result.push_back(p1);
		return result;
	}

	double dx = p1.x - p0.x;
	double dy = p1.y - p0.y;
	double chord = std::sqrt(dx * dx + dy * dy);
	if (chord < 1e-12) {
		result.push_back(p1);
		return result;
	}

	double theta = 4.0 * std::atan(bulge);          // 有符号圆心角
	double radius = chord * (1.0 + bulge * bulge)
	                / (4.0 * std::abs(bulge));

	double midX = (p0.x + p1.x) * 0.5;
	double midY = (p0.y + p1.y) * 0.5;
	double nx = -dy / chord;
	double ny =  dx / chord;
	double centerDist = chord * (1.0 - bulge * bulge)
	                    / (4.0 * bulge);

	double cx = midX + nx * centerDist;
	double cy = midY + ny * centerDist;

	double startAngle = std::atan2(p0.y - cy, p0.x - cx);

	// 根据容差计算分段数
	if (tolerance <= 0.0) tolerance = 0.01;
	int segmentCount = 1;
	if (radius > tolerance) {
		double maxAngle = 2.0 * std::acos(
			std::max(-1.0, std::min(1.0,
				1.0 - tolerance / radius)));
		segmentCount = static_cast<int>(
			std::ceil(std::abs(theta) / maxAngle));
	}
	segmentCount = std::max(1, std::min(segmentCount, 10000));

	for (int i = 1; i < segmentCount; ++i) {
		double angle = startAngle
		               + theta * static_cast<double>(i)
		                 / static_cast<double>(segmentCount);
		result.push_back(Point3D(
			cx + radius * std::cos(angle),
			cy + radius * std::sin(angle),
			p0.z + (p1.z - p0.z)
			       * static_cast<double>(i)
			       / static_cast<double>(segmentCount)));
	}
	result.push_back(p1);

	// 强制首尾点等于原始端点，避免浮点累积误差
	result.front() = p0;
	result.back() = p1;
	return result;
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
	double baseX;
	double baseY;
	double baseZ;
	args.Begin();
	args.Get(filePath);
	args.Get(baseX);
	args.Get(baseY);
	args.Get(baseZ);
	args.End();

	// ---- 阶段 1: DXF 解析 ----
	DxfData data;
	DxfParser parser;
	if (!parser.parseFile(filePath, data)) {
		QMessageBox::warning(
			nullptr,
			QStringLiteral("DXF Import Failed"),
			data.errorMessage);
		return new omuPrimNumber(0);
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
		gslPoint pt1(start.x-baseX, start.y-baseY, start.z-baseZ);
		gslPoint pt2(end.x-baseX, end.y-baseY, end.z-baseZ);
		geometryFactory.CreateLine(pt1, pt2, skc_CONSTRUCTION, false);
		++validCount;
	}
	for (const DxfCircle& circle : data.circles) {
		const Point3D& center = circle.center;
		double radius = circle.radius;

		if (!std::isfinite(center.x) || !std::isfinite(center.y) || !std::isfinite(center.z))
			continue;
		if (!std::isfinite(radius) || radius <= 0.0)
			continue;

		gslPoint ptCenter(center.x-baseX, center.y-baseY, center.z-baseZ);
		gslPoint ptOnCircle(center.x - baseX + radius, center.y - baseY, center.z - baseZ);

		geometryFactory.CreateCircle(ptCenter, ptOnCircle, skc_CONSTRUCTION, false);
		++validCount;
	}
	// 阶段 2.3: 离散化 bulge 段（LWPOLYLINE 圆弧段 → 直线段）
	const double bulgeTolerance = 0.01; // 默认容差，后续可从 GUI 传入
	for (const DxfPolylineSegment& seg : data.polylineSegments) {
		const Point3D& start = seg.start;
		const Point3D& end = seg.end;
		if (!std::isfinite(start.x) || !std::isfinite(start.y) || !std::isfinite(start.z) ||
		    !std::isfinite(end.x)   || !std::isfinite(end.y)   || !std::isfinite(end.z))
			continue;
		std::vector<Point3D> pts =
			tessellateBulgeArc(start, end, seg.bulge, bulgeTolerance);
		for (size_t i = 1; i < pts.size(); ++i) {
			gslPoint pt1(pts[i - 1].x - baseX, pts[i - 1].y - baseY, pts[i - 1].z - baseZ);
			gslPoint pt2(pts[i].x - baseX,     pts[i].y - baseY,     pts[i].z - baseZ);
			geometryFactory.CreateLine(pt1, pt2, skc_CONSTRUCTION, false);
			++validCount;
		}
	}
	if (validCount == 0) {
		return new omuPrimNumber(0); 
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
