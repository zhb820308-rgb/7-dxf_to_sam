#pragma once

#include <cmath>

namespace DxfNumeric {

inline bool isFinite(double value)
{
    return std::isfinite(value);
}

inline bool areFinite(double first, double second, double third)
{
    return isFinite(first) && isFinite(second) && isFinite(third);
}

inline bool isPositiveFinite(double value)
{
    return isFinite(value) && value > 0.0;
}

inline bool isNonNegativeFinite(double value)
{
    return isFinite(value) && value >= 0.0;
}

inline bool isWithinInclusive(double value, double minimum, double maximum)
{
    return isFinite(value) && value >= minimum && value <= maximum;
}

} // namespace DxfNumeric
