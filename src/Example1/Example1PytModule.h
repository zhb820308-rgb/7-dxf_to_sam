/* -*- mode: c++ -*- */
#ifndef Example1PytModule_h
#define Example1PytModule_h

// Forward declarations
class DxfData;
class SamData;
class SamBuilder;

// Includes

// Begin local includes

#include <ptsKPartFragment.h>
#include <pyoModule.h>

#include <set>
#include <string>

// Class definition

class Example1PytModule : public pyoModule
{
public:

	Example1PytModule();
	virtual ~Example1PytModule();

	virtual void DefineConstants();

public:
	omuPrimitive* importDxf(omuArguments& args);

private:

	Example1PytModule(const Example1PytModule&);
	Example1PytModule& operator=(const Example1PytModule&);

	// === DXF import helpers ===

	// Stage 1: parse DXF file, returns false on failure
	bool parseDxfFile(const QString& filePath, DxfData& outData,
	                  const std::set<std::string>& ignoredLayers = {});

	// Stage 2: coordinate conversion (incl. discretization), returns false on failure
	bool convertToSamData(const DxfData& dxfData, double baseX, double baseY,
	                      double baseZ, double tolerance, SamData& outData);

	// Stage 3: build SAM sketch and commit, auto-rollback on failure, returns created count or -1
	int buildSamSketch(const SamData& samData, SamBuilder& builder);
};

#endif  // #ifdef shipPytModule_h