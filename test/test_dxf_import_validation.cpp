#include "DxfImportValidation.h"

#include <gtest/gtest.h>

#include <cmath>
#include <limits>

namespace {

DxfImportValidation::Result validateDefaults(int maxOutputEntities)
{
    return DxfImportValidation::validate(
        0.0, 0.0, 0.0, 0.1, 0.0, maxOutputEntities);
}

} // namespace

TEST(DxfImportValidation, AcceptsSupportedOutputLimits)
{
    const auto small = validateDefaults(100000);
    EXPECT_TRUE(small.valid);
    EXPECT_EQ(small.outputLimit, 100000u);

    const auto large = validateDefaults(500000);
    EXPECT_TRUE(large.valid);
    EXPECT_EQ(large.outputLimit, 500000u);

    const auto unlimited = validateDefaults(-1);
    EXPECT_TRUE(unlimited.valid);
    EXPECT_EQ(unlimited.outputLimit,
        std::numeric_limits<std::size_t>::max());
}

TEST(DxfImportValidation, RejectsNonFiniteBaseCoordinates)
{
    const auto result = DxfImportValidation::validate(
        std::numeric_limits<double>::infinity(),
        0.0, 0.0, 0.1, 0.0, 100000);

    EXPECT_FALSE(result.valid);
    EXPECT_EQ(result.detail, " invalid_base_coordinates");
    EXPECT_EQ(result.message,
        QStringLiteral("[importDxf] ERROR: base coordinates must be finite"));
}

TEST(DxfImportValidation, RejectsInvalidCurveTolerance)
{
    const auto zero = DxfImportValidation::validate(
        0.0, 0.0, 0.0, 0.0, 0.0, 100000);
    EXPECT_FALSE(zero.valid);
    EXPECT_EQ(zero.detail, " invalid_curveTolerance");

    const auto nan = DxfImportValidation::validate(
        0.0, 0.0, 0.0,
        std::numeric_limits<double>::quiet_NaN(),
        0.0, 100000);
    EXPECT_FALSE(nan.valid);
    EXPECT_EQ(nan.detail, " invalid_curveTolerance");

    const auto belowMinimum = DxfImportValidation::validate(
        0.0, 0.0, 0.0,
        std::nextafter(
            DxfImportValidation::kMinimumCurveTolerance, 0.0),
        0.0, 100000);
    EXPECT_FALSE(belowMinimum.valid);

    const auto aboveMaximum = DxfImportValidation::validate(
        0.0, 0.0, 0.0,
        std::nextafter(
            DxfImportValidation::kMaximumCurveTolerance,
            std::numeric_limits<double>::infinity()),
        0.0, 100000);
    EXPECT_FALSE(aboveMaximum.valid);
}

TEST(DxfImportValidation, AcceptsCurveToleranceEndpoints)
{
    const auto minimum = DxfImportValidation::validate(
        0.0, 0.0, 0.0,
        DxfImportValidation::kMinimumCurveTolerance,
        0.0, 100000);
    const auto maximum = DxfImportValidation::validate(
        0.0, 0.0, 0.0,
        DxfImportValidation::kMaximumCurveTolerance,
        0.0, 100000);

    EXPECT_TRUE(minimum.valid);
    EXPECT_TRUE(maximum.valid);
}

TEST(DxfImportValidation, RejectsInvalidNodeMergeTolerance)
{
    const auto result = DxfImportValidation::validate(
        0.0, 0.0, 0.0, 0.1, -0.1, 100000);

    EXPECT_FALSE(result.valid);
    EXPECT_EQ(result.detail, " invalid_nodeMergeTolerance");
}

TEST(DxfImportValidation, RejectsUnsupportedOutputLimit)
{
    const auto result = validateDefaults(200000);

    EXPECT_FALSE(result.valid);
    EXPECT_EQ(result.detail,
        " error_code=INVALID_ARGUMENT invalid_maxOutputEntities");
    EXPECT_EQ(result.message, QStringLiteral(
        "[importDxf] ERROR [INVALID_ARGUMENT]: invalid maxOutputEntities"));
}
