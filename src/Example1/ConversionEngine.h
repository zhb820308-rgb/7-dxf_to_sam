#ifndef ConversionEngine_h
#define ConversionEngine_h

#include "DxfData.h"
#include "SamData.h"

// Geometry conversion engine.
//   - filters invalid entities (zero-length lines, zero-radius circles, ...)
//   - translates all coordinates by the user-specified base point
//   - delegates curve discretization to GeometryUtils
//   - output SamData is ready for skcGeomFactory
class ConversionEngine {
public:
    ConversionEngine() = default;

    bool convert(const DxfData& dxfData,
                 double baseX, double baseY, double baseZ,
                 double tolerance,
                 SamData& outData) const;

    static double defaultBulgeTolerance() { return 0.01; }

private:
    static DxfPoint translate(const DxfPoint& pt,
                              double bx, double by, double bz);
};

#endif
