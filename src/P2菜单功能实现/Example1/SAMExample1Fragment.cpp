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

