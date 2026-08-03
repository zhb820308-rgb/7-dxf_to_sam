#include "DxfData.h"

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

TEST(DxfImportRequest, DefaultsHaveOneCanonicalSource)
{
    const DxfImportRequest request;

    EXPECT_DOUBLE_EQ(
        request.curveTolerance,
        DxfImportDefaults::kCurveTolerance);
    EXPECT_DOUBLE_EQ(
        request.nodeMergeTolerance,
        DxfImportDefaults::kNodeMergeTolerance);
    EXPECT_EQ(
        request.maxOutputEntities,
        DxfImportDefaults::kDefaultMaxOutputEntities);
    EXPECT_TRUE(request.importMode.isEmpty());
}

TEST(DxfImportPreflight, DefaultRequestSelectsSketchAndSmallLimit)
{
    const DxfImportPreflightResult result =
        validateDxfImportRequest(DxfImportRequest());

    EXPECT_TRUE(result.valid);
    EXPECT_EQ(result.mode, DxfImportMode::Sketch);
    EXPECT_EQ(
        result.outputLimit,
        DxfImportDefaults::kSmallDrawingEntityLimit);
}

TEST(DxfImportPreflight, RejectsUnsupportedModeBeforeImportWork)
{
    DxfImportRequest request;
    request.importMode = QStringLiteral("Mesh");

    const DxfImportPreflightResult result =
        validateDxfImportRequest(request);

    EXPECT_FALSE(result.valid);
    EXPECT_EQ(result.stage, "validate_mode");
    EXPECT_EQ(result.detail, " mode=\"Mesh\"");
}

TEST(DxfImportPreflight, FiniteElementRequiresNames)
{
    DxfImportRequest request;
    request.importMode = QStringLiteral("FiniteElement");

    const DxfImportPreflightResult missingModel =
        validateDxfImportRequest(request);
    EXPECT_FALSE(missingModel.valid);
    EXPECT_EQ(missingModel.stage, "validate_params");
    EXPECT_EQ(missingModel.detail, " missing=\"modelName\"");

    request.modelName = QStringLiteral("Model-1");
    const DxfImportPreflightResult missingPart =
        validateDxfImportRequest(request);
    EXPECT_FALSE(missingPart.valid);
    EXPECT_EQ(missingPart.detail, " missing=\"partName\"");

    request.partName = QStringLiteral("Part-1");
    const DxfImportPreflightResult valid =
        validateDxfImportRequest(request);
    EXPECT_TRUE(valid.valid);
    EXPECT_EQ(valid.mode, DxfImportMode::FiniteElement);
}

TEST(DxfImportPreflight, NumericValidationRunsBeforeModeValidation)
{
    DxfImportRequest request;
    request.curveTolerance = 0.0;
    request.importMode = QStringLiteral("Mesh");

    const DxfImportPreflightResult result =
        validateDxfImportRequest(request);

    EXPECT_FALSE(result.valid);
    EXPECT_EQ(result.stage, "validate_params");
    EXPECT_EQ(result.detail, " invalid_curveTolerance");
}
