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
#include <QString>

// Class definition

/// @brief Python-callable module that imports DXF geometry as SAM sketches.
///
/// Pipeline: DxfParser -> ConversionEngine -> SamBuilder.
/// All accessible from Python via the importDxf() method.
class Example1PytModule : public pyoModule
{
public:

	Example1PytModule();
	virtual ~Example1PytModule();

	virtual void DefineConstants();

public:
	/// @brief Import a DXF file as a SAM sketch.
	/// @param args Arguments: filePath (str), baseX/Y/Z (float),
	///             curveTolerance (float, optional), ignoreLayers (str, optional).
	/// @return Number of created entities, or nullptr on failure.
	omuPrimitive* importDxf(omuArguments& args);

private:

	Example1PytModule(const Example1PytModule&);
	Example1PytModule& operator=(const Example1PytModule&);

	// === DXF import helpers ===

	enum class BuildStatus {
		Success,
		Canceled,
		Failed
	};

	struct BuildResult {
		BuildStatus status;
		int createdCount;
		QString error;
	};

	/// @brief Stage 3: build and commit, rolling back immediately on cancellation or failure.
	BuildResult buildSamSketch(const SamData& samData, SamBuilder& builder);
};

#endif  // #ifndef Example1PytModule_h
