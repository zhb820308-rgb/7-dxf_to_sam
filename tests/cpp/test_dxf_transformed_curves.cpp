#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include "SketchConversion.h"
#include "DxfBlockExpansion.h"
#include "DxfData.h"
#include "FeConversion.h"

namespace {

constexpr double kTolerance = 1e-8;
constexpr double kCurveTolerance = 0.01;
constexpr double kPi = 3.14159265358979323846;

using BlockMap = std::unordered_map<std::string, DxfBlock>;

void expectPointNear(
    const DxfPoint& point,
    double x,
    double y,
    double z,
    double tolerance = kTolerance)
{
    EXPECT_NEAR(point.x(), x, tolerance);
    EXPECT_NEAR(point.y(), y, tolerance);
    EXPECT_NEAR(point.z(), z, tolerance);
}

void expectLineNear(
    const DxfLine& line,
    double sx,
    double sy,
    double sz,
    double ex,
    double ey,
    double ez,
    double tolerance = kTolerance)
{
    expectPointNear(line.start(), sx, sy, sz, tolerance);
    expectPointNear(line.end(), ex, ey, ez, tolerance);
}

InsertInfo insertOf(
    const std::string& blockName,
    double x = 0.0,
    double y = 0.0,
    double z = 0.0)
{
    InsertInfo insert;
    insert.blockName = blockName;
    insert.insertX = x;
    insert.insertY = y;
    insert.insertZ = z;
    return insert;
}

bool expand(
    DxfData& output,
    const BlockMap& blocks,
    const InsertInfo& insert,
    QString& error,
    double tolerance = kCurveTolerance)
{
    const std::vector<InsertInfo> inserts{insert};
    const std::set<std::string> ignoredLayers;
    const DxfBlockExpansionResult result = expandDxfBlocks({
        output,
        blocks,
        inserts,
        ignoredLayers,
        tolerance,
        0,
        DxfImportDefaults::kDefaultMaxOutputEntities});
    error = result.message;
    return result.succeeded();
}

DxfSpline fitSpline(
    const DxfPoint& start,
    const DxfPoint& middle,
    const DxfPoint& end)
{
    return DxfSpline(
        {}, {}, {}, {start, middle, end}, 2, 0,
        1.0, 0.0, 0.5,
        0.0, 1.0, -0.5);
}

DxfBlock complexCurveBlock()
{
    DxfBlock block;
    block.setName("CURVES");
    block.addArc(DxfArc(
        DxfPoint(1.0, 2.0, 3.0), 2.0, 0.0, kPi / 2.0, true));
    block.addEllipse(DxfEllipse(
        DxfPoint(2.0, -1.0, 4.0), DxfPoint(3.0, 0.0, 1.0),
        0.5, 0.0, kPi, true));
    block.addLWPolyline(DxfLWPolyline(
        {DxfPoint(0.0, 0.0, 5.0), DxfPoint(1.0, 0.0, 5.0)},
        {0.5}, false, 5.0));
    block.addSpline(fitSpline(
        DxfPoint(0.0, 0.0, 1.0),
        DxfPoint(1.0, 2.0, 1.5),
        DxfPoint(2.0, 1.0, 2.0)));
    return block;
}

bool samePoint(
    const DxfPoint& point,
    const FeNode& node,
    double tolerance)
{
    return std::fabs(point.x() - node.x) <= tolerance
        && std::fabs(point.y() - node.y) <= tolerance
        && std::fabs(point.z() - node.z) <= tolerance;
}

bool matchesTruss(
    const DxfLine& line,
    const FeTruss& truss,
    const std::vector<FeNode>& nodes,
    double tolerance)
{
    const FeNode& start = nodes.at(static_cast<std::size_t>(truss.startNodeId));
    const FeNode& end = nodes.at(static_cast<std::size_t>(truss.endNodeId));
    return (samePoint(line.start(), start, tolerance)
            && samePoint(line.end(), end, tolerance))
        || (samePoint(line.start(), end, tolerance)
            && samePoint(line.end(), start, tolerance));
}

} // namespace

TEST(DxfTransformedCurves, MirroredRotationPreservesCurveParametersAndZ)
{
    BlockMap blocks;
    blocks.emplace("CURVES", complexCurveBlock());

    InsertInfo insert = insertOf("CURVES", 10.0, 20.0, 30.0);
    insert.scaleX = -2.0;
    insert.scaleY = 2.0;
    insert.scaleZ = 3.0;
    insert.angle = kPi / 2.0;

    DxfData output;
    QString error;
    ASSERT_TRUE(expand(output, blocks, insert, error))
        << error.toStdString();

    ASSERT_EQ(output.arcs().size(), 1u);
    const DxfArc& arc = output.arcs().front();
    expectPointNear(arc.center(), 6.0, 18.0, 39.0);
    EXPECT_NEAR(arc.radius(), 4.0, kTolerance);
    EXPECT_NEAR(arc.startAngle(), -kPi / 2.0, kTolerance);
    EXPECT_NEAR(arc.endAngle(), -kPi, kTolerance);
    EXPECT_FALSE(arc.isCCW());

    ASSERT_EQ(output.ellipses().size(), 1u);
    const DxfEllipse& ellipse = output.ellipses().front();
    expectPointNear(ellipse.center(), 12.0, 16.0, 42.0);
    expectPointNear(ellipse.majorAxisEnd(), 0.0, -6.0, 3.0);
    EXPECT_NEAR(ellipse.ratio(), 0.5, kTolerance);
    EXPECT_NEAR(ellipse.startParam(), 0.0, kTolerance);
    EXPECT_NEAR(ellipse.endParam(), -kPi, kTolerance);
    EXPECT_FALSE(ellipse.isCCW());

    ASSERT_EQ(output.lwPolylines().size(), 1u);
    const DxfLWPolyline& polyline = output.lwPolylines().front();
    ASSERT_EQ(polyline.vertices().size(), 2u);
    expectPointNear(polyline.vertices()[0], 10.0, 20.0, 45.0);
    expectPointNear(polyline.vertices()[1], 10.0, 18.0, 45.0);
    ASSERT_EQ(polyline.bulges().size(), 1u);
    EXPECT_DOUBLE_EQ(polyline.bulges()[0], -0.5);
    EXPECT_NEAR(polyline.constZ(), 45.0, kTolerance);

    ASSERT_EQ(output.splines().size(), 1u);
    const DxfSpline& spline = output.splines().front();
    ASSERT_EQ(spline.fitPoints().size(), 3u);
    expectPointNear(spline.fitPoints().front(), 10.0, 20.0, 33.0);
    expectPointNear(spline.fitPoints().back(), 8.0, 16.0, 36.0);
    EXPECT_NEAR(spline.tgStartX(), 0.0, kTolerance);
    EXPECT_NEAR(spline.tgStartY(), -2.0, kTolerance);
    EXPECT_NEAR(spline.tgStartZ(), 1.5, kTolerance);
    EXPECT_NEAR(spline.tgEndX(), -2.0, kTolerance);
    EXPECT_NEAR(spline.tgEndY(), 0.0, kTolerance);
    EXPECT_NEAR(spline.tgEndZ(), -1.5, kTolerance);
}

TEST(DxfTransformedCurves, EachNegativeScaleCombinationHasExactCoordinates)
{
    DxfBlock block;
    block.setName("POLYLINE");
    block.addLWPolyline(DxfLWPolyline(
        {DxfPoint(1.0, 2.0, 5.0), DxfPoint(3.0, 4.0, 5.0)},
        {0.25}, false, 5.0));
    BlockMap blocks;
    blocks.emplace("POLYLINE", block);

    struct ScaleCase {
        double x;
        double y;
        double expectedBulge;
    };
    const std::array<ScaleCase, 3> cases = {{
        {-1.0,  1.0, -0.25},
        { 1.0, -1.0, -0.25},
        {-1.0, -1.0,  0.25},
    }};

    for (const ScaleCase& scale : cases) {
        InsertInfo insert = insertOf("POLYLINE", 10.0, 20.0, 30.0);
        insert.scaleX = scale.x;
        insert.scaleY = scale.y;
        insert.scaleZ = -2.0;

        DxfData output;
        QString error;
        ASSERT_TRUE(expand(output, blocks, insert, error))
            << error.toStdString();
        ASSERT_EQ(output.lwPolylines().size(), 1u);
        const DxfLWPolyline& polyline = output.lwPolylines().front();
        expectPointNear(
            polyline.vertices()[0],
            10.0 + scale.x, 20.0 + 2.0 * scale.y, 20.0);
        expectPointNear(
            polyline.vertices()[1],
            10.0 + 3.0 * scale.x, 20.0 + 4.0 * scale.y, 20.0);
        EXPECT_DOUBLE_EQ(polyline.bulges().front(), scale.expectedBulge);
        EXPECT_NEAR(polyline.constZ(), 20.0, kTolerance);
    }
}

TEST(DxfTransformedCurves, NestedNonuniformRotationHasExactEndpointsAndBounds)
{
    DxfBlock leaf;
    leaf.setName("LEAF");
    leaf.addArc(DxfArc(
        DxfPoint(0.0, 0.0, 2.0), 1.0, 0.0, kPi / 2.0, true));

    DxfBlock middle;
    middle.setName("MIDDLE");
    InsertInfo leafInsert = insertOf("LEAF", 0.0, 0.0, 5.0);
    leafInsert.scaleX = 2.0;
    leafInsert.scaleY = 1.0;
    leafInsert.scaleZ = 2.0;
    middle.addInsert(leafInsert);

    DxfBlock outer;
    outer.setName("OUTER");
    outer.addInsert(insertOf("MIDDLE"));

    BlockMap blocks;
    blocks.emplace("LEAF", leaf);
    blocks.emplace("MIDDLE", middle);
    blocks.emplace("OUTER", outer);

    InsertInfo outerInsert = insertOf("OUTER", 10.0, 20.0, 7.0);
    outerInsert.scaleX = 1.0;
    outerInsert.scaleY = 3.0;
    outerInsert.scaleZ = 3.0;
    outerInsert.angle = kPi / 2.0;

    DxfData output;
    QString error;
    ASSERT_TRUE(expand(output, blocks, outerInsert, error, 0.001))
        << error.toStdString();
    ASSERT_FALSE(output.lines().empty());
    EXPECT_TRUE(output.arcs().empty());

    expectPointNear(output.lines().front().start(), 10.0, 22.0, 34.0);
    expectPointNear(output.lines().back().end(), 7.0, 20.0, 34.0);

    double minX = std::numeric_limits<double>::infinity();
    double minY = std::numeric_limits<double>::infinity();
    double maxX = -std::numeric_limits<double>::infinity();
    double maxY = -std::numeric_limits<double>::infinity();
    for (const DxfLine& line : output.lines()) {
        for (const DxfPoint* point : {&line.start(), &line.end()}) {
            minX = std::min(minX, point->x());
            minY = std::min(minY, point->y());
            maxX = std::max(maxX, point->x());
            maxY = std::max(maxY, point->y());
            EXPECT_NEAR(point->z(), 34.0, kTolerance);
        }
    }
    EXPECT_NEAR(minX, 7.0, kTolerance);
    EXPECT_NEAR(minY, 20.0, kTolerance);
    EXPECT_NEAR(maxX, 10.0, kTolerance);
    EXPECT_NEAR(maxY, 22.0, kTolerance);
}

TEST(DxfTransformedCurves, SketchAndFeSegmentsMatchAfterBlockTransform)
{
    BlockMap blocks;
    blocks.emplace("CURVES", complexCurveBlock());

    InsertInfo insert = insertOf("CURVES", 40.0, -20.0, 6.0);
    insert.scaleX = 1.5;
    insert.scaleY = 1.5;
    insert.scaleZ = 2.0;
    insert.angle = kPi / 6.0;

    DxfData expanded;
    QString error;
    ASSERT_TRUE(expand(expanded, blocks, insert, error))
        << error.toStdString();

    SamData sketch;
    ASSERT_TRUE(ConversionEngine().convert(
        expanded, 0.0, 0.0, 0.0, kCurveTolerance, sketch));

    FeData fe;
    ASSERT_TRUE(FeConversionEngine().convert(
        expanded, 0.0, 0.0, 0.0, kCurveTolerance,
        1e-9, fe));

    ASSERT_EQ(sketch.lines().size(), fe.trusses().size());
    std::vector<bool> matched(fe.trusses().size(), false);
    for (const DxfLine& line : sketch.lines()) {
        bool found = false;
        for (std::size_t index = 0; index < fe.trusses().size(); ++index) {
            if (!matched[index]
                && matchesTruss(
                    line, fe.trusses()[index], fe.nodes(), 1e-7)) {
                matched[index] = true;
                found = true;
                break;
            }
        }
        EXPECT_TRUE(found);
    }

    std::vector<const CurveSegmentSource*> splineSegments;
    for (const CurveSegmentSource& source : sketch.curveSegments()) {
        if (source.parentType == EntityType::Spline)
            splineSegments.push_back(&source);
    }
    ASSERT_FALSE(splineSegments.empty());
    std::sort(
        splineSegments.begin(), splineSegments.end(),
        [](const CurveSegmentSource* left, const CurveSegmentSource* right) {
            return left->segmentIndex < right->segmentIndex;
        });
    const DxfLine& first = sketch.lines().at(splineSegments.front()->lineIndex);
    const DxfLine& last = sketch.lines().at(splineSegments.back()->lineIndex);
    expectPointNear(first.start(), 40.0, -20.0, 8.0, 1e-7);
    expectPointNear(
        last.end(),
        40.0 + 3.0 * std::cos(kPi / 6.0)
            - 1.5 * std::sin(kPi / 6.0),
        -20.0 + 3.0 * std::sin(kPi / 6.0)
            + 1.5 * std::cos(kPi / 6.0),
        10.0,
        1e-7);
}
