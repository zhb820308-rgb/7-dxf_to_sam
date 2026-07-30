#include "DxfParser.h"

#include "GeometryUtils.h"
#include "libdxfrw.h"
#include <QDebug>
#include <cmath>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace {

// ========================================================================
//  Block / insert structures (internal)
// ========================================================================

struct InsertInfo {
    std::string blockName;
    double insertX = 0.0;
    double insertY = 0.0;
    double insertZ = 0.0;
    double scaleX  = 1.0;
    double scaleY  = 1.0;
    double scaleZ  = 1.0;
    double angle   = 0.0;   // radians
    int    colCount = 1;
    int    rowCount = 1;
    double colSpace = 0.0;
    double rowSpace = 0.0;
};

struct BlockInfo {
    std::string name;
    double baseX = 0.0;
    double baseY = 0.0;
    double baseZ = 0.0;

    std::vector<DxfPoint>       points;
    std::vector<DxfLine>        lines;
    std::vector<DxfCircle>      circles;
    std::vector<DxfArc>         arcs;
    std::vector<DxfLWPolyline>  lwPolylines;
    std::vector<DxfEllipse>     ellipses;
    std::vector<DxfSpline>      splines;
    std::vector<InsertInfo>     inserts;  // nested INSERTs
};

// ========================================================================
//  2D affine transform (stored row-major: 2×3)
//   [ m00  m01  m02 ]   [x]
//   [ m10  m11  m12 ] × [y]
//                        [1]
// ========================================================================

struct Transform2D {
    double m00 = 1.0, m01 = 0.0, m02 = 0.0;
    double m10 = 0.0, m11 = 1.0, m12 = 0.0;

    static Transform2D identity() { return Transform2D(); }

    static Transform2D translation(double tx, double ty) {
        Transform2D t;
        t.m02 = tx;
        t.m12 = ty;
        return t;
    }

    static Transform2D rotation(double angle) {
        Transform2D t;
        double c = std::cos(angle);
        double s = std::sin(angle);
        t.m00 = c; t.m01 = -s;
        t.m10 = s; t.m11 =  c;
        return t;
    }

    static Transform2D scaling(double sx, double sy) {
        Transform2D t;
        t.m00 = sx;
        t.m11 = sy;
        return t;
    }

    /// Compose: this × other (apply `other` first, then `this`).
    Transform2D compose(const Transform2D& other) const {
        Transform2D r;
        r.m00 = m00 * other.m00 + m01 * other.m10;
        r.m01 = m00 * other.m01 + m01 * other.m11;
        r.m02 = m00 * other.m02 + m01 * other.m12 + m02;
        r.m10 = m10 * other.m00 + m11 * other.m10;
        r.m11 = m10 * other.m01 + m11 * other.m11;
        r.m12 = m10 * other.m02 + m11 * other.m12 + m12;
        return r;
    }

    /// Transform a 2D point.
    void apply(double& x, double& y) const {
        double nx = m00 * x + m01 * y + m02;
        double ny = m10 * x + m11 * y + m12;
        x = nx;
        y = ny;
    }

    DxfPoint applyTo(const DxfPoint& pt) const {
        double x = pt.x(), y = pt.y();
        apply(x, y);
        return DxfPoint(x, y, pt.z());  // z untouched (blocks are 2D)
    }

    /// Check whether this transform preserves angles and uniform scale
    /// (i.e. is a similarity: m00 == ±m11 && m01 == ∓m10 && |m00| == |m10| != 0)
    bool isSimilarity(double* outScale = nullptr) const {
        double a = m00, b = m01;
        double c = m10, d = m11;
        // similarity conditions: a* a + c* c == b* b + d* d  &&  a* b + c* d == 0
        double dot = a * b + c * d;
        if (std::fabs(dot) > 1e-12) return false;
        double norm2_0 = a * a + c * c;
        double norm2_1 = b * b + d * d;
        if (std::fabs(norm2_0 - norm2_1) > 1e-9 * std::max(norm2_0, 1.0)) return false;
        if (norm2_0 < 1e-24) return false;
        if (outScale) *outScale = std::sqrt(norm2_0);
        return true;
    }
};

// ========================================================================
//  DxfReader — DRW_Interface implementation (internal, only used here)
// ========================================================================

class DxfReader : public DRW_Interface {
public:
    // --- Parsing context ---
    DxfData                      m_modelSpace;
    std::unordered_map<std::string, BlockInfo> m_blocks;
    std::vector<InsertInfo>      m_modelSpaceInserts;

    BlockInfo* m_currentBlock = nullptr;

    // --- Implemented entity callbacks ---
    void addLine(const DRW_Line& data) override;
    void addCircle(const DRW_Circle& data) override;
    void addArc(const DRW_Arc& data) override;
    void addEllipse(const DRW_Ellipse& data) override;
    void addLWPolyline(const DRW_LWPolyline& data) override;
    void addSpline(const DRW_Spline* data) override;
    void addPoint(const DRW_Point& data) override;

    // --- Block / Insert callbacks ---
    void addBlock(const DRW_Block& data) override;
    void endBlock() override;
    void addInsert(const DRW_Insert& data) override;

    // --- Stub callbacks (no-op) ---
    void addHeader(const DRW_Header* data) override {}
    void addLType(const DRW_LType& data) override {}
    void addLayer(const DRW_Layer& data) override {}
    void addDimStyle(const DRW_Dimstyle& data) override {}
    void addVport(const DRW_Vport& data) override {}
    void addTextStyle(const DRW_Textstyle& data) override {}
    void addAppId(const DRW_AppId& data) override {}
    void setBlock(const int handle) override {}
    void addRay(const DRW_Ray& data) override {}
    void addXline(const DRW_Xline& data) override {}
    void addPolyline(const DRW_Polyline& data) override {}
    void addKnot(const DRW_Entity& data) override {}
    void addTrace(const DRW_Trace& data) override {}
    void add3dFace(const DRW_3Dface& data) override {}
    void addSolid(const DRW_Solid& data) override {}
    void addMText(const DRW_MText& data) override {}
    void addText(const DRW_Text& data) override {}
    void addDimAlign(const DRW_DimAligned* data) override {}
    void addDimLinear(const DRW_DimLinear* data) override {}
    void addDimRadial(const DRW_DimRadial* data) override {}
    void addDimDiametric(const DRW_DimDiametric* data) override {}
    void addDimAngular(const DRW_DimAngular* data) override {}
    void addDimAngular3P(const DRW_DimAngular3p* data) override {}
    void addDimOrdinate(const DRW_DimOrdinate* data) override {}
    void addLeader(const DRW_Leader* data) override {}
    void addHatch(const DRW_Hatch* data) override {}
    void addViewport(const DRW_Viewport& data) override {}
    void addImage(const DRW_Image* data) override {}
    void linkImage(const DRW_ImageDef* data) override {}
    void addComment(const char* comment) override {}
    void addPlotSettings(const DRW_PlotSettings* data) override {}
    void writeHeader(DRW_Header& data) override {}
    void writeBlocks() override {}
    void writeBlockRecords() override {}
    void writeEntities() override {}
    void writeLTypes() override {}
    void writeLayers() override {}
    void writeTextstyles() override {}
    void writeVports() override {}
    void writeDimstyles() override {}
    void writeObjects() override {}
    void writeAppId() override {}
};

// ========================================================================
//  Block expansion — recursive, matrix-based
// ========================================================================

static const int kMaxExpandDepth = 32;

/// Forward declaration.
static void expandBlockRecursive(
    DxfData& output,
    const std::unordered_map<std::string, BlockInfo>& blocks,
    const std::string& blockName,
    const Transform2D& parentTransform,
    int depth,
    std::unordered_set<std::string>& visiting,
    int& totalExpanded);

/// Expand a block's direct entities under a given transform.
static void expandBlockEntities(
    DxfData& output,
    const BlockInfo& blk,
    const Transform2D& t)
{
    // --- Points ---
    for (const DxfPoint& pt : blk.points) {
        if (!pt.isValid()) continue;
        output.addPoint(t.applyTo(pt));
    }

    // --- Lines ---
    for (const DxfLine& line : blk.lines) {
        if (!line.isValid()) continue;
        output.addLine(DxfLine(t.applyTo(line.start()), t.applyTo(line.end())));
    }

    // --- Circles ---
    double scale = 1.0;
    bool isSim = t.isSimilarity(&scale);
    for (const DxfCircle& circle : blk.circles) {
        if (!circle.isValid()) continue;
        if (isSim) {
            DxfPoint c = t.applyTo(circle.center());
            double r = circle.radius() * scale;
            if (r > 0.0)
                output.addCircle(DxfCircle(c, r));
        } else {
            // Non-similarity → discretize
            std::vector<DxfLine> segs = GeometryUtils::tessellateArc(
                DxfArc(circle.center(), circle.radius(), 0.0, 2.0 * M_PI, true), 0.01);
            for (const DxfLine& seg : segs) {
                if (!seg.isValid()) continue;
                output.addLine(DxfLine(t.applyTo(seg.start()), t.applyTo(seg.end())));
            }
        }
    }

    // --- Arcs: discretize + transform ---
    for (const DxfArc& arc : blk.arcs) {
        if (!arc.isValid()) continue;
        std::vector<DxfLine> segs = GeometryUtils::tessellateArc(arc, 0.01);
        for (const DxfLine& seg : segs) {
            if (!seg.isValid()) continue;
            output.addLine(DxfLine(t.applyTo(seg.start()), t.applyTo(seg.end())));
        }
    }

    // --- LWPolylines ---
    for (const DxfLWPolyline& poly : blk.lwPolylines) {
        if (!poly.isValid()) continue;
        std::vector<DxfLine> segs = GeometryUtils::tessellateLWPolyline(poly, 0.01);
        for (const DxfLine& seg : segs) {
            if (!seg.isValid()) continue;
            output.addLine(DxfLine(t.applyTo(seg.start()), t.applyTo(seg.end())));
        }
    }

    // --- Ellipses ---
    for (const DxfEllipse& ellipse : blk.ellipses) {
        if (!ellipse.isValid()) continue;
        std::vector<DxfLine> segs = GeometryUtils::tessellateEllipse(ellipse, 0.01);
        for (const DxfLine& seg : segs) {
            if (!seg.isValid()) continue;
            output.addLine(DxfLine(t.applyTo(seg.start()), t.applyTo(seg.end())));
        }
    }

    // --- Splines ---
    for (const DxfSpline& spline : blk.splines) {
        if (!spline.isValid()) continue;
        std::vector<DxfLine> segs = GeometryUtils::tessellateSpline(spline, 0.01);
        for (const DxfLine& seg : segs) {
            if (!seg.isValid()) continue;
            output.addLine(DxfLine(t.applyTo(seg.start()), t.applyTo(seg.end())));
        }
    }
}

/// Build per-instance transform from an InsertInfo and block base point,
/// for array element (col, row).
static Transform2D buildInsertTransform(const InsertInfo& ins,
                                        double blockBaseX,
                                        double blockBaseY,
                                        int col, int row)
{
    // Array spacing in local (rotated) coordinates:
    // offset_local = (col*colSpace, row*rowSpace)
    // offset_world = Rotate(angle) × offset_local
    double ox = col * ins.colSpace;
    double oy = row * ins.rowSpace;
    double cosA = std::cos(ins.angle);
    double sinA = std::sin(ins.angle);
    double offX = cosA * ox - sinA * oy;
    double offY = sinA * ox + cosA * oy;

    // Transform: Translate(insertPoint + offset) × Rotate(angle) × Scale(sx,sy) × Translate(-base)
    return Transform2D::translation(ins.insertX + offX, ins.insertY + offY)
        .compose(Transform2D::rotation(ins.angle))
        .compose(Transform2D::scaling(ins.scaleX, ins.scaleY))
        .compose(Transform2D::translation(-blockBaseX, -blockBaseY));
}

/// Recursively expand a block.
static void expandBlockRecursive(
    DxfData& output,
    const std::unordered_map<std::string, BlockInfo>& blocks,
    const std::string& blockName,
    const Transform2D& parentTransform,
    int depth,
    std::unordered_set<std::string>& visiting,
    int& totalExpanded)
{
    if (depth > kMaxExpandDepth) {
        qWarning() << "[BlockExpand] max depth exceeded at block:" << blockName.c_str();
        return;
    }

    auto it = blocks.find(blockName);
    if (it == blocks.end()) {
        qWarning() << "[BlockExpand] unknown block:" << blockName.c_str();
        return;
    }

    const BlockInfo& blk = it->second;

    // Cycle detection
    if (visiting.count(blockName)) {
        qWarning() << "[BlockExpand] cycle detected for block:" << blockName.c_str() << "- skipping recursion";
        return;
    }
    visiting.insert(blockName);

    // 1. Expand direct entities
    expandBlockEntities(output, blk, parentTransform);

    // 2. Recursively expand nested INSERTs
    for (const InsertInfo& ins : blk.inserts) {
        const int nCols = std::max(1, ins.colCount);
        const int nRows = std::max(1, ins.rowCount);

        for (int row = 0; row < nRows; ++row) {
            for (int col = 0; col < nCols; ++col) {
                // sub-block's own base point
                auto subIt = blocks.find(ins.blockName);
                double subBaseX = (subIt != blocks.end()) ? subIt->second.baseX : 0.0;
                double subBaseY = (subIt != blocks.end()) ? subIt->second.baseY : 0.0;

                Transform2D local = buildInsertTransform(ins, subBaseX, subBaseY, col, row);
                Transform2D child = parentTransform.compose(local);

                expandBlockRecursive(output, blocks, ins.blockName, child,
                                     depth + 1, visiting, totalExpanded);
                ++totalExpanded;
            }
        }
    }

    visiting.erase(blockName);
}

/// Top-level block expansion.
static void expandBlocks(DxfData& output,
                         const DxfData& modelSpace,
                         const std::unordered_map<std::string, BlockInfo>& blocks,
                         const std::vector<InsertInfo>& inserts)
{
    // 1. Copy model-space entities directly
    for (const DxfPoint& pt : modelSpace.points()) output.addPoint(pt);
    for (const DxfLine& line : modelSpace.lines()) output.addLine(line);
    for (const DxfCircle& circle : modelSpace.circles()) output.addCircle(circle);
    for (const DxfArc& arc : modelSpace.arcs()) output.addArc(arc);
    for (const DxfLWPolyline& poly : modelSpace.lwPolylines()) output.addLWPolyline(poly);
    for (const DxfEllipse& ellipse : modelSpace.ellipses()) output.addEllipse(ellipse);
    for (const DxfSpline& spline : modelSpace.splines()) output.addSpline(spline);

    // 2. Expand model-space INSERTs
    int expandedInserts = 0;
    int skippedInserts  = 0;
    std::unordered_set<std::string> visiting;

    for (const InsertInfo& ins : inserts) {
        auto it = blocks.find(ins.blockName);
        if (it == blocks.end()) {
            qWarning() << "[BlockExpand] INSERT references unknown block:" << ins.blockName.c_str() << "- skipped";
            ++skippedInserts;
            continue;
        }

        const int nCols = std::max(1, ins.colCount);
        const int nRows = std::max(1, ins.rowCount);

        for (int row = 0; row < nRows; ++row) {
            for (int col = 0; col < nCols; ++col) {
                Transform2D local = buildInsertTransform(ins, it->second.baseX,
                                                         it->second.baseY, col, row);
                visiting.clear();
                expandBlockRecursive(output, blocks, ins.blockName, local,
                                     0, visiting, expandedInserts);
                ++expandedInserts;
            }
        }
    }

    qDebug() << "[BlockExpand] expanded inserts:" << expandedInserts
             << "skipped:" << skippedInserts;
}

// ========================================================================
//  DxfReader member functions — entity callbacks
// ========================================================================

void DxfReader::addLine(const DRW_Line& data) {
    DxfPoint start(data.basePoint.x, data.basePoint.y, data.basePoint.z);
    DxfPoint end(data.secPoint.x, data.secPoint.y, data.secPoint.z);
    DxfLine line(start, end);

    if (m_currentBlock) {
        m_currentBlock->lines.push_back(line);
    } else {
        m_modelSpace.addLine(line);
    }
}

void DxfReader::addCircle(const DRW_Circle& data)
{
    DxfPoint center(data.basePoint.x, data.basePoint.y, data.basePoint.z);
    DxfCircle circle(center, data.radious);

    if (m_currentBlock) {
        m_currentBlock->circles.push_back(circle);
    } else {
        m_modelSpace.addCircle(circle);
    }
}

void DxfReader::addArc(const DRW_Arc& data) {
    const DRW_Coord center = data.basePoint;
    const double radius = data.radious;

    if (!std::isfinite(center.x) || !std::isfinite(center.y) || !std::isfinite(center.z)) {
        qDebug() << "[DxfReader] addArc skipped: invalid center";
        return;
    }
    if (!std::isfinite(radius) || radius <= 0.0) {
        qDebug() << "[DxfReader] addArc skipped: invalid radius" << radius;
        return;
    }
    double start = data.staangle;
    double end = data.endangle;
    if (!std::isfinite(start) || !std::isfinite(end)) {
        qDebug() << "[DxfReader] addArc skipped: NaN angles";
        return;
    }

    DxfPoint c(center.x, center.y, center.z);
    DxfArc arc(c, radius, start, end, data.isccw);

    if (m_currentBlock) {
        m_currentBlock->arcs.push_back(arc);
    } else {
        m_modelSpace.addArc(arc);
    }
}

void DxfReader::addEllipse(const DRW_Ellipse& data) {
    const double majorX = data.secPoint.x;
    const double majorY = data.secPoint.y;
    const double majorLen = std::sqrt(majorX * majorX + majorY * majorY);

    if (majorLen <= 0.0) return;
    if (!std::isfinite(data.ratio) || data.ratio <= 0.0) return;

    DxfPoint center(data.basePoint.x, data.basePoint.y, data.basePoint.z);
    DxfPoint majorAxisEnd(data.secPoint.x, data.secPoint.y, data.secPoint.z);
    DxfEllipse ellipse(center, majorAxisEnd, data.ratio,
                        data.staparam, data.endparam, data.isccw);

    if (m_currentBlock) {
        m_currentBlock->ellipses.push_back(ellipse);
    } else {
        m_modelSpace.addEllipse(ellipse);
    }
}

void DxfReader::addLWPolyline(const DRW_LWPolyline& data)
{
    const int numVerts = std::min(data.vertexnum, static_cast<int>(data.vertlist.size()));
    if (numVerts < 2) return;

    const bool isClosed = (data.flags & 1) != 0;

    std::vector<DxfPoint> vertices;
    std::vector<double>   bulges;
    vertices.reserve(numVerts);
    bulges.reserve(isClosed ? numVerts : numVerts - 1);

    for (int i = 0; i < numVerts; ++i) {
        const DRW_Vertex2D& v = *data.vertlist[i];
        vertices.push_back(DxfPoint(v.x, v.y, 0.0));
    }

    if (isClosed) {
        for (int i = 0; i < numVerts; ++i) {
            bulges.push_back(data.vertlist[i]->bulge);
        }
    } else {
        for (int i = 0; i < numVerts - 1; ++i) {
            bulges.push_back(data.vertlist[i]->bulge);
        }
    }

    DxfLWPolyline poly(vertices, bulges, isClosed, 0.0);

    if (m_currentBlock) {
        m_currentBlock->lwPolylines.push_back(poly);
    } else {
        m_modelSpace.addLWPolyline(poly);
    }
}

void DxfReader::addSpline(const DRW_Spline* data)
{
    if (!data) return;

    const bool isRational = (data->flags & 4) != 0;
    const bool isPeriodic = (data->flags & 2) != 0;
    const bool isClosed   = (data->flags & 1) != 0;

    const bool hasControlData =
        data->ncontrol > 0
        && static_cast<int>(data->controllist.size()) >= data->ncontrol
        && data->degree >= 1
        && data->ncontrol > data->degree;

    const bool hasFitData =
        data->nfit >= 2
        && static_cast<int>(data->fitlist.size()) >= data->nfit;

    if (!hasControlData && !hasFitData) {
        qDebug() << "[Spline] no valid control or fit data; ncontrol=" << data->ncontrol
                 << " degree=" << data->degree << " nfit=" << data->nfit;
        return;
    }

    if (hasControlData) {
        const int numCtrl = data->ncontrol;
        const int degree  = data->degree;
        const std::vector<double>& knots = data->knotslist;
        const int expectedKnotCount = numCtrl + degree + 1;

        if (static_cast<int>(knots.size()) != expectedKnotCount) {
            qDebug() << "[Spline] ERROR: knot count mismatch";
            return;
        }
        for (int i = 0; i < expectedKnotCount; ++i) {
            if (!std::isfinite(knots[i]) || (i > 0 && knots[i] < knots[i - 1])) {
                qDebug() << "[Spline] ERROR: invalid knot at index" << i;
                return;
            }
        }
        for (int i = 0; i < numCtrl; ++i) {
            const auto& point = data->controllist[i];
            if (!point || !std::isfinite(point->x) || !std::isfinite(point->y) || !std::isfinite(point->z)) {
                qDebug() << "[Spline] ERROR: invalid control point at index" << i;
                return;
            }
        }
        if (isRational) {
            if (static_cast<int>(data->weightlist.size()) < numCtrl) {
                qDebug() << "[Spline] ERROR: insufficient weights";
                return;
            }
            for (int i = 0; i < numCtrl; ++i) {
                const double w = data->weightlist[i];
                if (!std::isfinite(w) || w <= 0.0) {
                    qDebug() << "[Spline] ERROR: invalid weight at index" << i;
                    return;
                }
            }
        }
    }

    std::vector<DxfPoint> ctrlPts;
    for (int i = 0; i < data->ncontrol; ++i) {
        const DRW_Coord& pt = *(data->controllist[i]);
        ctrlPts.push_back(DxfPoint(pt.x, pt.y, pt.z));
    }

    std::vector<double> knots = data->knotslist;

    std::vector<double> weights;
    if (isRational) {
        for (int i = 0; i < data->ncontrol; ++i)
            weights.push_back(data->weightlist[i]);
    }

    std::vector<DxfPoint> fitPts;
    for (int i = 0; i < data->nfit; ++i) {
        const auto& sp = data->fitlist[i];
        if (sp && std::isfinite(sp->x) && std::isfinite(sp->y) && std::isfinite(sp->z))
            fitPts.push_back(DxfPoint(sp->x, sp->y, sp->z));
    }

    double tgStartX = data->tgStart.x, tgStartY = data->tgStart.y, tgStartZ = data->tgStart.z;
    double tgEndX   = data->tgEnd.x,   tgEndY   = data->tgEnd.y,   tgEndZ   = data->tgEnd.z;

    DxfSpline spline(ctrlPts, knots, weights, fitPts,
                      data->degree, data->flags,
                      tgStartX, tgStartY, tgStartZ,
                      tgEndX, tgEndY, tgEndZ);

    if (m_currentBlock) {
        m_currentBlock->splines.push_back(spline);
    } else {
        m_modelSpace.addSpline(spline);
    }
}

void DxfReader::addPoint(const DRW_Point& data) {
    DxfPoint pt(data.basePoint.x, data.basePoint.y, data.basePoint.z);
    if (m_currentBlock) {
        m_currentBlock->points.push_back(pt);
    } else {
        m_modelSpace.addPoint(pt);
    }
}

// ========================================================================
//  DxfReader member functions — block / insert callbacks
// ========================================================================

void DxfReader::addBlock(const DRW_Block& data) {
    BlockInfo blk;
    blk.name  = data.name;
    blk.baseX = data.basePoint.x;
    blk.baseY = data.basePoint.y;
    blk.baseZ = data.basePoint.z;

    qDebug() << "[DxfReader] addBlock:" << blk.name.c_str()
             << "base:(" << blk.baseX << "," << blk.baseY << "," << blk.baseZ << ")"
             << "flags:" << data.flags;

    if (blk.name == "*Model_Space" || blk.name == "*Paper_Space" || blk.name == "*Paper_Space0") {
        qDebug() << "[DxfReader] skipping layout block:" << blk.name.c_str();
        m_currentBlock = nullptr;
        return;
    }

    m_blocks[blk.name] = blk;
    m_currentBlock = &m_blocks[blk.name];
}

void DxfReader::endBlock() {
    if (m_currentBlock) {
        qDebug() << "[DxfReader] endBlock:" << m_currentBlock->name.c_str()
                 << "lines:" << m_currentBlock->lines.size()
                 << "circles:" << m_currentBlock->circles.size()
                 << "arcs:" << m_currentBlock->arcs.size()
                 << "lwPolylines:" << m_currentBlock->lwPolylines.size()
                 << "ellipses:" << m_currentBlock->ellipses.size()
                 << "splines:" << m_currentBlock->splines.size()
                 << "inserts:" << m_currentBlock->inserts.size();
    }
    m_currentBlock = nullptr;
}

void DxfReader::addInsert(const DRW_Insert& data) {
    InsertInfo ins;
    ins.blockName = data.name;
    ins.insertX   = data.basePoint.x;
    ins.insertY   = data.basePoint.y;
    ins.insertZ   = data.basePoint.z;
    ins.scaleX    = data.xscale;
    ins.scaleY    = data.yscale;
    ins.scaleZ    = data.zscale;
    ins.angle     = data.angle;
    ins.colCount  = data.colcount;
    ins.rowCount  = data.rowcount;
    ins.colSpace  = data.colspace;
    ins.rowSpace  = data.rowspace;

    if (m_currentBlock) {
        // Nested INSERT — store in block definition for recursive expansion
        m_currentBlock->inserts.push_back(ins);
        if (m_currentBlock->inserts.size() <= 3)
            qDebug() << "[DxfReader] addInsert (nested):" << ins.blockName.c_str()
                     << "inside block" << m_currentBlock->name.c_str();
        return;
    }

    qDebug() << "[DxfReader] addInsert:" << ins.blockName.c_str()
             << "at (" << ins.insertX << "," << ins.insertY << "," << ins.insertZ << ")"
             << "scale (" << ins.scaleX << "," << ins.scaleY << "," << ins.scaleZ << ")"
             << "angle" << ins.angle << "rad"
             << "cols" << ins.colCount << "rows" << ins.rowCount;

    m_modelSpaceInserts.push_back(ins);
}

}  // namespace

// ========================================================================
//  DxfParser::parseFile
// ========================================================================

bool DxfParser::parseFile(const QString& filePath, DxfData& outData) {
    outData = DxfData();
    if (filePath.isEmpty()) {
        outData.setErrorMessage(QStringLiteral("DXF file is empty"));
        return false;
    }
    const QByteArray pathBytes = filePath.toLocal8Bit();
    dxfRW dxf(pathBytes.constData());
    DxfReader reader;
    if (!dxf.read(&reader, true)) {
        outData.setErrorMessage(
            QStringLiteral("Failed to read DXF file. Error code: %1")
            .arg(static_cast<int>(dxf.getError())));
        return false;
    }

    // --- Expand blocks into flat DxfData ---
    expandBlocks(outData, reader.m_modelSpace, reader.m_blocks, reader.m_modelSpaceInserts);

    outData.setValid(true);
    qDebug() << "[DxfParser] read ok:"
             << "lines=" << outData.lines().size()
             << "circles=" << outData.circles().size()
             << "arcs=" << outData.arcs().size()
             << "lwPolylines=" << outData.lwPolylines().size()
             << "ellipses=" << outData.ellipses().size()
             << "splines=" << outData.splines().size()
             << "blocks=" << reader.m_blocks.size()
             << "inserts=" << reader.m_modelSpaceInserts.size();

    const int total = outData.entityCount();
    if (total == 0) {
        outData.setErrorMessage(
            QStringLiteral("DXF was read successfully, but no supported entities were found."));
        return false;
    }
    return true;
}