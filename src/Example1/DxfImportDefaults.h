#pragma once

#include <cstddef>

namespace DxfImportDefaults {

constexpr double kCurveTolerance = 0.01;
constexpr double kLargeDrawingCurveTolerance = 0.05;
constexpr double kNodeMergeTolerance = 1.0e-6;
constexpr double kMinimumCurveTolerance = 1.0e-12;
constexpr double kMaximumCurveTolerance = 1000.0;
constexpr std::size_t kSmallDrawingEntityLimit = 100000;
constexpr int kLargeDrawingEntityLimit = 500000;
constexpr int kUnlimitedOutputEntities = -1;
constexpr int kDefaultMaxOutputEntities =
    static_cast<int>(kSmallDrawingEntityLimit);

} // namespace DxfImportDefaults
