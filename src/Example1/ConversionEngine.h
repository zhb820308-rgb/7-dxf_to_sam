#ifndef ConversionEngine_h
#define ConversionEngine_h

#include "DxfData.h"
#include "SamData.h"

// Geometry conversion engine.
//   - filters invalid entities (zero-length lines, zero-radius circles, ...)
//   - translates all coordinates by the user-specified base point
//   - discretizes arc / polyline bulge / ellipse into line segments via GeometryUtils
//   - output SamData is ready for skcGeomFactory
class ConversionEngine {
public:
    ConversionEngine() = default;

    bool convert(const DxfData& dxfData,
                 double baseX, double baseY, double baseZ,
                 SamData& outData);

    static double defaultBulgeTolerance() { return 0.01; }

private:
    static DxfPoint translate(const DxfPoint& pt,
                              double bx, double by, double bz);

    // 将曲线实体离散化为线段列表（未平移）
    static std::vector<DxfLine> tessellateArc(const DxfArc& arc,
                                              double tolerance);
    static std::vector<DxfLine> tessellateLWPolyline(const DxfLWPolyline& poly,
                                                     double tolerance);
    static std::vector<DxfLine> tessellateEllipse(const DxfEllipse& ellipse,
                                                  double tolerance);
};

#endif
