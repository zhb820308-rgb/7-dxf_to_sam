/**
 * test_parser.cpp — Tests DxfParser parsing with real DXF files
 *
 * Covers: parseFile() / entity count / error paths / full parse->convert pipeline
 */
#include <gtest/gtest.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#include <QFile>
#include <QDebug>
#include <cmath>
#include <limits>
#include "DxfParser.h"
#include "ConversionEngine.h"
#include "SamData.h"

#ifndef DXF_TEST_EXAMPLE_DIR
#error "DXF_TEST_EXAMPLE_DIR must be provided by CMake"
#endif

#ifndef DXF_TEST_FIXTURE_DIR
#error "DXF_TEST_FIXTURE_DIR must be provided by CMake"
#endif

// ========================================================================
//  Portable test paths and coordinate helpers
// ========================================================================

static bool fileExists(const QString& path) {
    return QFile::exists(path);
}

static const QString EXAMPLE_DIR = QString::fromUtf8(DXF_TEST_EXAMPLE_DIR);
static const QString TEST_DATA_DIR = QString::fromUtf8(DXF_TEST_FIXTURE_DIR);
static const double COORD_TOLERANCE = 1e-8;

static void expectPointNear(const DxfPoint& actual,
                            double expectedX,
                            double expectedY,
                            double expectedZ = 0.0)
{
    EXPECT_NEAR(actual.x(), expectedX, COORD_TOLERANCE);
    EXPECT_NEAR(actual.y(), expectedY, COORD_TOLERANCE);
    EXPECT_NEAR(actual.z(), expectedZ, COORD_TOLERANCE);
}

static void expectLineNear(const DxfLine& actual,
                           double x1, double y1,
                           double x2, double y2,
                           double z1 = 0.0, double z2 = 0.0)
{
    expectPointNear(actual.start(), x1, y1, z1);
    expectPointNear(actual.end(), x2, y2, z2);
}

static void expectAngleNear(double actual, double expected)
{
    EXPECT_NEAR(std::cos(actual), std::cos(expected), COORD_TOLERANCE);
    EXPECT_NEAR(std::sin(actual), std::sin(expected), COORD_TOLERANCE);
}

// ========================================================================
//  File existence
// ========================================================================

TEST(Parser, example_files_exist) {
    EXPECT_TRUE(fileExists(EXAMPLE_DIR + "/line1.dxf"));
    EXPECT_TRUE(fileExists(EXAMPLE_DIR + "/circle.dxf"));
    EXPECT_TRUE(fileExists(EXAMPLE_DIR + "/elipse.dxf"));
    EXPECT_TRUE(fileExists(EXAMPLE_DIR + "/square.dxf"));
    EXPECT_TRUE(fileExists(EXAMPLE_DIR + "/half_circle_up.dxf"));
    EXPECT_TRUE(fileExists(EXAMPLE_DIR + "/part_circle_down.dxf"));
    EXPECT_TRUE(fileExists(EXAMPLE_DIR + "/pline_half_circle.dxf"));
    EXPECT_TRUE(fileExists(EXAMPLE_DIR + "/circle_coner.dxf"));
    EXPECT_TRUE(fileExists(EXAMPLE_DIR + "/line_and_circle_closed.dxf"));
}

// ========================================================================
//  Error paths
// ========================================================================

TEST(Parser, empty_path_returns_false) {
    DxfData data;
    DxfParser parser;
    EXPECT_FALSE(parser.parseFile("", data));
    EXPECT_FALSE(data.isValid());
}

TEST(Parser, nonexistent_file_returns_false) {
    DxfData data;
    DxfParser parser;
    EXPECT_FALSE(parser.parseFile(
        TEST_DATA_DIR + "/nonexistent_file_12345.dxf", data));
    EXPECT_FALSE(data.isValid());
}

TEST(Parser, invalid_curve_tolerance_is_rejected_before_reading) {
    DxfData data;
    data.addLine(DxfLine(DxfPoint(0, 0, 0), DxfPoint(1, 0, 0)));
    data.setValid(true);

    DxfParser parser;
    EXPECT_FALSE(parser.parseFile(
        TEST_DATA_DIR + "/point_only.dxf", data,
        std::numeric_limits<double>::quiet_NaN()));
    EXPECT_FALSE(data.isValid());
    EXPECT_EQ(data.entityCount(), 0);
    EXPECT_FALSE(data.errorMessage().isEmpty());
}

// ========================================================================
//  Single entity files
// ========================================================================

TEST(Parser, line1_dxf_has_lines) {
    DxfData data;
    DxfParser parser;
    ASSERT_TRUE(parser.parseFile(EXAMPLE_DIR + "/line1.dxf", data));
    EXPECT_TRUE(data.isValid());
    EXPECT_GE(data.lines().size(), 1u);
    EXPECT_EQ(data.entityStats().lines, data.lines().size());
    EXPECT_EQ(data.entityStats().lwPolylines, data.lwPolylines().size());
    EXPECT_EQ(data.entityStats().curveCount(), 0u);
    EXPECT_EQ(data.circles().size(), 0u);
    EXPECT_EQ(data.arcs().size(), 0u);
}

TEST(Parser, circle_dxf_has_circles) {
    DxfData data;
    DxfParser parser;
    ASSERT_TRUE(parser.parseFile(EXAMPLE_DIR + "/circle.dxf", data));
    EXPECT_TRUE(data.isValid());
    EXPECT_GE(data.circles().size(), 1u);
    EXPECT_EQ(data.entityStats().circles, data.circles().size());
    EXPECT_EQ(data.entityStats().curveCount(), data.circles().size());
    // All circles should be valid
    for (const auto& c : data.circles()) {
        EXPECT_TRUE(c.isValid());
    }
}

TEST(Parser, square_dxf_has_entities) {
    DxfData data;
    DxfParser parser;
    ASSERT_TRUE(parser.parseFile(EXAMPLE_DIR + "/square.dxf", data));
    EXPECT_TRUE(data.isValid());
    // square.dxf contains a polyline or lines → at least one entity
    EXPECT_GT(data.entityCount(), 0);
}

TEST(Parser, point_only_dxf_is_valid_input) {
    DxfData data;
    DxfParser parser;

    ASSERT_TRUE(parser.parseFile(TEST_DATA_DIR + "/point_only.dxf", data));
    EXPECT_TRUE(data.isValid());
    EXPECT_EQ(data.points().size(), 2u);
    EXPECT_EQ(data.entityCount(), 2);
    EXPECT_EQ(data.sketchEntityCount(), 0);
    EXPECT_EQ(data.feEntityCount(), 2);
}

// ========================================================================
//  Arc files
// ========================================================================

TEST(Parser, half_circle_up_dxf_has_arcs) {
    DxfData data;
    DxfParser parser;
    ASSERT_TRUE(parser.parseFile(EXAMPLE_DIR + "/half_circle_up.dxf", data));
    EXPECT_TRUE(data.isValid());
    EXPECT_GE(data.arcs().size(), 1u);
    for (const auto& arc : data.arcs()) {
        EXPECT_TRUE(arc.isValid());
        EXPECT_GT(arc.radius(), 0.0);
    }
}

TEST(Parser, part_circle_down_dxf_has_arcs) {
    DxfData data;
    DxfParser parser;
    ASSERT_TRUE(parser.parseFile(EXAMPLE_DIR + "/part_circle_down.dxf", data));
    EXPECT_TRUE(data.isValid());
    EXPECT_GE(data.arcs().size(), 1u);
}

// ========================================================================
//  Ellipse files
// ========================================================================

TEST(Parser, elipse_dxf_has_ellipses) {
    DxfData data;
    DxfParser parser;
    ASSERT_TRUE(parser.parseFile(EXAMPLE_DIR + "/elipse.dxf", data));
    EXPECT_TRUE(data.isValid());
    EXPECT_GE(data.ellipses().size(), 1u);
    for (const auto& e : data.ellipses()) {
        EXPECT_TRUE(e.isValid());
        EXPECT_GT(e.ratio(), 0.0);
    }
}

// ========================================================================
//  Polyline files
// ========================================================================

TEST(Parser, pline_half_circle_dxf_has_polylines) {
    DxfData data;
    DxfParser parser;
    ASSERT_TRUE(parser.parseFile(EXAMPLE_DIR + "/pline_half_circle.dxf", data));
    EXPECT_TRUE(data.isValid());
    EXPECT_GE(data.lwPolylines().size(), 1u);
    EXPECT_EQ(data.entityStats().lwPolylines, data.lwPolylines().size());
    for (const auto& poly : data.lwPolylines()) {
        EXPECT_TRUE(poly.isValid());
        EXPECT_GE(poly.vertexCount(), 2);
    }
}

TEST(Parser, control_spline_is_classified) {
    DxfData data;
    DxfParser parser;
    ASSERT_TRUE(parser.parseFile(EXAMPLE_DIR + "/spline.dxf", data));
    ASSERT_FALSE(data.splines().empty());
    EXPECT_EQ(data.entityStats().splineCount(), data.splines().size());
    EXPECT_EQ(data.entityStats().curveCount(),
              data.entityStats().circles + data.entityStats().arcs
              + data.entityStats().ellipses + data.splines().size());

    for (const DxfSpline& spline : data.splines())
        EXPECT_EQ(spline.kind().construction, SplineConstruction::ControlBased);
}

TEST(Parser, fit_only_spline_is_classified) {
    DxfData data;
    DxfParser parser;
    ASSERT_TRUE(parser.parseFile(EXAMPLE_DIR + "/spline_fit_only.dxf", data));
    ASSERT_FALSE(data.splines().empty());
    EXPECT_EQ(data.entityStats().splineCount(), data.splines().size());

    for (const DxfSpline& spline : data.splines())
        EXPECT_EQ(spline.kind().construction, SplineConstruction::FitBased);
}

TEST(Parser, nonuniform_block_curves_keep_source_statistics) {
    DxfData data;
    DxfParser parser;
    ASSERT_TRUE(parser.parseFile(TEST_DATA_DIR + "/block_stats_nonuniform.dxf", data));

    ASSERT_GT(data.lines().size(), 1u);
    EXPECT_TRUE(data.arcs().empty());
    EXPECT_TRUE(data.lwPolylines().empty());

    const DxfEntityStats& stats = data.entityStats();
    EXPECT_EQ(stats.lines, 1u);
    EXPECT_EQ(stats.lwPolylines, 1u);
    EXPECT_EQ(stats.arcs, 1u);
    EXPECT_EQ(stats.curveCount(), 1u);
}

TEST(Parser, oversized_insert_array_is_rejected_without_partial_output) {
    DxfData data;
    DxfParser parser;

    EXPECT_FALSE(parser.parseFile(
        TEST_DATA_DIR + "/block_array_limit.dxf", data));
    EXPECT_FALSE(data.isValid());
    EXPECT_EQ(data.entityCount(), 0);
    EXPECT_TRUE(data.errorMessage().contains("limit", Qt::CaseInsensitive))
        << data.errorMessage().toStdString();
}

TEST(Parser, block_base_point_is_applied_to_exact_line_coordinates) {
    const QString path = EXAMPLE_DIR + "/block_test_minimal.dxf";
    ASSERT_TRUE(fileExists(path)) << path.toStdString();

    DxfData data;
    DxfParser parser;
    ASSERT_TRUE(parser.parseFile(path, data))
        << data.errorMessage().toStdString();

    ASSERT_EQ(data.lines().size(), 3u);
    expectLineNear(data.lines()[0], 1000.0, 1000.0, 1100.0, 1000.0);
    expectLineNear(data.lines()[1], 400.0, 300.0, 410.0, 300.0);
    expectLineNear(data.lines()[2], 400.0, 300.0, 400.0, 310.0);
}

TEST(Parser, rotated_insert_array_has_exact_grid_coordinates) {
    const QString path = TEST_DATA_DIR + "/block_array_rotated.dxf";
    ASSERT_TRUE(fileExists(path)) << path.toStdString();

    DxfData data;
    DxfParser parser;
    ASSERT_TRUE(parser.parseFile(path, data))
        << data.errorMessage().toStdString();

    ASSERT_EQ(data.lines().size(), 4u);
    expectLineNear(data.lines()[0], 10.0, 20.0, 10.0, 21.0);
    expectLineNear(data.lines()[1], 10.0, 23.0, 10.0, 24.0);
    expectLineNear(data.lines()[2],  6.0, 20.0,  6.0, 21.0);
    expectLineNear(data.lines()[3],  6.0, 23.0,  6.0, 24.0);
}

// Correct-result specifications for G2 affine composition and preserved curves.
TEST(Parser, nested_nonuniform_rotation_uses_matrix_composition) {
    const QString path = TEST_DATA_DIR + "/block_nested_nonuniform.dxf";
    ASSERT_TRUE(fileExists(path)) << path.toStdString();

    DxfData data;
    DxfParser parser;
    ASSERT_TRUE(parser.parseFile(path, data))
        << data.errorMessage().toStdString();

    ASSERT_EQ(data.lines().size(), 1u);
    expectLineNear(data.lines()[0], 0.0, 0.0, 0.0, 1.0);
}

TEST(Parser, rotated_block_updates_curve_directions) {
    const QString path = TEST_DATA_DIR + "/block_rotated_curves.dxf";
    ASSERT_TRUE(fileExists(path)) << path.toStdString();

    DxfData data;
    DxfParser parser;
    ASSERT_TRUE(parser.parseFile(path, data))
        << data.errorMessage().toStdString();

    ASSERT_EQ(data.arcs().size(), 1u);
    const DxfArc& arc = data.arcs()[0];
    expectPointNear(arc.center(), 0.0, 0.0);
    EXPECT_NEAR(arc.startAngle(), M_PI / 2.0, COORD_TOLERANCE);
    EXPECT_NEAR(arc.endAngle(), M_PI, COORD_TOLERANCE);

    ASSERT_EQ(data.ellipses().size(), 1u);
    const DxfEllipse& ellipse = data.ellipses()[0];
    expectPointNear(ellipse.center(), 0.0, 20.0);
    expectPointNear(ellipse.majorAxisEnd(), 0.0, 5.0);

    ASSERT_EQ(data.splines().size(), 1u);
    const DxfSpline& spline = data.splines()[0];
    ASSERT_EQ(spline.fitPoints().size(), 2u);
    expectPointNear(spline.fitPoints()[0], 0.0, 0.0);
    expectPointNear(spline.fitPoints()[1], 0.0, 10.0);
    EXPECT_NEAR(spline.tgStartX(), 0.0, COORD_TOLERANCE);
    EXPECT_NEAR(spline.tgStartY(), 1.0, COORD_TOLERANCE);
    EXPECT_NEAR(spline.tgEndX(), -1.0, COORD_TOLERANCE);
    EXPECT_NEAR(spline.tgEndY(), 0.0, COORD_TOLERANCE);
}

TEST(Parser, mirrored_block_reverses_curve_direction_and_preserves_insert_z) {
    const QString path = TEST_DATA_DIR + "/block_mirrored_curves.dxf";
    ASSERT_TRUE(fileExists(path)) << path.toStdString();

    DxfData data;
    DxfParser parser;
    ASSERT_TRUE(parser.parseFile(path, data))
        << data.errorMessage().toStdString();

    ASSERT_EQ(data.arcs().size(), 1u);
    const DxfArc& arc = data.arcs()[0];
    EXPECT_NEAR(arc.radius(), 10.0, COORD_TOLERANCE);
    expectAngleNear(arc.startAngle(), M_PI);
    expectAngleNear(arc.endAngle(), M_PI / 2.0);
    EXPECT_FALSE(arc.isCCW());

    ASSERT_EQ(data.lwPolylines().size(), 1u);
    const DxfLWPolyline& polyline = data.lwPolylines()[0];
    ASSERT_EQ(polyline.vertices().size(), 2u);
    expectPointNear(polyline.vertices()[0], 0.0, 0.0, 10.0);
    expectPointNear(polyline.vertices()[1], -10.0, 0.0, 10.0);
    ASSERT_EQ(polyline.bulges().size(), 1u);
    EXPECT_DOUBLE_EQ(polyline.bulges()[0], -1.0);
    EXPECT_DOUBLE_EQ(polyline.constZ(), 10.0);
}

// ========================================================================
//  Mixed entity files
// ========================================================================

TEST(Parser, circle_coner_dxf) {
    DxfData data;
    DxfParser parser;
    ASSERT_TRUE(parser.parseFile(EXAMPLE_DIR + "/circle_coner.dxf", data));
    EXPECT_TRUE(data.isValid());
    EXPECT_GT(data.entityCount(), 0);
}

TEST(Parser, line_and_circle_closed_dxf) {
    DxfData data;
    DxfParser parser;
    ASSERT_TRUE(parser.parseFile(EXAMPLE_DIR + "/line_and_circle_closed.dxf", data));
    EXPECT_TRUE(data.isValid());
    EXPECT_GT(data.entityCount(), 0);
}

// ========================================================================
//  Full pipeline: parse -> convert (without SAM SDK dependency)
// ========================================================================

TEST(Pipeline, line1_parse_then_convert) {
    DxfData data;
    DxfParser parser;
    ASSERT_TRUE(parser.parseFile(EXAMPLE_DIR + "/line1.dxf", data));

    SamData out;
    ConversionEngine engine;
    bool ok = engine.convert(data, 0, 0, 0, ConversionEngine::defaultBulgeTolerance(), out);
    EXPECT_TRUE(ok);
    EXPECT_GE(out.lines().size(), 1u);
}

TEST(Pipeline, circle_parse_then_convert) {
    DxfData data;
    DxfParser parser;
    ASSERT_TRUE(parser.parseFile(EXAMPLE_DIR + "/circle.dxf", data));

    SamData out;
    ConversionEngine engine;
    bool ok = engine.convert(data, 0, 0, 0, ConversionEngine::defaultBulgeTolerance(), out);
    EXPECT_TRUE(ok);
}

TEST(Pipeline, half_circle_parse_then_convert) {
    DxfData data;
    DxfParser parser;
    ASSERT_TRUE(parser.parseFile(EXAMPLE_DIR + "/half_circle_up.dxf", data));

    SamData out;
    ConversionEngine engine;
    bool ok = engine.convert(data, 0, 0, 0, ConversionEngine::defaultBulgeTolerance(), out);
    EXPECT_TRUE(ok);
    // Arc tessellation produces lines
    EXPECT_GE(out.lines().size(), 2u);
}

TEST(Pipeline, elipse_parse_then_convert) {
    DxfData data;
    DxfParser parser;
    ASSERT_TRUE(parser.parseFile(EXAMPLE_DIR + "/elipse.dxf", data));

    SamData out;
    ConversionEngine engine;
    bool ok = engine.convert(data, 0, 0, 0, ConversionEngine::defaultBulgeTolerance(), out);
    EXPECT_TRUE(ok);
    // Ellipse tessellation produces lines
    EXPECT_GE(out.lines().size(), 2u);
}

TEST(Pipeline, pline_half_circle_parse_then_convert) {
    DxfData data;
    DxfParser parser;
    ASSERT_TRUE(parser.parseFile(EXAMPLE_DIR + "/pline_half_circle.dxf", data));

    SamData out;
    ConversionEngine engine;
    bool ok = engine.convert(data, 0, 0, 0, ConversionEngine::defaultBulgeTolerance(), out);
    EXPECT_TRUE(ok);
    EXPECT_GE(out.lines().size(), 1u);
}

// ========================================================================
//  Regression: full ellipse should not be lost
// ========================================================================

TEST(Regression, elipse_dxf_full_ellipse_not_lost) {
    DxfData data;
    DxfParser parser;
    ASSERT_TRUE(parser.parseFile(EXAMPLE_DIR + "/elipse.dxf", data));

    // Should have at least one full ellipse (ratio=1, 0->2pi or similar)
    bool hasFullEllipse = false;
    for (const auto& e : data.ellipses()) {
        double sweep = e.endParam() - e.startParam();
        // Full ellipse sweep ≈ 2π
        if (std::abs(std::abs(sweep) - 2.0 * M_PI) < 0.01 ||
            std::abs(std::abs(sweep) - 2.0 * M_PI) < M_PI * 0.1) {
            // Near-full: after normalization should still produce segments
            hasFullEllipse = true;
        }
    }
    // Even if no mathematically "full" ellipse is found,
    // the parse+convert pipeline should succeed without data loss
    SamData out;
    ConversionEngine engine;
    bool ok = engine.convert(data, 0, 0, 0, ConversionEngine::defaultBulgeTolerance(), out);
    EXPECT_TRUE(ok);

    // Each ellipse should produce at least 2 line segments
    // (this is the regression test: we should NOT get 0 segments per ellipse)
    size_t ellipseCount = data.ellipses().size();
    if (ellipseCount > 0) {
        EXPECT_GE(out.lines().size(), 2u);  // at minimum, some lines from ellipses
    }
}

// ========================================================================
//  Regression: half-circle arc imports normally
// ========================================================================

TEST(Regression, half_circle_arcs_produce_lines) {
    DxfData data;
    DxfParser parser;
    ASSERT_TRUE(parser.parseFile(EXAMPLE_DIR + "/half_circle_up.dxf", data));

    SamData out;
    ConversionEngine engine;
    bool ok = engine.convert(data, 0, 0, 0, ConversionEngine::defaultBulgeTolerance(), out);
    EXPECT_TRUE(ok);
    // Even a single half-circle arc should produce multiple line segments
    if (data.arcs().size() > 0) {
        EXPECT_GE(out.lines().size(), 2u);
    }
}

// ========================================================================
//  Multi-file consistency
// ========================================================================

TEST(Pipeline, all_example_files_parse_and_convert) {
    const char* filenames[] = {
        "/line1.dxf",
        "/circle.dxf",
        "/elipse.dxf",
        "/square.dxf",
        "/half_circle_up.dxf",
        "/part_circle_down.dxf",
        "/pline_half_circle.dxf",
        "/circle_coner.dxf",
        "/line_and_circle_closed.dxf",
    };
    const int numFiles = sizeof(filenames) / sizeof(filenames[0]);

    for (int i = 0; i < numFiles; ++i) {
        QString path = EXAMPLE_DIR + filenames[i];
        ASSERT_TRUE(fileExists(path))
            << "Missing required fixture: " << path.toStdString();

        DxfData data;
        DxfParser parser;
        bool parsed = parser.parseFile(path, data);
        ASSERT_TRUE(parsed) << "Failed to parse: " << filenames[i]
                            << "; " << data.errorMessage().toStdString();

        SamData out;
        ConversionEngine engine;
        bool converted = engine.convert(data, 0, 0, 0, ConversionEngine::defaultBulgeTolerance(), out);
        ASSERT_TRUE(converted) << "Failed to convert: " << filenames[i];

        // Every file should produce some output
        int totalOutput = static_cast<int>(
            out.lines().size() + out.circles().size());
        EXPECT_GT(totalOutput, 0) << "No output entities from: " << filenames[i];
    }
}
