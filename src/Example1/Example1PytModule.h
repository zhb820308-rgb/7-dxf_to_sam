/* -*- mode: c++ -*- */
#ifndef Example1PytModule_h
#define Example1PytModule_h

// Forward declarations
class DxfData;
class FeData;
class PythonFiniteElementBuilder;
class SamData;
class SamBuilder;

// Includes

// Begin local includes

#include <ptsKPartFragment.h>
#include <pyoModule.h>

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

// Stage 3: build SAM sketch and commit, auto-rollback on failure, returns created count or -1
int buildSamSketch(const SamData& samData, SamBuilder& builder);

// FE mode: build nodes + trusses and commit, auto-rollback on failure, returns node+truss count or -1
int buildFePart(FeData& feData, const QString& modelName,
	const QString& partName, PythonFiniteElementBuilder& builder);
};

#endif  // #ifdef shipPytModule_h