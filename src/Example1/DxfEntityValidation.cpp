#include "DxfEntityValidation.h"
#include "DxfNumeric.h"

#include <drw_entities.h>

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

namespace DxfEntityValidation {

DxfLine makeLine(const DRW_Line& source)
{
    DxfLine line(
        DxfPoint(source.basePoint.x, source.basePoint.y, source.basePoint.z),
        DxfPoint(source.secPoint.x, source.secPoint.y, source.secPoint.z));
    line.setLayer(source.layer);
    return line;
}

DxfCircle makeCircle(const DRW_Circle& source)
{
    DxfCircle circle(
        DxfPoint(source.basePoint.x, source.basePoint.y, source.basePoint.z),
        source.radious);
    circle.setLayer(source.layer);
    return circle;
}

std::optional<DxfArc> makeArc(const DRW_Arc& source)
{
    const DRW_Coord center = source.basePoint;
    const double radius = source.radious;
    if (!DxfNumeric::areFinite(center.x, center.y, center.z)) {
        return std::nullopt;
    }
    if (!DxfNumeric::isPositiveFinite(radius)) {
        return std::nullopt;
    }
    if (!DxfNumeric::isFinite(source.staangle)
        || !DxfNumeric::isFinite(source.endangle)) {
        return std::nullopt;
    }

    DxfArc arc(DxfPoint(center.x, center.y, center.z), radius,
               source.staangle, source.endangle, source.isccw);
    arc.setLayer(source.layer);
    return arc;
}

std::optional<DxfEllipse> makeEllipse(const DRW_Ellipse& source)
{
    const double majorX = source.secPoint.x;
    const double majorY = source.secPoint.y;
    const double majorLength = std::sqrt(
        majorX * majorX + majorY * majorY);
    if (majorLength <= 0.0) {
        return std::nullopt;
    }
    if (!DxfNumeric::isPositiveFinite(source.ratio)) {
        return std::nullopt;
    }

    DxfEllipse ellipse(
        DxfPoint(source.basePoint.x, source.basePoint.y, source.basePoint.z),
        DxfPoint(source.secPoint.x, source.secPoint.y, source.secPoint.z),
        source.ratio, source.staparam, source.endparam, source.isccw);
    ellipse.setLayer(source.layer);
    return ellipse;
}

std::optional<DxfLWPolyline> makeLWPolyline(const DRW_LWPolyline& source)
{
    const int vertexCount = std::min(
        source.vertexnum, static_cast<int>(source.vertlist.size()));
    if (vertexCount < 2) {
        return std::nullopt;
    }

    const bool isClosed = (source.flags & 1) != 0;
    std::vector<DxfPoint> vertices;
    std::vector<double> bulges;
    vertices.reserve(vertexCount);
    bulges.reserve(isClosed ? vertexCount : vertexCount - 1);

    for (int i = 0; i < vertexCount; ++i) {
        const DRW_Vertex2D& vertex = *source.vertlist[i];
        vertices.push_back(DxfPoint(vertex.x, vertex.y, 0.0));
    }

    const int bulgeCount = isClosed ? vertexCount : vertexCount - 1;
    for (int i = 0; i < bulgeCount; ++i) {
        bulges.push_back(source.vertlist[i]->bulge);
    }

    DxfLWPolyline polyline(vertices, bulges, isClosed, 0.0);
    polyline.setLayer(source.layer);
    return polyline;
}

std::optional<DxfSpline> makeSpline(const DRW_Spline& source)
{
    const bool isRational = (source.flags & 4) != 0;
    const bool hasControlData = source.ncontrol > 0
        && static_cast<int>(source.controllist.size()) >= source.ncontrol
        && source.degree >= 1
        && source.ncontrol > source.degree;
    const bool hasFitData = source.nfit >= 2
        && static_cast<int>(source.fitlist.size()) >= source.nfit;
    if (!hasControlData && !hasFitData) {
        return std::nullopt;
    }

    std::vector<DxfPoint> controlPoints;
    std::vector<double> knots;
    std::vector<double> weights;
    if (hasControlData) {
        const int controlCount = source.ncontrol;
        const int expectedKnotCount = controlCount + source.degree + 1;
        if (static_cast<int>(source.knotslist.size()) != expectedKnotCount) {
            return std::nullopt;
        }

        knots.reserve(expectedKnotCount);
        for (int i = 0; i < expectedKnotCount; ++i) {
            const double knot = source.knotslist[i];
            if (!DxfNumeric::isFinite(knot)
                || (i > 0 && knot < knots.back())) {
                return std::nullopt;
            }
            knots.push_back(knot);
        }

        controlPoints.reserve(controlCount);
        for (int i = 0; i < controlCount; ++i) {
            const auto& point = source.controllist[i];
            if (!point
                || !DxfNumeric::areFinite(point->x, point->y, point->z)) {
                return std::nullopt;
            }
            controlPoints.push_back(DxfPoint(point->x, point->y, point->z));
        }

        if (isRational) {
            if (static_cast<int>(source.weightlist.size()) < controlCount) {
                return std::nullopt;
            }
            weights.reserve(controlCount);
            for (int i = 0; i < controlCount; ++i) {
                const double weight = source.weightlist[i];
                if (!DxfNumeric::isPositiveFinite(weight)) {
                    return std::nullopt;
                }
                weights.push_back(weight);
            }
        }
    }

    std::vector<DxfPoint> fitPoints;
    fitPoints.reserve(source.nfit);
    for (int i = 0; i < source.nfit; ++i) {
        const auto& point = source.fitlist[i];
        if (point
            && DxfNumeric::areFinite(point->x, point->y, point->z)) {
            fitPoints.push_back(DxfPoint(point->x, point->y, point->z));
        }
    }

    DxfSpline spline(
        std::move(controlPoints), std::move(knots), std::move(weights),
        std::move(fitPoints), source.degree, source.flags,
        source.tgStart.x, source.tgStart.y, source.tgStart.z,
        source.tgEnd.x, source.tgEnd.y, source.tgEnd.z);
    spline.setLayer(source.layer);
    return spline;
}

DxfPoint makePoint(const DRW_Point& source)
{
    DxfPoint point(
        source.basePoint.x, source.basePoint.y, source.basePoint.z);
    point.setLayer(source.layer);
    return point;
}

} // namespace DxfEntityValidation
