#ifndef FeData_h
#define FeData_h

#include <cstddef>
#include <QString>
#include "DxfImportError.h"
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

// Container that holds nodes and truss elements produced by
// FeConversionEngine.  Owns node merging (incremental 3-D spatial hash +
// merge tolerance), element deduplication, and ID assignment.
//
// No SAM SDK dependency — suitable for unit testing.
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
    // 3-D spatial hash for O(1) approximate look-up of nearby nodes.
    // Cell size = tolerance × 2 (conservative).
    struct SpatialKey {
        int cx = 0, cy = 0, cz = 0;
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
