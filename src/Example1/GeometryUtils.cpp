#include "GeometryUtils.h"
#include <cmath>

namespace GeometryUtils {

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

    // Calculate segment count based on tolerance
    if (tolerance <= 0.0) tolerance = 0.01;
    int segmentCount = 1;
    if (radius > tolerance) {
        double maxAngle = 2.0 * std::acos(
            std::max(-1.0, std::min(1.0,
                1.0 - tolerance / radius)));
        segmentCount = static_cast<int>(
            std::ceil(std::abs(theta) / maxAngle));
    }
    segmentCount = std::max(1, std::min(segmentCount, 10000));

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
