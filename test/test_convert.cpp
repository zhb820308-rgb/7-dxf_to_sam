/**
 * test_convert.cpp — Tests ConversionEngine::convert() integration flow
 *
 * Covers: Empty input / mixed entities / invalid filtering / translation verification / return values
 *
 * convert() is the core dispatch function of the pipeline:
 *   DxfData -> [tessellation + translation + filtering] -> SamData
 * Does not depend on SAM SDK; all inputs and outputs are pure data classes.
 */
#include <gtest/gtest.h>
#include <cmath>
#include <limits>
#include "ConversionEngine.h"
#include "SamData.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ========================================================================
//  Empty / trivial input
// ========================================================================

TEST(Convert, empty_dxfdata_returns_false) {
    DxfData dxf;
    dxf.setValid(true);
    SamData out;
    bool ok = ConversionEngine().convert(dxf, 0, 0, 0, ConversionEngine::defaultBulgeTolerance(), out);
    EXPECT_FALSE(ok);
    EXPECT_EQ(out.lines().size(), 0u);
    EXPECT_EQ(out.circles().size(), 0u);
}

TEST(Convert, only_invalid_entities_returns_false) {
    DxfData dxf;
    dxf.setValid(true);
    // zero-length line is invalid
    dxf.addLine(DxfLine(DxfPoint(5, 5, 0), DxfPoint(5, 5, 0)));
    // zero-radius circle is invalid
    dxf.addCircle(DxfCircle(DxfPoint(0, 0, 0), 0.0));
    // same-angle arc is invalid
    dxf.addArc(DxfArc(DxfPoint(0, 0, 0), 10.0, 1.0, 1.0, true));

    SamData out;
    bool ok = ConversionEngine().convert(dxf, 0, 0, 0, ConversionEngine::defaultBulgeTolerance(), out);
    EXPECT_FALSE(ok);
}

// ========================================================================
//  Single entity types
// ========================================================================

TEST(Convert, single_line_passes_through) {
    DxfData dxf;
    dxf.setValid(true);
    dxf.addLine(DxfLine(DxfPoint(0, 0, 0), DxfPoint(10, 0, 0)));

    SamData out;
    bool ok = ConversionEngine().convert(dxf, 0, 0, 0, ConversionEngine::defaultBulgeTolerance(), out);
    EXPECT_TRUE(ok);
    ASSERT_EQ(out.lines().size(), 1u);
    EXPECT_DOUBLE_EQ(out.lines()[0].start().x(), 0.0);
    EXPECT_DOUBLE_EQ(out.lines()[0].end().x(), 10.0);
}

TEST(Convert, single_circle) {
    DxfData dxf;
    dxf.setValid(true);
    dxf.addCircle(DxfCircle(DxfPoint(0, 0, 0), 5.0));

    SamData out;
    bool ok = ConversionEngine().convert(dxf, 0, 0, 0, ConversionEngine::defaultBulgeTolerance(), out);
    EXPECT_TRUE(ok);
    ASSERT_EQ(out.circles().size(), 1u);
    EXPECT_DOUBLE_EQ(out.circles()[0].radius(), 5.0);
}

TEST(Convert, single_arc_produces_lines) {
    DxfData dxf;
    dxf.setValid(true);
    dxf.addArc(DxfArc(DxfPoint(0, 0, 0), 100.0, 0.0, M_PI / 2.0, true));

    SamData out;
    bool ok = ConversionEngine().convert(dxf, 0, 0, 0, ConversionEngine::defaultBulgeTolerance(), out);
    EXPECT_TRUE(ok);
    // Arc → tessellated into multiple lines, zero circles
    EXPECT_GE(out.lines().size(), 2u);
    EXPECT_EQ(out.circles().size(), 0u);
}

TEST(Convert, single_lwpolyline_produces_lines) {
    DxfData dxf;
    dxf.setValid(true);
    std::vector<DxfPoint> verts = {
        DxfPoint(0, 0, 0), DxfPoint(10, 0, 0), DxfPoint(10, 10, 0)
    };
    std::vector<double> bulges = { 0.0, 0.0 };
    dxf.addLWPolyline(DxfLWPolyline(verts, bulges, false));

    SamData out;
    bool ok = ConversionEngine().convert(dxf, 0, 0, 0, ConversionEngine::defaultBulgeTolerance(), out);
    EXPECT_TRUE(ok);
    EXPECT_EQ(out.lines().size(), 2u);
}

TEST(Convert, single_ellipse_produces_lines) {
    DxfData dxf;
    dxf.setValid(true);
    dxf.addEllipse(DxfEllipse(DxfPoint(0, 0, 0), DxfPoint(100, 0, 0),
                              0.5, 0.0, M_PI, true));

    SamData out;
    bool ok = ConversionEngine().convert(dxf, 0, 0, 0, ConversionEngine::defaultBulgeTolerance(), out);
    EXPECT_TRUE(ok);
    EXPECT_GE(out.lines().size(), 2u);
    EXPECT_EQ(out.circles().size(), 0u);
}

// ========================================================================
//  Translation
// ========================================================================

TEST(Convert, translation_applied_to_lines) {
    DxfData dxf;
    dxf.setValid(true);
    dxf.addLine(DxfLine(DxfPoint(1, 2, 3), DxfPoint(4, 5, 6)));

    SamData out;
    ConversionEngine().convert(dxf, 10, 20, 30, ConversionEngine::defaultBulgeTolerance(), out);
    ASSERT_EQ(out.lines().size(), 1u);
    EXPECT_DOUBLE_EQ(out.lines()[0].start().x(), 11.0);
    EXPECT_DOUBLE_EQ(out.lines()[0].start().y(), 22.0);
    EXPECT_DOUBLE_EQ(out.lines()[0].start().z(), 33.0);
    EXPECT_DOUBLE_EQ(out.lines()[0].end().x(), 14.0);
    EXPECT_DOUBLE_EQ(out.lines()[0].end().y(), 25.0);
    EXPECT_DOUBLE_EQ(out.lines()[0].end().z(), 36.0);
}

TEST(Convert, translation_applied_to_circles) {
    DxfData dxf;
    dxf.setValid(true);
    dxf.addCircle(DxfCircle(DxfPoint(5, 5, 5), 10.0));

    SamData out;
    ConversionEngine().convert(dxf, -5, -5, -5, ConversionEngine::defaultBulgeTolerance(), out);
    ASSERT_EQ(out.circles().size(), 1u);
    EXPECT_DOUBLE_EQ(out.circles()[0].center().x(), 0.0);
    EXPECT_DOUBLE_EQ(out.circles()[0].center().y(), 0.0);
    EXPECT_DOUBLE_EQ(out.circles()[0].center().z(), 0.0);
    EXPECT_DOUBLE_EQ(out.circles()[0].radius(), 10.0);  // radius unchanged
}

TEST(Convert, nonfinite_base_coordinate_is_rejected) {
    DxfData dxf;
    dxf.addLine(DxfLine(DxfPoint(0, 0, 0), DxfPoint(1, 0, 0)));

    SamData out;
    EXPECT_FALSE(ConversionEngine().convert(
        dxf,
        std::numeric_limits<double>::quiet_NaN(), 0.0, 0.0,
        ConversionEngine::defaultBulgeTolerance(), out));
    EXPECT_TRUE(out.lines().empty());
    EXPECT_TRUE(out.circles().empty());
}

TEST(Convert, translation_applied_to_arc_output) {
    DxfData dxf;
    dxf.setValid(true);
    dxf.addArc(DxfArc(DxfPoint(0, 0, 0), 100.0, 0.0, M_PI, true));

    SamData out;
    ConversionEngine().convert(dxf, 100, 200, 50, ConversionEngine::defaultBulgeTolerance(), out);
    ASSERT_GE(out.lines().size(), 2u);
    // All output lines should be shifted
    for (const auto& line : out.lines()) {
        EXPECT_NEAR(line.start().x(), line.start().x(), 0);  // just check they exist
        EXPECT_GT(line.start().x(), 0.0);  // original was around -100 to 100, now shifted +100
    }
}

// ========================================================================
//  Mixed entities
// ========================================================================

TEST(Convert, mixed_entities_all_processed) {
    DxfData dxf;
    dxf.setValid(true);
    dxf.addLine(DxfLine(DxfPoint(0, 0, 0), DxfPoint(1, 0, 0)));
    dxf.addCircle(DxfCircle(DxfPoint(0, 0, 0), 5.0));
    dxf.addArc(DxfArc(DxfPoint(0, 0, 0), 10.0, 0.0, M_PI / 2.0, true));

    SamData out;
    bool ok = ConversionEngine().convert(dxf, 0, 0, 0, ConversionEngine::defaultBulgeTolerance(), out);
    EXPECT_TRUE(ok);
    // Lines: 1 direct + N from arc tessellation (no direct lines from arcs → all lines come from tessellation)
    EXPECT_GE(out.lines().size(), 3u);  // 1 original + at least 2 from arc
    EXPECT_EQ(out.circles().size(), 1u);
}

TEST(Convert, mixed_valid_and_invalid) {
    DxfData dxf;
    dxf.setValid(true);
    // valid
    dxf.addLine(DxfLine(DxfPoint(0, 0, 0), DxfPoint(10, 0, 0)));
    // invalid (zero-length)
    dxf.addLine(DxfLine(DxfPoint(5, 5, 0), DxfPoint(5, 5, 0)));
    // valid
    dxf.addCircle(DxfCircle(DxfPoint(0, 0, 0), 5.0));
    // invalid (zero radius)
    dxf.addCircle(DxfCircle(DxfPoint(0, 0, 0), 0.0));

    SamData out;
    bool ok = ConversionEngine().convert(dxf, 0, 0, 0, ConversionEngine::defaultBulgeTolerance(), out);
    EXPECT_TRUE(ok);
    EXPECT_EQ(out.lines().size(), 1u);   // only the valid line
    EXPECT_EQ(out.circles().size(), 1u); // only the valid circle
}

// ========================================================================
//  Full pipeline: arc + polyline + ellipse together
// ========================================================================

TEST(Convert, full_curve_suite) {
    DxfData dxf;
    dxf.setValid(true);

    // Line
    dxf.addLine(DxfLine(DxfPoint(100, 0, 0), DxfPoint(200, 0, 0)));

    // Circle
    dxf.addCircle(DxfCircle(DxfPoint(500, 0, 0), 50.0));

    // Arc: quarter circle
    dxf.addArc(DxfArc(DxfPoint(0, 0, 0), 10.0, 0.0, M_PI / 2.0, true));

    // LWPolyline: triangle
    std::vector<DxfPoint> verts = {
        DxfPoint(0, 0, 0), DxfPoint(100, 0, 0), DxfPoint(50, 50, 0)
    };
    std::vector<double> bulges = { 0.0, 0.0 };
    dxf.addLWPolyline(DxfLWPolyline(verts, bulges, false));

    // Ellipse: half
    dxf.addEllipse(DxfEllipse(DxfPoint(300, 0, 0), DxfPoint(50, 0, 0),
                              0.5, 0.0, M_PI, true));

    SamData out;
    bool ok = ConversionEngine().convert(dxf, 0, 0, 0, ConversionEngine::defaultBulgeTolerance(), out);
    EXPECT_TRUE(ok);

    // At minimum: 1 line + 2 arc segments + 2 poly segments + 2 ellipse segments + 1 circle
    EXPECT_GE(out.lines().size(), 7u);
    EXPECT_EQ(out.circles().size(), 1u);
}

// ========================================================================
//  Unsupported POINT entities
// ========================================================================

TEST(Convert, point_only_input_is_not_convertible) {
    DxfData dxf;
    dxf.setValid(true);
    dxf.addPoint(DxfPoint(1, 2, 3));
    dxf.addPoint(DxfPoint(4, 5, 6));

    SamData out;
    bool ok = ConversionEngine().convert(
        dxf, 0, 0, 0, ConversionEngine::defaultBulgeTolerance(), out);

    EXPECT_FALSE(ok);
    EXPECT_TRUE(out.lines().empty());
    EXPECT_TRUE(out.circles().empty());
}

// ========================================================================
//  Bulk entity stress
// ========================================================================

TEST(Convert, many_arcs) {
    DxfData dxf;
    dxf.setValid(true);
    for (int i = 0; i < 100; ++i) {
        dxf.addArc(DxfArc(DxfPoint(0, 0, 0), 10.0 * (i + 1),
                          0.0, M_PI / 4.0, true));
    }

    SamData out;
    bool ok = ConversionEngine().convert(dxf, 0, 0, 0, ConversionEngine::defaultBulgeTolerance(), out);
    EXPECT_TRUE(ok);
    // Each arc produces at least 2 segments
    EXPECT_GE(out.lines().size(), 200u);
}

TEST(Convert, many_polylines) {
    DxfData dxf;
    dxf.setValid(true);
    for (int i = 0; i < 50; ++i) {
        std::vector<DxfPoint> verts = {
            DxfPoint(0, double(i), 0),
            DxfPoint(10, double(i), 0),
            DxfPoint(10, double(i) + 10, 0)
        };
        std::vector<double> bulges = { 0.0, 0.0 };
        dxf.addLWPolyline(DxfLWPolyline(verts, bulges, false));
    }

    SamData out;
    bool ok = ConversionEngine().convert(dxf, 0, 0, 0, ConversionEngine::defaultBulgeTolerance(), out);
    EXPECT_TRUE(ok);
    EXPECT_EQ(out.lines().size(), 100u);  // 50 polys × 2 segments each
}

// ========================================================================
//  Edge: invalid DxfData (not set valid by parser)
// ========================================================================

TEST(Convert, dfxdata_not_valid_still_converts) {
    // convert() doesn't check dxfData.isValid() — it processes entities regardless
    DxfData dxf;
    // isValid defaults to false
    dxf.addLine(DxfLine(DxfPoint(0, 0, 0), DxfPoint(10, 0, 0)));

    SamData out;
    bool ok = ConversionEngine().convert(dxf, 0, 0, 0, ConversionEngine::defaultBulgeTolerance(), out);
    // Should still convert — isValid is just a parser flag
    EXPECT_TRUE(ok);
    EXPECT_EQ(out.lines().size(), 1u);
}
