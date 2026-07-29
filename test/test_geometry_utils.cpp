/**
 * test_geometry_utils.cpp — 测试 GeometryUtils 纯数学函数
 *
 * 覆盖: calculateArcSegmentCount / tessellateBulgeArc / normalizeSweep
 */
#include <gtest/gtest.h>
#include <cmath>
#include "GeometryUtils.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

using namespace GeometryUtils;

// ========================================================================
//  normalizeSweep
// ========================================================================

TEST(NormalizeSweep, CCW_positive_stays_positive) {
    EXPECT_GT(normalizeSweep(1.0, true), 0.0);
}

TEST(NormalizeSweep, CCW_zero_becomes_2pi) {
    double s = normalizeSweep(0.0, true);
    EXPECT_NEAR(s, 2.0 * M_PI, 1e-9);
}

TEST(NormalizeSweep, CCW_negative_becomes_positive) {
    double s = normalizeSweep(-1.0, true);
    EXPECT_GT(s, 0.0);
    EXPECT_LT(s, 2.0 * M_PI + 0.1);
}

TEST(NormalizeSweep, CCW_full_circle_preserved) {
    double s = normalizeSweep(2.0 * M_PI, true);
    EXPECT_NEAR(s, 2.0 * M_PI, 1e-9);
}

TEST(NormalizeSweep, CW_negative_stays_negative) {
    EXPECT_LT(normalizeSweep(-1.0, false), 0.0);
}

TEST(NormalizeSweep, CW_zero_becomes_minus_2pi) {
    double s = normalizeSweep(0.0, false);
    EXPECT_NEAR(s, -2.0 * M_PI, 1e-9);
}

TEST(NormalizeSweep, CW_positive_becomes_negative) {
    double s = normalizeSweep(1.0, false);
    EXPECT_LT(s, 0.0);
}

TEST(NormalizeSweep, CW_full_circle_preserved) {
    double s = normalizeSweep(-2.0 * M_PI, false);
    EXPECT_NEAR(s, -2.0 * M_PI, 1e-9);
}

// ========================================================================
//  calculateArcSegmentCount
// ========================================================================

TEST(CalcArcSegCount, normal_arc) {
    int n = calculateArcSegmentCount(100.0, M_PI / 2.0, 0.01);
    EXPECT_GE(n, 2);
    EXPECT_LE(n, 10000);
}

TEST(CalcArcSegCount, full_circle) {
    int n = calculateArcSegmentCount(100.0, 2.0 * M_PI, 0.01);
    EXPECT_GE(n, 2);
    EXPECT_LE(n, 10000);
}

TEST(CalcArcSegCount, min_return_is_2) {
    // Very large radius with tiny sweep → ceil(0.0001 / maxAngle) = 1,
    // but clamped to minimum 2
    int n = calculateArcSegmentCount(1e6, 1e-6, 0.01);
    EXPECT_EQ(n, 2);
}

TEST(CalcArcSegCount, tolerance_too_large_returns_2) {
    // tolerance >= radius → returns 2
    int n = calculateArcSegmentCount(0.005, M_PI, 0.01);
    EXPECT_EQ(n, 2);
}

TEST(CalcArcSegCount, zero_radius_returns_1) {
    int n = calculateArcSegmentCount(0.0, M_PI, 0.01);
    EXPECT_EQ(n, 1);
}

TEST(CalcArcSegCount, zero_sweep_returns_1) {
    int n = calculateArcSegmentCount(100.0, 0.0, 0.01);
    EXPECT_EQ(n, 1);
}

TEST(CalcArcSegCount, negative_tolerance_fallback) {
    int n = calculateArcSegmentCount(100.0, M_PI, -0.01);
    // Negative tolerance → fallback to 1e-6 and still valid
    EXPECT_GE(n, 2);
}

// ========================================================================
//  tessellateBulgeArc
// ========================================================================

TEST(TessellateBulgeArc, straight_line_bulge_zero) {
    DxfPoint p0(0, 0, 0);
    DxfPoint p1(10, 0, 0);
    auto pts = tessellateBulgeArc(p0, p1, 0.0);
    ASSERT_EQ(pts.size(), 2u);
    EXPECT_DOUBLE_EQ(pts[0].x(), 0.0);
    EXPECT_DOUBLE_EQ(pts[1].x(), 10.0);
}

TEST(TessellateBulgeArc, straight_line_tiny_bulge) {
    DxfPoint p0(0, 0, 0);
    DxfPoint p1(10, 0, 0);
    auto pts = tessellateBulgeArc(p0, p1, 1e-14);
    ASSERT_EQ(pts.size(), 2u);
}

TEST(TessellateBulgeArc, zero_chord_returns_two_points) {
    DxfPoint p0(5, 5, 0);
    DxfPoint p1(5, 5, 0);
    auto pts = tessellateBulgeArc(p0, p1, 0.5);
    // chord == 0 → returns {p0, p1} unchanged
    ASSERT_EQ(pts.size(), 2u);
}

TEST(TessellateBulgeArc, quarter_circle_bulge) {
    // 90° arc: bulge = tan(π/8) ≈ 0.4142
    DxfPoint p0(10, 0, 0);
    DxfPoint p1(0, 10, 0);
    double bulge = std::tan(M_PI / 8.0);
    auto pts = tessellateBulgeArc(p0, p1, bulge);
    ASSERT_GE(pts.size(), 3u);  // at least p0, mid, p1 (min=2 segments)
    // endpoints preserved
    EXPECT_DOUBLE_EQ(pts.front().x(), 10.0);
    EXPECT_DOUBLE_EQ(pts.front().y(), 0.0);
    EXPECT_DOUBLE_EQ(pts.back().x(), 0.0);
    EXPECT_DOUBLE_EQ(pts.back().y(), 10.0);
}

TEST(TessellateBulgeArc, semicircle) {
    // 180° arc: bulge = tan(π/4) = 1.0
    DxfPoint p0(-10, 0, 0);
    DxfPoint p1(10, 0, 0);
    auto pts = tessellateBulgeArc(p0, p1, 1.0);
    ASSERT_GE(pts.size(), 3u);
    // Midpoint should be near (0, 10) for CCW semicircle
    // Verify endpoints preserved
    EXPECT_NEAR(pts.front().x(), -10.0, 1e-6);
    EXPECT_NEAR(pts.back().x(), 10.0, 1e-6);
}

TEST(TessellateBulgeArc, endpoints_snapped) {
    DxfPoint p0(1.234, 5.678, 0);
    DxfPoint p1(9.876, 5.432, 0);
    double bulge = 0.5;
    auto pts = tessellateBulgeArc(p0, p1, bulge);
    // Endpoints are exactly snapped to input values
    EXPECT_DOUBLE_EQ(pts.front().x(), p0.x());
    EXPECT_DOUBLE_EQ(pts.front().y(), p0.y());
    EXPECT_DOUBLE_EQ(pts.back().x(), p1.x());
    EXPECT_DOUBLE_EQ(pts.back().y(), p1.y());
}

TEST(TessellateBulgeArc, z_interpolation_linear) {
    DxfPoint p0(0, 0, 0);
    DxfPoint p1(10, 0, 10);
    double bulge = 0.5;
    auto pts = tessellateBulgeArc(p0, p1, bulge);
    ASSERT_GE(pts.size(), 3u);
    EXPECT_DOUBLE_EQ(pts.front().z(), 0.0);
    EXPECT_DOUBLE_EQ(pts.back().z(), 10.0);
    // Intermediate point should have interpolated Z
    EXPECT_GT(pts[1].z(), 0.0);
    EXPECT_LT(pts[1].z(), 10.0);
}

// ========================================================================
//  boundary / edge cases
// ========================================================================

TEST(TessellateBulgeArc, cw_negative_bulge) {
    // CW 90° arc: bulge = tan(-π/8) ≈ -0.4142
    DxfPoint p0(0, 10, 0);
    DxfPoint p1(10, 0, 0);
    double bulge = std::tan(-M_PI / 8.0);
    auto pts = tessellateBulgeArc(p0, p1, bulge);
    ASSERT_GE(pts.size(), 3u);
    // Endpoints preserved
    EXPECT_DOUBLE_EQ(pts.front().x(), 0.0);
    EXPECT_DOUBLE_EQ(pts.front().y(), 10.0);
    EXPECT_DOUBLE_EQ(pts.back().x(), 10.0);
    EXPECT_DOUBLE_EQ(pts.back().y(), 0.0);
}

TEST(TessellateBulgeArc, large_bulge_near_semicircle) {
    // bulge = tan(3π/8) ≈ 2.414 → 270° CCW
    DxfPoint p0(-10, 0, 0);
    DxfPoint p1(0, -10, 0);
    double bulge = std::tan(3.0 * M_PI / 8.0);
    auto pts = tessellateBulgeArc(p0, p1, bulge, 0.01);
    ASSERT_GE(pts.size(), 3u);
}

TEST(TessellateBulgeArc, zero_tolerance_fallback) {
    DxfPoint p0(10, 0, 0);
    DxfPoint p1(0, 10, 0);
    double bulge = std::tan(M_PI / 8.0);
    auto pts = tessellateBulgeArc(p0, p1, bulge, 0.0);
    // tolerance=0 → fallback to 0.01, still produces segments
    ASSERT_GE(pts.size(), 2u);
}

TEST(CalcArcSegCount, negative_radius_returns_1) {
    int n = calculateArcSegmentCount(-100.0, M_PI, 0.01);
    EXPECT_EQ(n, 1);
}

TEST(CalcArcSegCount, at_max_angle_boundary) {
    // sweep = π/4 exactly → should produce ceil(1.0) = 1, clamped to 2
    int n = calculateArcSegmentCount(1.0, M_PI / 4.0, 0.01);
    EXPECT_GE(n, 2);
}

TEST(CalcArcSegCount, extreme_radius_small_tolerance) {
    // radius=100000, tolerance=0.001 → many segments
    int n = calculateArcSegmentCount(100000.0, 2.0 * M_PI, 0.001);
    EXPECT_GT(n, 2);
    EXPECT_LE(n, 10000);
}

TEST(NormalizeSweep, ccw_multiple_turns_preserved) {
    double s = normalizeSweep(4.0 * M_PI, true);
    EXPECT_NEAR(s, 4.0 * M_PI, 1e-9);
}

TEST(NormalizeSweep, cw_multiple_turns_preserved) {
    double s = normalizeSweep(-4.0 * M_PI, false);
    EXPECT_NEAR(s, -4.0 * M_PI, 1e-9);
}

TEST(NormalizeSweep, ccw_exactly_pi_preserved) {
    double s = normalizeSweep(M_PI, true);
    EXPECT_NEAR(s, M_PI, 1e-9);
}

TEST(NormalizeSweep, cw_exactly_minus_pi_preserved) {
    double s = normalizeSweep(-M_PI, false);
    EXPECT_NEAR(s, -M_PI, 1e-9);
}
