#include "DxfParser.h"

#include "GeometryUtils.h"
#include "libdxfrw.h"
#include <QDebug>
#include <cstdint>
#include <cmath>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace {

// ========================================================================
//  DxfReader — DRW_Interface implementation (internal, only used here)
// ========================================================================

class DxfReader : public DRW_Interface {
public:
    DxfData m_data;                         // final flattened output

    // --- Parsing context ---
    std::unordered_map<std::string, DxfBlock> m_blocks;
    std::vector<InsertInfo>      m_modelSpaceInserts;

    // current block being parsed (nullptr = model space)
    DxfBlock* m_currentBlock = nullptr;

    // --- Layer filter ---
    std::set<std::string> m_ignoredLayers;
    std::set<std::string> m_allLayers;

    const std::set<std::string>& allLayers() const { return m_allLayers; }

    /// Helper: returns true if the entity's layer is in the ignored set.
    bool isLayerIgnored(const DRW_Entity& ent) const {
        if (m_ignoredLayers.empty()) return false;
        return m_ignoredLayers.count(ent.layer) != 0;
    }

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

    // --- Layer callback (collect layer names) ---
    void addLayer(const DRW_Layer& data) override;

    // --- Stub callbacks (no-op) ---
    void addHeader(const DRW_Header* data) override {}
    void addLType(const DRW_LType& data) override {}
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
//  Block expansion helpers
// ========================================================================

constexpr std::uint64_t kMaxArrayInstancesPerInsert = 100000;
constexpr std::uint64_t kMaxExpandedBlockInstances = 100000;
// Match the Web/server geometry ceiling so both import paths reject the same scale.
constexpr std::size_t kMaxExpandedEntities = 100000;

struct ExpansionBudget {
    std::uint64_t blockInstances = 0;
    std::size_t entities = 0;
    QString error;

    bool consumeArray(int rows, int columns, const std::string& blockName)
    {
        const std::uint64_t rowCount = static_cast<std::uint64_t>(std::max(1, rows));
        const std::uint64_t columnCount = static_cast<std::uint64_t>(std::max(1, columns));
        if (rowCount > kMaxArrayInstancesPerInsert / columnCount) {
            error = QStringLiteral("INSERT expansion limit exceeded for block '%1': %2 rows x %3 columns")
                .arg(QString::fromStdString(blockName)).arg(rowCount).arg(columnCount);
            return false;
        }
        const std::uint64_t count = rowCount * columnCount;
        if (blockInstances > kMaxExpandedBlockInstances - count) {
            error = QStringLiteral("INSERT expansion limit exceeded: more than %1 block instances")
                .arg(kMaxExpandedBlockInstances);
            return false;
        }
        blockInstances += count;
        return true;
    }

    bool consumeEntities(std::size_t count)
    {
        if (count > kMaxExpandedEntities || entities > kMaxExpandedEntities - count) {
            error = QStringLiteral("INSERT expansion limit exceeded: more than %1 generated entities")
                .arg(kMaxExpandedEntities);
            return false;
        }
        entities += count;
        return true;
    }
};

/// Precomputed 2D affine transform for one INSERT instance:
///   x' = m00*x + m01*y + tx
///   y' = m10*x + m11*y + ty
///   z' = scaleZ * z + offsetZ
/// The matrix is R×S (rotation × scale), and the translation absorbs the
/// block base point so no per-point subtraction is needed. Raw axis scales
/// are kept for curve-preservation paths.
struct Transform2D {
    double m00 = 1.0, m01 = 0.0;
    double m10 = 0.0, m11 = 1.0;
    double tx = 0.0, ty = 0.0;
    double scaleZ = 1.0;
    double offsetZ = 0.0;
    double scaleX = 1.0, scaleY = 1.0;

    static Transform2D fromInsert(const InsertInfo& ins,
                                  double bx, double by, double bz)
    {
        Transform2D tf;
        const double cosA = std::cos(ins.angle);
        const double sinA = std::sin(ins.angle);
        tf.m00 = cosA * ins.scaleX;
        tf.m01 = -sinA * ins.scaleY;
        tf.m10 = sinA * ins.scaleX;
        tf.m11 = cosA * ins.scaleY;
        tf.tx  = ins.insertX - tf.m00 * bx - tf.m01 * by;
        tf.ty  = ins.insertY - tf.m10 * bx - tf.m11 * by;
        tf.scaleZ  = ins.scaleZ;
        tf.offsetZ = ins.insertZ - ins.scaleZ * bz;
        tf.scaleX  = ins.scaleX;
        tf.scaleY  = ins.scaleY;
        return tf;
    }

    DxfPoint apply(double x, double y, double z) const
    {
        return DxfPoint(m00 * x + m01 * y + tx,
                        m10 * x + m11 * y + ty,
                        scaleZ * z + offsetZ);
    }

    DxfPoint apply(const DxfPoint& pt) const
    {
        return apply(pt.x(), pt.y(), pt.z());
    }
};

/// Transform and append discretized segments to output.
static bool addTransformedSegments(DxfData& output,
                                   const std::vector<DxfLine>& segments,
                                   const Transform2D& tf,
                                   ExpansionBudget& budget)
{
    if (!budget.consumeEntities(segments.size())) return false;
    output.reserveLines(output.lines().size() + segments.size());
    for (const DxfLine& seg : segments) {
        if (!seg.isValid()) continue;
        output.addGeneratedLine(DxfLine(tf.apply(seg.start()), tf.apply(seg.end())));
    }
    return true;
}

/// Check whether scales are approximately uniform in XY (for circle preservation).
static bool isScaleUniformXY(const InsertInfo& ins) {
    return std::fabs(ins.scaleX - ins.scaleY) < 1e-9
        && std::fabs(ins.scaleX) > 1e-12;
}

/// Record the source entity before a block curve is discretized into lines.
template<typename Entity>
static void recordGeneratedEntity(DxfData& output, const Entity& entity)
{
    output.recordGeneratedEntity(entity.getType());
}

static void recordGeneratedEntity(DxfData& output, const DxfSpline& spline)
{
    output.recordGeneratedSpline(spline.kind());
}

/// Expand a group of curve entities under uniform or non-uniform scale.
/// Uniform scale → call preserve() to keep original entity type.
/// Non-uniform  → call tessellate() then transform segments to output.
template<typename Entity, typename TessFn, typename PreserveFn>
static bool expandCurveGroup(DxfData& output,
                             const std::vector<Entity>& entities,
                             const Transform2D& tf,
                             double tolerance, bool uniformXY,
                             ExpansionBudget& budget,
                             TessFn tessellate,
                             PreserveFn preserve)
{
    for (const Entity& e : entities) {
        if (!e.isValid()) continue;
        if (uniformXY) {
            if (!budget.consumeEntities(1)) return false;
            preserve(output, e, tf);
        } else {
            recordGeneratedEntity(output, e);
            const std::vector<DxfLine> segments = tessellate(e, tolerance);
            if (!addTransformedSegments(output, segments, tf, budget)) return false;
        }
    }
    return true;
}

/// Forward declaration for recursive expansion.
static bool expandSingleBlock(DxfData& output,
                              const DxfBlock& blk,
                              const InsertInfo& ins,
                              double tolerance,
                              const std::unordered_map<std::string, DxfBlock>& blocks,
                              int depth,
                              std::unordered_set<std::string>& visiting,
                              ExpansionBudget& budget);

/// Compose two INSERT transforms: outer × inner.
/// Transforms the inner INSERT's insertion point by the outer INSERT,
/// then combines scales (multiply) and angles (add).
///
/// NOTE: Scale/angle composition assumes the transforms are decomposed
/// as (Translate × Rotate × Scale). This is correct for standard DXF
/// INSERTs with uniform scale. Non-uniform nested transforms involving
/// shearing or mirroring are not handled by this composition.
static InsertInfo composeInsertTransform(const InsertInfo& outer,
                                          const InsertInfo& inner,
                                          const DxfPoint& nestedPos)
{
    InsertInfo composed;
    composed.blockName = inner.blockName;
    composed.insertX   = nestedPos.x();
    composed.insertY   = nestedPos.y();
    composed.insertZ   = nestedPos.z();
    composed.scaleX    = outer.scaleX * inner.scaleX;
    composed.scaleY    = outer.scaleY * inner.scaleY;
    composed.scaleZ    = outer.scaleZ * inner.scaleZ;
    composed.angle     = outer.angle  + inner.angle;
    composed.colCount  = inner.colCount;
    composed.rowCount  = inner.rowCount;
    composed.colSpace  = inner.colSpace;
    composed.rowSpace  = inner.rowSpace;
    return composed;
}

/// Expand one INSERT with array (row × col) support.
/// Generates all array instances and delegates each to expandSingleBlock.
static bool expandInsertArray(DxfData& output,
                              const DxfBlock& blk,
                              const InsertInfo& ins,
                              double tolerance,
                              const std::unordered_map<std::string, DxfBlock>& blocks,
                              int depth,
                              std::unordered_set<std::string>& visiting,
                              ExpansionBudget& budget)
{
    const int nCols = std::max(1, ins.colCount);
    const int nRows = std::max(1, ins.rowCount);
    if (!budget.consumeArray(nRows, nCols, blk.name())) return false;

    // Degenerate array: no real repetition.
    if (nCols * nRows <= 1) {
        return expandSingleBlock(output, blk, ins, tolerance, blocks, depth, visiting, budget);
    }

    const double cosA = std::cos(ins.angle);
    const double sinA = std::sin(ins.angle);

    // One-step increments along the rotated array grid.
    // pos(row,col) = insertion + col * colVec + row * rowVec
    const double colDx = cosA * ins.colSpace;
    const double colDy = sinA * ins.colSpace;
    const double rowDx = -sinA * ins.rowSpace;
    const double rowDy =  cosA * ins.rowSpace;

    // Reserve the full array output in one go, avoiding repeated realloc.
    // Clamp the estimate to the expansion entity budget so a malicious INSERT
    // (e.g. 100000 instances of a block with many entities) cannot force an
    // out-of-budget multi-GB allocation before per-entity accounting kicks in.
    const std::size_t instances = static_cast<std::size_t>(nCols) * nRows;
    const auto clampReserve = [instances](std::size_t current, std::size_t perInstance) {
        const std::size_t estimate = current
            + perInstance * std::min<std::size_t>(instances, kMaxExpandedEntities);
        return std::min(estimate, kMaxExpandedEntities);
    };
    output.reserveLines(clampReserve(output.lines().size(), blk.lines().size()));
    output.reservePoints(clampReserve(output.points().size(), blk.points().size()));
    output.reserveLWPolylines(clampReserve(output.lwPolylines().size(), blk.lwPolylines().size()));
    output.reserveSplines(clampReserve(output.splines().size(), blk.splines().size()));

    // Copy the INSERT once; inner loops only touch the two doubles.
    InsertInfo insCopy = ins;

    // Walk the grid with pure additions (no per-cell multiply/rotate).
    double baseX = ins.insertX;
    double baseY = ins.insertY;
    for (int row = 0; row < nRows; ++row) {
        double curX = baseX;
        double curY = baseY;
        for (int col = 0; col < nCols; ++col) {
            insCopy.insertX = curX;
            insCopy.insertY = curY;
            if (!expandSingleBlock(output, blk, insCopy,
                                   tolerance, blocks, depth, visiting, budget))
                return false;
            curX += colDx;
            curY += colDy;
        }
        baseX += rowDx;
        baseY += rowDy;
    }
    return true;
}

/// Expand a single block instance (one INSERT, one array element).
/// @param blocks  block definitions map, needed for recursive nested INSERT expansion
static bool expandSingleBlock(DxfData& output,
                              const DxfBlock& blk,
                              const InsertInfo& ins,
                              double tolerance,
                              const std::unordered_map<std::string, DxfBlock>& blocks,
                              int depth,
                              std::unordered_set<std::string>& visiting,
                              ExpansionBudget& budget)
{
    static const int kMaxExpandDepth = 32;
    if (depth > kMaxExpandDepth) {
        qWarning() << "[BlockExpand] max depth" << kMaxExpandDepth
                   << "exceeded at block:" << blk.name().c_str();
        budget.error = QStringLiteral("INSERT expansion depth limit exceeded at block '%1'")
            .arg(QString::fromStdString(blk.name()));
        return false;
    }

    // Cycle detection
    if (visiting.count(blk.name())) {
        qWarning() << "[BlockExpand] cycle detected for block:"
                   << blk.name().c_str() << "- skipping recursion";
        return true;
    }
    visiting.insert(blk.name());
    struct VisitingGuard {
        std::unordered_set<std::string>& names;
        std::string name;
        ~VisitingGuard() { names.erase(name); }
    } guard{visiting, blk.name()};

    const double bx = blk.baseX(), by = blk.baseY(), bz = blk.baseZ();
    // Precompute the full affine transform once for this INSERT instance,
    // eliminating per-point cos/sin and base-point subtraction.
    const Transform2D tf = Transform2D::fromInsert(ins, bx, by, bz);
    const bool uniformXY = isScaleUniformXY(ins);

    // --- Points: direct transform ---
    output.reservePoints(output.points().size() + blk.points().size());
    output.reserveLines(output.lines().size() + blk.lines().size());
    output.reserveLWPolylines(output.lwPolylines().size() + blk.lwPolylines().size());
    output.reserveSplines(output.splines().size() + blk.splines().size());

    for (const DxfPoint& pt : blk.points()) {
        if (!pt.isValid()) continue;
        if (!budget.consumeEntities(1)) return false;
        output.addPoint(tf.apply(pt));
    }

    // --- Lines: direct transform ---
    for (const DxfLine& line : blk.lines()) {
        if (!line.isValid()) continue;
        if (!budget.consumeEntities(1)) return false;
        DxfPoint s = tf.apply(line.start());
        DxfPoint e = tf.apply(line.end());
        output.addLine(DxfLine(s, e));
    }

    // --- Circles ---
    for (const DxfCircle& circle : blk.circles()) {
        if (!circle.isValid()) continue;
        if (isScaleUniformXY(ins) && std::fabs(ins.scaleZ - ins.scaleX) < 1e-9) {
            // Uniform scale → preserve as circle
            DxfPoint c = tf.apply(circle.center());
            double   r = circle.radius() * tf.scaleX;
            if (r > 0.0) {
                if (!budget.consumeEntities(1)) return false;
                output.addCircle(DxfCircle(c, r));
            } else
                output.recordGeneratedEntity(EntityType::Circle);
        } else {
            // Non-uniform or negative scale → discretize to lines
            output.recordGeneratedEntity(EntityType::Circle);
            std::vector<DxfLine> segs = GeometryUtils::tessellateArc(
                DxfArc(circle.center(), circle.radius(), 0.0, 2.0 * M_PI, true),
                tolerance);
            if (!addTransformedSegments(output, segs, tf, budget)) return false;
        }
    }

    // --- Arcs: uniform scale → preserve Arc; non-uniform → discretize ---
    if (!expandCurveGroup(output, blk.arcs(), tf, tolerance, uniformXY, budget,
        GeometryUtils::tessellateArc,
        [](DxfData& out, const DxfArc& arc, const Transform2D& t) {
            out.addArc(DxfArc(t.apply(arc.center()),
                        arc.radius() * t.scaleX,
                        arc.startAngle(), arc.endAngle(), arc.isCCW()));
        })) return false;

    // --- LWPolylines: uniform scale → preserve; non-uniform → discretize ---
    if (!expandCurveGroup(output, blk.lwPolylines(), tf, tolerance, uniformXY, budget,
        GeometryUtils::tessellateLWPolyline,
        [](DxfData& out, const DxfLWPolyline& poly, const Transform2D& t) {
            std::vector<DxfPoint> verts;
            verts.reserve(poly.vertices().size());
            for (const DxfPoint& v : poly.vertices())
                verts.push_back(t.apply(v));
            out.addLWPolyline(DxfLWPolyline(verts, poly.bulges(),
                                             poly.isClosed(), poly.constZ() * t.scaleZ));
        })) return false;

    // --- Ellipses: uniform scale → preserve; non-uniform → discretize ---
    if (!expandCurveGroup(output, blk.ellipses(), tf, tolerance, uniformXY, budget,
        GeometryUtils::tessellateEllipse,
        [](DxfData& out, const DxfEllipse& ellipse, const Transform2D& t) {
            DxfPoint c = t.apply(ellipse.center());
            DxfPoint m(ellipse.majorAxisEnd().x() * t.scaleX,
                       ellipse.majorAxisEnd().y() * t.scaleY,
                       ellipse.majorAxisEnd().z() * t.scaleZ);
            out.addEllipse(DxfEllipse(c, m, ellipse.ratio(),
                                       ellipse.startParam(), ellipse.endParam(), ellipse.isCCW()));
        })) return false;

    // --- Splines: uniform scale → preserve; non-uniform → discretize ---
    if (!expandCurveGroup(output, blk.splines(), tf, tolerance, uniformXY, budget,
        GeometryUtils::tessellateSpline,
        [](DxfData& out, const DxfSpline& spline, const Transform2D& t) {
            std::vector<DxfPoint> ctrlPts;
            ctrlPts.reserve(spline.controlPoints().size());
            for (const DxfPoint& cp : spline.controlPoints())
                ctrlPts.push_back(t.apply(cp));
            std::vector<DxfPoint> fitPts;
            fitPts.reserve(spline.fitPoints().size());
            for (const DxfPoint& fp : spline.fitPoints())
                fitPts.push_back(t.apply(fp));
            out.addSpline(DxfSpline(ctrlPts, spline.knots(), spline.weights(), fitPts,
                                     spline.degree(), spline.flags(),
                                     spline.tgStartX(), spline.tgStartY(), spline.tgStartZ(),
                                     spline.tgEndX(), spline.tgEndY(), spline.tgEndZ()));
        })) return false;

    // --- Nested INSERTs: recursive expansion ---
    for (const InsertInfo& nested : blk.inserts()) {
        auto it = blocks.find(nested.blockName);
        if (it == blocks.end()) {
            qWarning() << "[BlockExpand] nested INSERT references unknown block:"
                       << nested.blockName.c_str() << "- skipped";
            continue;
        }
        const DxfBlock& nestedBlk = it->second;

        // Compose transforms: outer × inner
        const DxfPoint nestedPos = tf.apply(nested.insertX, nested.insertY, nested.insertZ);
        InsertInfo composed = composeInsertTransform(ins, nested, nestedPos);

        // Generate nested array instances
        if (!expandInsertArray(output, nestedBlk, composed, tolerance, blocks,
                               depth + 1, visiting, budget))
            return false;
    }
    return true;
}

/// Expand all model-space inserts into the output DxfData.
/// @param output  [in/out] already contains model-space entities; INSERT entities appended here
static bool expandBlocks(DxfData& output,
                         const std::unordered_map<std::string, DxfBlock>& blocks,
                         const std::vector<InsertInfo>& inserts,
                         double tolerance,
                         ExpansionBudget& budget)
{
    // Model-space entities are already in output (written directly during parsing).
    // Only INSERT expansion is needed here.

    // Expand INSERTs
    std::unordered_set<std::string> visiting;

    for (const InsertInfo& ins : inserts) {
        auto it = blocks.find(ins.blockName);
        if (it == blocks.end()) {
            qWarning() << "[BlockExpand] INSERT references unknown block:" << ins.blockName.c_str() << "- skipped";
            continue;
        }
        const DxfBlock& blk = it->second;
        if (!expandInsertArray(output, blk, ins, tolerance, blocks, 0, visiting, budget))
            return false;
    }
    return true;
}

// ========================================================================
//  DxfReader member functions — entity callbacks
// ========================================================================

void DxfReader::addLine(const DRW_Line& data) {
    if (isLayerIgnored(data)) return;
    DxfPoint start(data.basePoint.x, data.basePoint.y, data.basePoint.z);
    DxfPoint end(data.secPoint.x, data.secPoint.y, data.secPoint.z);
    DxfLine line(start, end);

    if (m_currentBlock) {
        m_currentBlock->addLine(line);
    } else {
        m_data.addLine(line);
    }
}

void DxfReader::addCircle(const DRW_Circle& data)
{
    if (isLayerIgnored(data)) return;
    DxfPoint center(data.basePoint.x, data.basePoint.y, data.basePoint.z);
    DxfCircle circle(center, data.radious);

    if (m_currentBlock) {
        m_currentBlock->addCircle(circle);
    } else {
        m_data.addCircle(circle);
    }
}

void DxfReader::addArc(const DRW_Arc& data) {
    if (isLayerIgnored(data)) return;
    const DRW_Coord center = data.basePoint;
    const double radius = data.radious;

    if (!std::isfinite(center.x) || !std::isfinite(center.y) || !std::isfinite(center.z))
        return;
    if (!std::isfinite(radius) || radius <= 0.0)
        return;
    double start = data.staangle;
    double end = data.endangle;
    if (!std::isfinite(start) || !std::isfinite(end))
        return;

    DxfPoint c(center.x, center.y, center.z);
    DxfArc arc(c, radius, start, end, data.isccw);

    if (m_currentBlock) {
        m_currentBlock->addArc(arc);
    } else {
        m_data.addArc(arc);
    }
}

void DxfReader::addEllipse(const DRW_Ellipse& data) {
    if (isLayerIgnored(data)) return;
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
        m_currentBlock->addEllipse(ellipse);
    } else {
        m_data.addEllipse(ellipse);
    }
}

void DxfReader::addLWPolyline(const DRW_LWPolyline& data)
{
    if (isLayerIgnored(data)) return;
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
        m_currentBlock->addLWPolyline(poly);
    } else {
        m_data.addLWPolyline(poly);
    }
}

void DxfReader::addSpline(const DRW_Spline* data)
{
    if (!data) return;
    if (isLayerIgnored(*data)) return;

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

    if (!hasControlData && !hasFitData)
        return;

    if (hasControlData) {
        const int numCtrl = data->ncontrol;
        const int degree  = data->degree;
        const std::vector<double>& knots = data->knotslist;
        const int expectedKnotCount = numCtrl + degree + 1;

        if (static_cast<int>(knots.size()) != expectedKnotCount)
            return;
        for (int i = 0; i < expectedKnotCount; ++i) {
            if (!std::isfinite(knots[i]) || (i > 0 && knots[i] < knots[i - 1]))
                return;
        }
        for (int i = 0; i < numCtrl; ++i) {
            const auto& point = data->controllist[i];
            if (!point || !std::isfinite(point->x) || !std::isfinite(point->y) || !std::isfinite(point->z))
                return;
        }
        if (isRational) {
            if (static_cast<int>(data->weightlist.size()) < numCtrl)
                return;
            for (int i = 0; i < numCtrl; ++i) {
                const double w = data->weightlist[i];
                if (!std::isfinite(w) || w <= 0.0)
                    return;
            }
        }
    }

    std::vector<DxfPoint> ctrlPts;
    ctrlPts.reserve(data->ncontrol);
    for (int i = 0; i < data->ncontrol; ++i) {
        const DRW_Coord& pt = *(data->controllist[i]);
        ctrlPts.push_back(DxfPoint(pt.x, pt.y, pt.z));
    }

    std::vector<double> knots = data->knotslist;

    std::vector<double> weights;
    if (isRational) {
        weights.reserve(data->ncontrol);
        for (int i = 0; i < data->ncontrol; ++i)
            weights.push_back(data->weightlist[i]);
    }

    std::vector<DxfPoint> fitPts;
    fitPts.reserve(data->nfit);
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
        m_currentBlock->addSpline(spline);
    } else {
        m_data.addSpline(spline);
    }
}

void DxfReader::addPoint(const DRW_Point& data) {
    if (isLayerIgnored(data)) return;
    DxfPoint pt(data.basePoint.x, data.basePoint.y, data.basePoint.z);
    if (m_currentBlock) {
        m_currentBlock->addPoint(pt);
    } else {
        m_data.addPoint(pt);
    }
}

// ========================================================================
//  DxfReader member functions — block / insert callbacks
// ========================================================================

void DxfReader::addBlock(const DRW_Block& data) {
    DxfBlock blk;
    blk.setName(data.name);
    blk.setBase(data.basePoint.x, data.basePoint.y, data.basePoint.z);

    // Skip layout blocks
    if (blk.name() == "*Model_Space" || blk.name() == "*Paper_Space" || blk.name() == "*Paper_Space0") {
        m_currentBlock = nullptr;
        return;
    }

    m_blocks[blk.name()] = blk;
    m_currentBlock = &m_blocks[blk.name()];
}

void DxfReader::endBlock() {
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
        // Nested INSERT — store in parent block for recursive expansion
        m_currentBlock->addInsert(ins);
        return;
    }

    m_modelSpaceInserts.push_back(ins);
}

void DxfReader::addLayer(const DRW_Layer& data) {
    m_allLayers.insert(data.name);
}

}  // namespace

// ========================================================================
//  DxfParser::parseFile
// ========================================================================

bool DxfParser::parseFile(const QString& filePath, DxfData& outData,
                          double curveTolerance,
                          const std::set<std::string>& ignoredLayers) {
    outData = DxfData();
    if (filePath.isEmpty()) {
        outData.setErrorMessage(QStringLiteral("DXF file is empty"));
        return false;
    }
    if (!std::isfinite(curveTolerance) || curveTolerance <= 0.0) {
        outData.setErrorMessage(
            QStringLiteral("curveTolerance must be finite and greater than zero"));
        return false;
    }

    const QByteArray pathBytes = filePath.toLocal8Bit();
    dxfRW dxf(pathBytes.constData());
    DxfReader reader;
    reader.m_ignoredLayers = ignoredLayers;
    if (!dxf.read(&reader, true)) {
        outData.setErrorMessage(
            QStringLiteral("Failed to read DXF file. Error code: %1")
            .arg(static_cast<int>(dxf.getError())));
        return false;
    }

    // --- Move parsed entities from reader to output, then expand blocks ---
    outData = std::move(reader.m_data);

    // --- Expand blocks into flat DxfData ---
    ExpansionBudget expansionBudget;
    if (!expandBlocks(outData, reader.m_blocks, reader.m_modelSpaceInserts,
                      curveTolerance, expansionBudget)) {
        const QString error = expansionBudget.error.isEmpty()
            ? QStringLiteral("INSERT expansion failed") : expansionBudget.error;
        outData.clear();
        outData.setErrorMessage(error);
        return false;
    }

    const int total = outData.entityCount();
    if (total == 0) {
        outData.setValid(false);
        outData.setErrorMessage(
            QStringLiteral("DXF was read successfully, but no supported entities were found."));
        return false;
    }

    outData.setValid(true);
    return true;
}