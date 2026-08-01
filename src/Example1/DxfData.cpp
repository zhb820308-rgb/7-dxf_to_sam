#include "DxfData.h"
#include <atomic>
#include <cmath>
#include <limits>

// ========================================================================
//  DxfEntity
// ========================================================================

static std::atomic<int> s_nextId{1};

bool SplineKind::operator<(const SplineKind& other) const
{
    if (construction != other.construction)
        return construction < other.construction;
    if (rational != other.rational)
        return rational < other.rational;
    if (periodic != other.periodic)
        return periodic < other.periodic;
    return closed < other.closed;
}

std::size_t DxfEntityStats::splineCount() const
{
    std::size_t count = 0;
    for (const auto& entry : splineKinds)
        count += entry.second;
    return count;
}

std::size_t DxfEntityStats::curveCount() const
{
    return circles + arcs + ellipses + splineCount();
}

DxfEntity::DxfEntity(EntityType entityType)
    : id(s_nextId++)
    , type(entityType)
{
}

// ========================================================================
//  DxfPoint
// ========================================================================

DxfPoint::DxfPoint()
    : DxfEntity(EntityType::Point)
    , m_x(0.0), m_y(0.0), m_z(0.0)
{
}

DxfPoint::DxfPoint(double ix, double iy, double iz)
    : DxfEntity(EntityType::Point)
    , m_x(ix), m_y(iy), m_z(iz)
{
}

bool DxfPoint::isValid() const
{
    return std::isfinite(m_x) && std::isfinite(m_y) && std::isfinite(m_z);
}

// ========================================================================
//  DxfLine
// ========================================================================

DxfLine::DxfLine()
    : DxfEntity(EntityType::Line)
{
}

DxfLine::DxfLine(const DxfPoint& start, const DxfPoint& end)
    : DxfEntity(EntityType::Line)
    , m_start(start)
    , m_end(end)
{
}

bool DxfLine::isValid() const
{
    if (!m_start.isValid() || !m_end.isValid())
        return false;
    // reject zero-length line
    return m_start.x() != m_end.x()
        || m_start.y() != m_end.y()
        || m_start.z() != m_end.z();
}

// ========================================================================
//  DxfCircle
// ========================================================================

DxfCircle::DxfCircle()
    : DxfEntity(EntityType::Circle)
{
}

DxfCircle::DxfCircle(const DxfPoint& center, double radius)
    : DxfEntity(EntityType::Circle)
    , m_center(center)
    , m_radius(radius)
{
}

bool DxfCircle::isValid() const
{
    return m_center.isValid() && std::isfinite(m_radius) && m_radius > 0.0;
}

// ========================================================================
//  DxfArc
// ========================================================================

DxfArc::DxfArc()
    : DxfEntity(EntityType::Arc)
{
}

DxfArc::DxfArc(const DxfPoint& center, double radius,
               double startAngle, double endAngle, bool isCCW)
    : DxfEntity(EntityType::Arc)
    , m_center(center)
    , m_radius(radius)
    , m_startAngle(startAngle)
    , m_endAngle(endAngle)
    , m_isCCW(isCCW)
{
}

bool DxfArc::isValid() const
{
    return m_center.isValid()
        && std::isfinite(m_radius) && m_radius > 0.0
        && std::isfinite(m_startAngle)
        && std::isfinite(m_endAngle)
        && (m_startAngle != m_endAngle);
}

// ========================================================================
//  DxfEllipse
// ========================================================================

DxfEllipse::DxfEllipse()
    : DxfEntity(EntityType::Ellipse)
{
}

DxfEllipse::DxfEllipse(const DxfPoint& center, const DxfPoint& majorAxisEnd,
                       double ratio, double startParam, double endParam, bool isCCW)
    : DxfEntity(EntityType::Ellipse)
    , m_center(center)
    , m_majorAxisEnd(majorAxisEnd)
    , m_ratio(ratio)
    , m_startParam(startParam)
    , m_endParam(endParam)
    , m_isCCW(isCCW)
{
}

bool DxfEllipse::isValid() const
{
    const double majorLen = std::hypot(
        m_majorAxisEnd.x(), m_majorAxisEnd.y());
    return m_center.isValid()
        && m_majorAxisEnd.isValid()
        && std::isfinite(majorLen) && majorLen > 0.0
        && std::isfinite(m_ratio) && m_ratio > 0.0
        && std::isfinite(m_startParam)
        && std::isfinite(m_endParam)
        && (m_startParam != m_endParam);
}

// ========================================================================
//  DxfLWPolyline
// ========================================================================

DxfLWPolyline::DxfLWPolyline()
    : DxfEntity(EntityType::LWPolyline)
{
}

DxfLWPolyline::DxfLWPolyline(const std::vector<DxfPoint>& vertices,
                             const std::vector<double>& bulges,
                             bool closed, double constZ)
    : DxfEntity(EntityType::LWPolyline)
    , m_vertices(vertices)
    , m_bulges(bulges)
    , m_closed(closed)
    , m_constZ(constZ)
{
}

bool DxfLWPolyline::isValid() const
{
    if (m_vertices.size() < 2) return false;
    for (const DxfPoint& v : m_vertices) {
        if (!v.isValid()) return false;
    }
    // bulges size should match vertex count (last bulge for closing segment if closed)
    if (m_closed) {
        if (static_cast<int>(m_bulges.size()) != static_cast<int>(m_vertices.size()))
            return false;
    } else {
        if (static_cast<int>(m_bulges.size()) != static_cast<int>(m_vertices.size()) - 1)
            return false;
    }
    for (double b : m_bulges) {
        if (!std::isfinite(b)) return false;
    }
    return std::isfinite(m_constZ);
}

// ========================================================================
//  DxfData
// ========================================================================

void DxfData::clear()
{
    m_points.clear();
    m_lines.clear();
    m_circles.clear();
    m_arcs.clear();
    m_lwPolylines.clear();
    m_ellipses.clear();
    m_splines.clear();
    m_inserts.clear();
    m_entityStats = DxfEntityStats();
    m_errorMessage.clear();
    m_isValid = false;
}

void DxfData::reserveLines(std::size_t count)
{
    if (m_lines.capacity() < count)
        m_lines.reserve(count);
}

void DxfData::reservePoints(std::size_t count)
{
    if (m_points.capacity() < count)
        m_points.reserve(count);
}

void DxfData::reserveLWPolylines(std::size_t count)
{
    if (m_lwPolylines.capacity() < count)
        m_lwPolylines.reserve(count);
}

void DxfData::reserveSplines(std::size_t count)
{
    if (m_splines.capacity() < count)
        m_splines.reserve(count);
}

void DxfData::addPoint(const DxfPoint& pt)
{
    ++m_entityStats.sourceEntities;
    if (!pt.isValid()) {
        recordRejected("invalid point");
        return;
    }
    m_points.push_back(pt);
    recordAccepted();
}

void DxfData::addLine(const DxfLine& line)
{
    ++m_entityStats.sourceEntities;
    if (!line.isValid()) {
        recordRejected("invalid line");
        return;
    }
    m_lines.push_back(line);
    ++m_entityStats.lines;
    recordAccepted();
}

void DxfData::addCircle(const DxfCircle& circle)
{
    ++m_entityStats.sourceEntities;
    if (!circle.isValid()) {
        recordRejected("invalid circle");
        return;
    }
    m_circles.push_back(circle);
    ++m_entityStats.circles;
    recordAccepted();
}

void DxfData::addArc(const DxfArc& arc)
{
    ++m_entityStats.sourceEntities;
    if (!arc.isValid()) {
        recordRejected("invalid arc");
        return;
    }
    m_arcs.push_back(arc);
    ++m_entityStats.arcs;
    recordAccepted();
}

void DxfData::addLWPolyline(const DxfLWPolyline& poly)
{
    ++m_entityStats.sourceEntities;
    if (!poly.isValid()) {
        recordRejected("invalid lwpolyline");
        return;
    }
    m_lwPolylines.push_back(poly);
    ++m_entityStats.lwPolylines;
    recordAccepted();
}

void DxfData::addEllipse(const DxfEllipse& ellipse)
{
    ++m_entityStats.sourceEntities;
    if (!ellipse.isValid()) {
        recordRejected("invalid ellipse");
        return;
    }
    m_ellipses.push_back(ellipse);
    ++m_entityStats.ellipses;
    recordAccepted();
}

void DxfData::addSpline(const DxfSpline& spline)
{
    ++m_entityStats.sourceEntities;
    if (!spline.isValid()) {
        recordRejected("invalid spline");
        return;
    }
    m_splines.push_back(spline);
    ++m_entityStats.splineKinds[spline.kind()];
    recordAccepted();
}

void DxfData::addSpline(DxfSpline&& spline)
{
    const SplineKind kind = spline.kind();
    ++m_entityStats.sourceEntities;
    if (!spline.isValid()) {
        recordRejected("invalid spline");
        return;
    }
    m_splines.push_back(std::move(spline));
    ++m_entityStats.splineKinds[kind];
    recordAccepted();
}

void DxfData::addGeneratedPoint(const DxfPoint& point)
{
    if (!point.isValid()) {
        recordRejected("invalid generated point");
        return;
    }
    m_points.push_back(point);
    ++m_entityStats.generatedEntities;
}

void DxfData::addGeneratedLine(const DxfLine& line)
{
    if (!line.isValid()) {
        recordRejected("invalid generated line");
        return;
    }
    m_lines.push_back(line);
    ++m_entityStats.generatedEntities;
}

void DxfData::addGeneratedCircle(const DxfCircle& circle)
{
    if (!circle.isValid()) {
        recordRejected("invalid generated circle");
        return;
    }
    m_circles.push_back(circle);
    ++m_entityStats.generatedEntities;
}

void DxfData::addGeneratedArc(const DxfArc& arc)
{
    if (!arc.isValid()) {
        recordRejected("invalid generated arc");
        return;
    }
    m_arcs.push_back(arc);
    ++m_entityStats.generatedEntities;
}

void DxfData::addGeneratedLWPolyline(const DxfLWPolyline& poly)
{
    if (!poly.isValid()) {
        recordRejected("invalid generated lwpolyline");
        return;
    }
    m_lwPolylines.push_back(poly);
    ++m_entityStats.generatedEntities;
}

void DxfData::addGeneratedEllipse(const DxfEllipse& ellipse)
{
    if (!ellipse.isValid()) {
        recordRejected("invalid generated ellipse");
        return;
    }
    m_ellipses.push_back(ellipse);
    ++m_entityStats.generatedEntities;
}

void DxfData::addGeneratedSpline(DxfSpline&& spline)
{
    if (!spline.isValid()) {
        recordRejected("invalid generated spline");
        return;
    }
    m_splines.push_back(std::move(spline));
    ++m_entityStats.generatedEntities;
}

void DxfData::recordRejected(const char* reason)
{
    ++m_entityStats.rejectedEntities;
    ++m_entityStats.rejectionReasons[reason];
}

void DxfData::recordAccepted()
{
    ++m_entityStats.acceptedEntities;
}

void DxfData::recordGeneratedEntity(EntityType sourceType)
{
    ++m_entityStats.sourceEntities;
    switch (sourceType) {
    case EntityType::Line:
        ++m_entityStats.lines;
        break;
    case EntityType::Circle:
        ++m_entityStats.circles;
        break;
    case EntityType::Arc:
        ++m_entityStats.arcs;
        break;
    case EntityType::LWPolyline:
        ++m_entityStats.lwPolylines;
        break;
    case EntityType::Ellipse:
        ++m_entityStats.ellipses;
        break;
    default:
        break;
    }
}

void DxfData::recordGeneratedSpline(const SplineKind& kind)
{
    ++m_entityStats.sourceEntities;
    ++m_entityStats.splineKinds[kind];
}

// ========================================================================
//  DxfSpline
// ========================================================================

DxfSpline::DxfSpline()
    : DxfEntity(EntityType::Spline)
{
}

DxfSpline::DxfSpline(const std::vector<DxfPoint>& ctrlPts,
                     const std::vector<double>& knots,
                     const std::vector<double>& weights,
                     const std::vector<DxfPoint>& fitPts,
                     int degree, int flags,
                     double tgStartX, double tgStartY, double tgStartZ,
                     double tgEndX, double tgEndY, double tgEndZ)
    : DxfEntity(EntityType::Spline)
    , m_ctrlPts(ctrlPts)
    , m_knots(knots)
    , m_weights(weights)
    , m_fitPts(fitPts)
    , m_degree(degree)
    , m_flags(flags)
    , m_tgStartX(tgStartX), m_tgStartY(tgStartY), m_tgStartZ(tgStartZ)
    , m_tgEndX(tgEndX), m_tgEndY(tgEndY), m_tgEndZ(tgEndZ)
{
}

DxfSpline::DxfSpline(std::vector<DxfPoint>&& ctrlPts,
                     std::vector<double>&& knots,
                     std::vector<double>&& weights,
                     std::vector<DxfPoint>&& fitPts,
                     int degree, int flags,
                     double tgStartX, double tgStartY, double tgStartZ,
                     double tgEndX, double tgEndY, double tgEndZ)
    : DxfEntity(EntityType::Spline)
    , m_ctrlPts(std::move(ctrlPts))
    , m_knots(std::move(knots))
    , m_weights(std::move(weights))
    , m_fitPts(std::move(fitPts))
    , m_degree(degree)
    , m_flags(flags)
    , m_tgStartX(tgStartX), m_tgStartY(tgStartY), m_tgStartZ(tgStartZ)
    , m_tgEndX(tgEndX), m_tgEndY(tgEndY), m_tgEndZ(tgEndZ)
{
}

SplineKind DxfSpline::kind() const
{
    const bool hasControlData =
        static_cast<int>(m_ctrlPts.size()) > m_degree
        && static_cast<int>(m_knots.size()) >=
           static_cast<int>(m_ctrlPts.size()) + m_degree + 1;

    SplineKind result;
    result.construction = hasControlData
        ? SplineConstruction::ControlBased
        : SplineConstruction::FitBased;
    result.rational = isRational();
    result.periodic = isPeriodic();
    result.closed = isClosed();
    return result;
}

bool DxfSpline::isValid() const
{
    constexpr int maxOcctDegree = 25;
    if (m_degree < 1 || m_degree > maxOcctDegree)
        return false;

    for (const DxfPoint& point : m_ctrlPts)
        if (!point.isValid()) return false;
    for (const DxfPoint& point : m_fitPts)
        if (!point.isValid()) return false;
    for (std::size_t i = 0; i < m_knots.size(); ++i) {
        if (!std::isfinite(m_knots[i])) return false;
        if (i > 0 && m_knots[i] < m_knots[i - 1]) return false;
    }
    for (double weight : m_weights)
        if (!std::isfinite(weight) || weight <= 0.0) return false;

    const bool hasCtrl = m_ctrlPts.size() > static_cast<std::size_t>(m_degree);
    const bool hasFit = m_fitPts.size() >= 2;
    if (!hasCtrl && !hasFit) return false;

    if (hasCtrl) {
        constexpr double knotTolerance = 1.0e-12;
        if (m_knots.size() != m_ctrlPts.size()
            + static_cast<std::size_t>(m_degree) + 1)
            return false;
        if (std::abs(m_knots.front() - m_knots.back()) <= knotTolerance)
            return false;
        for (std::size_t begin = 0; begin < m_knots.size(); ) {
            std::size_t end = begin + 1;
            while (end < m_knots.size()
                   && std::abs(m_knots[end] - m_knots[begin]) <= knotTolerance)
                ++end;
            const std::size_t multiplicity = end - begin;
            const bool boundary = begin == 0 || end == m_knots.size();
            const std::size_t maximum = static_cast<std::size_t>(
                boundary ? m_degree + 1 : m_degree);
            if (multiplicity > maximum) return false;
            begin = end;
        }
        if (isRational()) {
            if (m_weights.size() != m_ctrlPts.size()) return false;
        }
    } else if (isRational()) {
        return false;
    }

    // Validate tangents are finite
    if (!std::isfinite(m_tgStartX) || !std::isfinite(m_tgStartY) || !std::isfinite(m_tgStartZ))
        return false;
    if (!std::isfinite(m_tgEndX) || !std::isfinite(m_tgEndY) || !std::isfinite(m_tgEndZ))
        return false;

    return true;
}
