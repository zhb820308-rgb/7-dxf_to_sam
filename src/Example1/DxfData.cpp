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
    double majorLen = std::sqrt(m_majorAxisEnd.x() * m_majorAxisEnd.x()
                              + m_majorAxisEnd.y() * m_majorAxisEnd.y());
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

void DxfData::addPoint(const DxfPoint& pt)
{
    m_points.push_back(pt);
}

void DxfData::addLine(const DxfLine& line)
{
    m_lines.push_back(line);
    ++m_entityStats.lines;
}

void DxfData::addCircle(const DxfCircle& circle)
{
    m_circles.push_back(circle);
    ++m_entityStats.circles;
}

void DxfData::addArc(const DxfArc& arc)
{
    m_arcs.push_back(arc);
    ++m_entityStats.arcs;
}

void DxfData::addLWPolyline(const DxfLWPolyline& poly)
{
    m_lwPolylines.push_back(poly);
    ++m_entityStats.lwPolylines;
}

void DxfData::addEllipse(const DxfEllipse& ellipse)
{
    m_ellipses.push_back(ellipse);
    ++m_entityStats.ellipses;
}

void DxfData::addSpline(const DxfSpline& spline)
{
    m_splines.push_back(spline);
    ++m_entityStats.splineKinds[spline.kind()];
}

void DxfData::addGeneratedLine(const DxfLine& line)
{
    m_lines.push_back(line);
}

void DxfData::recordGeneratedEntity(EntityType sourceType)
{
    switch (sourceType) {
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
    // Must have at least one data source
    const bool hasCtrl = m_ctrlPts.size() >= static_cast<std::size_t>(m_degree + 1)
                      && m_knots.size() >= static_cast<std::size_t>(m_ctrlPts.size() + m_degree + 1);
    const bool hasFit = m_fitPts.size() >= 2;
    if (!hasCtrl && !hasFit)
        return false;

    if (m_degree < 1)
        return false;

    // Validate tangents are finite
    if (!std::isfinite(m_tgStartX) || !std::isfinite(m_tgStartY) || !std::isfinite(m_tgStartZ))
        return false;
    if (!std::isfinite(m_tgEndX) || !std::isfinite(m_tgEndY) || !std::isfinite(m_tgEndZ))
        return false;

    return true;
}
