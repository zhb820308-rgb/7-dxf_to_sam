#include <gtest/gtest.h>

#include <limits>

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

TEST(FeConversionEngine, final_nodes_and_trusses_share_budget)
{
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
