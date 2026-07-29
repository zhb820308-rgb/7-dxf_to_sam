#include "GeometryUtils.h"
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace GeometryUtils {

int calculateArcSegmentCount(double radius, double sweep, double tolerance)
{
    if (radius <= 0.0 || sweep <= 0.0)
        return 1;
    if (tolerance <= 0.0)
        tolerance = 1e-6;
    if (tolerance >= radius)
        return 2;

    double maxAngle = 2.0 * std::acos(
        std::max(-1.0, std::min(1.0, 1.0 - tolerance / radius)));

    // 每段角度上限 45°，保证视觉质量
    const double minAngle = M_PI / 4.0;
    if (maxAngle > minAngle)
        maxAngle = minAngle;

    int segments = static_cast<int>(std::ceil(sweep / maxAngle));
    return std::max(2, std::min(segments, 10000));
}

std::vector<DxfPoint> tessellateBulgeArc(
    const DxfPoint& p0, const DxfPoint& p1,
    double bulge, double tolerance)
{
    std::vector<DxfPoint> result;
    result.push_back(p0);

    // bulge ~ 0 → straight line
    if (std::abs(bulge) < 1e-12) {
        result.push_back(p1);
        return result;
    }

    double dx = p1.x() - p0.x();
    double dy = p1.y() - p0.y();
    double chord = std::sqrt(dx * dx + dy * dy);
    if (chord < 1e-12) {
        result.push_back(p1);
        return result;
    }

    double theta = 4.0 * std::atan(bulge);          // signed central angle
    double radius = chord * (1.0 + bulge * bulge)
                    / (4.0 * std::abs(bulge));

    double midX = (p0.x() + p1.x()) * 0.5;
    double midY = (p0.y() + p1.y()) * 0.5;
    double nx = -dy / chord;
    double ny =  dx / chord;
    double centerDist = chord * (1.0 - bulge * bulge)
                        / (4.0 * bulge);

    double cx = midX + nx * centerDist;
    double cy = midY + ny * centerDist;

    double startAngle = std::atan2(p0.y() - cy, p0.x() - cx);

    // Calculate segment count via shared utility
    if (tolerance <= 0.0) tolerance = 0.01;
    int segmentCount = calculateArcSegmentCount(radius, std::abs(theta), tolerance);

    for (int i = 1; i < segmentCount; ++i) {
        double angle = startAngle
                       + theta * static_cast<double>(i)
                         / static_cast<double>(segmentCount);
        result.push_back(DxfPoint(
            cx + radius * std::cos(angle),
            cy + radius * std::sin(angle),
            p0.z() + (p1.z() - p0.z())
                   * static_cast<double>(i)
                   / static_cast<double>(segmentCount)));
    }
    result.push_back(p1);

    // Snap endpoints to original points to avoid floating-point drift
    result.front() = p0;
    result.back()  = p1;
    return result;
}

}  // namespace GeometryUtils
