// ============================================================================
// FE 数据模型
// ============================================================================
#include "FeConversion.h"
#include "DxfData.h"
#include "GeometryUtils.h"

#include <QDebug>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>
#include <unordered_map>

// ========================================================================
//  SpatialKey helpers
// ========================================================================

FeData::SpatialKey FeData::makeKey(double x, double y, double z,
                                   double cellSize) const
{
    SpatialKey key;
    if (cellSize <= 0.0) {
        key.cx = 0; key.cy = 0; key.cz = 0;
        return key;
    }
    const auto coordinate = [cellSize](double value) {
        // floor 让负坐标也稳定落入网格；先检查 finite/上下界，避免极端 DXF 坐标
        // 从 double 转 int64 时产生未定义或实现相关行为。
        using Coordinate = std::int64_t;
        const Coordinate lower = std::numeric_limits<Coordinate>::min() + 1;
        const Coordinate upper = std::numeric_limits<Coordinate>::max() - 1;
        const double scaled = std::floor(value / cellSize);
        if (!DxfNumeric::isFinite(scaled))
            return std::signbit(scaled) ? lower : upper;
        if (scaled <= static_cast<double>(lower)) return lower;
        if (scaled >= static_cast<double>(upper)) return upper;
        return static_cast<Coordinate>(scaled);
    };
    key.cx = coordinate(x);
    key.cy = coordinate(y);
    key.cz = coordinate(z);
    return key;
}

// ========================================================================
//  Spatial index (lazy rebuild)
// ========================================================================

void FeData::rebuildIndex(double tolerance) const
{
    m_index.clear();
    m_indexTolerance = tolerance;
    if (tolerance <= 0.0) return;

    const double cellSize = tolerance * 2.0;
    for (int i = 0; i < static_cast<int>(m_nodes.size()); ++i) {
        const FeNode& n = m_nodes[i];
        SpatialKey key = makeKey(n.x, n.y, n.z, cellSize);
        m_index[key].push_back(i);
    }
}

int FeData::findNearbyNode(double x, double y, double z,
                           double tolerance) const
{
    if (tolerance <= 0.0) return -1;
    if (m_nodes.empty()) return -1;

    // Rebuild index when tolerance changed or never built
    if (m_indexTolerance != tolerance || m_index.empty()) {
        rebuildIndex(tolerance);
    }

    const double tolSq = tolerance * tolerance;
    const double cellSize = tolerance * 2.0;
    SpatialKey center = makeKey(x, y, z, cellSize);

    // 候选点可能落在当前格或相邻格；三轴各 -1/0/+1，共搜索 3^3 = 27 格。
    // 这只是缩小候选集，下面仍比较距离平方，避免 sqrt 开销且保持同一阈值判断。
    for (int dx = -1; dx <= 1; ++dx) {
        for (int dy = -1; dy <= 1; ++dy) {
            for (int dz = -1; dz <= 1; ++dz) {
                SpatialKey neighbour = center;
                neighbour.cx += dx;
                neighbour.cy += dy;
                neighbour.cz += dz;

                auto it = m_index.find(neighbour);
                if (it == m_index.end()) continue;

                for (int idx : it->second) {
                    const FeNode& n = m_nodes[idx];
                    double dxv = n.x - x;
                    double dyv = n.y - y;
                    double dzv = n.z - z;
                    if (dxv * dxv + dyv * dyv + dzv * dzv <= tolSq)
                        return n.id;
                }
            }
        }
    }
    return -1;
}

// ========================================================================
//  Public API
// ========================================================================

int FeData::addOrGetNode(double x, double y, double z, double tolerance)
{
    // Check for an existing node within tolerance.
    int existing = findNearbyNode(x, y, z, tolerance);
    if (existing >= 0) {
        ++m_stats.mergedNodes;
        return existing;
    }

    FeNode node;
    node.id = static_cast<int>(m_nodes.size());  // sequential 0..N-1
    node.x  = x;
    node.y  = y;
    node.z  = z;
    m_nodes.push_back(node);

    if (tolerance > 0.0) {
        // findNearbyNode() ensures an existing non-empty data set is indexed
        // with this tolerance. The first node is the only case where no index
        // exists yet, so initialise it directly.
        if (m_indexTolerance != tolerance) {
            rebuildIndex(tolerance);
        } else {
            const double cellSize = tolerance * 2.0;
            m_index[makeKey(x, y, z, cellSize)].push_back(node.id);
        }
    } else {
        // A node inserted while merging is disabled is absent from any
        // existing positive-tolerance index. Force one rebuild if merging is
        // enabled by a later call.
        m_index.clear();
        m_indexTolerance = -1.0;
    }
    return node.id;
}

int FeData::addTruss(int startNodeId, int endNodeId)
{
    // Degenerate truss (same start/end node).
    if (startNodeId == endNodeId) {
        ++m_stats.skippedZeroLength;
        return -1;
    }

    // Truss 几何上无方向，把较小 ID 统一放前面：边 (2,5) 和 (5,2) 得到同一 key。
    int a = startNodeId;
    int b = endNodeId;
    if (a > b) std::swap(a, b);

    // Check for duplicate (same unordered pair) in expected O(1) time.
    const TrussKey key{a, b};
    if (m_trussIndex.find(key) != m_trussIndex.end()) {
        ++m_stats.skippedDuplicateTruss;
        return -1;
    }

    FeTruss truss;
    truss.id          = static_cast<int>(m_trusses.size());
    truss.startNodeId = a;
    truss.endNodeId   = b;
    m_trusses.push_back(truss);
    m_trussIndex.emplace(key);
    return truss.id;
}

void FeData::clear()
{
    m_nodes.clear();
    m_trusses.clear();
    m_trussIndex.clear();
    m_stats = FeConversionStats{};
    m_errorCode = DxfImportErrorCode::None;
    m_errorMessage.clear();
    m_index.clear();
    m_indexTolerance = -1.0;
}

void FeData::reserve(std::size_t nodeCapacity, std::size_t trussCapacity)
{
    m_nodes.reserve(nodeCapacity);
    m_trusses.reserve(trussCapacity);
    m_trussIndex.reserve(trussCapacity);
}

// ============================================================================
// FE 转换：DxfData -> FeData
// ============================================================================
namespace {
constexpr std::size_t kMaxReserveEntities = 500000;

int progressTotal(std::size_t count)
{
    return static_cast<int>(std::min(
        count, static_cast<std::size_t>(std::numeric_limits<int>::max())));
}
}

// ========================================================================
//  Helpers
// ========================================================================

DxfPoint FeConversionEngine::translate(const DxfPoint& pt,
                                        double bx, double by, double bz)
{
    return DxfPoint(pt.x() + bx, pt.y() + by, pt.z() + bz);
}

bool FeConversionEngine::reportProgress(
    const QString& stage, int current, int total) const
{
    return !m_progressCallback || m_progressCallback(stage, current, total);
}

// Feed a segment (DxfLine) into FeData — merges nodes and adds a
// truss.  Skips degenerate (same-node) and zero-length segments.
static bool feedSegment(FeData& outData,
                        const DxfPoint& start, const DxfPoint& end,
                        double nodeMergeTolerance,
                        std::size_t maxOutputEntities)
{
    int s = outData.addOrGetNode(start.x(), start.y(), start.z(),
                                  nodeMergeTolerance);
    if (outData.nodes().size() + outData.trusses().size() > maxOutputEntities)
        return false;
    int e = outData.addOrGetNode(end.x(),   end.y(),   end.z(),
                                  nodeMergeTolerance);
    if (outData.nodes().size() + outData.trusses().size() > maxOutputEntities)
        return false;
    if (s == e) {
        ++outData.stats().skippedZeroLength;
        return true;
    }
    outData.addTruss(s, e);
    return outData.nodes().size() + outData.trusses().size() <= maxOutputEntities;
}

// Feed a batch of tessellated DxfLine segments (already translated).
static bool feedSegments(FeData& outData,
                         const std::vector<DxfLine>& segments,
                         double nodeMergeTolerance,
                         std::size_t maxOutputEntities)
{
    for (const DxfLine& seg : segments) {
        if (!seg.isValid()) continue;
        if (!feedSegment(outData, seg.start(), seg.end(), nodeMergeTolerance,
                         maxOutputEntities)) return false;
    }
    return true;
}

// ========================================================================
//  convert
// ========================================================================

bool FeConversionEngine::convert(const DxfData& dxfData,
                                 double baseX, double baseY, double baseZ,
                                 double curveTolerance,
                                 double nodeMergeTolerance,
                                 FeData& outData,
                                 std::size_t maxOutputEntities) const
{
    const auto startedAt = std::chrono::steady_clock::now();
    outData.clear();

    if (!DxfNumeric::isWithinInclusive(
            curveTolerance,
            DxfImportValidation::kMinimumCurveTolerance,
            DxfImportValidation::kMaximumCurveTolerance)) {
        outData.setError(DxfImportErrorCode::InvalidArgument,
                         QStringLiteral("invalid curve tolerance"));
        qWarning() << "[FeConversionEngine] invalid curve tolerance:"
                    << curveTolerance;
        return false;
    }
    if (!DxfNumeric::isNonNegativeFinite(nodeMergeTolerance)) {
        outData.setError(DxfImportErrorCode::InvalidArgument,
                         QStringLiteral("invalid node merge tolerance"));
        qWarning() << "[FeConversionEngine] invalid node merge tolerance:"
                    << nodeMergeTolerance;
        return false;
    }
    if (!DxfNumeric::areFinite(baseX, baseY, baseZ)) {
        outData.setError(DxfImportErrorCode::InvalidArgument,
                         QStringLiteral("base coordinates must be finite"));
        qWarning() << "[FeConversionEngine] base coordinates must be finite:"
                   << baseX << baseY << baseZ;
        return false;
    }

    // Curves can tessellate into additional segments and merging can reduce
    // them again, so this is only an initial capacity hint for common inputs.
    const std::size_t entityCount = dxfData.points().size()
                                  + dxfData.lines().size()
                                  + dxfData.circles().size()
                                  + dxfData.arcs().size()
                                  + dxfData.lwPolylines().size()
                                  + dxfData.ellipses().size()
                                  + dxfData.splines().size();
    if (maxOutputEntities == 0) {
        outData.setError(DxfImportErrorCode::InvalidArgument,
                         QStringLiteral("maxOutputEntities must be greater than zero"));
        return false;
    }

    auto failLimit = [&]() {
        outData.clear();
        outData.setError(
            DxfImportErrorCode::ConversionLimit,
            QStringLiteral("FE output exceeds %1 nodes and elements")
                .arg(static_cast<qulonglong>(maxOutputEntities)));
        return false;
    };

    const std::size_t reserveHint = std::min(
        std::min(entityCount, maxOutputEntities), kMaxReserveEntities);
    outData.reserve(reserveHint, reserveHint);

    FeConversionStats& stats = outData.stats();

    const int pointTotal = progressTotal(dxfData.points().size());
    const int lineTotal = progressTotal(
        dxfData.lines().size()
        + dxfData.circles().size()
        + dxfData.arcs().size()
        + dxfData.lwPolylines().size()
        + dxfData.ellipses().size()
        + dxfData.splines().size());
    auto cancelConversion = [&]() {
        outData.clear();
        outData.setError(
            DxfImportErrorCode::Canceled,
            QStringLiteral("FE conversion canceled"));
        return false;
    };
    auto reportMergeProgress = [&](
        const QString& stage, int current, int total) {
        if (current != 0 && current % 1000 != 0 && current != total)
            return true;
        return reportProgress(stage, current, total);
    };

    // ---- POINT → standalone nodes (no truss) ----
    int pointCurrent = 0;
    if (!reportMergeProgress(
            QStringLiteral("Merging FE points"), 0, pointTotal))
        return cancelConversion();
    for (const DxfPoint& pt : dxfData.points()) {
        if (pt.isValid()) {
            DxfPoint tp = translate(pt, baseX, baseY, baseZ);
            outData.addOrGetNode(tp.x(), tp.y(), tp.z(), nodeMergeTolerance);
            if (outData.nodes().size() + outData.trusses().size() > maxOutputEntities)
                return failLimit();
            ++stats.pointsProcessed;
        }
        ++pointCurrent;
        if (!reportMergeProgress(
                QStringLiteral("Merging FE points"),
                pointCurrent, pointTotal))
            return cancelConversion();
    }

    // ---- LINE → direct truss ----
    int lineCurrent = 0;
    if (!reportMergeProgress(
            QStringLiteral("Merging FE lines"), 0, lineTotal))
        return cancelConversion();
    for (const DxfLine& line : dxfData.lines()) {
        if (line.isValid()) {
            DxfPoint s = translate(line.start(), baseX, baseY, baseZ);
            DxfPoint e = translate(line.end(),   baseX, baseY, baseZ);
            if (!feedSegment(outData, s, e, nodeMergeTolerance, maxOutputEntities))
                return failLimit();
            ++stats.linesProcessed;
        }
        ++lineCurrent;
        if (!reportMergeProgress(
                QStringLiteral("Merging FE lines"), lineCurrent, lineTotal))
            return cancelConversion();
    }

    // ---- CIRCLE → closed tessellation chain ----
    for (const DxfCircle& circle : dxfData.circles()) {
        if (circle.isValid()) {
            DxfPoint c = translate(circle.center(), baseX, baseY, baseZ);
            DxfArc equiv(c, circle.radius(), 0.0, 2.0 * M_PI, true);
            auto segments = GeometryUtils::tessellateArc(equiv, curveTolerance);
            if (!feedSegments(outData, segments, nodeMergeTolerance, maxOutputEntities))
                return failLimit();
            ++stats.circlesDiscretized;
        }
        ++lineCurrent;
        if (!reportMergeProgress(
                QStringLiteral("Merging FE lines"), lineCurrent, lineTotal))
            return cancelConversion();
    }

    // ---- ARC ----
    for (const DxfArc& arc : dxfData.arcs()) {
        if (arc.isValid()) {
            DxfPoint c = translate(arc.center(), baseX, baseY, baseZ);
            DxfArc shifted(c, arc.radius(),
                           arc.startAngle(), arc.endAngle(), arc.isCCW());
            if (!feedSegments(outData,
                              GeometryUtils::tessellateArc(shifted, curveTolerance),
                              nodeMergeTolerance, maxOutputEntities))
                return failLimit();
            ++stats.arcsDiscretized;
        }
        ++lineCurrent;
        if (!reportMergeProgress(
                QStringLiteral("Merging FE lines"), lineCurrent, lineTotal))
            return cancelConversion();
    }

    // ---- LWPolyline ----
    for (const DxfLWPolyline& poly : dxfData.lwPolylines()) {
        if (poly.isValid()) {
            std::vector<DxfPoint> verts;
            for (const DxfPoint& v : poly.vertices())
                verts.push_back(translate(v, baseX, baseY, baseZ));
            DxfLWPolyline shifted(verts, poly.bulges(),
                                  poly.isClosed(), poly.constZ() + baseZ);
            if (!feedSegments(outData,
                              GeometryUtils::tessellateLWPolyline(shifted,
                                                                  curveTolerance),
                              nodeMergeTolerance, maxOutputEntities))
                return failLimit();
            ++stats.lwPolylinesDiscretized;
        }
        ++lineCurrent;
        if (!reportMergeProgress(
                QStringLiteral("Merging FE lines"), lineCurrent, lineTotal))
            return cancelConversion();
    }

    // ---- Ellipse ----
    for (const DxfEllipse& ellipse : dxfData.ellipses()) {
        if (ellipse.isValid()) {
            const DxfPoint c = translate(ellipse.center(), baseX, baseY, baseZ);
            // DXF group 11/21/31 is a vector relative to the center. Translation
            // applies to the center only; subtracting the translated center from
            // this vector corrupts both its direction and length.
            DxfEllipse shifted(c, ellipse.majorAxisEnd(),
                               ellipse.ratio(),
                               ellipse.startParam(), ellipse.endParam(),
                               ellipse.isCCW());
            if (!feedSegments(outData,
                              GeometryUtils::tessellateEllipse(shifted,
                                                                curveTolerance),
                              nodeMergeTolerance, maxOutputEntities))
                return failLimit();
            ++stats.ellipsesDiscretized;
        }
        ++lineCurrent;
        if (!reportMergeProgress(
                QStringLiteral("Merging FE lines"), lineCurrent, lineTotal))
            return cancelConversion();
    }

    // ---- Spline ----
    for (const DxfSpline& spline : dxfData.splines()) {
        if (spline.isValid()) {
            std::vector<DxfPoint> ctrlPts;
            for (const DxfPoint& p : spline.controlPoints())
                ctrlPts.push_back(translate(p, baseX, baseY, baseZ));
            std::vector<DxfPoint> fitPts;
            for (const DxfPoint& p : spline.fitPoints())
                fitPts.push_back(translate(p, baseX, baseY, baseZ));
            DxfSpline shifted(ctrlPts, spline.knots(), spline.weights(),
                              fitPts, spline.degree(), spline.flags(),
                              spline.tgStartX(), spline.tgStartY(), spline.tgStartZ(),
                              spline.tgEndX(),   spline.tgEndY(),   spline.tgEndZ());
            if (!feedSegments(outData,
                              GeometryUtils::tessellateSpline(shifted, curveTolerance),
                              nodeMergeTolerance, maxOutputEntities))
                return failLimit();
            ++stats.splinesDiscretized;
        }
        ++lineCurrent;
        if (!reportMergeProgress(
                QStringLiteral("Merging FE lines"), lineCurrent, lineTotal))
            return cancelConversion();
    }

    stats.totalInputEntities = stats.pointsProcessed
                             + stats.linesProcessed
                             + stats.circlesDiscretized
                             + stats.arcsDiscretized
                             + stats.lwPolylinesDiscretized
                             + stats.ellipsesDiscretized
                             + stats.splinesDiscretized;

    const bool hasNodes = !outData.nodes().empty();
    if (!hasNodes) {
        outData.setError(DxfImportErrorCode::ConversionFailed,
                         QStringLiteral("no valid FE entities to convert"));
    }
    qInfo().noquote() << "[FeConversionEngine] convert:"
                      << stats.totalInputEntities << "entities ->"
                      << outData.nodes().size() << "nodes,"
                      << outData.trusses().size() << "trusses in"
                      << std::chrono::duration_cast<std::chrono::milliseconds>(
                             std::chrono::steady_clock::now() - startedAt).count()
                      << "ms";
    return hasNodes;
}

// ============================================================================
// FE 事务顺序（纯 C++）
// ============================================================================

namespace DxfImportBuildService {

ImportBuildResult buildFePart(
    FeData& feData,
    const QString& modelName,
    const QString& partName,
    IFeImportBuilder& builder)
{
    // Truss 引用节点，所以 FE 必须先完整创建 nodes，再创建 trusses，最后 commit。
    ImportBuildResult result = builder.beginImport(modelName, partName);
    if (!result.succeeded())
        return result;

    result = builder.createNodes(feData.nodes());
    if (!result.succeeded())
        return DxfImportBuildDetail::rollbackAfterFailure(builder, result);
    const int nodeCount = result.createdCount;

    result = builder.createTrusses(feData.trusses());
    if (!result.succeeded())
        return DxfImportBuildDetail::rollbackAfterFailure(builder, result, nodeCount);
    const int trussCount = result.createdCount;

    result = builder.commit();
    if (!result.succeeded())
        return DxfImportBuildDetail::rollbackAfterFailure(builder, result);

    return ImportBuildResult::success(nodeCount + trussCount);
}

} // namespace DxfImportBuildService
