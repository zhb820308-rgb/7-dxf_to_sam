#include "DxfData.h"
#include <cmath>
#include <limits>

// ========================================================================
//  DxfEntity
// ========================================================================

static int s_nextId = 1;

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
}

void DxfData::addCircle(const DxfCircle& circle)
{
    m_circles.push_back(circle);
}

void DxfData::addArc(const DxfArc& arc)
{
    m_arcs.push_back(arc);
}

void DxfData::addLWPolyline(const DxfLWPolyline& poly)
{
    m_lwPolylines.push_back(poly);
}

void DxfData::addEllipse(const DxfEllipse& ellipse)
{
    m_ellipses.push_back(ellipse);
}
