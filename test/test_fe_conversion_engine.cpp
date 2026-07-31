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
