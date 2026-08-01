#include "DxfParser.h"

#include "GeometryUtils.h"
#include "libdxfrw.h"
#include <QDebug>
#include <algorithm>
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

    /// Model-space entities can be filtered immediately. Block entities need
    /// their INSERT context before layer 0 inheritance can be resolved.
    bool shouldSkipDuringRead(const DRW_Entity& ent) const {
        return m_currentBlock == nullptr && isLayerIgnored(ent);
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

/// Complete affine transform from block-local coordinates to world space:
///   x' = m00*x + m01*y + tx
///   y' = m10*x + m11*y + ty
///   z' = scaleZ * z + offsetZ
/// A transform can be composed with a nested INSERT without decomposing it
/// back into scale/rotation parameters, so shear and mirror combinations are
/// preserved exactly.
struct Transform2D {
    double m00 = 1.0, m01 = 0.0;
    double m10 = 0.0, m11 = 1.0;
    double tx = 0.0, ty = 0.0;
    double scaleZ = 1.0;
    double offsetZ = 0.0;

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
        return tf;
    }

    /// Return this × child: child is applied first, then this transform.
    Transform2D composedWith(const Transform2D& child) const
    {
        Transform2D result;
        result.m00 = m00 * child.m00 + m01 * child.m10;
        result.m01 = m00 * child.m01 + m01 * child.m11;
        result.m10 = m10 * child.m00 + m11 * child.m10;
        result.m11 = m10 * child.m01 + m11 * child.m11;
        result.tx = m00 * child.tx + m01 * child.ty + tx;
        result.ty = m10 * child.tx + m11 * child.ty + ty;
        result.scaleZ = scaleZ * child.scaleZ;
        result.offsetZ = scaleZ * child.offsetZ + offsetZ;
        return result;
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

    DxfPoint applyVector(double x, double y, double z = 0.0) const
    {
        return DxfPoint(m00 * x + m01 * y,
                        m10 * x + m11 * y,
                        scaleZ * z);
    }

    DxfPoint applyVector(const DxfPoint& vector) const
    {
        return applyVector(vector.x(), vector.y(), vector.z());
    }

    double determinant() const { return m00 * m11 - m01 * m10; }
    bool reversesOrientation() const { return determinant() < 0.0; }

    bool isPlanarSimilarity() const
    {
        const double firstLength2 = m00 * m00 + m10 * m10;
        const double secondLength2 = m01 * m01 + m11 * m11;
        const double dot = m00 * m01 + m10 * m11;
        const double scale = std::max({1.0, firstLength2, secondLength2});
        return firstLength2 > 1e-24
            && std::fabs(firstLength2 - secondLength2) <= 1e-9 * scale
            && std::fabs(dot) <= 1e-9 * scale;
    }

    double planarScale() const
    {
        return std::sqrt(m00 * m00 + m10 * m10);
    }

    /// Map an unwrapped direction angle under a planar similarity transform.
    double applyAngle(double angle) const
    {
        const double rotation = std::atan2(m10, m00);
        return reversesOrientation() ? rotation - angle : rotation + angle;
    }

    double applyZ(double z) const { return scaleZ * z + offsetZ; }
};

static const std::string& resolveEffectiveLayer(const std::string& sourceLayer,
                                                const std::string& inheritedLayer)
{
    static const std::string defaultLayer("0");
    if (sourceLayer.empty() || sourceLayer == "0")
        return inheritedLayer.empty() ? defaultLayer : inheritedLayer;
    return sourceLayer;
}

static bool isIgnoredLayer(const std::string& layer,
                           const std::set<std::string>& ignoredLayers)
{
    return ignoredLayers.count(layer) != 0;
}

/// Transform and append discretized segments to output.
static bool addTransformedSegments(DxfData& output,
                                   const std::vector<DxfLine>& segments,
                                   const Transform2D& tf,
                                   const std::string& effectiveLayer,
                                   ExpansionBudget& budget)
{
    if (!budget.consumeEntities(segments.size())) return false;
    output.reserveLines(output.lines().size() + segments.size());
    for (const DxfLine& seg : segments) {
        if (!seg.isValid()) continue;
        DxfLine transformed(tf.apply(seg.start()), tf.apply(seg.end()));
        transformed.setLayer(effectiveLayer);
        output.addGeneratedLine(transformed);
    }
    return true;
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
                             const std::string& insertLayer,
                             const std::set<std::string>& ignoredLayers,
                             double tolerance, bool uniformXY,
                             ExpansionBudget& budget,
                             TessFn tessellate,
                             PreserveFn preserve)
{
    for (const Entity& e : entities) {
        if (!e.isValid()) continue;
        const std::string& effectiveLayer = resolveEffectiveLayer(e.layer(), insertLayer);
        if (isIgnoredLayer(effectiveLayer, ignoredLayers)) continue;
        recordGeneratedEntity(output, e);
        if (uniformXY) {
            if (!budget.consumeEntities(1)) return false;
            preserve(output, e, tf, effectiveLayer);
        } else {
            const std::vector<DxfLine> segments = tessellate(e, tolerance);
            if (!addTransformedSegments(output, segments, tf, effectiveLayer, budget)) return false;
        }
    }
    return true;
}

/// Forward declaration for recursive expansion.
static bool expandSingleBlock(DxfData& output,
                              const DxfBlock& blk,
                              const Transform2D& tf,
                              const std::string& insertLayer,
                              const std::set<std::string>& ignoredLayers,
                              double tolerance,
                              const std::unordered_map<std::string, DxfBlock>& blocks,
                              int depth,
                              std::unordered_set<std::string>& visiting,
                              ExpansionBudget& budget);

/// Expand one INSERT with array (row × col) support.
/// Generates all array instances and delegates each to expandSingleBlock.
static bool expandInsertArray(DxfData& output,
                              const DxfBlock& blk,
                              const InsertInfo& ins,
                              const Transform2D& parentTf,
                              const std::string& insertLayer,
                              const std::set<std::string>& ignoredLayers,
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
            const Transform2D localTf = Transform2D::fromInsert(
                insCopy, blk.baseX(), blk.baseY(), blk.baseZ());
            const Transform2D worldTf = parentTf.composedWith(localTf);
            if (!expandSingleBlock(output, blk, worldTf, insertLayer, ignoredLayers,
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
                              const Transform2D& tf,
                              const std::string& insertLayer,
                              const std::set<std::string>& ignoredLayers,
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

    const bool preserveRoundCurves = tf.isPlanarSimilarity();

    // --- Points: direct transform ---
    output.reservePoints(output.points().size() + blk.points().size());
    output.reserveLines(output.lines().size() + blk.lines().size());
    output.reserveLWPolylines(output.lwPolylines().size() + blk.lwPolylines().size());
    output.reserveSplines(output.splines().size() + blk.splines().size());

    for (const DxfPoint& pt : blk.points()) {
        if (!pt.isValid()) continue;
        const std::string& layer = resolveEffectiveLayer(pt.layer(), insertLayer);
        if (isIgnoredLayer(layer, ignoredLayers)) continue;
        if (!budget.consumeEntities(1)) return false;
        DxfPoint transformed = tf.apply(pt);
        transformed.setLayer(layer);
        output.recordGeneratedEntity(EntityType::Point);
        output.addGeneratedPoint(transformed);
    }

    // --- Lines: direct transform ---
    for (const DxfLine& line : blk.lines()) {
        if (!line.isValid()) continue;
        const std::string& layer = resolveEffectiveLayer(line.layer(), insertLayer);
        if (isIgnoredLayer(layer, ignoredLayers)) continue;
        if (!budget.consumeEntities(1)) return false;
        DxfPoint s = tf.apply(line.start());
        DxfPoint e = tf.apply(line.end());
        DxfLine transformed(s, e);
        transformed.setLayer(layer);
        output.recordGeneratedEntity(EntityType::Line);
        output.addGeneratedLine(transformed);
    }

    // --- Circles ---
    for (const DxfCircle& circle : blk.circles()) {
        if (!circle.isValid()) continue;
        const std::string& layer = resolveEffectiveLayer(circle.layer(), insertLayer);
        if (isIgnoredLayer(layer, ignoredLayers)) continue;
        output.recordGeneratedEntity(EntityType::Circle);
        if (preserveRoundCurves) {
            // A planar similarity preserves circles, including mirrored ones.
            DxfPoint c = tf.apply(circle.center());
            const double r = circle.radius() * tf.planarScale();
            if (r > 0.0) {
                if (!budget.consumeEntities(1)) return false;
                DxfCircle transformed(c, r);
                transformed.setLayer(layer);
                output.addGeneratedCircle(transformed);
            }
        } else {
            // Non-uniform or negative scale → discretize to lines
            std::vector<DxfLine> segs = GeometryUtils::tessellateArc(
                DxfArc(circle.center(), circle.radius(), 0.0, 2.0 * M_PI, true),
                tolerance);
            if (!addTransformedSegments(output, segs, tf, layer, budget)) return false;
        }
    }

    // --- Arcs: uniform scale → preserve Arc; non-uniform → discretize ---
    if (!expandCurveGroup(output, blk.arcs(), tf, insertLayer, ignoredLayers,
        tolerance, preserveRoundCurves, budget,
        GeometryUtils::tessellateArc,
        [](DxfData& out, const DxfArc& arc, const Transform2D& t, const std::string& layer) {
            DxfArc transformed(t.apply(arc.center()),
                        arc.radius() * t.planarScale(),
                        t.applyAngle(arc.startAngle()),
                        t.applyAngle(arc.endAngle()),
                        t.reversesOrientation() ? !arc.isCCW() : arc.isCCW());
            transformed.setLayer(layer);
            out.addGeneratedArc(transformed);
        })) return false;

    // --- LWPolylines: uniform scale → preserve; non-uniform → discretize ---
    if (!expandCurveGroup(output, blk.lwPolylines(), tf, insertLayer, ignoredLayers,
        tolerance, preserveRoundCurves, budget,
        GeometryUtils::tessellateLWPolyline,
        [](DxfData& out, const DxfLWPolyline& poly, const Transform2D& t, const std::string& layer) {
            std::vector<DxfPoint> verts;
            verts.reserve(poly.vertices().size());
            for (const DxfPoint& v : poly.vertices())
                verts.push_back(t.apply(v));
            std::vector<double> bulges = poly.bulges();
            if (t.reversesOrientation()) {
                for (double& bulge : bulges) bulge = -bulge;
            }
            DxfLWPolyline transformed(
                verts, bulges, poly.isClosed(), t.applyZ(poly.constZ()));
            transformed.setLayer(layer);
            out.addGeneratedLWPolyline(transformed);
        })) return false;

    // --- Ellipses: uniform scale → preserve; non-uniform → discretize ---
    if (!expandCurveGroup(output, blk.ellipses(), tf, insertLayer, ignoredLayers,
        tolerance, preserveRoundCurves, budget,
        GeometryUtils::tessellateEllipse,
        [](DxfData& out, const DxfEllipse& ellipse, const Transform2D& t, const std::string& layer) {
            DxfPoint c = t.apply(ellipse.center());
            const DxfPoint m = t.applyVector(ellipse.majorAxisEnd());
            const bool reflected = t.reversesOrientation();
            DxfEllipse transformed(c, m, ellipse.ratio(),
                reflected ? -ellipse.startParam() : ellipse.startParam(),
                reflected ? -ellipse.endParam() : ellipse.endParam(),
                reflected ? !ellipse.isCCW() : ellipse.isCCW());
            transformed.setLayer(layer);
            out.addGeneratedEllipse(transformed);
        })) return false;

    // --- Splines: preserve under similarities; tessellate under general affine transforms. ---
    if (!expandCurveGroup(output, blk.splines(), tf, insertLayer, ignoredLayers,
        tolerance, preserveRoundCurves, budget,
        GeometryUtils::tessellateSpline,
        [](DxfData& out, const DxfSpline& spline, const Transform2D& t, const std::string& layer) {
            std::vector<DxfPoint> ctrlPts;
            ctrlPts.reserve(spline.controlPoints().size());
            for (const DxfPoint& cp : spline.controlPoints())
                ctrlPts.push_back(t.apply(cp));
            std::vector<DxfPoint> fitPts;
            fitPts.reserve(spline.fitPoints().size());
            for (const DxfPoint& fp : spline.fitPoints())
                fitPts.push_back(t.apply(fp));
            const DxfPoint startTangent = t.applyVector(
                spline.tgStartX(), spline.tgStartY(), spline.tgStartZ());
            const DxfPoint endTangent = t.applyVector(
                spline.tgEndX(), spline.tgEndY(), spline.tgEndZ());
            DxfSpline transformed(
                ctrlPts, spline.knots(), spline.weights(), fitPts,
                spline.degree(), spline.flags(),
                startTangent.x(), startTangent.y(), startTangent.z(),
                endTangent.x(), endTangent.y(), endTangent.z());
            transformed.setLayer(layer);
            out.addGeneratedSpline(std::move(transformed));
        })) return false;

    // --- Nested INSERTs: recursive expansion ---
    for (const InsertInfo& nested : blk.inserts()) {
        const std::string nestedLayer = resolveEffectiveLayer(nested.layer, insertLayer);
        if (isIgnoredLayer(nestedLayer, ignoredLayers)) continue;
        auto it = blocks.find(nested.blockName);
        if (it == blocks.end()) {
            qWarning() << "[BlockExpand] nested INSERT references unknown block:"
                       << nested.blockName.c_str() << "- skipped";
            continue;
        }
        const DxfBlock& nestedBlk = it->second;

        // Generate nested array instances in the current block coordinate
        // system, then compose their local transforms with this world matrix.
        if (!expandInsertArray(output, nestedBlk, nested, tf, nestedLayer, ignoredLayers,
                               tolerance, blocks,
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
                         const std::set<std::string>& ignoredLayers,
                         double tolerance,
                         ExpansionBudget& budget)
{
    // Model-space entities are already in output (written directly during parsing).
    // Only INSERT expansion is needed here.

    // Expand INSERTs
    std::unordered_set<std::string> visiting;

    for (const InsertInfo& ins : inserts) {
        const std::string insertLayer = resolveEffectiveLayer(ins.layer, "0");
        if (isIgnoredLayer(insertLayer, ignoredLayers)) continue;
        auto it = blocks.find(ins.blockName);
        if (it == blocks.end()) {
            qWarning() << "[BlockExpand] INSERT references unknown block:" << ins.blockName.c_str() << "- skipped";
            continue;
        }
        const DxfBlock& blk = it->second;
        if (!expandInsertArray(output, blk, ins, Transform2D(), insertLayer, ignoredLayers,
                               tolerance,
                               blocks, 0, visiting, budget))
            return false;
    }
    return true;
}

// ========================================================================
//  DxfReader member functions — entity callbacks
// ========================================================================

void DxfReader::addLine(const DRW_Line& data) {
    if (shouldSkipDuringRead(data)) return;
    DxfPoint start(data.basePoint.x, data.basePoint.y, data.basePoint.z);
    DxfPoint end(data.secPoint.x, data.secPoint.y, data.secPoint.z);
    DxfLine line(start, end);
    line.setLayer(data.layer);

    if (m_currentBlock) {
        m_currentBlock->addLine(line);
    } else {
        m_data.addLine(line);
    }
}

void DxfReader::addCircle(const DRW_Circle& data)
{
    if (shouldSkipDuringRead(data)) return;
    DxfPoint center(data.basePoint.x, data.basePoint.y, data.basePoint.z);
    DxfCircle circle(center, data.radious);
    circle.setLayer(data.layer);

    if (m_currentBlock) {
        m_currentBlock->addCircle(circle);
    } else {
        m_data.addCircle(circle);
    }
}

void DxfReader::addArc(const DRW_Arc& data) {
    if (shouldSkipDuringRead(data)) return;
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
    arc.setLayer(data.layer);

    if (m_currentBlock) {
        m_currentBlock->addArc(arc);
    } else {
        m_data.addArc(arc);
    }
}

void DxfReader::addEllipse(const DRW_Ellipse& data) {
    if (shouldSkipDuringRead(data)) return;
    const double majorX = data.secPoint.x;
    const double majorY = data.secPoint.y;
    const double majorLen = std::sqrt(majorX * majorX + majorY * majorY);

    if (majorLen <= 0.0) return;
    if (!std::isfinite(data.ratio) || data.ratio <= 0.0) return;

    DxfPoint center(data.basePoint.x, data.basePoint.y, data.basePoint.z);
    DxfPoint majorAxisEnd(data.secPoint.x, data.secPoint.y, data.secPoint.z);
    DxfEllipse ellipse(center, majorAxisEnd, data.ratio,
                        data.staparam, data.endparam, data.isccw);
    ellipse.setLayer(data.layer);

    if (m_currentBlock) {
        m_currentBlock->addEllipse(ellipse);
    } else {
        m_data.addEllipse(ellipse);
    }
}

void DxfReader::addLWPolyline(const DRW_LWPolyline& data)
{
    if (shouldSkipDuringRead(data)) return;
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
    poly.setLayer(data.layer);

    if (m_currentBlock) {
        m_currentBlock->addLWPolyline(poly);
    } else {
        m_data.addLWPolyline(poly);
    }
}

void DxfReader::addSpline(const DRW_Spline* data)
{
    if (!data) return;
    if (shouldSkipDuringRead(*data)) return;

    const bool isRational = (data->flags & 4) != 0;

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

    // --- Control data: validation + assembly in a single pass ---
    std::vector<DxfPoint> ctrlPts;
    std::vector<double>   knots;
    std::vector<double>   weights;
    if (hasControlData) {
        const int numCtrl = data->ncontrol;
        const int degree  = data->degree;
        const std::vector<double>& srcKnots = data->knotslist;
        const int expectedKnotCount = numCtrl + degree + 1;

        if (static_cast<int>(srcKnots.size()) != expectedKnotCount)
            return;

        knots.reserve(expectedKnotCount);
        for (int i = 0; i < expectedKnotCount; ++i) {
            const double k = srcKnots[i];
            if (!std::isfinite(k) || (i > 0 && k < knots.back()))
                return;
            knots.push_back(k);
        }

        ctrlPts.reserve(numCtrl);
        for (int i = 0; i < numCtrl; ++i) {
            const auto& point = data->controllist[i];
            if (!point || !std::isfinite(point->x)
                || !std::isfinite(point->y) || !std::isfinite(point->z))
                return;
            ctrlPts.push_back(DxfPoint(point->x, point->y, point->z));
        }

        if (isRational) {
            if (static_cast<int>(data->weightlist.size()) < numCtrl)
                return;
            weights.reserve(numCtrl);
            for (int i = 0; i < numCtrl; ++i) {
                const double w = data->weightlist[i];
                if (!std::isfinite(w) || w <= 0.0)
                    return;
                weights.push_back(w);
            }
        }
    }

    // --- Fit data: validation + assembly in a single pass ---
    std::vector<DxfPoint> fitPts;
    fitPts.reserve(data->nfit);
    for (int i = 0; i < data->nfit; ++i) {
        const auto& sp = data->fitlist[i];
        if (sp && std::isfinite(sp->x) && std::isfinite(sp->y) && std::isfinite(sp->z))
            fitPts.push_back(DxfPoint(sp->x, sp->y, sp->z));
    }

    double tgStartX = data->tgStart.x, tgStartY = data->tgStart.y, tgStartZ = data->tgStart.z;
    double tgEndX   = data->tgEnd.x,   tgEndY   = data->tgEnd.y,   tgEndZ   = data->tgEnd.z;

    // Move-assemble: transfers ownership of the local containers, and the
    // rvalue addSpline moves the spline into the output — no deep copies here.
    DxfSpline spline(std::move(ctrlPts), std::move(knots),
                     std::move(weights), std::move(fitPts),
                     data->degree, data->flags,
                     tgStartX, tgStartY, tgStartZ,
                     tgEndX, tgEndY, tgEndZ);
    spline.setLayer(data->layer);

    if (m_currentBlock) {
        m_currentBlock->addSpline(std::move(spline));
    } else {
        m_data.addSpline(std::move(spline));
    }
}

void DxfReader::addPoint(const DRW_Point& data) {
    if (shouldSkipDuringRead(data)) return;
    DxfPoint pt(data.basePoint.x, data.basePoint.y, data.basePoint.z);
    pt.setLayer(data.layer);
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
    ins.layer     = data.layer;
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
                      ignoredLayers,
                      curveTolerance, expansionBudget)) {
        const QString error = expansionBudget.error.isEmpty()
            ? QStringLiteral("INSERT expansion failed") : expansionBudget.error;
        outData.clear();
        outData.setErrorMessage(error);
        return false;
    }

    const DxfEntityStats& stats = outData.entityStats();
    const std::size_t usable = stats.acceptedEntities + stats.generatedEntities;
    if (usable == 0 || outData.entityCount() == 0) {
        outData.setValid(false);
        outData.setErrorMessage(
            QStringLiteral("DXF was read successfully, but no valid supported entities were found (%1 rejected).")
            .arg(stats.rejectedEntities));
        return false;
    }

    outData.setValid(true);
    return true;
}
