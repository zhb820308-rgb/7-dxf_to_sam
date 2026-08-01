#include "DxfImportMode.h"

#include <gtest/gtest.h>

TEST(DxfImportMode, EmptyModeDefaultsToSketch)
{
    const auto result = selectDxfImportMode(QString(), QString(), QString());
    EXPECT_TRUE(result.valid);
    EXPECT_EQ(result.mode, DxfImportMode::Sketch);
}

TEST(DxfImportMode, ModeNamesAreCaseInsensitive)
{
    const auto sketch = selectDxfImportMode(
        QStringLiteral("sKeTcH"), QString(), QString());
    EXPECT_TRUE(sketch.valid);
    EXPECT_EQ(sketch.mode, DxfImportMode::Sketch);

    const auto finiteElement = selectDxfImportMode(
        QStringLiteral("fINITEeLEMENT"),
        QStringLiteral("Model"), QStringLiteral("Part"));
    EXPECT_TRUE(finiteElement.valid);
    EXPECT_EQ(finiteElement.mode, DxfImportMode::FiniteElement);
}

TEST(DxfImportMode, RejectsUnsupportedMode)
{
    const auto result = selectDxfImportMode(
        QStringLiteral("Mesh"), QString(), QString());
    EXPECT_FALSE(result.valid);
    EXPECT_STREQ(result.stage, "validate_mode");
    EXPECT_EQ(result.detail, " mode=\"Mesh\"");
    EXPECT_EQ(result.message,
        QStringLiteral("[importDxf] ERROR: unsupported importMode 'Mesh'"));
}

TEST(DxfImportMode, FiniteElementRequiresModelNameFirst)
{
    const auto result = selectDxfImportMode(
        QStringLiteral("FiniteElement"),
        QString(), QString());
    EXPECT_FALSE(result.valid);
    EXPECT_EQ(result.mode, DxfImportMode::FiniteElement);
    EXPECT_STREQ(result.stage, "validate_params");
    EXPECT_EQ(result.detail, " missing=\"modelName\"");
    EXPECT_EQ(result.message,
        QStringLiteral("[importDxf] ERROR: FE mode requires 'modelName'"));
}

TEST(DxfImportMode, FiniteElementRequiresPartName)
{
    const auto result = selectDxfImportMode(
        QStringLiteral("FiniteElement"),
        QStringLiteral("Model"), QString());
    EXPECT_FALSE(result.valid);
    EXPECT_STREQ(result.stage, "validate_params");
    EXPECT_EQ(result.detail, " missing=\"partName\"");
    EXPECT_EQ(result.message,
        QStringLiteral("[importDxf] ERROR: FE mode requires 'partName'"));
}
