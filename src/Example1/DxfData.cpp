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
//  DxfData
// ========================================================================

void DxfData::clear()
{
    m_points.clear();
    m_lines.clear();
    m_circles.clear();
    m_polylineSegments.clear();
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

void DxfData::addPolylineSegment(const DxfPolylineSegment& seg)
{
    m_polylineSegments.push_back(seg);
}
