#include "ConversionEngine.h"
#include <QDebug>
#include <cmath>

bool ConversionEngine::convert(const DxfData& dxfData,
                               double baseX, double baseY, double baseZ,
                               SamData& outData)
{
    outData.clear();

    // --- points ---
    for (const DxfPoint& pt : dxfData.points()) {
        if (!pt.isValid())
            continue;
        outData.addPoint(translate(pt, baseX, baseY, baseZ));
    }

    // --- lines ---
    for (const DxfLine& line : dxfData.lines()) {
        if (!line.isValid())
            continue;
        DxfPoint s = translate(line.start(), baseX, baseY, baseZ);
        DxfPoint e = translate(line.end(),   baseX, baseY, baseZ);
        outData.addLine(DxfLine(s, e));
    }

    // --- circles ---
    for (const DxfCircle& circle : dxfData.circles()) {
        if (!circle.isValid())
            continue;
        DxfPoint c = translate(circle.center(), baseX, baseY, baseZ);
        outData.addCircle(DxfCircle(c, circle.radius()));
    }

    // --- polyline segments (bulge arcs → line segments) ---
    const double tol = defaultBulgeTolerance();
    int polyLineCount = 0;
    for (const DxfPolylineSegment& seg : dxfData.polylineSegments()) {
        std::vector<DxfPoint> pts = tessellateBulgeArc(seg.start, seg.end, seg.bulge, tol);
        for (size_t i = 1; i < pts.size(); ++i) {
            DxfPoint s = translate(pts[i - 1], baseX, baseY, baseZ);
            DxfPoint e = translate(pts[i],     baseX, baseY, baseZ);
            // 跳过退化线段
            if (s.x() == e.x() && s.y() == e.y() && s.z() == e.z())
                continue;
            outData.addLine(DxfLine(s, e));
            ++polyLineCount;
        }
    }
    if (polyLineCount > 0)
        qDebug() << "[ConversionEngine] bulge arcs →" << polyLineCount << "line segments";

    bool ok = !outData.points().empty() ||
              !outData.lines().empty()  ||
              !outData.circles().empty();

    qDebug() << "[ConversionEngine] done:"
             << "points=" << outData.points().size()
             << "lines="  << outData.lines().size()
             << "circles="<< outData.circles().size();

    return ok;
}

DxfPoint ConversionEngine::translate(const DxfPoint& pt,
                                      double bx, double by, double bz)
{
    return DxfPoint(pt.x() + bx,
                    pt.y() + by,
                    pt.z() + bz);
}

// ========================================================================
//  tessellateBulgeArc — 将 bulge 弧段离散为采样点（含首尾端点）
// ========================================================================

std::vector<DxfPoint> ConversionEngine::tessellateBulgeArc(
    const DxfPoint& p0, const DxfPoint& p1,
    double bulge, double tolerance)
{
    std::vector<DxfPoint> result;
    result.push_back(p0);

    // bulge ≈ 0 → 直线段
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

    double theta = 4.0 * std::atan(bulge);          // 有符号圆心角
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

    // 根据容差计算分段数
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

    // 强制首尾点等于原始端点，避免浮点累积误差
    result.front() = p0;
    result.back()  = p1;
    return result;
}
