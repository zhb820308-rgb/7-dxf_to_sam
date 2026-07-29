#include "ConversionEngine.h"
#include "GeometryUtils.h"
#include <QDebug>
#include <cmath>

// ========================================================================
//  convert
// ========================================================================

bool ConversionEngine::convert(const DxfData& dxfData,
                               double baseX, double baseY, double baseZ,
                               double tolerance,
                               SamData& outData) const
{
    outData.clear();

    if (!std::isfinite(tolerance) || tolerance <= 0.0) {
        qWarning() << "[ConversionEngine] invalid curve tolerance:" << tolerance;
        return false;
    }

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
        addSegments(GeometryUtils::tessellateArc(arc, tolerance));
    }

    // --- lwPolylines (离散化 + 平移) ---
    for (const DxfLWPolyline& poly : dxfData.lwPolylines()) {
        if (!poly.isValid()) continue;
        addSegments(GeometryUtils::tessellateLWPolyline(poly, tolerance));
    }

    // --- ellipses (离散化 + 平移) ---
    for (const DxfEllipse& ellipse : dxfData.ellipses()) {
        if (!ellipse.isValid()) continue;
        addSegments(GeometryUtils::tessellateEllipse(ellipse, tolerance));
    }

    // --- splines (OCCT B-spline 构建 + 离散化 + 平移) ---
    for (const DxfSpline& spline : dxfData.splines()) {
        if (!spline.isValid()) continue;
        addSegments(GeometryUtils::tessellateSpline(spline, tolerance));
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
