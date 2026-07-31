#ifndef CSSFRAGMENT_H
#define CSSFRAGMENT_H

#include <cowList.h>
#include <g3dVector.h>
#include <ptsKPartFragment.h>
#include <QString>
#include <QPair>

class omuPrimitive;
class SectionDraftHelper;

class CSSFragment: public ptsKPartFragment
{
public:
    CSSFragment();
    virtual ~CSSFragment();

    omuPrimitive* Copy() const;

    omuPrimitive* draftOuterHull(omuArguments&);
    omuPrimitive* draftCamber(omuArguments&);
    omuPrimitive* draftInnerHull(omuArguments&);
    omuPrimitive* draftGrider(omuArguments&);
    omuPrimitive* draftStringer(omuArguments&);
    omuPrimitive* draftHatchCoaming(omuArguments&);

    omuPrimitive* rollbackDraft(omuArguments&);
    omuPrimitive* clearDraft(omuArguments&);

    omuPrimitive* draftToNodesAndBeams(omuArguments&);
    omuPrimitive* mirrorSection(omuArguments&);
    omuPrimitive* extrudeSection(omuArguments&);
    omuPrimitive* refineSection(omuArguments&);
    omuPrimitive* addStiffener(omuArguments&);

    omuPrimitive* fitDraftView(omuArguments&);
	omuPrimitive* createContour(omuArguments&);
	omuPrimitive* clearContour(omuArguments&);

private:
    SectionDraftHelper* helper_ptr;
    QString cmd_target;

    void createNodesAndBeams(const cowList<g3dVector>&, const cowList<QPair<int, int>>&);
    void DrawLine(const cowList<g3dVector>&, int, bool, const QString&);
};

#endif // CSSFRAGMENT_H
