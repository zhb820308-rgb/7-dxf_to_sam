// ============================================================================
// Python 模块对象
// ============================================================================
#include <omuArguments.h>
#include <omuPrimNumber.h>

#include "DxfImportApplication.h"

#include <Example1Module.h>

// SAM SDK 不会自动认识普通 C++ 成员函数，必须用“方法表”把脚本名映射到函数指针。
// 强制类型转换是因为 SDK 使用统一的 methodFunc 类型保存不同类的成员函数指针。
// 最后的 {0, 0} 是哨兵项（sentinel），告诉旧式 C API“数组到这里结束”。
static omuInterfaceObj::methodTable Example1PytModuleMethods[] =
{
	{"importDxf", ((omuInterfaceObj::methodFunc)&Example1PytModule::importDxf)},
	{0, 0}
};

Example1PytModule::Example1PytModule()
	: pyoModule("Example1", Example1PytModuleMethods, pyoModule::NO_IMPORT)
{
	// "Example1" 必须与 GUI 发送的 `import Example1`、CMake 产物 Example1.pyd 对应。
}

Example1PytModule::~Example1PytModule()
{
}

void Example1PytModule::DefineConstants()
{
}

omuPrimitive* Example1PytModule::importDxf(omuArguments& args)
{
	// 请求对象带默认成员值，因此可选参数缺省时仍有安全默认值。
	DxfImportRequest request;

	// SAM 参数解析协议：Begin 开始；前四个 Get 是必需位置参数；Optional 之后按
	// 名称读取可选参数；End 做收尾检查。这里的名字必须与 GUI 的 args.Put 一致。
	args.Begin();
	args.Get(request.filePath);
	args.Get(request.baseX);
	args.Get(request.baseY);
	args.Get(request.baseZ);
	args.Optional();
	args.Get(request.curveTolerance, "curveTolerance");
	args.Get(request.ignoredLayers, "ignoreLayers");
	args.Get(request.importMode, "importMode");
	args.Get(request.modelName, "modelName");
	args.Get(request.partName, "partName");
	args.Get(request.nodeMergeTolerance, "nodeMergeTolerance");
	args.Get(request.maxOutputEntities, "maxOutputEntities");
	args.End();

	const DxfImportOutcome outcome = runDxfImport(request);
	if (!outcome.succeeded())
		// nullptr 表示脚本层没有成功结果；详细失败原因已由编排器写日志/界面。
		return nullptr;
	// `new` 出来的 omuPrimNumber 交给 SAM 脚本运行时管理，返回创建数量。
	return new omuPrimNumber(outcome.createdCount);
}

// ============================================================================
// PYD 导出与注册
// ============================================================================
#include <iniPythonModuleRegistrar.h>
#include <omuAtom.h>

// 这些变量只在本 .cpp 中可见。旧式代码用 0 表示空指针；现代 C++ 更常写 nullptr。
// 生命周期必须覆盖整个模块注册期，所以不能创建成 Initialize 内的局部对象。
static Example1PytModule* Example1PytModulePtr = 0;
static SAMExample1Fragment* SAMExample1FragmentPtr = 0;
static int Example1ReferenceCount = 0;

void Example1Initialize(int& count)
{
	// 后置 ++ 先返回旧值再加一。因此第一次调用时 !0 为 true，才真正创建对象；
	// 后续调用只增加引用计数。它等价于更冗长的“若为 0 则创建，然后 ++”。
	if (!Example1ReferenceCount++)
	{
		// Example1PytModule 注册模块级方法 Example1.importDxf；Fragment 注册附着在
		// SAM Part 对象上的示例方法。二者作用不同，教程会分别说明。
		Example1PytModulePtr = new Example1PytModule;
		SAMExample1FragmentPtr = new SAMExample1Fragment;
		omuAtom::CreateAtom("kefKLine");
		++count;
	}
}

void Example1Finalize(int& count)
{
	// 前置 -- 先减一再判断；仅最后一次 finalize 负责 delete。
	if (!--Example1ReferenceCount)
	{
		if (Example1PytModulePtr)
		{
			delete Example1PytModulePtr;
			Example1PytModulePtr = 0;

			delete SAMExample1FragmentPtr;
			SAMExample1FragmentPtr = 0;
		}

		--count;
	}
}

// `extern "C"` 关闭 C++ 名字修饰，确保 SAM/Python 能按固定导出名
// `initExample1` 找到入口。它相当于这个 .pyd 的“插件入口”，不是 exe 的 main()。
extern "C" void initExample1(void)
{
	// iniPythonModuleRegistrar 是 SAM SDK 的注册器单例。SAM 负责在合适时机回调
	// Example1Initialize / Example1Finalize。
	iniPythonModuleRegistrar::Instance().Register(Example1Initialize, Example1Finalize);
}

// ============================================================================
// 遗留教学 Fragment
// ============================================================================
#include <ptoKPart.h>
#include <bmeMesh.h>
#include <omeMesh.h>
#include <mesUtils.h>
#include <bmeElementClass.h>
#include <shpShape.h>
#include <bmgUtils.h>

#include <gdyScene.h>
#include <gdyEditor.h>
#include <cowList.T>
#include <omuPrimTuple.h>

static omuInterfaceObj::methodTable SAMExample1FMethods[] =
{
	{"drawExample", (omuInterfaceObj::methodFunc)&SAMExample1Fragment::drawExample},
	{"getNodeLocation", (omuInterfaceObj::methodFunc)&SAMExample1Fragment::getNodeLocation},
	{ 0, 0 }
};

static omuInterfaceObj::memberTable SAMExample1FMembers[] =
{
	{ 0, 0, 0 }
};

SAMExample1Fragment::SAMExample1Fragment()
	: ptsKPartFragment()
{
	omuInterfaceObj::DescribeType("SAMExample1Fragment", SAMExample1FMethods, SAMExample1FMembers);
}
SAMExample1Fragment::~SAMExample1Fragment()
{
	// todo
}
omuPrimitive* SAMExample1Fragment::getNodeLocation(omuArguments& args)
{
	int nodeID;
	args.Begin();
	args.Get(nodeID, "NodeID");
	args.End();

	if (nodeID < 0)
		return 0;

	ptoKPart* part = GetPart();
	if (!part)
		return 0;

	ftrFeatureList* features = part->GetFeatureList();
	if (!features)
		return 0;

	bmeMesh* mesh = features->GetMesh(bdoDefaultInstId);
	if (!mesh)
		return 0;

	const bmeNodeData& nodeData = mesh->NodeData();
	if (nodeID >= nodeData.NumNodes())
		return 0;

	float x, y, z;
	if (!nodeData.GetNodalCoord(nodeID, x, y, z))
		return 0;

	omuPrimTuple* location = new omuPrimTuple(3);
	location->Put((double)x);
	location->Put((double)y);
	location->Put((double)z);
	return location;
}

omuPrimitive* SAMExample1Fragment::Copy() const
{
	return new SAMExample1Fragment(*this);
}

omuPrimitive* SAMExample1Fragment::drawExample(omuArguments& args)
{
	double length, width;
	args.Begin();
	args.Get(length);
	args.Get(width);
	args.End();

	g3dVector startPoint, endPoint;
	startPoint.X() = 0.;
	startPoint.Y() = 0.;
	startPoint.Z() = 0.;

	endPoint.X() = 0.;
	endPoint.Y() = length;
	endPoint.Z() = 0.;

	int segID = 0;
	DrawLine(segID, startPoint, endPoint);

	startPoint.X() = 0.;
	startPoint.Y() = length;
	startPoint.Z() = 0.;

	endPoint.X() = 0.;
	endPoint.Y() = length;
	endPoint.Z() = width;

	DrawLine(++segID, startPoint, endPoint);

	startPoint.X() = 0.;
	startPoint.Y() = length;
	startPoint.Z() = width;

	endPoint.X() = 0.;
	endPoint.Y() = 0.;
	endPoint.Z() = width;

	DrawLine(++segID, startPoint, endPoint);

	startPoint.X() = 0.;
	startPoint.Y() = 0.;
	startPoint.Z() = width;

	endPoint.X() = 0.;
	endPoint.Y() = 0.;
	endPoint.Z() = 0.;

	DrawLine(++segID, startPoint, endPoint);

	return 0;
}

void SAMExample1Fragment::DrawLine(int& segID, g3dVector startPoint, g3dVector endPoint)
{
	cowList<g3dVector> vertexXYZ;
	vertexXYZ.Clear();
	vertexXYZ.Append(startPoint);
	vertexXYZ.Append(endPoint);

	gdyScene* scn = gdyScene::GetCurrentScene(true);
	gdyEditor* edit = scn->Editor();
	kefKLine* drawLine = static_cast<kefKLine*>(edit->GetGeomEditorByType("kefKLine"));
	if (!drawLine)
	{
		edit->AddGeomEditor(new kefKLine);
		drawLine = static_cast<kefKLine*>(edit->GetGeomEditorByType("kefKLine"));
	}

	drawLine->createOneObject(segID, vertexXYZ);
	scn->ExposeVP();// update

}

// ============================================================================
// 遗留教学线对象
// ============================================================================

#include <sesKSessionState.h>


#include <kefCEngineeringFeatures.h>

#include <gdrRenderer.h>

#include <gdyDisplayRep.h>
#include <dgpKScene.h>
#include <g3dPoint.h>
#include <g3dMarker.h>

#include <smgSceneManagerRole.h>

#include <ftrFeature.h>
#include <ftrEvaluatedRep.h>

#include <nexDynamicCast.h>


kefKLine::kefKLine()
	: gdyNonPageGeomEditor(),
	timeStamp(0.0),
	type_atom("kefKLine"),
	_geomVertexXYZs(cowList<cowList<g3dVector>>()),
	_segID(cowListInt()),
	_maxSegID(0)
{
	// do nothing
}

kefKLine::~kefKLine()
{
	// do nothing
}

gdyGeomEditor* kefKLine::Copy() const
{
	return new kefKLine(*this);
}

void kefKLine::Load(const gdyDisplayRep* r, g3dSphere* extent)
{
	timeStamp = 0.0;
}

void kefKLine::Unload()
{
	timeStamp = 0.0;
}

void kefKLine::Draft(gdrRenderer& r) const
{
	Draw(r, false);
}

void kefKLine::Paint(gdrRenderer& r) const
{
	Draw(r, true);
}

void kefKLine::ASelect(gdrASelector& as) const
{
	// do nothing
}

void kefKLine::Highlight(gdrRenderer& r) const
{
	// todo
}

int kefKLine::findObject(int segID) const
{
	if (_segID.IsEmpty())
		return -1;
	return _segID.FindMember(segID);
}

void kefKLine::createOneObject(int & segID, const cowList<g3dVector>& vertexXYZ)
{
	const int objectIndex = findObject(segID);

	// modify existing object
	if (objectIndex >= 0)
	{
		_geomVertexXYZs.Get(objectIndex) = vertexXYZ;
	}
	// create a new object
	else
	{
		_geomVertexXYZs.Append(vertexXYZ);
		segID = _maxSegID++;
		_segID.Append(segID);
	}
}

void kefKLine::Draw(gdrRenderer& drafter, bool paintMode) const
{
	drafter.DepthTest(false);

	g3dPoint pnt;
	g3dVector normal(0.0f, 0.0f, 1.0f);
	drafter.Normal(normal);

	for (int i = 0; i < _geomVertexXYZs.Length(); i++)
	{

		if (_geomVertexXYZs[i].Length() == 1)
			drafter.Begin(g3d_Points);
		else if (_geomVertexXYZs[i].Length() == 2)
			drafter.Begin(g3d_Lines);
		else if (_geomVertexXYZs[i].Length() == 3)
			drafter.Begin(g3d_Tris);
		else if (_geomVertexXYZs[i].Length() == 4)
			drafter.Begin(g3d_Quads);
		else if (_geomVertexXYZs[i].Length() > 4)
			drafter.Begin(g3d_Polygon);

		for (int j = 0; j < _geomVertexXYZs[i].Length(); j++)
		{
			pnt.Set(_geomVertexXYZs[i][j].GetX(), _geomVertexXYZs[i][j].GetY(), _geomVertexXYZs[i][j].GetZ(), 1.0f);
			drafter.Vertex(pnt);
		}

		drafter.End();
	}

}
