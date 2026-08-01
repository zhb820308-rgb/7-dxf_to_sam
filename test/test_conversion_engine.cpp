/**
 * test_conversion_engine.cpp — Tests tessellation and translation
 *
 * Covers: GeometryUtils::tessellateArc / tessellateLWPolyline / tessellateEllipse
 *       + ConversionEngine::translate
 */
#include <gtest/gtest.h>
#include <cmath>
#include "ConversionEngine.h"
#include "GeometryUtils.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ========================================================================
//  tessellateArc
// ========================================================================

TEST(TessellateArc, quarter_arc_produces_segments) {
    DxfArc arc(DxfPoint(0, 0, 0), 100.0, 0.0, M_PI / 2.0, true);
    auto lines = GeometryUtils::tessellateArc(arc, 0.01);
    ASSERT_GE(lines.size(), 2u);  // at least 2 segments
    // First line starts at angle 0
    EXPECT_NEAR(lines.front().start().x(), 100.0, 1e-6);
    EXPECT_NEAR(lines.front().start().y(), 0.0, 1e-6);
    // Last line ends at angle π/2
    EXPECT_NEAR(lines.back().end().x(), 0.0, 1e-6);
    EXPECT_NEAR(lines.back().end().y(), 100.0, 1e-6);
}

TEST(TessellateArc, full_circle_arc) {
    DxfArc arc(DxfPoint(0, 0, 0), 10.0, 0.0, 2.0 * M_PI, true);
    auto lines = GeometryUtils::tessellateArc(arc, 0.01);
    // Should produce segments around the full circle
    ASSERT_GE(lines.size(), 2u);
    // Total length of lines should approximate the circle perimeter
    double totalLength = 0.0;
    for (const auto& seg : lines) {
        double dx = seg.end().x() - seg.start().x();
        double dy = seg.end().y() - seg.start().y();
        totalLength += std::sqrt(dx * dx + dy * dy);
    }
    double circumference = 2.0 * M_PI * 10.0;
    EXPECT_NEAR(totalLength, circumference, circumference * 0.05);  // within 5%
}

TEST(TessellateArc, cw_arc) {
    DxfArc arc(DxfPoint(0, 0, 0), 100.0, 0.0, -M_PI / 2.0, false);
    auto lines = GeometryUtils::tessellateArc(arc, 0.01);
    ASSERT_GE(lines.size(), 2u);
    // CW from 0 to -π/2: end should be at (0, -100)
    EXPECT_NEAR(lines.back().end().x(), 0.0, 1e-6);
    EXPECT_NEAR(lines.back().end().y(), -100.0, 1e-6);
}

TEST(TessellateArc, respect_tolerance) {
    DxfArc arc(DxfPoint(0, 0, 0), 10.0, 0.0, M_PI, true);
    auto fine   = GeometryUtils::tessellateArc(arc, 0.001);
    auto coarse = GeometryUtils::tessellateArc(arc, 1.0);
    // Tighter tolerance → more segments
    EXPECT_GT(fine.size(), coarse.size());
}

// ========================================================================
//  tessellateEllipse
// ========================================================================

TEST(TessellateEllipse, half_ellipse) {
    DxfEllipse e(DxfPoint(0, 0, 0), DxfPoint(100, 0, 0),
                 0.5, 0.0, M_PI, true);
    auto lines = GeometryUtils::tessellateEllipse(e, 0.01);
    ASSERT_GE(lines.size(), 2u);
    // Start at (100, 0)
    EXPECT_NEAR(lines.front().start().x(), 100.0, 1e-6);
    EXPECT_NEAR(lines.front().start().y(), 0.0, 1e-6);
}

TEST(TessellateEllipse, full_ellipse) {
    DxfEllipse e(DxfPoint(0, 0, 0), DxfPoint(100, 0, 0),
                 1.0, 0.0, 2.0 * M_PI, true);
    auto lines = GeometryUtils::tessellateEllipse(e, 0.01);
    ASSERT_GE(lines.size(), 2u);
    // Full ellipse should close back to start point (±tolerance)
    double endX = lines.back().end().x();
    double endY = lines.back().end().y();
    EXPECT_NEAR(endX, 100.0, 1e-4);
    EXPECT_NEAR(endY, 0.0, 1e-4);
}

TEST(TessellateEllipse, cw_ellipse) {
    DxfEllipse e(DxfPoint(0, 0, 0), DxfPoint(100, 0, 0),
                 1.0, 0.0, -M_PI, false);
    auto lines = GeometryUtils::tessellateEllipse(e, 0.01);
    ASSERT_GE(lines.size(), 2u);
}

// ========================================================================
//  tessellateLWPolyline
// ========================================================================

TEST(TessellateLWPolyline, straight_segments) {
    std::vector<DxfPoint> verts = {
        DxfPoint(0, 0, 0), DxfPoint(10, 0, 0), DxfPoint(10, 10, 0)
    };
    std::vector<double> bulges = { 0.0, 0.0 };
    DxfLWPolyline poly(verts, bulges, false);
    auto lines = GeometryUtils::tessellateLWPolyline(poly, 0.01);
    // Two straight segments → each produces 1 line (bulge=0 → {p0, p1})
    EXPECT_EQ(lines.size(), 2u);
}

TEST(TessellateLWPolyline, closed_polyline) {
    // Triangle
    std::vector<DxfPoint> verts = {
        DxfPoint(0, 0, 0), DxfPoint(10, 0, 0), DxfPoint(0, 10, 0)
    };
    std::vector<double> bulges = { 0.0, 0.0, 0.0 };
    DxfLWPolyline poly(verts, bulges, true);
    auto lines = GeometryUtils::tessellateLWPolyline(poly, 0.01);
    // 3 edges in triangle
    EXPECT_GE(lines.size(), 3u);
}

TEST(TessellateLWPolyline, bulge_arc_segment) {
    // Two vertices with a 90° bulge arc between them
    std::vector<DxfPoint> verts = {
        DxfPoint(10, 0, 0), DxfPoint(0, 10, 0)
    };
    double bulge90 = std::tan(M_PI / 8.0);  // quarter circle
    std::vector<double> bulges = { bulge90 };
    DxfLWPolyline poly(verts, bulges, false);
    auto lines = GeometryUtils::tessellateLWPolyline(poly, 0.01);
    // Bulge arc produces multiple segments
    ASSERT_GE(lines.size(), 2u);
}

TEST(TessellateLWPolyline, constZ_applied) {
    std::vector<DxfPoint> verts = {
        DxfPoint(0, 0, 0), DxfPoint(10, 0, 0)
    };
    std::vector<double> bulges = { 0.0 };
    DxfLWPolyline poly(verts, bulges, false, 7.5);
    auto lines = GeometryUtils::tessellateLWPolyline(poly, 0.01);
    ASSERT_EQ(lines.size(), 1u);
    EXPECT_DOUBLE_EQ(lines[0].start().z(), 7.5);
    EXPECT_DOUBLE_EQ(lines[0].end().z(), 7.5);
}

// ========================================================================
//  Translation through the public conversion API
// ========================================================================

TEST(Translate, basic_offset) {
    DxfData data;
    data.addLine(DxfLine(DxfPoint(1, 2, 3), DxfPoint(2, 2, 3)));
    SamData output;
    ASSERT_TRUE(ConversionEngine().convert(data, 10, 20, 30, 0.01, output));
    ASSERT_EQ(output.lines().size(), 1u);
    const DxfPoint& result = output.lines()[0].start();
    EXPECT_DOUBLE_EQ(result.x(), 11.0);
    EXPECT_DOUBLE_EQ(result.y(), 22.0);
    EXPECT_DOUBLE_EQ(result.z(), 33.0);
}

TEST(Translate, zero_offset) {
    DxfData data;
    data.addLine(DxfLine(DxfPoint(5, 5, 5), DxfPoint(6, 5, 5)));
    SamData output;
    ASSERT_TRUE(ConversionEngine().convert(data, 0, 0, 0, 0.01, output));
    ASSERT_EQ(output.lines().size(), 1u);
    const DxfPoint& result = output.lines()[0].start();
    EXPECT_DOUBLE_EQ(result.x(), 5.0);
    EXPECT_DOUBLE_EQ(result.y(), 5.0);
    EXPECT_DOUBLE_EQ(result.z(), 5.0);
}

TEST(Translate, negative_offset) {
    DxfData data;
    data.addLine(DxfLine(DxfPoint(10, 20, 30), DxfPoint(11, 20, 30)));
    SamData output;
    ASSERT_TRUE(ConversionEngine().convert(
        data, -5, -10, -15, 0.01, output));
    ASSERT_EQ(output.lines().size(), 1u);
    const DxfPoint& result = output.lines()[0].start();
    EXPECT_DOUBLE_EQ(result.x(), 5.0);
    EXPECT_DOUBLE_EQ(result.y(), 10.0);
    EXPECT_DOUBLE_EQ(result.z(), 15.0);
}

TEST(ConversionBudget, tessellated_output_is_limited)
{
    DxfData data;
    data.addCircle(DxfCircle(DxfPoint(0, 0, 0), 100.0));

    SamData output;
    EXPECT_FALSE(ConversionEngine().convert(
        data, 0, 0, 0, 0.01, output, 10));
    EXPECT_EQ(output.errorCode(), DxfImportErrorCode::ConversionLimit);
    EXPECT_TRUE(output.lines().empty());
    EXPECT_TRUE(output.circles().empty());
}

// ========================================================================
//  boundary / edge cases
// ========================================================================

TEST(TessellateArc, z_nonzero_center) {
    DxfArc arc(DxfPoint(0, 0, 5.0), 100.0, 0.0, M_PI, true);
    auto lines = GeometryUtils::tessellateArc(arc, 0.01);
    ASSERT_GE(lines.size(), 2u);
    // All output should have Z = 5.0
    for (const auto& seg : lines) {
        EXPECT_DOUBLE_EQ(seg.start().z(), 5.0);
        EXPECT_DOUBLE_EQ(seg.end().z(), 5.0);
    }
}

TEST(TessellateArc, zero_tolerance_fallback) {
    DxfArc arc(DxfPoint(0, 0, 0), 10.0, 0.0, M_PI / 2.0, true);
    auto lines = GeometryUtils::tessellateArc(arc, 0.0);
    ASSERT_GE(lines.size(), 2u);
}

TEST(TessellateArc, barely_positive_sweep) {
    // start and end angles very close
    DxfArc arc(DxfPoint(0, 0, 0), 100.0, 0.0, 0.001, true);
    auto lines = GeometryUtils::tessellateArc(arc, 0.01);
    // Should still produce some segments
    ASSERT_GE(lines.size(), 2u);
}

TEST(TessellateEllipse, ratio_one_circle) {
    DxfEllipse e(DxfPoint(0, 0, 0), DxfPoint(100, 0, 0),
                 1.0, 0.0, 2.0 * M_PI, true);
    auto lines = GeometryUtils::tessellateEllipse(e, 0.01);
    ASSERT_GE(lines.size(), 2u);
    // All points should be ~100 units from center (circle)
    for (const auto& seg : lines) {
        double d = std::sqrt(seg.start().x() * seg.start().x()
                           + seg.start().y() * seg.start().y());
        EXPECT_NEAR(d, 100.0, 0.5);
    }
}

TEST(TessellateEllipse, z_nonzero_center) {
    DxfEllipse e(DxfPoint(0, 0, 3.0), DxfPoint(100, 0, 0),
                 0.5, 0.0, M_PI, true);
    auto lines = GeometryUtils::tessellateEllipse(e, 0.01);
    ASSERT_GE(lines.size(), 2u);
    for (const auto& seg : lines) {
        EXPECT_DOUBLE_EQ(seg.start().z(), 3.0);
        EXPECT_DOUBLE_EQ(seg.end().z(), 3.0);
    }
}

TEST(TessellateLWPolyline, all_bulge_segments) {
    // 3-segment arc chain (approximate a circle)
    double bulge60 = std::tan(M_PI / 12.0);  // 60° per segment
    std::vector<DxfPoint> verts = {
        DxfPoint(10, 0, 0), DxfPoint(0, 10, 0),
        DxfPoint(-10, 0, 0), DxfPoint(0, -10, 0)
    };
    std::vector<double> bulges = { bulge60, bulge60, bulge60 };
    DxfLWPolyline poly(verts, bulges, false);
    auto lines = GeometryUtils::tessellateLWPolyline(poly, 0.01);
    // Each bulge arc → multiple lines, total > 3
    ASSERT_GT(lines.size(), 3u);
}

TEST(TessellateLWPolyline, mixed_bulge_segments) {
    // Line, then arc, then line
    double bulge90 = std::tan(M_PI / 8.0);
    std::vector<DxfPoint> verts = {
        DxfPoint(0, 0, 0), DxfPoint(10, 0, 0),
        DxfPoint(0, 10, 0), DxfPoint(0, 20, 0)
    };
    std::vector<double> bulges = { 0.0, bulge90, 0.0 };
    DxfLWPolyline poly(verts, bulges, false);
    auto lines = GeometryUtils::tessellateLWPolyline(poly, 0.01);
    // Seg0=1 line, Seg1=multiple lines, Seg2=1 line
    ASSERT_GT(lines.size(), 3u);
}

TEST(TessellateLWPolyline, two_vertex_closed) {
    // Two vertices, closed → 2 edges (A→B, B→A)
    std::vector<DxfPoint> verts = {
        DxfPoint(0, 0, 0), DxfPoint(10, 0, 0)
    };
    std::vector<double> bulges = { 0.0, 0.5 };  // straight + arc closing
    DxfLWPolyline poly(verts, bulges, true);
    auto lines = GeometryUtils::tessellateLWPolyline(poly, 0.01);
    ASSERT_GE(lines.size(), 2u);
}

TEST(Translate, large_offset) {
    DxfData data;
    data.addLine(DxfLine(DxfPoint(0, 0, 0), DxfPoint(1, 0, 0)));
    SamData output;
    ASSERT_TRUE(ConversionEngine().convert(
        data, 1e9, -1e9, 1e9, 0.01, output));
    ASSERT_EQ(output.lines().size(), 1u);
    const DxfPoint& result = output.lines()[0].start();
    EXPECT_DOUBLE_EQ(result.x(), 1e9);
    EXPECT_DOUBLE_EQ(result.y(), -1e9);
    EXPECT_DOUBLE_EQ(result.z(), 1e9);
}

TEST(TessellateArc, non_zero_based_angles) {
    // Arc from π/4 to 3π/4
    DxfArc arc(DxfPoint(0, 0, 0), 100.0, M_PI / 4.0, 3.0 * M_PI / 4.0, true);
    auto lines = GeometryUtils::tessellateArc(arc, 0.01);
    ASSERT_GE(lines.size(), 2u);
}
