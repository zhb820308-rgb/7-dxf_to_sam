#include "ConversionEngine.h"
#include "GeometryUtils.h"
#include <QDebug>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ========================================================================
//  扫掠角归一化（内部工具函数）
//  仅做符号归一化：CCW → 正角度，CW → 负角度，不钳位上界
//  满圆/满椭圆（sweep=2π）由调用者通过直接角度采样处理
// ========================================================================

static double normalizeSweep(double sweep, bool isCCW)
{
    if (isCCW) {
        while (sweep <= 0.0) { sweep += 2.0 * M_PI; }
    } else {
        while (sweep >= 0.0) { sweep -= 2.0 * M_PI; }
    }
    return sweep;
}

// ========================================================================
//  tessellateArc
// ========================================================================

std::vector<DxfLine> ConversionEngine::tessellateArc(
    const DxfArc& arc, double tolerance)
{
    std::vector<DxfLine> result;

    const DxfPoint& center = arc.center();
    double cx = center.x();
    double cy = center.y();
    double cz = center.z();
    double r  = arc.radius();
    double a0 = arc.startAngle();
    double sweep = normalizeSweep(arc.endAngle() - a0, arc.isCCW());

    int segments = GeometryUtils::calculateArcSegmentCount(r, std::abs(sweep), tolerance);

    // 直接从圆心 + 角度采样，不经过 bulge 往返
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

// ========================================================================
//  tessellateLWPolyline
// ========================================================================

std::vector<DxfLine> ConversionEngine::tessellateLWPolyline(
    const DxfLWPolyline& poly, double tolerance)
{
    std::vector<DxfLine> result;

    const auto& vertices = poly.vertices();
    const auto& bulges   = poly.bulges();
    const int N = poly.vertexCount();

    auto tessellateSegment = [&](int i, int j, double bulge) {
        DxfPoint p0 = vertices[i];
        DxfPoint p1 = vertices[j];
        // 统一使用多段线的 Z 高度
        p0.setZ(poly.constZ());
        p1.setZ(poly.constZ());

        std::vector<DxfPoint> pts =
            GeometryUtils::tessellateBulgeArc(p0, p1, bulge, tolerance);

        for (size_t k = 1; k < pts.size(); ++k) {
            result.push_back(DxfLine(pts[k - 1], pts[k]));
        }
    };

    // 遍历 N-1 个开放段
    for (int i = 0; i < N - 1; ++i) {
        tessellateSegment(i, i + 1, bulges[i]);
    }

    // 闭合段
    if (poly.isClosed()) {
        tessellateSegment(N - 1, 0, bulges[N - 1]);
    }

    return result;
}

// ========================================================================
//  tessellateEllipse
// ========================================================================

std::vector<DxfLine> ConversionEngine::tessellateEllipse(
    const DxfEllipse& ellipse, double tolerance)
{
    std::vector<DxfLine> result;

    const DxfPoint& center = ellipse.center();
    const DxfPoint& majorEnd = ellipse.majorAxisEnd();
    double majorX = majorEnd.x();
    double majorY = majorEnd.y();
    double majorLen = std::sqrt(majorX * majorX + majorY * majorY);

    if (majorLen <= 0.0) return result;

    // 次轴向量：垂直于长轴，长度 = majorLen * ratio
    double ratio = ellipse.ratio();
    double minorX = -majorY * ratio;
    double minorY =  majorX * ratio;

    // 参数范围处理
    double startParam = ellipse.startParam();
    double endParam   = ellipse.endParam();
    double sweep = normalizeSweep(endParam - startParam, ellipse.isCCW());

    // 分段数（基于长轴半径）
    const double maxRadius = (majorLen > majorLen * ratio)
        ? majorLen : majorLen * ratio;
    const int segments = GeometryUtils::calculateArcSegmentCount(
        maxRadius, std::abs(sweep), tolerance);

    // 参数方程采样
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

// ========================================================================
//  convert
// ========================================================================

bool ConversionEngine::convert(const DxfData& dxfData,
                               double baseX, double baseY, double baseZ,
                               SamData& outData)
{
    outData.clear();

    const double tol = defaultBulgeTolerance();

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

    // --- 辅助：将离散化后的线段平移并添加到输出 ---
    auto addSegments = [&](const std::vector<DxfLine>& segments) {
        for (const DxfLine& seg : segments) {
            if (!seg.isValid()) continue;
            outData.addLine(DxfLine(
                translate(seg.start(), baseX, baseY, baseZ),
                translate(seg.end(),   baseX, baseY, baseZ)));
        }
    };

    // --- arcs (离散化 + 平移) ---
    for (const DxfArc& arc : dxfData.arcs()) {
        if (!arc.isValid()) continue;
        addSegments(tessellateArc(arc, tol));
    }

    // --- lwPolylines (离散化 + 平移) ---
    for (const DxfLWPolyline& poly : dxfData.lwPolylines()) {
        if (!poly.isValid()) continue;
        addSegments(tessellateLWPolyline(poly, tol));
    }

    // --- ellipses (离散化 + 平移) ---
    for (const DxfEllipse& ellipse : dxfData.ellipses()) {
        if (!ellipse.isValid()) continue;
        addSegments(tessellateEllipse(ellipse, tol));
    }

    bool ok = !outData.points().empty() ||
              !outData.lines().empty()  ||
              !outData.circles().empty();

    qDebug() << "[ConversionEngine] done:"
             << "points=" << outData.points().size()
             << "lines="  << outData.lines().size()
             << "circles="<< outData.circles().size();

    return ok;
}

// ========================================================================
//  translate
// ========================================================================

DxfPoint ConversionEngine::translate(const DxfPoint& pt,
                                      double bx, double by, double bz)
{
    return DxfPoint(pt.x() + bx,
                    pt.y() + by,
                    pt.z() + bz);
}
