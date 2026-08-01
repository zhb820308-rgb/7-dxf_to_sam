#ifndef DxfData_h
#define DxfData_h

#include <map>
#include <string>
#include <utility>
#include <vector>
#include <QString>
#include "DxfImportError.h"

// ======== InsertInfo — shared by DxfBlock and DxfData ========

struct InsertInfo {
    std::string blockName;
    std::string layer = "0";
    double insertX = 0, insertY = 0, insertZ = 0;
    double scaleX  = 1, scaleY  = 1, scaleZ  = 1;
    double angle   = 0;  // radians
    int    colCount = 1, rowCount = 1;
    double colSpace = 0, rowSpace = 0;
};

// ======== EntityType ========

enum class EntityType {
    Point,
    Line,
    Circle,
    Arc,
    LWPolyline,
    Ellipse,
    Spline,
};

enum class SplineConstruction {
    ControlBased,
    FitBased,
};

struct SplineKind {
    SplineConstruction construction = SplineConstruction::ControlBased;
    bool rational = false;
    bool periodic = false;
    bool closed = false;

    bool operator<(const SplineKind& other) const;
};

struct DxfEntityStats {
    std::size_t sourceEntities = 0;
    std::size_t acceptedEntities = 0;
    std::size_t rejectedEntities = 0;
    std::size_t generatedEntities = 0;
    std::map<std::string, std::size_t> rejectionReasons;
    std::size_t lines = 0;
    std::size_t lwPolylines = 0;
    std::size_t circles = 0;
    std::size_t arcs = 0;
    std::size_t ellipses = 0;
    std::map<SplineKind, std::size_t> splineKinds;

    std::size_t splineCount() const;
    std::size_t curveCount() const;
};

// ======== DxfEntity (abstract base) ========

class DxfEntity {
public:
    DxfEntity(EntityType entityType);
    virtual ~DxfEntity() = default;

    int getId() const { return id; }
    EntityType getType() const { return type; }
    const std::string& layer() const { return m_layer; }
    void setLayer(const std::string& layerName) { m_layer = layerName; }

    virtual bool isValid() const = 0;

private:
    int id;
    EntityType type;
    std::string m_layer = "0";
};

// ======== DxfPoint ========

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

// ======== DxfLine ========

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

// ======== DxfCircle ========

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

// ======== DxfArc ========
/// Circular arc defined by center, radius, start/end angles (radians), and direction.

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

// ======== DxfEllipse ========
/// Elliptical arc defined by center, major axis endpoint (relative to center),
/// axis ratio, parameter range, and direction.

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
    DxfPoint m_majorAxisEnd;  ///< Major axis endpoint relative to center.
    double m_ratio       = 1.0;
    double m_startParam  = 0.0;
    double m_endParam    = 0.0;
    bool   m_isCCW       = true;
};

// ======== DxfLWPolyline ========
/// Lightweight polyline defined by vertex list and per-segment bulge values,
/// preserving raw geometry for tessellation.

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
    std::vector<double>   m_bulges;    ///< bulge[i] describes arc segment from vertex i to vertex i+1
    bool   m_closed = false;
    double m_constZ = 0.0;
};

// ======== DxfSpline ========
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
    /// Move-assemble constructor: transfers ownership of the input containers,
    /// avoiding one layer of deep copy in the hot SPLINE parse path.
    DxfSpline(std::vector<DxfPoint>&& ctrlPts,
              std::vector<double>&& knots,
              std::vector<double>&& weights,
              std::vector<DxfPoint>&& fitPts,
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
    SplineKind kind() const;

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

// ======== DxfData (container) ========

class DxfData {
public:
    DxfData() = default;

    // ======== modifiers ========
    void addPoint(const DxfPoint& pt);
    void addLine(const DxfLine& line);
    void addCircle(const DxfCircle& circle);
    void addArc(const DxfArc& arc);
    void addLWPolyline(const DxfLWPolyline& poly);
    void addEllipse(const DxfEllipse& ellipse);
    void addSpline(const DxfSpline& spline);
    void addSpline(DxfSpline&& spline);
    void addGeneratedPoint(const DxfPoint& point);
    void addGeneratedLine(const DxfLine& line);
    void addGeneratedCircle(const DxfCircle& circle);
    void addGeneratedArc(const DxfArc& arc);
    void addGeneratedLWPolyline(const DxfLWPolyline& poly);
    void addGeneratedEllipse(const DxfEllipse& ellipse);
    void addGeneratedSpline(DxfSpline&& spline);
    void recordGeneratedEntity(EntityType sourceType);
    void recordGeneratedSpline(const SplineKind& kind);
    void addInsert(const InsertInfo& ins) { m_inserts.push_back(ins); }
    void reserveLines(std::size_t count);
    void reservePoints(std::size_t count);
    void reserveLWPolylines(std::size_t count);
    void reserveSplines(std::size_t count);
    void clear();

    // ======== accessors ========
    const std::vector<DxfPoint>&       points()       const { return m_points; }
    const std::vector<DxfLine>&        lines()        const { return m_lines; }
    const std::vector<DxfCircle>&      circles()      const { return m_circles; }
    const std::vector<DxfArc>&         arcs()         const { return m_arcs; }
    const std::vector<DxfLWPolyline>&  lwPolylines()  const { return m_lwPolylines; }
    const std::vector<DxfEllipse>&     ellipses()     const { return m_ellipses; }
    const std::vector<DxfSpline>&      splines()      const { return m_splines; }
    const std::vector<InsertInfo>&     inserts()      const { return m_inserts; }
    const DxfEntityStats&             entityStats()  const { return m_entityStats; }

    /// Number of all supported DXF entities, including standalone POINTs.
    std::size_t entityCount() const {
        return m_points.size() + m_lines.size() + m_circles.size() +
               m_arcs.size() + m_lwPolylines.size() + m_ellipses.size() +
               m_splines.size();
    }

    /// Number of entities supported by the Sketch conversion path.
    int sketchEntityCount() const {
        return static_cast<int>(
            m_lines.size() + m_circles.size() +
            m_arcs.size() + m_lwPolylines.size() + m_ellipses.size() +
            m_splines.size());
    }

    /// Number of entities supported by the finite-element conversion path.
    int feEntityCount() const { return entityCount(); }

    // ======== error / validity ========
    QString errorMessage() const { return m_errorMessage; }
    void setErrorMessage(const QString& msg) { m_errorMessage = msg; }
    DxfImportErrorCode errorCode() const { return m_errorCode; }
    void setError(DxfImportErrorCode code, const QString& msg) {
        m_errorCode = code;
        m_errorMessage = msg;
    }

    bool isValid() const { return m_isValid; }
    void setValid(bool v) { m_isValid = v; }

private:
    void recordRejected(const char* reason);
    void recordAccepted();

    std::vector<DxfPoint>       m_points;
    std::vector<DxfLine>        m_lines;
    std::vector<DxfCircle>      m_circles;
    std::vector<DxfArc>         m_arcs;
    std::vector<DxfLWPolyline>  m_lwPolylines;
    std::vector<DxfEllipse>     m_ellipses;
    std::vector<DxfSpline>      m_splines;
    std::vector<InsertInfo>     m_inserts;
    DxfEntityStats m_entityStats;
    QString m_errorMessage;
    DxfImportErrorCode m_errorCode = DxfImportErrorCode::None;
    bool m_isValid = false;
};

// ======== DxfBlock — named block definition (parsed, not yet expanded) ========

class DxfBlock {
public:
    DxfBlock() = default;

    const std::string& name()  const { return m_name; }
    double baseX() const { return m_baseX; }
    double baseY() const { return m_baseY; }
    double baseZ() const { return m_baseZ; }

    void setName(const std::string& n)    { m_name = n; }
    void setBase(double x, double y, double z) { m_baseX = x; m_baseY = y; m_baseZ = z; }

    const std::vector<DxfPoint>&      points()      const { return m_points; }
    const std::vector<DxfLine>&       lines()       const { return m_lines; }
    const std::vector<DxfCircle>&     circles()     const { return m_circles; }
    const std::vector<DxfArc>&        arcs()        const { return m_arcs; }
    const std::vector<DxfLWPolyline>& lwPolylines() const { return m_lwPolylines; }
    const std::vector<DxfEllipse>&    ellipses()    const { return m_ellipses; }
    const std::vector<DxfSpline>&     splines()     const { return m_splines; }

    void addPoint(const DxfPoint& pt)           { if (pt.isValid()) m_points.push_back(pt); }
    void addLine(const DxfLine& line)           { if (line.isValid()) m_lines.push_back(line); }
    void addCircle(const DxfCircle& circle)     { if (circle.isValid()) m_circles.push_back(circle); }
    void addArc(const DxfArc& arc)              { if (arc.isValid()) m_arcs.push_back(arc); }
    void addLWPolyline(const DxfLWPolyline& p)  { if (p.isValid()) m_lwPolylines.push_back(p); }
    void addEllipse(const DxfEllipse& e)        { if (e.isValid()) m_ellipses.push_back(e); }
    void addSpline(const DxfSpline& s)          { if (s.isValid()) m_splines.push_back(s); }
    void addSpline(DxfSpline&& s)               { if (s.isValid()) m_splines.push_back(std::move(s)); }

    // --- Nested INSERTs ---
    void addInsert(const InsertInfo& ins) { m_inserts.push_back(ins); }
    const std::vector<InsertInfo>& inserts() const { return m_inserts; }

private:
    std::string m_name;
    double m_baseX = 0.0, m_baseY = 0.0, m_baseZ = 0.0;
    std::vector<DxfPoint>      m_points;
    std::vector<DxfLine>       m_lines;
    std::vector<DxfCircle>     m_circles;
    std::vector<DxfArc>        m_arcs;
    std::vector<DxfLWPolyline> m_lwPolylines;
    std::vector<DxfEllipse>    m_ellipses;
    std::vector<DxfSpline>     m_splines;
    std::vector<InsertInfo>    m_inserts;
};

#endif // DxfData_h
