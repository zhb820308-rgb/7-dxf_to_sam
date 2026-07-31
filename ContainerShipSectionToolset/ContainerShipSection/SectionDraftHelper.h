#ifndef SECTIONDRAFTHELPER_H
#define SECTIONDRAFTHELPER_H

#include <cowList.h>
#include <cowListDouble.h>
#include <g3dVector.h>
#include <omuArguments.h>
#include <QPair>

class SectionDraftHelper
{
public:
    SectionDraftHelper() = default;
    virtual ~SectionDraftHelper() = default;

    cowList<g3dVector> connectAtOuterHull(omuArguments&);
    cowList<g3dVector> connectAtCamber(omuArguments&);
    cowList<g3dVector> connectAtInnerHull(omuArguments&);
    cowList<g3dVector> connectAtGrider(omuArguments&);
    cowList<g3dVector> connectAtStringer(omuArguments&);
    cowList<g3dVector> connectAtHatchCoaming(omuArguments&);

    cowList<g3dVector> connectAtPreciseZOuterHull() const;
    cowList<g3dVector> connectAtPreciseYZOuterHull() const;

    void calcNodesAndBeams(cowList<g3dVector>&, cowList<QPair<int, int>>&) const;

    void draftBoundingBox(const QString&, double&, double&) const;

    void saveExtrudeOptions(double, int);
    double getStiffenerLen() const;
    cowList<g3dVector> getStiffenerPos(bool) const;

private:
    // Outer Hull 外壳参数
    double half_breadth = -1;
    double depth = -1;
    double bilge_radius = -1;
    double deck_corner_radius = -1;
    // Camber 外倾角
    double camber_height = -1;
    double deck_distance = -1;
    // Inner Hull 内壳参数
    double inner_hull_height = -1;
    double inner_hull_distance = -1;
    // Grider 双层底纵桁参数
    cowListDouble grider_distances;
    // Stringer 双壳纵桁参数
    cowListDouble stringer_distances;
    // Hatch Coaming 舱口围参数
    double hatch_coaming_height = -1;
    double hatch_coaming_width = -1;
    // Mesh Option 网格选项
    double extrude_direction = 0.0;
    int extrude_cnt = -1;
};

#endif // SECTIONDRAFTHELPER_H
