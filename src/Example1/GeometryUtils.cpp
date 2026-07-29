#include "GeometryUtils.h"
#include <cmath>

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

double normalizeSweep(double sweep, bool isCCW)
{
    if (isCCW) {
        while (sweep <= 0.0) { sweep += 2.0 * M_PI; }
    } else {
        while (sweep >= 0.0) { sweep -= 2.0 * M_PI; }
    }
    return sweep;
}

std::vector<DxfLine> tessellateArc(const DxfArc& arc, double tolerance)
{
    std::vector<DxfLine> result;

    double cx = arc.center().x();
    double cy = arc.center().y();
    double cz = arc.center().z();
    double r  = arc.radius();
    double a0 = arc.startAngle();
    double sweep = normalizeSweep(arc.endAngle() - a0, arc.isCCW());

    int segments = calculateArcSegmentCount(r, std::abs(sweep), tolerance);

    auto pointAt = [&](double t) -> DxfPoint {
        return DxfPoint(cx + r * std::cos(t), cy + r * std::sin(t), cz);
    };

    DxfPoint previous = pointAt(a0);
    for (int i = 1; i <= segments; ++i) {
        double t = a0 + sweep * static_cast<double>(i) / static_cast<double>(segments);
        DxfPoint current = pointAt(t);
        result.push_back(DxfLine(previous, current));
        previous = current;
    }

    return result;
}

std::vector<DxfLine> tessellateLWPolyline(const DxfLWPolyline& poly,
                                          double tolerance)
{
    std::vector<DxfLine> result;

    const auto& vertices = poly.vertices();
    const auto& bulges   = poly.bulges();
    const int N = poly.vertexCount();

    auto tessellateSegment = [&](int i, int j, double bulge) {
        DxfPoint p0 = vertices[i];
        DxfPoint p1 = vertices[j];
        p0.setZ(poly.constZ());
        p1.setZ(poly.constZ());

        std::vector<DxfPoint> pts = tessellateBulgeArc(p0, p1, bulge, tolerance);

        for (size_t k = 1; k < pts.size(); ++k) {
            result.push_back(DxfLine(pts[k - 1], pts[k]));
        }
    };

    for (int i = 0; i < N - 1; ++i) {
        tessellateSegment(i, i + 1, bulges[i]);
    }
    if (poly.isClosed()) {
        tessellateSegment(N - 1, 0, bulges[N - 1]);
    }

    return result;
}

std::vector<DxfLine> tessellateEllipse(const DxfEllipse& ellipse,
                                       double tolerance)
{
    std::vector<DxfLine> result;

    const DxfPoint& center = ellipse.center();
    const DxfPoint& majorEnd = ellipse.majorAxisEnd();
    double majorX = majorEnd.x();
    double majorY = majorEnd.y();
    double majorLen = std::sqrt(majorX * majorX + majorY * majorY);

    if (majorLen <= 0.0) return result;

    double ratio = ellipse.ratio();
    double minorX = -majorY * ratio;
    double minorY =  majorX * ratio;

    double startParam = ellipse.startParam();
    double endParam   = ellipse.endParam();
    double sweep = normalizeSweep(endParam - startParam, ellipse.isCCW());

    const double maxRadius = (majorLen > majorLen * ratio)
        ? majorLen : majorLen * ratio;
    const int segments = calculateArcSegmentCount(
        maxRadius, std::abs(sweep), tolerance);

    auto pointAt = [&](double t) -> DxfPoint {
        return DxfPoint(
            center.x() + majorX * std::cos(t) + minorX * std::sin(t),
            center.y() + majorY * std::cos(t) + minorY * std::sin(t),
            center.z());
    };

    DxfPoint previous = pointAt(startParam);
    for (int i = 1; i <= segments; ++i) {
        double t = startParam + sweep * static_cast<double>(i)
                   / static_cast<double>(segments);
        DxfPoint current = pointAt(t);
        result.push_back(DxfLine(previous, current));
        previous = current;
    }

    return result;
}

}  // namespace GeometryUtils
