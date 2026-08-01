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
                               SamData& outData,
                               std::size_t maxOutputEntities) const
{
    outData.clear();

    if (!std::isfinite(tolerance) || tolerance <= 0.0) {
        outData.setError(DxfImportErrorCode::InvalidArgument,
                         QStringLiteral("invalid curve tolerance"));
        qWarning() << "[ConversionEngine] invalid curve tolerance:" << tolerance;
        return false;
    }
    if (maxOutputEntities == 0) {
        outData.setError(DxfImportErrorCode::InvalidArgument,
                         QStringLiteral("maxOutputEntities must be greater than zero"));
        return false;
    }
    if (!std::isfinite(baseX) ||
        !std::isfinite(baseY) ||
        !std::isfinite(baseZ)) {
        outData.setError(DxfImportErrorCode::InvalidArgument,
                         QStringLiteral("base coordinates must be finite"));
        qWarning() << "[ConversionEngine] base coordinates must be finite:"
                   << baseX << baseY << baseZ;
        return false;
    }

    // POINT entities are parsed and logged as raw DXF data, but they are not
    // converted because the current SAM builder only creates lines and circles.

    auto consumeOutput = [&](std::size_t count) {
        const std::size_t current = outData.lines().size() + outData.circles().size();
        if (count > maxOutputEntities || current > maxOutputEntities - count) {
            outData.clear();
            outData.setError(
                DxfImportErrorCode::ConversionLimit,
                QStringLiteral("conversion output exceeds %1 entities")
                    .arg(static_cast<qulonglong>(maxOutputEntities)));
            return false;
        }
        return true;
    };

    // --- lines ---
    for (const DxfLine& line : dxfData.lines()) {
        if (!line.isValid())
            continue;
        if (!consumeOutput(1)) return false;
        DxfPoint s = translate(line.start(), baseX, baseY, baseZ);
        DxfPoint e = translate(line.end(),   baseX, baseY, baseZ);
        outData.addLine(DxfLine(s, e));
    }

    // --- circles ---
    for (const DxfCircle& circle : dxfData.circles()) {
        if (!circle.isValid())
            continue;
        if (!consumeOutput(1)) return false;
        DxfPoint c = translate(circle.center(), baseX, baseY, baseZ);
        outData.addCircle(DxfCircle(c, circle.radius()));
    }

    // --- helper: translate tessellated segments and append to output ---
    auto addSegments = [&](const std::vector<DxfLine>& segments,
                           EntityType parentType, int parentId) {
        std::size_t validCount = 0;
        for (const DxfLine& seg : segments) {
            if (seg.isValid()) ++validCount;
        }
        if (!consumeOutput(validCount)) return false;
        size_t segmentIndex = 0;
        for (const DxfLine& seg : segments) {
            if (!seg.isValid()) continue;
            outData.addCurveSegment(parentType, parentId, segmentIndex, DxfLine(
                translate(seg.start(), baseX, baseY, baseZ),
                translate(seg.end(),   baseX, baseY, baseZ)));

            ++segmentIndex;
        }
        return true;
    };

    // --- arcs (tessellation + translation) ---
    for (const DxfArc& arc : dxfData.arcs()) {
        if (!arc.isValid()) continue;
        if (!addSegments(GeometryUtils::tessellateArc(arc, tolerance),
                         EntityType::Arc, arc.getId())) return false;
    }

    // --- lwPolylines (tessellation + translation) ---
    for (const DxfLWPolyline& poly : dxfData.lwPolylines()) {
        if (!poly.isValid()) continue;
        if (!addSegments(GeometryUtils::tessellateLWPolyline(poly, tolerance),
                         EntityType::LWPolyline, poly.getId())) return false;
    }

    // --- ellipses (tessellation + translation) ---
    for (const DxfEllipse& ellipse : dxfData.ellipses()) {
        if (!ellipse.isValid()) continue;
        if (!addSegments(GeometryUtils::tessellateEllipse(ellipse, tolerance),
                         EntityType::Ellipse, ellipse.getId())) return false;
    }

    // --- splines (OCCT B-spline construction + tessellation + translation) ---
    for (const DxfSpline& spline : dxfData.splines()) {
        if (!spline.isValid()) continue;
        if (!addSegments(GeometryUtils::tessellateSpline(spline, tolerance),
                         EntityType::Spline, spline.getId())) return false;
    }

    bool ok = !outData.lines().empty() ||
              !outData.circles().empty();

    if (!ok) {
        outData.setError(DxfImportErrorCode::ConversionFailed,
                         QStringLiteral("no valid entities to convert"));
    }
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
