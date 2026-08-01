#pragma once

#include "DxfData.h"

#include <optional>

class DRW_Arc;
class DRW_Circle;
class DRW_Ellipse;
class DRW_Line;
class DRW_LWPolyline;
class DRW_Point;
class DRW_Spline;

namespace DxfEntityValidation {

DxfLine makeLine(const DRW_Line& source);
DxfCircle makeCircle(const DRW_Circle& source);
std::optional<DxfArc> makeArc(const DRW_Arc& source);
std::optional<DxfEllipse> makeEllipse(const DRW_Ellipse& source);
std::optional<DxfLWPolyline> makeLWPolyline(const DRW_LWPolyline& source);
std::optional<DxfSpline> makeSpline(const DRW_Spline& source);
DxfPoint makePoint(const DRW_Point& source);

} // namespace DxfEntityValidation
