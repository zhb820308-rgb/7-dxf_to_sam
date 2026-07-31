/* -*- mode: c++ -*- */
#ifndef SAMExample1Fragment_h
#define SAMExample1Fragment_h

#include <Example1Utils.h>
#include <ptsKPartFragment.h>
#include <g3dVector.h>

class omuArguments;

/// @brief Example SAM fragment demonstrating basic line drawing.
///
/// Demonstrates: custom geometry editor registration, line creation,
/// and node location queries. KLine is a line editor registered with the scene.
class SAMExample1Fragment : public ptsKPartFragment
{
public:
	SAMExample1Fragment();
	virtual ~SAMExample1Fragment();

	omuPrimitive* Copy() const;

	/// @brief Draw example geometry — demonstrates creating a rectangle.
	omuPrimitive* drawExample(omuArguments& args);

	/// @brief Query the location of a specified node.
	omuPrimitive* getNodeLocation(omuArguments& args);

	/// @brief Draw a line segment and register it with the scene editor.
	void DrawLine(int& segID, g3dVector startPoint, g3dVector endPoint);
};

#endif  // #ifndef SAMExample1Fragment_h
