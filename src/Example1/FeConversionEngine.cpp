#include "FeConversionEngine.h"
#include "GeometryUtils.h"
#include <QDebug>
#include <chrono>
#include <cmath>

// ========================================================================
//  Helpers
// ========================================================================

DxfPoint FeConversionEngine::translate(const DxfPoint& pt,
                                        double bx, double by, double bz)
{
    return DxfPoint(pt.x() + bx, pt.y() + by, pt.z() + bz);
}

// Feed a segment (DxfLine) into FeData — merges nodes and adds a
// truss.  Skips degenerate (same-node) and zero-length segments.
static void feedSegment(FeData& outData,
                        const DxfPoint& start, const DxfPoint& end,
                        double nodeMergeTolerance)
{
    int s = outData.addOrGetNode(start.x(), start.y(), start.z(),
                                  nodeMergeTolerance);
    int e = outData.addOrGetNode(end.x(),   end.y(),   end.z(),
                                  nodeMergeTolerance);
    if (s == e) {
        ++outData.stats().skippedZeroLength;
        return;
    }
    outData.addTruss(s, e);
}

// Feed a batch of tessellated DxfLine segments (already translated).
static void feedSegments(FeData& outData,
                         const std::vector<DxfLine>& segments,
                         double nodeMergeTolerance)
{
    for (const DxfLine& seg : segments) {
        if (!seg.isValid()) continue;
        feedSegment(outData, seg.start(), seg.end(), nodeMergeTolerance);
    }
}

// ========================================================================
//  convert
// ========================================================================

bool FeConversionEngine::convert(const DxfData& dxfData,
                                 double baseX, double baseY, double baseZ,
                                 double curveTolerance,
                                 double nodeMergeTolerance,
                                 FeData& outData) const
{
    const auto startedAt = std::chrono::steady_clock::now();
    outData.clear();

    if (!std::isfinite(curveTolerance) || curveTolerance <= 0.0) {
        qWarning() << "[FeConversionEngine] invalid curve tolerance:"
                    << curveTolerance;
        return false;
    }
    if (!std::isfinite(nodeMergeTolerance) || nodeMergeTolerance < 0.0) {
        qWarning() << "[FeConversionEngine] invalid node merge tolerance:"
                    << nodeMergeTolerance;
        return false;
    }
    if (!std::isfinite(baseX) ||
        !std::isfinite(baseY) ||
        !std::isfinite(baseZ)) {
        qWarning() << "[FeConversionEngine] base coordinates must be finite:"
                   << baseX << baseY << baseZ;
        return false;
    }

    // Curves can tessellate into additional segments and merging can reduce
    // them again, so this is only an initial capacity hint for common inputs.
    const std::size_t entityCount = dxfData.points().size()
                                  + dxfData.lines().size()
                                  + dxfData.circles().size()
                                  + dxfData.arcs().size()
                                  + dxfData.lwPolylines().size()
                                  + dxfData.ellipses().size()
                                  + dxfData.splines().size();
    outData.reserve(entityCount, entityCount);

    FeConversionStats& stats = outData.stats();

    // ---- POINT → standalone nodes (no truss) ----
    for (const DxfPoint& pt : dxfData.points()) {
        if (!pt.isValid()) continue;
        DxfPoint tp = translate(pt, baseX, baseY, baseZ);
        outData.addOrGetNode(tp.x(), tp.y(), tp.z(), nodeMergeTolerance);
        ++stats.pointsProcessed;
    }

    // ---- LINE → direct truss ----
    for (const DxfLine& line : dxfData.lines()) {
        if (!line.isValid()) continue;
        DxfPoint s = translate(line.start(), baseX, baseY, baseZ);
        DxfPoint e = translate(line.end(),   baseX, baseY, baseZ);
        feedSegment(outData, s, e, nodeMergeTolerance);
        ++stats.linesProcessed;
    }

    // ---- CIRCLE → closed tessellation chain ----
    for (const DxfCircle& circle : dxfData.circles()) {
        if (!circle.isValid()) continue;
        DxfPoint c = translate(circle.center(), baseX, baseY, baseZ);
        DxfArc equiv(c, circle.radius(), 0.0, 2.0 * M_PI, true);
        auto segments = GeometryUtils::tessellateArc(equiv, curveTolerance);
        feedSegments(outData, segments, nodeMergeTolerance);
        ++stats.circlesDiscretized;
    }

    // ---- ARC ----
    for (const DxfArc& arc : dxfData.arcs()) {
        if (!arc.isValid()) continue;
        DxfPoint c = translate(arc.center(), baseX, baseY, baseZ);
        DxfArc shifted(c, arc.radius(),
                       arc.startAngle(), arc.endAngle(), arc.isCCW());
        feedSegments(outData,
                     GeometryUtils::tessellateArc(shifted, curveTolerance),
                     nodeMergeTolerance);
        ++stats.arcsDiscretized;
    }

    // ---- LWPolyline ----
    for (const DxfLWPolyline& poly : dxfData.lwPolylines()) {
        if (!poly.isValid()) continue;
        std::vector<DxfPoint> verts;
        for (const DxfPoint& v : poly.vertices())
            verts.push_back(translate(v, baseX, baseY, baseZ));
        DxfLWPolyline shifted(verts, poly.bulges(),
                              poly.isClosed(), poly.constZ() + baseZ);
        feedSegments(outData,
                     GeometryUtils::tessellateLWPolyline(shifted,
                                                         curveTolerance),
                     nodeMergeTolerance);
        ++stats.lwPolylinesDiscretized;
    }

    // ---- Ellipse ----
    for (const DxfEllipse& ellipse : dxfData.ellipses()) {
        if (!ellipse.isValid()) continue;
        const DxfPoint c = translate(ellipse.center(), baseX, baseY, baseZ);
        // DXF group 11/21/31 is a vector relative to the center. Translation
        // applies to the center only; subtracting the translated center from
        // this vector corrupts both its direction and length.
        DxfEllipse shifted(c, ellipse.majorAxisEnd(),
                           ellipse.ratio(),
                           ellipse.startParam(), ellipse.endParam(),
                           ellipse.isCCW());
        feedSegments(outData,
                     GeometryUtils::tessellateEllipse(shifted,
                                                      curveTolerance),
                     nodeMergeTolerance);
        ++stats.ellipsesDiscretized;
    }

    // ---- Spline ----
    for (const DxfSpline& spline : dxfData.splines()) {
        if (!spline.isValid()) continue;
        std::vector<DxfPoint> ctrlPts;
        for (const DxfPoint& p : spline.controlPoints())
            ctrlPts.push_back(translate(p, baseX, baseY, baseZ));
        std::vector<DxfPoint> fitPts;
        for (const DxfPoint& p : spline.fitPoints())
            fitPts.push_back(translate(p, baseX, baseY, baseZ));
        DxfSpline shifted(ctrlPts, spline.knots(), spline.weights(),
                          fitPts, spline.degree(), spline.flags(),
                          spline.tgStartX(), spline.tgStartY(), spline.tgStartZ(),
                          spline.tgEndX(),   spline.tgEndY(),   spline.tgEndZ());
        feedSegments(outData,
                     GeometryUtils::tessellateSpline(shifted, curveTolerance),
                     nodeMergeTolerance);
        ++stats.splinesDiscretized;
    }

    stats.totalInputEntities = stats.pointsProcessed
                             + stats.linesProcessed
                             + stats.circlesDiscretized
                             + stats.arcsDiscretized
                             + stats.lwPolylinesDiscretized
                             + stats.ellipsesDiscretized
                             + stats.splinesDiscretized;

    const bool hasNodes = !outData.nodes().empty();
    qInfo().noquote() << "[FeConversionEngine] convert:"
                      << stats.totalInputEntities << "entities ->"
                      << outData.nodes().size() << "nodes,"
                      << outData.trusses().size() << "trusses in"
                      << std::chrono::duration_cast<std::chrono::milliseconds>(
                             std::chrono::steady_clock::now() - startedAt).count()
                      << "ms";
    return hasNodes;
}
