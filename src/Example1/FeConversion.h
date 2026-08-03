#pragma once

// ============================================================================
// FE 中间数据 FeData
// ============================================================================
#ifndef FeData_h
#define FeData_h

#include <cstddef>
#include <cstdint>
#include <QString>
#include "DxfData.h"
#include <unordered_map>
#include <unordered_set>
#include <vector>

// A single finite-element node with a unique ID within FeData.
struct FeNode {
    int    id = -1;
    double x  = 0.0;
    double y  = 0.0;
    double z  = 0.0;
};

// A truss (2-node tension/compression-only bar element).
struct FeTruss {
    int id          = -1;
    int startNodeId = -1;
    int endNodeId   = -1;
};

// Statistics gathered during FE conversion.
struct FeConversionStats {
    std::size_t totalInputEntities   = 0;   // entities fed to the converter
    std::size_t pointsProcessed      = 0;
    std::size_t linesProcessed       = 0;
    std::size_t circlesDiscretized   = 0;
    std::size_t arcsDiscretized      = 0;
    std::size_t lwPolylinesDiscretized = 0;
    std::size_t ellipsesDiscretized  = 0;
    std::size_t splinesDiscretized   = 0;
    std::size_t mergedNodes          = 0;   // nodes absorbed by merge tolerance
    std::size_t skippedZeroLength    = 0;   // segments / edge cases skipped
    std::size_t skippedDuplicateTruss = 0;  // trusses identical to an existing one
};

/**
 * @brief FE 转换结果及其节点合并/Truss 去重索引。
 *
 * 它不依赖 SAM SDK，可独立测试。节点用三维空间哈希避免每加入一点都扫描全部旧节点；
 * Truss 用无序集合按“无方向节点对”去重。ID 始终是从 0 开始的连续 vector 下标，正好
 * 对应 Python Builder 后续用 `part.nodes[id]` 引用节点。
 */
class FeData {
public:
    FeData() = default;

    // --- read-only access ---
    const std::vector<FeNode>&  nodes()  const { return m_nodes; }
    const std::vector<FeTruss>& trusses() const { return m_trusses; }

    // --- modifiers ---

    // Add / look-up a node.  When a node already exists within
    // `tolerance` (Euclidean distance) the existing id is returned
    // and the new coordinate is NOT inserted.
    int addOrGetNode(double x, double y, double z, double tolerance);

    // Add a truss element.  Returns the new truss id, or -1 if the
    // truss would be degenerate (same start/end node) or would
    // duplicate an existing truss (same unordered node pair).
    int addTruss(int startNodeId, int endNodeId);

    // Clear all nodes, trusses, and stats.
    void clear();

    // Preallocate storage when the input-size estimate is known.  This is an
    // optimization hint only; node merging and curve tessellation may make
    // the final counts lower or higher than these values.
    void reserve(std::size_t nodeCapacity, std::size_t trussCapacity);

    // --- statistics ---
    const FeConversionStats& stats() const { return m_stats; }
    FeConversionStats&       stats()       { return m_stats; }
    DxfImportErrorCode errorCode() const { return m_errorCode; }
    const QString& errorMessage() const { return m_errorMessage; }
    void setError(DxfImportErrorCode code, const QString& message) {
        m_errorCode = code;
        m_errorMessage = message;
    }

private:
    // 把连续三维坐标量化成整数网格坐标，作为 unordered_map 的键。
    // 平均查找接近 O(1)，但最终仍用真实欧氏距离确认，网格本身不决定合并。
    struct SpatialKey {
        std::int64_t cx = 0, cy = 0, cz = 0;
        bool operator==(const SpatialKey& o) const {
            return cx == o.cx && cy == o.cy && cz == o.cz;
        }
    };
    struct SpatialKeyHash {
        std::size_t operator()(const SpatialKey& k) const {
            // simple triple-int hash (boost-compatible)
            std::size_t h = static_cast<std::size_t>(k.cx);
            h ^= static_cast<std::size_t>(k.cy) * 0x9e3779b9ULL;
            h ^= static_cast<std::size_t>(k.cz) * 0x9e3779b97f4a7c15ULL;
            return h;
        }
    };

    struct TrussKey {
        int first = -1;
        int second = -1;
        bool operator==(const TrussKey& o) const {
            return first == o.first && second == o.second;
        }
    };
    struct TrussKeyHash {
        std::size_t operator()(const TrussKey& k) const {
            std::size_t h = static_cast<std::size_t>(k.first);
            h ^= static_cast<std::size_t>(k.second) * 0x9e3779b97f4a7c15ULL;
            return h;
        }
    };

    SpatialKey makeKey(double x, double y, double z, double cellSize) const;

    // Find an existing node whose distance to (x,y,z) <= tolerance.
    // Returns its id, or -1 when none found.
    int findNearbyNode(double x, double y, double z, double tolerance) const;

    std::vector<FeNode>  m_nodes;
    std::vector<FeTruss> m_trusses;
    std::unordered_set<TrussKey, TrussKeyHash> m_trussIndex;
    FeConversionStats    m_stats;
    DxfImportErrorCode   m_errorCode = DxfImportErrorCode::None;
    QString              m_errorMessage;

    // Spatial index: key → vector of node indices.
    // Updated incrementally while tolerance is unchanged. Rebuilt only when
    // tolerance changes or after nodes were inserted with merging disabled.
    mutable double m_indexTolerance = -1.0;
    mutable std::unordered_map<SpatialKey, std::vector<int>, SpatialKeyHash> m_index;
    void rebuildIndex(double tolerance) const;
};

#endif // FeData_h

// ============================================================================
// FE 纯转换器
// ============================================================================
#ifndef FeConversionEngine_h
#define FeConversionEngine_h

#include <functional>

// Converts DxfData (parsed DXF geometry) into FeData (nodes + truss
// elements).  No SAM SDK dependency.
//
//   - POINT       → FeNode (no connectivity)
//   - LINE        → 2 nodes + 1 truss
//   - CIRCLE      → closed tessellation chain → trusses
//   - ARC         → tessellated → trusses
//   - LWPolyline  → tessellated → trusses
//   - Ellipse     → tessellated → trusses
//   - Spline      → tessellated → trusses
//
// All coordinates are offset by (baseX, baseY, baseZ) before node
// insertion, matching the convention used by ConversionEngine
// (output = DXF + base).
class FeConversionEngine {
public:
    using ProgressCallback = std::function<bool(
        const QString& stage, int current, int total)>;

    FeConversionEngine() = default;

    void setProgressCallback(const ProgressCallback& callback) {
        m_progressCallback = callback;
    }

    // Convert a parsed DXF dataset into nodes and truss elements.
    //
    // @param dxfData              parsed, expanded DXF data
    // @param baseX/Y/Z            coordinate offset
    // @param curveTolerance       chord-height tolerance for curve
    //                             tessellation
    // @param nodeMergeTolerance   maximum Euclidean distance for
    //                             merging distinct nodes into one
    // @param outData              output container (cleared first)
    //
    // @return true when at least one node was produced; false
    //         otherwise (empty or invalid input).
    bool convert(const DxfData& dxfData,
                 double baseX, double baseY, double baseZ,
                  double curveTolerance,
                  double nodeMergeTolerance,
                  FeData& outData,
                  std::size_t maxOutputEntities = 100000) const;

    // Convenience default node-merge tolerance (1e-6).
    static double defaultNodeMergeTolerance()
    {
        return DxfImportDefaults::kNodeMergeTolerance;
    }

private:
    bool reportProgress(
        const QString& stage, int current, int total) const;

    static DxfPoint translate(const DxfPoint& pt,
                              double bx, double by, double bz);

    ProgressCallback m_progressCallback;
};

#endif // FeConversionEngine_h

// ============================================================================
// FE Builder 抽象接口
// ============================================================================
class IFeImportBuilder
{
public:
    virtual ~IFeImportBuilder() = default;

    virtual ImportBuildResult beginImport(
        const QString& modelName, const QString& partName) = 0;
    virtual ImportBuildResult createNodes(
        const std::vector<FeNode>& nodes) = 0;
    virtual ImportBuildResult createTrusses(
        const std::vector<FeTruss>& trusses) = 0;
    virtual ImportBuildResult commit() = 0;
    virtual ImportBuildResult rollback() = 0;
};

namespace DxfImportBuildService {

ImportBuildResult buildFePart(
    FeData& feData,
    const QString& modelName,
    const QString& partName,
    IFeImportBuilder& builder);

} // namespace DxfImportBuildService
