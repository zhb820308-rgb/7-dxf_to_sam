#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <tuple>
#include <vector>

#include "DxfData.h"
#include "FeConversionEngine.h"
#include "FeData.h"

TEST(FeConversionEngine, point_only_input_creates_standalone_nodes)
{
    DxfData dxf;
    dxf.addPoint(DxfPoint(1.0, 2.0, 3.0));
    dxf.addPoint(DxfPoint(4.0, 5.0, 6.0));

    FeData out;
    ASSERT_TRUE(FeConversionEngine().convert(
        dxf, 10.0, 20.0, 30.0,
        0.01, FeConversionEngine::defaultNodeMergeTolerance(), out));

    ASSERT_EQ(out.nodes().size(), 2u);
    EXPECT_TRUE(out.trusses().empty());
    EXPECT_DOUBLE_EQ(out.nodes()[0].x, 11.0);
    EXPECT_DOUBLE_EQ(out.nodes()[0].y, 22.0);
    EXPECT_DOUBLE_EQ(out.nodes()[0].z, 33.0);
    EXPECT_EQ(out.stats().pointsProcessed, 2);
}

TEST(FeConversionEngine, nonfinite_base_coordinate_is_rejected)
{
    DxfData dxf;
    dxf.addPoint(DxfPoint(1.0, 2.0, 3.0));

    FeData out;
    EXPECT_FALSE(FeConversionEngine().convert(
        dxf,
        std::numeric_limits<double>::infinity(), 0.0, 0.0,
        0.01, FeConversionEngine::defaultNodeMergeTolerance(), out));
    EXPECT_TRUE(out.nodes().empty());
    EXPECT_TRUE(out.trusses().empty());
}

TEST(FeConversionEngine, invalid_tolerances_are_rejected)
{
    DxfData dxf;
    dxf.addPoint(DxfPoint(1.0, 2.0, 3.0));

    FeData out;
    EXPECT_FALSE(FeConversionEngine().convert(
        dxf, 0.0, 0.0, 0.0,
        std::numeric_limits<double>::quiet_NaN(),
        FeConversionEngine::defaultNodeMergeTolerance(), out));
    EXPECT_FALSE(FeConversionEngine().convert(
        dxf, 0.0, 0.0, 0.0,
        0.01, -1.0, out));
}

TEST(FeConversionEngine, ellipse_major_axis_remains_relative_when_base_is_added)
{
    DxfData dxf;
    dxf.addEllipse(DxfEllipse(
        DxfPoint(10.0, 20.0, 0.0), DxfPoint(3.0, 4.0, 0.0),
        0.5, 0.0, 2.0 * std::acos(-1.0), true));

    FeData out;
    ASSERT_TRUE(FeConversionEngine().convert(
        dxf, 100.0, 200.0, 0.0,
        0.1, FeConversionEngine::defaultNodeMergeTolerance(), out));

    ASSERT_FALSE(out.nodes().empty());
    // Parameter zero is center + the relative major-axis vector.
    EXPECT_NEAR(out.nodes().front().x, 113.0, 1e-9);
    EXPECT_NEAR(out.nodes().front().y, 224.0, 1e-9);
    EXPECT_NEAR(out.nodes().front().z, 0.0, 1e-9);
}

TEST(FeConversionEngine, final_nodes_and_trusses_share_budget) {
    DxfData dxf;
    dxf.addLine(DxfLine(
        DxfPoint(0.0, 0.0, 0.0), DxfPoint(1.0, 0.0, 0.0)));

    FeData out;
    EXPECT_FALSE(FeConversionEngine().convert(
        dxf, 0.0, 0.0, 0.0,
        0.01, FeConversionEngine::defaultNodeMergeTolerance(), out, 2));
    EXPECT_EQ(out.errorCode(), DxfImportErrorCode::ConversionLimit);
    EXPECT_TRUE(out.nodes().empty());
    EXPECT_TRUE(out.trusses().empty());
}

TEST(FeConversionEngine, reports_point_and_line_merge_progress)
{
    DxfData dxf;
    dxf.addPoint(DxfPoint(2.0, 3.0, 0.0));
    dxf.addLine(DxfLine(
        DxfPoint(0.0, 0.0, 0.0), DxfPoint(1.0, 0.0, 0.0)));

    std::vector<std::tuple<QString, int, int>> updates;
    FeConversionEngine engine;
    engine.setProgressCallback(
        [&updates](const QString& stage, int current, int total) {
            updates.emplace_back(stage, current, total);
            return true;
        });

    FeData out;
    ASSERT_TRUE(engine.convert(
        dxf, 0.0, 0.0, 0.0, 0.01,
        FeConversionEngine::defaultNodeMergeTolerance(), out));

    ASSERT_EQ(updates.size(), 4u);
    EXPECT_EQ(std::get<0>(updates[0]), QStringLiteral("Merging FE points"));
    EXPECT_EQ(std::get<1>(updates[0]), 0);
    EXPECT_EQ(std::get<2>(updates[0]), 1);
    EXPECT_EQ(std::get<0>(updates[1]), QStringLiteral("Merging FE points"));
    EXPECT_EQ(std::get<1>(updates[1]), 1);
    EXPECT_EQ(std::get<0>(updates[2]), QStringLiteral("Merging FE lines"));
    EXPECT_EQ(std::get<1>(updates[2]), 0);
    EXPECT_EQ(std::get<2>(updates[2]), 1);
    EXPECT_EQ(std::get<0>(updates[3]), QStringLiteral("Merging FE lines"));
    EXPECT_EQ(std::get<1>(updates[3]), 1);
}

TEST(FeConversionEngine, cancellation_clears_partial_conversion)
{
    DxfData dxf;
    dxf.addPoint(DxfPoint(2.0, 3.0, 0.0));
    dxf.addLine(DxfLine(
        DxfPoint(0.0, 0.0, 0.0), DxfPoint(1.0, 0.0, 0.0)));

    FeConversionEngine engine;
    engine.setProgressCallback(
        [](const QString& stage, int, int) {
            return stage != QStringLiteral("Merging FE lines");
        });

    FeData out;
    EXPECT_FALSE(engine.convert(
        dxf, 0.0, 0.0, 0.0, 0.01,
        FeConversionEngine::defaultNodeMergeTolerance(), out));
    EXPECT_EQ(out.errorCode(), DxfImportErrorCode::Canceled);
    EXPECT_TRUE(out.nodes().empty());
    EXPECT_TRUE(out.trusses().empty());
}

TEST(FeData, incrementally_merges_many_nearby_nodes)
{
    FeData data;
    constexpr double tolerance = 1e-6;

    for (int i = 0; i < 1000; ++i) {
        const double x = static_cast<double>(i) * 10.0;
        const int original = data.addOrGetNode(x, 0.0, 0.0, tolerance);
        const int nearby = data.addOrGetNode(
            x + 2e-7, -2e-7, 0.0, tolerance);
        EXPECT_EQ(nearby, original);
    }

    EXPECT_EQ(data.nodes().size(), 1000u);
    EXPECT_EQ(data.stats().mergedNodes, 1000u);
}

TEST(FeData, tolerance_change_reindexes_all_existing_nodes)
{
    FeData data;
    const int first = data.addOrGetNode(0.0, 0.0, 0.0, 0.0);
    const int second = data.addOrGetNode(0.5, 0.0, 0.0, 0.0);
    ASSERT_NE(first, second);

    const int merged = data.addOrGetNode(0.4, 0.0, 0.0, 1.0);

    EXPECT_TRUE(merged == first || merged == second);
    EXPECT_EQ(data.nodes().size(), 2u);
    EXPECT_EQ(data.stats().mergedNodes, 1u);
}

TEST(FeData, duplicate_trusses_are_found_independent_of_direction)
{
    FeData data;
    const int first = data.addTruss(7, 3);

    ASSERT_EQ(first, 0);
    EXPECT_EQ(data.trusses()[0].startNodeId, 3);
    EXPECT_EQ(data.trusses()[0].endNodeId, 7);
    EXPECT_EQ(data.addTruss(3, 7), -1);
    EXPECT_EQ(data.addTruss(7, 3), -1);
    EXPECT_EQ(data.trusses().size(), 1u);
    EXPECT_EQ(data.stats().skippedDuplicateTruss, 2u);
}

TEST(FeData, clear_resets_node_and_truss_indexes)
{
    FeData data;
    data.addOrGetNode(1.0, 2.0, 3.0, 1e-6);
    data.addTruss(0, 1);
    data.clear();

    EXPECT_EQ(data.addOrGetNode(1.0, 2.0, 3.0, 1e-6), 0);
    EXPECT_EQ(data.addTruss(0, 1), 0);
    EXPECT_EQ(data.nodes().size(), 1u);
    EXPECT_EQ(data.trusses().size(), 1u);
}

TEST(FeData, merge_tolerance_boundary_is_inclusive)
{
    constexpr double tolerance = 1.0;
    const double inside = std::nextafter(tolerance, 0.0);
    const double outside = std::nextafter(
        tolerance, std::numeric_limits<double>::infinity());

    FeData insideData;
    const int insideBase = insideData.addOrGetNode(0.0, 0.0, 0.0, tolerance);
    EXPECT_EQ(
        insideData.addOrGetNode(inside, 0.0, 0.0, tolerance),
        insideBase);

    FeData boundaryData;
    const int boundaryBase = boundaryData.addOrGetNode(
        0.0, 0.0, 0.0, tolerance);
    EXPECT_EQ(
        boundaryData.addOrGetNode(tolerance, 0.0, 0.0, tolerance),
        boundaryBase);

    FeData outsideData;
    const int outsideBase = outsideData.addOrGetNode(
        0.0, 0.0, 0.0, tolerance);
    EXPECT_NE(
        outsideData.addOrGetNode(outside, 0.0, 0.0, tolerance),
        outsideBase);
}

TEST(FeData, largeCoordinatesWithTinyToleranceRemainStable)
{
    constexpr double coordinate = 1.0e150;
    constexpr double tolerance = 1.0e-12;

    FeData data;
    const int first = data.addOrGetNode(
        coordinate, -coordinate, coordinate, tolerance);
    EXPECT_EQ(data.addOrGetNode(
        coordinate, -coordinate, coordinate, tolerance), first);
    EXPECT_NE(data.addOrGetNode(
        -coordinate, coordinate, -coordinate, tolerance), first);
    EXPECT_EQ(data.nodes().size(), 2u);
}
