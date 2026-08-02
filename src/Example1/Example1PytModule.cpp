#include <omuArguments.h>
#include <omuPrimNumber.h>

#include "DxfImportOrchestrator.h"

#include <Example1PytModule.h>

static omuInterfaceObj::methodTable Example1PytModuleMethods[] =
{
	{"importDxf", ((omuInterfaceObj::methodFunc)&Example1PytModule::importDxf)},
	{0, 0}
};

Example1PytModule::Example1PytModule()
	: pyoModule("Example1", Example1PytModuleMethods, pyoModule::NO_IMPORT)
{
}

Example1PytModule::~Example1PytModule()
{
}

void Example1PytModule::DefineConstants()
{
}

omuPrimitive* Example1PytModule::importDxf(omuArguments& args)
{
	DxfImportRequest request;

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
		return nullptr;
	return new omuPrimNumber(outcome.createdCount);
}
