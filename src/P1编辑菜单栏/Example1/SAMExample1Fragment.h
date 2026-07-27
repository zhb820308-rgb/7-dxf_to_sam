/* -*- mode: c++ -*- */
#ifndef SAMExample1Fragment_h
#define SAMExample1Fragment_h

#include <Example1Utils.h>
#include <ptsKPartFragment.h>

class omuArguments;

// Class definition

class SAMExample1Fragment : public ptsKPartFragment
{
public:
	SAMExample1Fragment();
	virtual ~SAMExample1Fragment();

	omuPrimitive* Copy() const;

};

#endif  // #ifdef SAMExample1Fragment_h
