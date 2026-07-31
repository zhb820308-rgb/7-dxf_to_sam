#ifndef GEOMCALCUTILS_H
#define GEOMCALCUTILS_H

#include <QtMath>
#include <cowList.h>
#include <g3dVector.h>

inline double calcSpecialY(double half_breadth, double bilge_radius, double inner_hull_height)
{
    double dz = bilge_radius - inner_hull_height;
    if(dz <= 0)
    {
        return half_breadth;
    }
    else
    {
        return qSqrt(bilge_radius * bilge_radius - dz * dz) + half_breadth - bilge_radius;
    }
}

inline double calcSpecialZ(double half_breadth, double bilge_radius, double inner_hull_distance)
{
    double dy = inner_hull_distance - half_breadth + bilge_radius;
    if(dy <= 0)
    {
        return 0;
    }
    else
    {
        return bilge_radius - qSqrt(bilge_radius * bilge_radius - dy * dy);
    }
}

inline cowList<g3dVector> calcArcInnerPoints(
    double half_breadth, double bilge_radius, uint arc_segmentation = 3)
{
    constexpr double C = 0.1339745962155;
    cowList<g3dVector> nodes(cowListConstruct::cowEstimateLength, arc_segmentation - 1);

    nodes.Append(g3dVector(0.0, half_breadth - bilge_radius * C, 0.5 * bilge_radius));
    nodes.Append(g3dVector(0.0, half_breadth - 0.5 * bilge_radius, bilge_radius * C));

    return nodes;
}

#endif // GEOMCALCUTILS_H
