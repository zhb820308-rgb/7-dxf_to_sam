#ifndef DxfData_h
#define DxfData_h

#include <vector>
#include <QString>

// ---- EntityType ----

enum class EntityType {
    Point,
    Line,
    Circle
};

// ---- DxfEntity (abstract base) ----

class DxfEntity {
public:
    DxfEntity(EntityType entityType);
    virtual ~DxfEntity() = default;

    int getId() const { return id; }
    EntityType getType() const { return type; }

    virtual bool isValid() const = 0;

private:
    int id;
    EntityType type;
};

// ---- DxfPoint ----

class DxfPoint : public DxfEntity {
public:
    DxfPoint();
    DxfPoint(double ix, double iy, double iz);

    double x() const { return m_x; }
    double y() const { return m_y; }
    double z() const { return m_z; }

    void setX(double v) { m_x = v; }
    void setY(double v) { m_y = v; }
    void setZ(double v) { m_z = v; }

    bool isValid() const override;

private:
    double m_x = 0.0;
    double m_y = 0.0;
    double m_z = 0.0;
};

// ---- DxfLine ----

class DxfLine : public DxfEntity {
public:
    DxfLine();
    DxfLine(const DxfPoint& start, const DxfPoint& end);

    const DxfPoint& start() const { return m_start; }
    const DxfPoint& end()   const { return m_end; }

    bool isValid() const override;

private:
    DxfPoint m_start;
    DxfPoint m_end;
};

// ---- DxfCircle ----

class DxfCircle : public DxfEntity {
public:
    DxfCircle();
    DxfCircle(const DxfPoint& center, double radius);

    const DxfPoint& center() const { return m_center; }
    double radius()          const { return m_radius; }

    bool isValid() const override;

private:
    DxfPoint m_center;
    double m_radius = 0.0;
};

// ---- DxfPolylineSegment ----
// 多段线的单段：start→end，bulge=0 为直线段，非0 为圆弧段
// 这是一个解析中间产物，不继承 DxfEntity

struct DxfPolylineSegment {
    DxfPoint start;
    DxfPoint end;
    double bulge = 0.0;

    DxfPolylineSegment() = default;
    DxfPolylineSegment(const DxfPoint& s, const DxfPoint& e, double b)
        : start(s), end(e), bulge(b) {}
};

// ---- DxfData (container) ----

class DxfData {
public:
    DxfData() = default;

    // --- modifiers ---
    void addPoint(const DxfPoint& pt);
    void addLine(const DxfLine& line);
    void addCircle(const DxfCircle& circle);
    void addPolylineSegment(const DxfPolylineSegment& seg);
    void clear();

    // --- accessors ---
    const std::vector<DxfPoint>&             points()            const { return m_points; }
    const std::vector<DxfLine>&              lines()             const { return m_lines; }
    const std::vector<DxfCircle>&            circles()           const { return m_circles; }
    const std::vector<DxfPolylineSegment>&   polylineSegments()  const { return m_polylineSegments; }

    int entityCount() const {
        return static_cast<int>(m_lines.size() + m_circles.size() + m_polylineSegments.size());
    }

    // --- error / validity ---
    QString errorMessage() const { return m_errorMessage; }
    void setErrorMessage(const QString& msg) { m_errorMessage = msg; }

    bool isValid() const { return m_isValid; }
    void setValid(bool v) { m_isValid = v; }

private:
    std::vector<DxfPoint>             m_points;
    std::vector<DxfLine>              m_lines;
    std::vector<DxfCircle>            m_circles;
    std::vector<DxfPolylineSegment>   m_polylineSegments;
    QString m_errorMessage;
    bool m_isValid = false;
};

#endif // DxfData_h
