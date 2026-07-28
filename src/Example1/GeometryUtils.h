#ifndef GeometryUtils_h
#define GeometryUtils_h

#include "DxfData.h"  // for DxfPoint
#include <vector>

namespace GeometryUtils {

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
