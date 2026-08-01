#pragma once

#include "DxfData.h"

/// Complete affine transform from block-local coordinates to world space:
///   x' = m00*x + m01*y + tx
///   y' = m10*x + m11*y + ty
///   z' = scaleZ*z + offsetZ
///
/// Transforms compose without decomposing back into scale and rotation, so
/// nested shear and mirror combinations retain their exact affine semantics.
struct Transform2D
{
    double m00 = 1.0;
    double m01 = 0.0;
    double m10 = 0.0;
    double m11 = 1.0;
    double tx = 0.0;
    double ty = 0.0;
    double scaleZ = 1.0;
    double offsetZ = 0.0;

    static Transform2D fromInsert(const InsertInfo& insert,
                                  double baseX,
                                  double baseY,
                                  double baseZ);

    /// Return this x child: child is applied first, then this transform.
    Transform2D composedWith(const Transform2D& child) const;

    DxfPoint apply(double x, double y, double z) const;
    DxfPoint apply(const DxfPoint& point) const;
    DxfPoint applyVector(double x, double y, double z = 0.0) const;
    DxfPoint applyVector(const DxfPoint& vector) const;

    double determinant() const;
    bool reversesOrientation() const;
    bool isPlanarSimilarity() const;
    double planarScale() const;
    double applyAngle(double angle) const;
    double applyZ(double z) const;
};
