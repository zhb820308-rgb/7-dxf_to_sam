#ifndef GeometryUtils_h
#define GeometryUtils_h

#include "DxfData.h"  // for DxfPoint
#include <vector>

namespace GeometryUtils {

/// Calculate the number of segments needed to approximate a circular arc.
/// @param radius    arc radius (> 0)
/// @param sweep     absolute sweep angle in radians
/// @param tolerance chord-to-arc sagitta tolerance (default 0.01)
/// @return segment count, at least 2, at most 10000
int calculateArcSegmentCount(double radius, double sweep,
                             double tolerance = 0.01);

/// Discretize a bulge arc segment into sample points (including p0 and p1).
/// bulge = tan(theta/4), where theta is the signed central angle.
/// Returns at least {p0, p1} (2 points) for a valid arc.
std::vector<DxfPoint> tessellateBulgeArc(
    const DxfPoint& p0,
    const DxfPoint& p1,
    double bulge,
    double tolerance = 0.01);

}  // namespace GeometryUtils

#endif  // GeometryUtils_h
