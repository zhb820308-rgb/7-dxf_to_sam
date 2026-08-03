#include "GeometryUtils.h"
#include "DxfData.h"

#include <Geom_BSplineCurve.hxx>
#include <GeomAdaptor_Curve.hxx>
#include <CPnts_AbscissaPoint.hxx>
#include <GCPnts_TangentialDeflection.hxx>
#include <GeomAPI_Interpolate.hxx>
#include <TColgp_Array1OfPnt.hxx>
#include <TColgp_HArray1OfPnt.hxx>
#include <TColStd_Array1OfReal.hxx>
#include <TColStd_HArray1OfReal.hxx>
#include <TColStd_Array1OfInteger.hxx>
#include <TColgp_Array1OfVec.hxx>
#include <gp_Pnt.hxx>
#include <gp_Vec.hxx>
#include <Precision.hxx>

#include <cmath>
#include <limits>

namespace GeometryUtils {

int calculateArcSegmentCount(double radius, double sweep, double tolerance)
{
    if (!DxfNumeric::isFinite(radius)
        || !DxfNumeric::isFinite(sweep)
        || !DxfNumeric::isFinite(tolerance))
        return 1;
    if (radius <= 0.0 || sweep <= 0.0)
        return 1;
    if (tolerance <= 0.0)
        tolerance = 1e-6;
    if (tolerance >= radius)
        return 2;

    double maxAngle = 2.0 * std::acos(
        std::max(-1.0, std::min(1.0, 1.0 - tolerance / radius)));

    // Cap segment angle at 45 degrees for visual quality
    const double minAngle = M_PI / 4.0;
    if (maxAngle > minAngle)
        maxAngle = minAngle;

    constexpr int maxSegments = 10000;
    if (!DxfNumeric::isPositiveFinite(maxAngle))
        return maxSegments;

    const double requested = std::ceil(sweep / maxAngle);
    if (!DxfNumeric::isFinite(requested) || requested >= maxSegments)
        return maxSegments;
    if (requested <= 2.0)
        return 2;
    return static_cast<int>(requested);
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
    if (tolerance <= 0.0)
        tolerance = DxfImportDefaults::kCurveTolerance;
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
    if (!DxfNumeric::isFinite(sweep))
        return std::numeric_limits<double>::quiet_NaN();

    const double fullTurn = 2.0 * M_PI;
    double normalized = std::fmod(sweep, fullTurn);
    if (normalized == 0.0)
        return isCCW ? fullTurn : -fullTurn;
    if (isCCW && normalized < 0.0)
        normalized += fullTurn;
    else if (!isCCW && normalized > 0.0)
        normalized -= fullTurn;
    return normalized;
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

    if (!arc.isValid() || !DxfNumeric::isFinite(tolerance)
        || !DxfNumeric::isFinite(sweep))
        return result;
    if (tolerance <= 0.0) tolerance = 1.0e-6;

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
    double majorLen = std::hypot(majorX, majorY);

    if (majorLen <= 0.0) return result;

    double ratio = ellipse.ratio();
    double minorX = -majorY * ratio;
    double minorY =  majorX * ratio;

    double startParam = ellipse.startParam();
    double endParam   = ellipse.endParam();
    double sweep = normalizeSweep(endParam - startParam, ellipse.isCCW());

    if (!ellipse.isValid() || !DxfNumeric::isFinite(tolerance)
        || !DxfNumeric::isFinite(sweep))
        return result;
    if (tolerance <= 0.0) tolerance = 1.0e-6;

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

// ========================================================================
//  Spline discretization (OCCT-based)
// ========================================================================

namespace {

Handle(Geom_BSplineCurve) buildCurveFromControlData(const DxfSpline& spline)
{
    const int numCtrl = static_cast<int>(spline.controlPoints().size());
    const int degree  = spline.degree();
    const bool isRational = spline.isRational();
    const std::vector<double>& knotsRaw = spline.knots();

    // Condense unique knots and multiplicities
    std::vector<double> uknots;
    std::vector<int>    umults;
    uknots.reserve(knotsRaw.size());
    umults.reserve(knotsRaw.size());
    for (std::size_t i = 0; i < knotsRaw.size(); ) {
        const double val = knotsRaw[i];
        int mult = 1;
        while (i + mult < knotsRaw.size()
               && std::abs(knotsRaw[i + mult] - val) <= 1.0e-12)
            ++mult;
        uknots.push_back(val);
        umults.push_back(mult);
        i += mult;
    }

    const int numUnique = static_cast<int>(uknots.size());
    if (numUnique < 2)
        return nullptr;

    TColgp_Array1OfPnt        poles(1, numCtrl);
    TColStd_Array1OfReal      wgts(1, numCtrl);
    TColStd_Array1OfReal      knotsArr(1, numUnique);
    TColStd_Array1OfInteger   multsArr(1, numUnique);

    const auto& ctrlPts = spline.controlPoints();
    const auto& weights = spline.weights();
    for (int i = 0; i < numCtrl; ++i) {
        const DxfPoint& pt = ctrlPts[i];
        poles.SetValue(i + 1, gp_Pnt(pt.x(), pt.y(), pt.z()));
        wgts.SetValue(i + 1, isRational ? weights[i] : 1.0);
    }
    for (int i = 0; i < numUnique; ++i) {
        knotsArr.SetValue(i + 1, uknots[i]);
        multsArr.SetValue(i + 1, umults[i]);
    }

    Handle(Geom_BSplineCurve) curve;
    if (isRational) {
        curve = new Geom_BSplineCurve(poles, wgts, knotsArr, multsArr,
                                      degree, Standard_False);
    } else {
        curve = new Geom_BSplineCurve(poles, knotsArr, multsArr,
                                      degree, Standard_False);
    }

    if (spline.isPeriodic() && curve->IsClosed()) {
        try {
            curve->SetPeriodic();
        } catch (const Standard_Failure&) {
            // Periodic conversion failed; keep non-periodic curve
        }
    }

    return curve;
}

Handle(Geom_BSplineCurve) buildCurveFromFitPoints(const DxfSpline& spline)
{
    const int nfit = static_cast<int>(spline.fitPoints().size());
    const auto& fitPts = spline.fitPoints();
    const bool isPeriodic = spline.isPeriodic();

    std::vector<gp_Pnt> uniquePts;
    uniquePts.reserve(nfit);
    const double tolFit = 1.0e-7;

    for (int i = 0; i < nfit; ++i) {
        const DxfPoint& sp = fitPts[i];
        if (!sp.isValid()) continue;
        gp_Pnt pt(sp.x(), sp.y(), sp.z());
        if (!uniquePts.empty()) {
            const gp_Pnt& prev = uniquePts.back();
            if (pt.Distance(prev) <= tolFit) continue;
        }
        uniquePts.push_back(pt);
    }

    const int actualCount = static_cast<int>(uniquePts.size());
    if (actualCount < 2) return nullptr;

    if (isPeriodic && actualCount >= 2) {
        if (uniquePts.back().Distance(uniquePts.front()) <= tolFit)
            uniquePts.pop_back();
    }

    const int finalCount = static_cast<int>(uniquePts.size());
    if (finalCount < 2) return nullptr;

    try {
        Handle(TColgp_HArray1OfPnt) points =
            new TColgp_HArray1OfPnt(1, finalCount);
        for (int i = 0; i < finalCount; ++i)
            points->SetValue(i + 1, uniquePts[i]);

        GeomAPI_Interpolate interpolator(
            points,
            isPeriodic ? Standard_True : Standard_False,
            tolFit);

        if (!isPeriodic) {
            const gp_Vec tgStart(spline.tgStartX(), spline.tgStartY(), spline.tgStartZ());
            const gp_Vec tgEnd(spline.tgEndX(), spline.tgEndY(), spline.tgEndZ());
            if (tgStart.Magnitude() > Precision::Confusion()
                && tgEnd.Magnitude() > Precision::Confusion()) {
                interpolator.Load(tgStart, tgEnd);
            }
        }

        interpolator.Perform();
        if (!interpolator.IsDone()) return nullptr;
        return interpolator.Curve();

    } catch (const Standard_Failure&) {
        return nullptr;
    } catch (const std::exception&) {
        return nullptr;
    }
    return nullptr;
}

bool discretizeByDeflection(
    const Handle(Geom_BSplineCurve)& curve,
    std::vector<DxfPoint>& sampledPoints,
    double linearDeflection)
{
    constexpr double angularDeflection = 0.10;
    constexpr int    minimumPoints = 2;

    try {
        GeomAdaptor_Curve adaptor(curve);
        GCPnts_TangentialDeflection discretizer(
            adaptor,
            curve->FirstParameter(),
            curve->LastParameter(),
            angularDeflection,
            linearDeflection,
            minimumPoints,
            1.0e-9,
            1.0e-7);

        const Standard_Integer nbPoints = discretizer.NbPoints();
        if (nbPoints < 2) return false;

        sampledPoints.reserve(static_cast<std::size_t>(nbPoints));
        for (Standard_Integer i = 1; i <= nbPoints; ++i) {
            const gp_Pnt p = discretizer.Value(i);
            sampledPoints.push_back(DxfPoint(p.X(), p.Y(), p.Z()));
        }
        return true;
    } catch (const Standard_Failure&) {
        return false;
    } catch (const std::exception&) {
        return false;
    }
}

}  // namespace (anonymous)

std::vector<DxfLine> tessellateSpline(const DxfSpline& spline, double tolerance)
{
    // 样条是最复杂的曲线入口：优先用控制点/节点/权重精确构造 OCCT B-spline；若 DXF
    // 只有拟合点，则让 OCCT 通过拟合点建曲线。所有 OCCT 异常都在此边界转为空结果。
    std::vector<DxfLine> result;
    if (!spline.isValid()) return result;

    Handle(Geom_BSplineCurve) curve;

    const bool hasControlData =
        static_cast<int>(spline.controlPoints().size()) > spline.degree()
        && static_cast<int>(spline.knots().size()) >=
           static_cast<int>(spline.controlPoints().size()) + spline.degree() + 1;
    const bool hasFitData = spline.fitPoints().size() >= 2;

    try {
        if (hasControlData) {
            curve = buildCurveFromControlData(spline);
        } else if (hasFitData) {
            curve = buildCurveFromFitPoints(spline);
        } else {
            return result;
        }
    } catch (const Standard_Failure&) {
        return result;
    } catch (const std::exception&) {
        return result;
    }

    if (!curve) return result;

    // Verify curve length
    try {
        GeomAdaptor_Curve adaptor(curve);
        const double totalLength = CPnts_AbscissaPoint::Length(adaptor);
        if (!DxfNumeric::isFinite(totalLength)
            || totalLength <= Precision::Confusion())
            return result;
    } catch (...) {
        return result;
    }

    // 按弦高误差自适应离散：弯曲剧烈处自动取更多点，平直处取更少点；tolerance 是
    // 曲线与折线允许的最大偏差，不是固定线段长度。
    std::vector<DxfPoint> sampledPoints;
    const double deflection = DxfNumeric::isPositiveFinite(tolerance)
        ? tolerance
        : DxfImportDefaults::kCurveTolerance;
    if (!discretizeByDeflection(curve, sampledPoints, deflection))
        return result;

    if (sampledPoints.size() < 2)
        return result;

    // 浮点计算会使理论闭合曲线首尾差一个极小量；足够近时把末点直接吸附到首点，
    // 防止后续 FE 转换产生肉眼不可见的缝隙或额外节点。
    if (sampledPoints.size() >= 2) {
        const DxfPoint& first = sampledPoints.front();
        const DxfPoint& last  = sampledPoints.back();
        const double dx = last.x() - first.x();
        const double dy = last.y() - first.y();
        const double dz = last.z() - first.z();
        const double gap = std::sqrt(dx * dx + dy * dy + dz * dz);

        if (gap <= 1.0e-7) {
            sampledPoints.back() = first;
        }
    }

    // Convert to line segments
    for (std::size_t i = 1; i < sampledPoints.size(); ++i) {
        const DxfPoint& p0 = sampledPoints[i - 1];
        const DxfPoint& p1 = sampledPoints[i];
        const double dx = p1.x() - p0.x();
        const double dy = p1.y() - p0.y();
        const double dz = p1.z() - p0.z();
        if (dx * dx + dy * dy + dz * dz <= 1.0e-20)
            continue;
        result.push_back(DxfLine(p0, p1));
    }

    return result;
}

}  // namespace GeometryUtils
