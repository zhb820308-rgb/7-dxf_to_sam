#ifndef SamData_h
#define SamData_h

#include "DxfData.h"
#include <vector>

// Output of ConversionEngine -- ready to pass to skcGeomFactory.
// Coordinates are already offset by the base point.
class SamData {
public:
    SamData() = default;

    // --- read-only access ---
    const std::vector<DxfPoint>&  points()  const { return m_points; }
    const std::vector<DxfLine>&   lines()   const { return m_lines; }
    const std::vector<DxfCircle>& circles() const { return m_circles; }

    // --- modifiers ---
    void addPoint(const DxfPoint& pt)   { m_points.push_back(pt); }
    void addLine(const DxfLine& line)   { m_lines.push_back(line); }
    void addCircle(const DxfCircle& c)  { m_circles.push_back(c); }

    void clear() {
        m_points.clear();
        m_lines.clear();
        m_circles.clear();
    }

private:
    std::vector<DxfPoint>  m_points;
    std::vector<DxfLine>   m_lines;
    std::vector<DxfCircle> m_circles;
};

#endif
