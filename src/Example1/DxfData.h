#ifndef DxfData_h
#define DxfData_h

#include <vector>
#include <QString>

// ---- EntityType ----

enum class EntityType {
    Point,
    Line,
    Circle,
    Arc,
    LWPolyline,
    Ellipse,
    Spline,
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

// ---- DxfArc ----
// 圆弧：圆心、半径、起止角度（弧度）、方向

class DxfArc : public DxfEntity {
public:
    DxfArc();
    DxfArc(const DxfPoint& center, double radius,
           double startAngle, double endAngle, bool isCCW);

    const DxfPoint& center()      const { return m_center; }
    double radius()               const { return m_radius; }
    double startAngle()           const { return m_startAngle; }
    double endAngle()             const { return m_endAngle; }
    bool   isCCW()                const { return m_isCCW; }

    bool isValid() const override;

private:
    DxfPoint m_center;
    double m_radius      = 0.0;
    double m_startAngle  = 0.0;
    double m_endAngle    = 0.0;
    bool   m_isCCW       = true;
};

// ---- DxfEllipse ----
// 椭圆弧：中心、长轴端点（相对中心）、短轴/长轴比例、参数范围、方向

class DxfEllipse : public DxfEntity {
public:
    DxfEllipse();
    DxfEllipse(const DxfPoint& center, const DxfPoint& majorAxisEnd,
               double ratio, double startParam, double endParam, bool isCCW);

    const DxfPoint& center()       const { return m_center; }
    const DxfPoint& majorAxisEnd() const { return m_majorAxisEnd; }
    double ratio()                 const { return m_ratio; }
    double startParam()            const { return m_startParam; }
    double endParam()              const { return m_endParam; }
    bool   isCCW()                 const { return m_isCCW; }

    bool isValid() const override;

private:
    DxfPoint m_center;
    DxfPoint m_majorAxisEnd;  // 长轴端点（相对于中心）
    double m_ratio       = 1.0;
    double m_startParam  = 0.0;
    double m_endParam    = 0.0;
    bool   m_isCCW       = true;
};

// ---- DxfLWPolyline ----
// 轻量多段线：顶点列表 + 每段的 bulge 值，保留原始几何信息

class DxfLWPolyline : public DxfEntity {
public:
    DxfLWPolyline();
    DxfLWPolyline(const std::vector<DxfPoint>& vertices,
                  const std::vector<double>& bulges,
                  bool closed, double constZ = 0.0);

    const std::vector<DxfPoint>& vertices() const { return m_vertices; }
    const std::vector<double>&   bulges()   const { return m_bulges; }
    bool   isClosed() const { return m_closed; }
    double constZ()   const { return m_constZ; }
    int    vertexCount() const { return static_cast<int>(m_vertices.size()); }

    bool isValid() const override;

private:
    std::vector<DxfPoint> m_vertices;
    std::vector<double>   m_bulges;    // bulge[i] 描述顶点i到顶点i+1的弧段
    bool   m_closed = false;
    double m_constZ = 0.0;
};

// ---- DxfSpline ----
// B-spline curve: control points, knots, weights, fit points.
// flags bit 0=closed, bit 1=periodic, bit 2=rational.

class DxfSpline : public DxfEntity {
public:
    DxfSpline();
    DxfSpline(const std::vector<DxfPoint>& ctrlPts,
              const std::vector<double>& knots,
              const std::vector<double>& weights,
              const std::vector<DxfPoint>& fitPts,
              int degree, int flags,
              double tgStartX, double tgStartY, double tgStartZ,
              double tgEndX, double tgEndY, double tgEndZ);

    const std::vector<DxfPoint>& controlPoints() const { return m_ctrlPts; }
    const std::vector<double>&   knots()          const { return m_knots; }
    const std::vector<double>&   weights()        const { return m_weights; }
    const std::vector<DxfPoint>& fitPoints()       const { return m_fitPts; }
    int    degree()      const { return m_degree; }
    int    flags()        const { return m_flags; }
    bool   isRational()  const { return (m_flags & 4) != 0; }
    bool   isPeriodic()  const { return (m_flags & 2) != 0; }
    bool   isClosed()    const { return (m_flags & 1) != 0; }
    double tgStartX() const { return m_tgStartX; }
    double tgStartY() const { return m_tgStartY; }
    double tgStartZ() const { return m_tgStartZ; }
    double tgEndX()   const { return m_tgEndX; }
    double tgEndY()   const { return m_tgEndY; }
    double tgEndZ()   const { return m_tgEndZ; }

    bool isValid() const override;

private:
    std::vector<DxfPoint> m_ctrlPts;
    std::vector<double>   m_knots;
    std::vector<double>   m_weights;
    std::vector<DxfPoint> m_fitPts;
    int    m_degree    = 0;
    int    m_flags     = 0;
    double m_tgStartX  = 0.0, m_tgStartY  = 0.0, m_tgStartZ  = 0.0;
    double m_tgEndX    = 0.0, m_tgEndY    = 0.0, m_tgEndZ    = 0.0;
};

// ---- DxfData (container) ----

class DxfData {
public:
    DxfData() = default;

    // --- modifiers ---
    void addPoint(const DxfPoint& pt);
    void addLine(const DxfLine& line);
    void addCircle(const DxfCircle& circle);
    void addArc(const DxfArc& arc);
    void addLWPolyline(const DxfLWPolyline& poly);
    void addEllipse(const DxfEllipse& ellipse);
    void addSpline(const DxfSpline& spline);
    void clear();

    // --- accessors ---
    const std::vector<DxfPoint>&       points()       const { return m_points; }
    const std::vector<DxfLine>&        lines()        const { return m_lines; }
    const std::vector<DxfCircle>&      circles()      const { return m_circles; }
    const std::vector<DxfArc>&         arcs()         const { return m_arcs; }
    const std::vector<DxfLWPolyline>&  lwPolylines()  const { return m_lwPolylines; }
    const std::vector<DxfEllipse>&     ellipses()     const { return m_ellipses; }
    const std::vector<DxfSpline>&      splines()      const { return m_splines; }

    int entityCount() const {
        return static_cast<int>(
            m_lines.size() + m_circles.size() +
            m_arcs.size() + m_lwPolylines.size() + m_ellipses.size() +
            m_splines.size());
    }

    // --- error / validity ---
    QString errorMessage() const { return m_errorMessage; }
    void setErrorMessage(const QString& msg) { m_errorMessage = msg; }

    bool isValid() const { return m_isValid; }
    void setValid(bool v) { m_isValid = v; }

private:
    std::vector<DxfPoint>       m_points;
    std::vector<DxfLine>        m_lines;
    std::vector<DxfCircle>      m_circles;
    std::vector<DxfArc>         m_arcs;
    std::vector<DxfLWPolyline>  m_lwPolylines;
    std::vector<DxfEllipse>     m_ellipses;
    std::vector<DxfSpline>      m_splines;
    QString m_errorMessage;
    bool m_isValid = false;
};

#endif // DxfData_h
