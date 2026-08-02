#ifndef FeConversionEngine_h
#define FeConversionEngine_h

#include "DxfData.h"
#include "DxfImportDefaults.h"
#include "FeData.h"
#include <cstddef>
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
