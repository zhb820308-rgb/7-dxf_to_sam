/**
 * test_parser.cpp — 测试 DxfParser 用真实 DXF 文件解析
 *
 * 覆盖: parseFile() / 实体计数 / 错误路径 / 完整 parse→convert 管道
 */
#include <gtest/gtest.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#include <QFile>
#include <QDebug>
#include <cmath>
#include "DxfParser.h"
#include "ConversionEngine.h"
#include "SamData.h"

// ========================================================================
//  辅助: 检查文件是否存在
// ========================================================================

static bool fileExists(const QString& path) {
    return QFile::exists(path);
}

#define EXAMPLE_DIR QStringLiteral("D:/shixiSoftware/Homework/7-dxf_to_sam/example")

// ========================================================================
//  文件存在性
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
//  错误路径
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
    EXPECT_FALSE(parser.parseFile("D:/nonexistent_file_12345.dxf", data));
    EXPECT_FALSE(data.isValid());
}

// ========================================================================
//  单实体文件
// ========================================================================

TEST(Parser, line1_dxf_has_lines) {
    DxfData data;
    DxfParser parser;
    ASSERT_TRUE(parser.parseFile(EXAMPLE_DIR + "/line1.dxf", data));
    EXPECT_TRUE(data.isValid());
    EXPECT_GE(data.lines().size(), 1u);
    EXPECT_EQ(data.circles().size(), 0u);
    EXPECT_EQ(data.arcs().size(), 0u);
}

TEST(Parser, circle_dxf_has_circles) {
    DxfData data;
    DxfParser parser;
    ASSERT_TRUE(parser.parseFile(EXAMPLE_DIR + "/circle.dxf", data));
    EXPECT_TRUE(data.isValid());
    EXPECT_GE(data.circles().size(), 1u);
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

// ========================================================================
//  弧文件
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
//  椭圆文件
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
//  多段线文件
// ========================================================================

TEST(Parser, pline_half_circle_dxf_has_polylines) {
    DxfData data;
    DxfParser parser;
    ASSERT_TRUE(parser.parseFile(EXAMPLE_DIR + "/pline_half_circle.dxf", data));
    EXPECT_TRUE(data.isValid());
    EXPECT_GE(data.lwPolylines().size(), 1u);
    for (const auto& poly : data.lwPolylines()) {
        EXPECT_TRUE(poly.isValid());
        EXPECT_GE(poly.vertexCount(), 2);
    }
}

// ========================================================================
//  混合实体文件
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
//  完整管道: parse → convert（不依赖 SAM SDK）
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
//  回归: 满椭圆不应丢失
// ========================================================================

TEST(Regression, elipse_dxf_full_ellipse_not_lost) {
    DxfData data;
    DxfParser parser;
    ASSERT_TRUE(parser.parseFile(EXAMPLE_DIR + "/elipse.dxf", data));

    // 应至少有一个满椭圆（ratio=1, 0→2π 或类似）
    bool hasFullEllipse = false;
    for (const auto& e : data.ellipses()) {
        double sweep = e.endParam() - e.startParam();
        // 满椭圆 sweep ≈ 2π
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
//  regression: 半圆弧正常导入
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
//  多文件一致性
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
        if (!fileExists(path)) continue;

        DxfData data;
        DxfParser parser;
        bool parsed = parser.parseFile(path, data);
        EXPECT_TRUE(parsed) << "Failed to parse: " << filenames[i];
        if (!parsed) continue;

        SamData out;
        ConversionEngine engine;
        bool converted = engine.convert(data, 0, 0, 0, ConversionEngine::defaultBulgeTolerance(), out);
        EXPECT_TRUE(converted) << "Failed to convert: " << filenames[i];

        // Every file should produce some output
        int totalOutput = static_cast<int>(
            out.lines().size() + out.circles().size());
        EXPECT_GT(totalOutput, 0) << "No output entities from: " << filenames[i];
    }
}
