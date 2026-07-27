#include <SAMExample1Fragment.h>
#include <ptoKPart.h>
#include <bmeMesh.h>
#include <omeMesh.h>
#include <mesUtils.h>
#include <bmeElementClass.h>
#include <shpShape.h>
#include <bmgUtils.h>
/*
#include <samMdbDrawable.h>
#include <samModelDrawable.h>
#include <samPartDrawable.h>
*/

#include <gdyScene.h>
#include <gdyEditor.h>
#include <kefKLine.h>
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
	QString color("red");
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
	
	drawLine->createOneObject(segID, vertexXYZ, color);
	scn->ExposeVP();// updata

}

