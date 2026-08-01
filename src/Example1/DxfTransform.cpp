#include "DxfTransform.h"

#include <algorithm>
#include <cmath>

Transform2D Transform2D::fromInsert(const InsertInfo& insert,
                                    double baseX,
                                    double baseY,
                                    double baseZ)
{
    Transform2D transform;
    const double cosAngle = std::cos(insert.angle);
    const double sinAngle = std::sin(insert.angle);
    transform.m00 = cosAngle * insert.scaleX;
    transform.m01 = -sinAngle * insert.scaleY;
    transform.m10 = sinAngle * insert.scaleX;
    transform.m11 = cosAngle * insert.scaleY;
    transform.tx = insert.insertX
        - transform.m00 * baseX - transform.m01 * baseY;
    transform.ty = insert.insertY
        - transform.m10 * baseX - transform.m11 * baseY;
    transform.scaleZ = insert.scaleZ;
    transform.offsetZ = insert.insertZ - insert.scaleZ * baseZ;
    return transform;
}

Transform2D Transform2D::composedWith(const Transform2D& child) const
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

DxfPoint Transform2D::apply(double x, double y, double z) const
{
    return DxfPoint(m00 * x + m01 * y + tx,
                    m10 * x + m11 * y + ty,
                    scaleZ * z + offsetZ);
}

DxfPoint Transform2D::apply(const DxfPoint& point) const
{
    return apply(point.x(), point.y(), point.z());
}

DxfPoint Transform2D::applyVector(double x, double y, double z) const
{
    return DxfPoint(m00 * x + m01 * y,
                    m10 * x + m11 * y,
                    scaleZ * z);
}

DxfPoint Transform2D::applyVector(const DxfPoint& vector) const
{
    return applyVector(vector.x(), vector.y(), vector.z());
}

double Transform2D::determinant() const
{
    return m00 * m11 - m01 * m10;
}

bool Transform2D::reversesOrientation() const
{
    return determinant() < 0.0;
}

bool Transform2D::isPlanarSimilarity() const
{
    const double firstLength2 = m00 * m00 + m10 * m10;
    const double secondLength2 = m01 * m01 + m11 * m11;
    const double dot = m00 * m01 + m10 * m11;
    const double scale = std::max({1.0, firstLength2, secondLength2});
    return firstLength2 > 1e-24
        && std::fabs(firstLength2 - secondLength2) <= 1e-9 * scale
        && std::fabs(dot) <= 1e-9 * scale;
}

double Transform2D::planarScale() const
{
    return std::sqrt(m00 * m00 + m10 * m10);
}

double Transform2D::applyAngle(double angle) const
{
    const double rotation = std::atan2(m10, m00);
    return reversesOrientation() ? rotation - angle : rotation + angle;
}

double Transform2D::applyZ(double z) const
{
    return scaleZ * z + offsetZ;
}
