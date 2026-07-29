#ifndef GeometryUtils_h
#define GeometryUtils_h

#include "DxfData.h"  // for DxfPoint, DxfArc, DxfLWPolyline, DxfEllipse, DxfLine, DxfSpline
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

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

/// Normalize a signed sweep angle to the canonical range.
/// CCW: ensures sweep > 0 (adds 2π until positive).
/// CW:  ensures sweep < 0 (subtracts 2π until negative).
/// Does NOT clamp upper bound — full-circle sweeps (±2π) are preserved
/// for direct angular sampling.
double normalizeSweep(double sweep, bool isCCW);

/// Discretize an arc (center + radius + angles) into line segments.
/// Uses direct angular sampling from the known center.
std::vector<DxfLine> tessellateArc(const DxfArc& arc, double tolerance);

/// Discretize a lightweight polyline into line segments.
/// Each segment's bulge is processed via tessellateBulgeArc.
std::vector<DxfLine> tessellateLWPolyline(const DxfLWPolyline& poly,
                                          double tolerance);

/// Discretize an elliptical arc into line segments via parametric sampling.
std::vector<DxfLine> tessellateEllipse(const DxfEllipse& ellipse,
                                       double tolerance);

/// Discretize a B-spline curve into line segments via OCCT.
/// Uses chord-height adaptive sampling (GCPnts_TangentialDeflection)
/// for smooth curve approximation.
/// @param  spline    DXF spline data (control points or fit points)
/// @param  tolerance discretization tolerance
/// @return sampled line segments; empty on failure
std::vector<DxfLine> tessellateSpline(const DxfSpline& spline,
                                      double tolerance);

}  // namespace GeometryUtils

#endif  // GeometryUtils_h
