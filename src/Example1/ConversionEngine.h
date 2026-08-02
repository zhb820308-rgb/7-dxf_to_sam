#ifndef ConversionEngine_h
#define ConversionEngine_h

#include "DxfData.h"
#include "DxfImportDefaults.h"
#include "SamData.h"
#include <cstddef>

/// @brief Geometry conversion engine.
///
/// Filters invalid entities (zero-length lines, zero-radius circles, etc.),
/// translates all coordinates by the user-specified base point,
/// delegates curve discretization to GeometryUtils,
/// and outputs SamData ready for skcGeomFactory.
class ConversionEngine {
public:
    ConversionEngine() = default;

    /// @brief Convert DXF entities to SAM-ready geometry.
    /// @param dxfData   Source DXF entity container.
    /// @param baseX     Translation offset X.
    /// @param baseY     Translation offset Y.
    /// @param baseZ     Translation offset Z.
    /// @param tolerance Curve tessellation sagitta tolerance.
    /// @param outData   [out] Resulting SAM-compatible entity container.
    /// @return true if at least one valid entity was converted.
    bool convert(const DxfData& dxfData,
                 double baseX, double baseY, double baseZ,
                 double tolerance,
                 SamData& outData,
                 std::size_t maxOutputEntities = 100000) const;

    /// @brief Default bulge/chord-height tolerance for curve tessellation.
    static double defaultBulgeTolerance()
    {
        return DxfImportDefaults::kCurveTolerance;
    }

private:
    /// @brief Translate a point by the given offsets.
    static DxfPoint translate(const DxfPoint& pt,
                              double bx, double by, double bz);
};

#endif
