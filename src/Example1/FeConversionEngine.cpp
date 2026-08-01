#include "FeConversionEngine.h"
#include "GeometryUtils.h"
#include <QDebug>
#include <algorithm>
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
static bool feedSegment(FeData& outData,
                        const DxfPoint& start, const DxfPoint& end,
                        double nodeMergeTolerance,
                        std::size_t maxOutputEntities)
{
    int s = outData.addOrGetNode(start.x(), start.y(), start.z(),
                                  nodeMergeTolerance);
    if (outData.nodes().size() + outData.trusses().size() > maxOutputEntities)
        return false;
    int e = outData.addOrGetNode(end.x(),   end.y(),   end.z(),
                                  nodeMergeTolerance);
    if (outData.nodes().size() + outData.trusses().size() > maxOutputEntities)
        return false;
    if (s == e) {
        ++outData.stats().skippedZeroLength;
        return true;
    }
    outData.addTruss(s, e);
    return outData.nodes().size() + outData.trusses().size() <= maxOutputEntities;
}

// Feed a batch of tessellated DxfLine segments (already translated).
static bool feedSegments(FeData& outData,
                         const std::vector<DxfLine>& segments,
                         double nodeMergeTolerance,
                         std::size_t maxOutputEntities)
{
    for (const DxfLine& seg : segments) {
        if (!seg.isValid()) continue;
        if (!feedSegment(outData, seg.start(), seg.end(), nodeMergeTolerance,
                         maxOutputEntities)) return false;
    }
    return true;
}

// ========================================================================
//  convert
// ========================================================================

bool FeConversionEngine::convert(const DxfData& dxfData,
                                 double baseX, double baseY, double baseZ,
                                 double curveTolerance,
                                 double nodeMergeTolerance,
                                 FeData& outData,
                                 std::size_t maxOutputEntities) const
{
    const auto startedAt = std::chrono::steady_clock::now();
    outData.clear();

    if (!std::isfinite(curveTolerance) || curveTolerance <= 0.0) {
        outData.setError(DxfImportErrorCode::InvalidArgument,
                         QStringLiteral("invalid curve tolerance"));
        qWarning() << "[FeConversionEngine] invalid curve tolerance:"
                    << curveTolerance;
        return false;
    }
    if (!std::isfinite(nodeMergeTolerance) || nodeMergeTolerance < 0.0) {
        outData.setError(DxfImportErrorCode::InvalidArgument,
                         QStringLiteral("invalid node merge tolerance"));
        qWarning() << "[FeConversionEngine] invalid node merge tolerance:"
                    << nodeMergeTolerance;
        return false;
    }
    if (!std::isfinite(baseX) ||
        !std::isfinite(baseY) ||
        !std::isfinite(baseZ)) {
        outData.setError(DxfImportErrorCode::InvalidArgument,
                         QStringLiteral("base coordinates must be finite"));
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
    if (maxOutputEntities == 0) {
        outData.setError(DxfImportErrorCode::InvalidArgument,
                         QStringLiteral("maxOutputEntities must be greater than zero"));
        return false;
    }

    auto failLimit = [&]() {
        outData.clear();
        outData.setError(
            DxfImportErrorCode::ConversionLimit,
            QStringLiteral("FE output exceeds %1 nodes and elements")
                .arg(static_cast<qulonglong>(maxOutputEntities)));
        return false;
    };

    const std::size_t reserveHint = std::min(entityCount, maxOutputEntities);
    outData.reserve(reserveHint, reserveHint);

    FeConversionStats& stats = outData.stats();

    // ---- POINT → standalone nodes (no truss) ----
    for (const DxfPoint& pt : dxfData.points()) {
        if (!pt.isValid()) continue;
        DxfPoint tp = translate(pt, baseX, baseY, baseZ);
        outData.addOrGetNode(tp.x(), tp.y(), tp.z(), nodeMergeTolerance);
        if (outData.nodes().size() + outData.trusses().size() > maxOutputEntities)
            return failLimit();
        ++stats.pointsProcessed;
    }

    // ---- LINE → direct truss ----
    for (const DxfLine& line : dxfData.lines()) {
        if (!line.isValid()) continue;
        DxfPoint s = translate(line.start(), baseX, baseY, baseZ);
        DxfPoint e = translate(line.end(),   baseX, baseY, baseZ);
        if (!feedSegment(outData, s, e, nodeMergeTolerance, maxOutputEntities))
            return failLimit();
        ++stats.linesProcessed;
    }

    // ---- CIRCLE → closed tessellation chain ----
    for (const DxfCircle& circle : dxfData.circles()) {
        if (!circle.isValid()) continue;
        DxfPoint c = translate(circle.center(), baseX, baseY, baseZ);
        DxfArc equiv(c, circle.radius(), 0.0, 2.0 * M_PI, true);
        auto segments = GeometryUtils::tessellateArc(equiv, curveTolerance);
        if (!feedSegments(outData, segments, nodeMergeTolerance, maxOutputEntities))
            return failLimit();
        ++stats.circlesDiscretized;
    }

    // ---- ARC ----
    for (const DxfArc& arc : dxfData.arcs()) {
        if (!arc.isValid()) continue;
        DxfPoint c = translate(arc.center(), baseX, baseY, baseZ);
        DxfArc shifted(c, arc.radius(),
                       arc.startAngle(), arc.endAngle(), arc.isCCW());
        if (!feedSegments(outData,
                          GeometryUtils::tessellateArc(shifted, curveTolerance),
                          nodeMergeTolerance, maxOutputEntities))
            return failLimit();
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
        if (!feedSegments(outData,
                          GeometryUtils::tessellateLWPolyline(shifted,
                                                              curveTolerance),
                          nodeMergeTolerance, maxOutputEntities))
            return failLimit();
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
        if (!feedSegments(outData,
                          GeometryUtils::tessellateEllipse(shifted,
                                                            curveTolerance),
                          nodeMergeTolerance, maxOutputEntities))
            return failLimit();
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
        if (!feedSegments(outData,
                          GeometryUtils::tessellateSpline(shifted, curveTolerance),
                          nodeMergeTolerance, maxOutputEntities))
            return failLimit();
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
    if (!hasNodes) {
        outData.setError(DxfImportErrorCode::ConversionFailed,
                         QStringLiteral("no valid FE entities to convert"));
    }
    qInfo().noquote() << "[FeConversionEngine] convert:"
                      << stats.totalInputEntities << "entities ->"
                      << outData.nodes().size() << "nodes,"
                      << outData.trusses().size() << "trusses in"
                      << std::chrono::duration_cast<std::chrono::milliseconds>(
                             std::chrono::steady_clock::now() - startedAt).count()
                      << "ms";
    return hasNodes;
}
