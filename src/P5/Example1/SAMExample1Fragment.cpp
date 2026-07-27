#include <SAMExample1Fragment.h>
#include <ptoKPart.h>
#include <bmeMesh.h>
#include <omeMesh.h>
#include <mesUtils.h>
#include <bmeElementClass.h>
#include <shpShape.h>
#include <bmgUtils.h>


static omuInterfaceObj::methodTable SAMExample1FMethods[] =
{
	{ "getNodeInfo", (omuInterfaceObj::methodFunc)&SAMExample1Fragment::getNodeInfo },
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

omuPrimitive* SAMExample1Fragment::Copy() const
{
	return new SAMExample1Fragment(*this);
}

omuPrimitive * SAMExample1Fragment::getNodeInfo(omuArguments & args)
{
	double nodeID;
	args.Begin();
	args.Get(nodeID, "NodeID");
	args.End();

	if (nodeID < 0)
		return nullptr;

	ptoKPart* m_part = GetPart();
	if (!m_part)
		return nullptr;

	ftrFeatureList* partFeature = m_part->GetFeatureList();
	if (!partFeature)
		return nullptr;

	bmeMesh* msh = partFeature->GetMesh(bdoDefaultInstId);
	if (!msh)
		return nullptr;

	bmeNodeData& nodeData = msh->GetNodeData();
	int numNodes = nodeData.NumNodes();
	if (nodeID >= numNodes)
		return nullptr;

	float x, y, z;
	nodeData.GetNodalCoord(nodeID, x, y, z);

	return nullptr;
}

