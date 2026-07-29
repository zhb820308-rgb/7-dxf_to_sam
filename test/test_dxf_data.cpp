/**
 * test_dxf_data.cpp — 测试 DxfData 实体有效性验证
 *
 * 覆盖: DxfPoint / DxfLine / DxfCircle / DxfArc / DxfEllipse / DxfLWPolyline
 */
#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include "DxfData.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ========================================================================
//  DxfPoint
// ========================================================================

TEST(DxfPoint, default_constructor) {
    DxfPoint p;
    EXPECT_TRUE(p.isValid());
    EXPECT_DOUBLE_EQ(p.x(), 0.0);
    EXPECT_DOUBLE_EQ(p.y(), 0.0);
    EXPECT_DOUBLE_EQ(p.z(), 0.0);
}

TEST(DxfPoint, valid_point) {
    DxfPoint p(1.0, 2.0, 3.0);
    EXPECT_TRUE(p.isValid());
}

TEST(DxfPoint, nan_rejected) {
    DxfPoint p(std::nan(""), 0.0, 0.0);
    EXPECT_FALSE(p.isValid());
}

TEST(DxfPoint, inf_rejected) {
    DxfPoint p(0.0, INFINITY, 0.0);
    EXPECT_FALSE(p.isValid());
}

TEST(DxfPoint, setters_work) {
    DxfPoint p;
    p.setX(5.0);
    p.setY(6.0);
    p.setZ(7.0);
    EXPECT_DOUBLE_EQ(p.x(), 5.0);
    EXPECT_DOUBLE_EQ(p.y(), 6.0);
    EXPECT_DOUBLE_EQ(p.z(), 7.0);
}

// ========================================================================
//  DxfLine
// ========================================================================

TEST(DxfLine, valid_line) {
    DxfPoint s(0.0, 0.0, 0.0);
    DxfPoint e(10.0, 0.0, 0.0);
    DxfLine line(s, e);
    EXPECT_TRUE(line.isValid());
}

TEST(DxfLine, zero_length_rejected) {
    DxfPoint s(5.0, 5.0, 5.0);
    DxfPoint e(5.0, 5.0, 5.0);
    DxfLine line(s, e);
    EXPECT_FALSE(line.isValid());
}

TEST(DxfLine, nan_endpoint_rejected) {
    DxfPoint s(0.0, 0.0, 0.0);
    DxfPoint e(std::nan(""), 0.0, 0.0);
    DxfLine line(s, e);
    EXPECT_FALSE(line.isValid());
}

// ========================================================================
//  DxfCircle
// ========================================================================

TEST(DxfCircle, valid_circle) {
    DxfCircle c(DxfPoint(0, 0, 0), 5.0);
    EXPECT_TRUE(c.isValid());
}

TEST(DxfCircle, zero_radius_rejected) {
    DxfCircle c(DxfPoint(0, 0, 0), 0.0);
    EXPECT_FALSE(c.isValid());
}

TEST(DxfCircle, negative_radius_rejected) {
    DxfCircle c(DxfPoint(0, 0, 0), -1.0);
    EXPECT_FALSE(c.isValid());
}

TEST(DxfCircle, nan_center_rejected) {
    DxfCircle c(DxfPoint(std::nan(""), 0.0, 0.0), 5.0);
    EXPECT_FALSE(c.isValid());
}

TEST(DxfCircle, nan_radius_rejected) {
    DxfCircle c(DxfPoint(0.0, 0.0, 0.0), std::nan(""));
    EXPECT_FALSE(c.isValid());
}

// ========================================================================
//  DxfArc
// ========================================================================

TEST(DxfArc, valid_arc) {
    DxfArc arc(DxfPoint(0, 0, 0), 10.0, 0.0, M_PI / 2.0, true);
    EXPECT_TRUE(arc.isValid());
}

TEST(DxfArc, same_angles_rejected) {
    DxfArc arc(DxfPoint(0, 0, 0), 10.0, 1.0, 1.0, true);
    EXPECT_FALSE(arc.isValid());
}

TEST(DxfArc, zero_radius_rejected) {
    DxfArc arc(DxfPoint(0, 0, 0), 0.0, 0.0, M_PI, true);
    EXPECT_FALSE(arc.isValid());
}

TEST(DxfArc, nan_angles_rejected) {
    DxfArc arc(DxfPoint(0.0, 0.0, 0.0), 10.0, std::nan(""), M_PI, true);
    EXPECT_FALSE(arc.isValid());
}

TEST(DxfArc, full_circle_arc_valid) {
    // start=0, end=2π are different values → passes isValid()
    DxfArc arc(DxfPoint(0, 0, 0), 10.0, 0.0, 2.0 * M_PI, true);
    EXPECT_TRUE(arc.isValid());
}

// ========================================================================
//  DxfEllipse
// ========================================================================

TEST(DxfEllipse, valid_ellipse) {
    DxfEllipse e(DxfPoint(0, 0, 0), DxfPoint(10, 0, 0),
                 0.5, 0.0, M_PI, true);
    EXPECT_TRUE(e.isValid());
}

TEST(DxfEllipse, full_ellipse_valid) {
    DxfEllipse e(DxfPoint(0, 0, 0), DxfPoint(10, 0, 0),
                 1.0, 0.0, 2.0 * M_PI, true);
    EXPECT_TRUE(e.isValid());
}

TEST(DxfEllipse, zero_major_axis_rejected) {
    DxfEllipse e(DxfPoint(0, 0, 0), DxfPoint(0, 0, 0),
                 1.0, 0.0, M_PI, true);
    EXPECT_FALSE(e.isValid());
}

TEST(DxfEllipse, zero_ratio_rejected) {
    DxfEllipse e(DxfPoint(0, 0, 0), DxfPoint(10, 0, 0),
                 0.0, 0.0, M_PI, true);
    EXPECT_FALSE(e.isValid());
}

TEST(DxfEllipse, negative_ratio_rejected) {
    DxfEllipse e(DxfPoint(0, 0, 0), DxfPoint(10, 0, 0),
                 -0.5, 0.0, M_PI, true);
    EXPECT_FALSE(e.isValid());
}

TEST(DxfEllipse, same_params_rejected) {
    DxfEllipse e(DxfPoint(0, 0, 0), DxfPoint(10, 0, 0),
                 1.0, 1.5, 1.5, true);
    EXPECT_FALSE(e.isValid());
}

// ========================================================================
//  DxfLWPolyline
// ========================================================================

TEST(DxfLWPolyline, valid_open_polyline) {
    std::vector<DxfPoint> verts = { DxfPoint(0, 0, 0), DxfPoint(10, 0, 0), DxfPoint(10, 10, 0) };
    std::vector<double> bulges = { 0.0, 0.0 };
    DxfLWPolyline poly(verts, bulges, false);
    EXPECT_TRUE(poly.isValid());
    EXPECT_FALSE(poly.isClosed());
    EXPECT_EQ(poly.vertexCount(), 3);
}

TEST(DxfLWPolyline, valid_closed_polyline) {
    std::vector<DxfPoint> verts = { DxfPoint(0, 0, 0), DxfPoint(10, 0, 0), DxfPoint(10, 10, 0) };
    std::vector<double> bulges = { 0.0, 0.0, 0.0 };  // N bulges for closed
    DxfLWPolyline poly(verts, bulges, true);
    EXPECT_TRUE(poly.isValid());
    EXPECT_TRUE(poly.isClosed());
}

TEST(DxfLWPolyline, too_few_vertices_rejected) {
    std::vector<DxfPoint> verts = { DxfPoint(0, 0, 0) };
    std::vector<double> bulges = {};
    DxfLWPolyline poly(verts, bulges, false);
    EXPECT_FALSE(poly.isValid());
}

TEST(DxfLWPolyline, bulge_count_mismatch_rejected) {
    std::vector<DxfPoint> verts = { DxfPoint(0, 0, 0), DxfPoint(10, 0, 0), DxfPoint(10, 10, 0) };
    std::vector<double> bulges = { 0.0 };  // should be 2 for open, but got 1
    DxfLWPolyline poly(verts, bulges, false);
    EXPECT_FALSE(poly.isValid());
}

TEST(DxfLWPolyline, closed_bulge_count_mismatch_rejected) {
    std::vector<DxfPoint> verts = { DxfPoint(0, 0, 0), DxfPoint(10, 0, 0) };
    std::vector<double> bulges = { 0.0 };  // should be 2 for closed, got 1
    DxfLWPolyline poly(verts, bulges, true);
    EXPECT_FALSE(poly.isValid());
}

TEST(DxfLWPolyline, nan_bulge_rejected) {
    std::vector<DxfPoint> verts = { DxfPoint(0.0, 0.0, 0.0), DxfPoint(10.0, 0.0, 0.0) };
    std::vector<double> bulges = { std::nan("") };
    DxfLWPolyline poly(verts, bulges, false);
    EXPECT_FALSE(poly.isValid());
}

TEST(DxfLWPolyline, nan_vertex_rejected) {
    std::vector<DxfPoint> verts = { DxfPoint(0.0, 0.0, 0.0), DxfPoint(std::nan(""), 0.0, 0.0) };
    std::vector<double> bulges = { 0.0 };
    DxfLWPolyline poly(verts, bulges, false);
    EXPECT_FALSE(poly.isValid());
}

TEST(DxfLWPolyline, constZ_preserved) {
    std::vector<DxfPoint> verts = { DxfPoint(0, 0, 0), DxfPoint(10, 0, 0) };
    std::vector<double> bulges = { 0.0 };
    DxfLWPolyline poly(verts, bulges, false, 5.0);
    EXPECT_DOUBLE_EQ(poly.constZ(), 5.0);
}

// ========================================================================
//  boundary / edge cases
// ========================================================================

TEST(DxfPoint, negative_infinity_rejected) {
    DxfPoint p(0.0, -INFINITY, 0.0);
    EXPECT_FALSE(p.isValid());
}

TEST(DxfLine, vertical_3d_line_valid) {
    // endpoints differ only in Z → should be valid
    DxfPoint s(0.0, 0.0, 0.0);
    DxfPoint e(0.0, 0.0, 5.0);
    DxfLine line(s, e);
    EXPECT_TRUE(line.isValid());
}

TEST(DxfLine, one_nan_endpoint_rejected) {
    DxfPoint s(0.0, 0.0, 0.0);
    DxfPoint e(0.0, std::nan(""), 0.0);
    DxfLine line(s, e);
    EXPECT_FALSE(line.isValid());
}

TEST(DxfCircle, tiny_positive_radius_valid) {
    DxfCircle c(DxfPoint(0.0, 0.0, 0.0), 1e-12);
    EXPECT_TRUE(c.isValid());
}

TEST(DxfCircle, infinite_radius_rejected) {
    DxfCircle c(DxfPoint(0.0, 0.0, 0.0), INFINITY);
    EXPECT_FALSE(c.isValid());
}

TEST(DxfArc, tiny_radius_valid) {
    DxfArc arc(DxfPoint(0.0, 0.0, 0.0), 1e-10, 0.0, M_PI, true);
    EXPECT_TRUE(arc.isValid());
}

TEST(DxfArc, infinite_radius_rejected) {
    DxfArc arc(DxfPoint(0.0, 0.0, 0.0), INFINITY, 0.0, M_PI, true);
    EXPECT_FALSE(arc.isValid());
}

TEST(DxfArc, barely_different_angles_valid) {
    DxfArc arc(DxfPoint(0.0, 0.0, 0.0), 10.0, 1.0, 1.0 + 1e-10, true);
    EXPECT_TRUE(arc.isValid());
}

TEST(DxfEllipse, ratio_one_circle) {
    // ratio=1.0 → circular ellipse
    DxfEllipse e(DxfPoint(0.0, 0.0, 0.0), DxfPoint(10.0, 0.0, 0.0),
                 1.0, 0.0, M_PI, true);
    EXPECT_TRUE(e.isValid());
}

TEST(DxfEllipse, vertical_major_axis) {
    // major axis along Y only
    DxfEllipse e(DxfPoint(0.0, 0.0, 0.0), DxfPoint(0.0, 10.0, 0.0),
                 0.5, 0.0, M_PI, true);
    EXPECT_TRUE(e.isValid());
    // major length should be 10
    EXPECT_DOUBLE_EQ(e.majorAxisEnd().x(), 0.0);
    EXPECT_DOUBLE_EQ(e.majorAxisEnd().y(), 10.0);
}

TEST(DxfEllipse, negative_major_axis) {
    DxfEllipse e(DxfPoint(5.0, 5.0, 0.0), DxfPoint(-10.0, 0.0, 0.0),
                 0.5, 0.0, M_PI, true);
    EXPECT_TRUE(e.isValid());
}

TEST(DxfEllipse, nan_major_axis_rejected) {
    DxfEllipse e(DxfPoint(0.0, 0.0, 0.0), DxfPoint(std::nan(""), 0.0, 0.0),
                 1.0, 0.0, M_PI, true);
    EXPECT_FALSE(e.isValid());
}

TEST(DxfEllipse, tiny_ratio_valid) {
    DxfEllipse e(DxfPoint(0.0, 0.0, 0.0), DxfPoint(10.0, 0.0, 0.0),
                 1e-10, 0.0, M_PI, true);
    EXPECT_TRUE(e.isValid());
}

TEST(DxfLWPolyline, single_open_segment) {
    std::vector<DxfPoint> verts = { DxfPoint(0.0, 0.0, 0.0), DxfPoint(10.0, 10.0, 0.0) };
    std::vector<double> bulges = { 0.5 };
    DxfLWPolyline poly(verts, bulges, false);
    EXPECT_TRUE(poly.isValid());
    EXPECT_EQ(poly.vertexCount(), 2);
}

TEST(DxfLWPolyline, inf_vertex_rejected) {
    std::vector<DxfPoint> verts = { DxfPoint(0.0, 0.0, 0.0), DxfPoint(INFINITY, 0.0, 0.0) };
    std::vector<double> bulges = { 0.0 };
    DxfLWPolyline poly(verts, bulges, false);
    EXPECT_FALSE(poly.isValid());
}

TEST(DxfLWPolyline, negative_constZ) {
    std::vector<DxfPoint> verts = { DxfPoint(0.0, 0.0, 0.0), DxfPoint(10.0, 0.0, 0.0) };
    std::vector<double> bulges = { 0.0 };
    DxfLWPolyline poly(verts, bulges, false, -3.5);
    EXPECT_TRUE(poly.isValid());
    EXPECT_DOUBLE_EQ(poly.constZ(), -3.5);
}

TEST(DxfData, entity_count_sums_correctly) {
    DxfData data;
    data.addLine(DxfLine(DxfPoint(0, 0, 0), DxfPoint(1, 0, 0)));
    data.addCircle(DxfCircle(DxfPoint(0, 0, 0), 5.0));
    data.addArc(DxfArc(DxfPoint(0, 0, 0), 10.0, 0.0, M_PI, true));
    EXPECT_EQ(data.entityCount(), 3);  // 1 line + 1 circle + 1 arc
}

TEST(DxfData, clear_resets_everything) {
    DxfData data;
    data.addLine(DxfLine(DxfPoint(0, 0, 0), DxfPoint(1, 0, 0)));
    data.setValid(true);
    data.clear();
    EXPECT_FALSE(data.isValid());
    EXPECT_EQ(data.entityCount(), 0);
}
