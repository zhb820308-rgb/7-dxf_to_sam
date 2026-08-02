/* -*- mode: c++ -*- */
#ifndef Example1PytModule_h
#define Example1PytModule_h

// Includes

// Begin local includes

#include <ptsKPartFragment.h>
#include <pyoModule.h>

// Class definition

/// @brief Python-callable adapter for Sketch and finite-element DXF imports.
///
/// Parses Python arguments, delegates the request to the shared import
/// orchestrator, and converts its structured outcome to the legacy Python
/// return contract.
class Example1PytModule : public pyoModule
{
public:

	Example1PytModule();
	virtual ~Example1PytModule();

	virtual void DefineConstants();

public:
	/// @brief Import a DXF file as a SAM sketch or finite-element part.
	/// @param args Arguments: filePath (str), baseX/Y/Z (float),
	///             curveTolerance, ignoreLayers, importMode, modelName,
	///             partName, nodeMergeTolerance and maxOutputEntities
	///             (all optional after baseX/Y/Z).
	/// @return Number of created entities, or nullptr on failure.
	omuPrimitive* importDxf(omuArguments& args);

private:

	Example1PytModule(const Example1PytModule&);
	Example1PytModule& operator=(const Example1PytModule&);
};

#endif  // #ifndef Example1PytModule_h
