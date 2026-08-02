#include "FeData.h"
#include "DxfNumeric.h"
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

    // Search the 27 neighbouring cells (±1 in each axis)
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

    // Normalise: store smaller id first so unordered pair comparison
    // works consistently.
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
