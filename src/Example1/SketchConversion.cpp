// ============================================================================
// Sketch 转换：DxfData -> SamData
// ============================================================================
#include "SketchConversion.h"
#include "DxfData.h"
#include "GeometryUtils.h"
#include <QDebug>
#include <spdlog/spdlog.h>

// ========================================================================
//  convert
// ========================================================================

bool ConversionEngine::convert(const DxfData& dxfData,
                               double baseX, double baseY, double baseZ,
                               double tolerance,
                               SamData& outData,
                               std::size_t maxOutputEntities) const
{
    outData.clear();

    if (!DxfNumeric::isWithinInclusive(
            tolerance,
            DxfImportValidation::kMinimumCurveTolerance,
            DxfImportValidation::kMaximumCurveTolerance)) {
        outData.setError(DxfImportErrorCode::InvalidArgument,
                         QStringLiteral("invalid curve tolerance"));
        qWarning() << "[ConversionEngine] invalid curve tolerance:" << tolerance;
        return false;
    }
    if (maxOutputEntities == 0) {
        outData.setError(DxfImportErrorCode::InvalidArgument,
                         QStringLiteral("maxOutputEntities must be greater than zero"));
        return false;
    }
    if (!DxfNumeric::areFinite(baseX, baseY, baseZ)) {
        outData.setError(DxfImportErrorCode::InvalidArgument,
                         QStringLiteral("base coordinates must be finite"));
        qWarning() << "[ConversionEngine] base coordinates must be finite:"
                   << baseX << baseY << baseZ;
        return false;
    }

    // POINT entities are parsed and logged as raw DXF data, but they are not
    // converted because the current SAM builder only creates lines and circles.

    // 局部 lambda（匿名函数）集中执行输出预算检查。`auto` 让编译器推导 lambda 的
    // 匿名类型；`[&]` 表示按引用捕获外部局部变量 outData/maxOutputEntities。
    auto consumeOutput = [&](std::size_t count) {
        const std::size_t current = outData.lines().size() + outData.circles().size();
        // 写成 current > max-count，而非 current+count > max，可避免无符号加法溢出。
        if (count > maxOutputEntities || current > maxOutputEntities - count) {
            outData.clear();
            outData.setError(
                DxfImportErrorCode::ConversionLimit,
                QStringLiteral("conversion output exceeds %1 entities")
                    .arg(static_cast<qulonglong>(maxOutputEntities)));
            return false;
        }
        return true;
    };

    // --- lines ---
    for (const DxfLine& line : dxfData.lines()) {
        if (!line.isValid())
            continue;
        if (!consumeOutput(1)) return false;
        DxfPoint s = translate(line.start(), baseX, baseY, baseZ);
        DxfPoint e = translate(line.end(),   baseX, baseY, baseZ);
        outData.addLine(DxfLine(s, e));
    }

    // --- circles ---
    for (const DxfCircle& circle : dxfData.circles()) {
        if (!circle.isValid())
            continue;
        if (!consumeOutput(1)) return false;
        DxfPoint c = translate(circle.center(), baseX, baseY, baseZ);
        outData.addCircle(DxfCircle(c, circle.radius()));
    }

    // --- helper: translate tessellated segments and append to output ---
    // 多种曲线离散后都得到 vector<DxfLine>，用 lambda 复用“过滤、预算、平移、记录
    // 来源”的流程，避免 Arc/Polyline/Ellipse/Spline 四段代码逐渐不一致。
    auto addSegments = [&](const std::vector<DxfLine>& segments,
                           EntityType parentType, int parentId) {
        std::size_t validCount = 0;
        for (const DxfLine& seg : segments) {
            if (seg.isValid()) ++validCount;
        }
        if (!consumeOutput(validCount)) return false;
        size_t segmentIndex = 0;
        for (const DxfLine& seg : segments) {
            if (!seg.isValid()) continue;
            outData.addCurveSegment(parentType, parentId, segmentIndex, DxfLine(
                translate(seg.start(), baseX, baseY, baseZ),
                translate(seg.end(),   baseX, baseY, baseZ)));

            ++segmentIndex;
        }
        return true;
    };

    // --- arcs (tessellation + translation) ---
    for (const DxfArc& arc : dxfData.arcs()) {
        if (!arc.isValid()) continue;
        if (!addSegments(GeometryUtils::tessellateArc(arc, tolerance),
                         EntityType::Arc, arc.getId())) return false;
    }

    // --- lwPolylines (tessellation + translation) ---
    for (const DxfLWPolyline& poly : dxfData.lwPolylines()) {
        if (!poly.isValid()) continue;
        if (!addSegments(GeometryUtils::tessellateLWPolyline(poly, tolerance),
                         EntityType::LWPolyline, poly.getId())) return false;
    }

    // --- ellipses (tessellation + translation) ---
    for (const DxfEllipse& ellipse : dxfData.ellipses()) {
        if (!ellipse.isValid()) continue;
        if (!addSegments(GeometryUtils::tessellateEllipse(ellipse, tolerance),
                         EntityType::Ellipse, ellipse.getId())) return false;
    }

    // --- splines (OCCT B-spline construction + tessellation + translation) ---
    // 范围 for：逐个借用 const 引用，不复制可能包含大量控制点的 DxfSpline。
    for (const DxfSpline& spline : dxfData.splines()) {
        if (!spline.isValid()) continue;
        if (!addSegments(GeometryUtils::tessellateSpline(spline, tolerance),
                         EntityType::Spline, spline.getId())) return false;
    }

    bool ok = !outData.lines().empty() ||
              !outData.circles().empty();

    if (!ok) {
        outData.setError(DxfImportErrorCode::ConversionFailed,
                         QStringLiteral("no valid entities to convert"));
    }
    return ok;
}

// ========================================================================
//  translate
// ========================================================================

DxfPoint ConversionEngine::translate(const DxfPoint& pt,
                                      double bx, double by, double bz)
{
    return DxfPoint(pt.x() + bx,
                    pt.y() + by,
                    pt.z() + bz);
}

// ============================================================================
// Sketch 事务顺序（纯 C++）
// ============================================================================

namespace DxfImportBuildService {

ImportBuildResult buildSamSketch(
    const SamData& samData,
    ISamImportBuilder& builder)
{
    // 这段顺序就是 Sketch 导入事务的“骨架”。任一步失败立即 return；如果已经 begin，
    // 则先 rollback，保证不会把半个 Sketch 留在 SAM 中。
    ImportBuildResult result = builder.beginImport();
    if (!result.succeeded())
        return result;

    int created = 0;
    result = builder.createLines(samData.lines());
    if (!result.succeeded())
        return DxfImportBuildDetail::rollbackAfterFailure(builder, result);
    created += result.createdCount;

    result = builder.createCircles(samData.circles());
    if (!result.succeeded())
        return DxfImportBuildDetail::rollbackAfterFailure(builder, result, created);
    created += result.createdCount;

    result = builder.commit();
    if (!result.succeeded())
        return DxfImportBuildDetail::rollbackAfterFailure(builder, result);

    return ImportBuildResult::success(created);
}

} // namespace DxfImportBuildService

// ============================================================================
// Sketch 转换结果日志（属于 Sketch Core，可独立单测）
// ============================================================================
namespace {

static const char* entityTypeName(EntityType type)
{
	switch (type)
	{
	case EntityType::Point:      return "POINT";
	case EntityType::Line:       return "LINE";
	case EntityType::Circle:     return "CIRCLE";
	case EntityType::Arc:        return "ARC";
	case EntityType::LWPolyline: return "LWPOLYLINE";
	case EntityType::Ellipse:    return "ELLIPSE";
	default:                     return "UNKNOWN";
	}
}

template <typename Entity, typename LogFunc>
static void logEntities(
	const std::shared_ptr<spdlog::logger>& logger,
	const std::string& importId,
	const std::string& tag,
	const std::vector<Entity>& entities,
	LogFunc formatter)
{
	if (!logger)
		return;

	for (size_t i = 0; i < entities.size(); ++i)
	{
		formatter(logger, importId, tag, i, entities[i]);
	}
}

} // namespace

void logConvertedSamData(
	const std::shared_ptr<spdlog::logger>& logger,
	const std::string& importId,
	const SamData& data)
{
	if (!logger)
		return;

	// Lines produced by curve tessellation are logged below with their parent
	// entity ID. Keep ordinary converted DXF lines in the original trace form.
	std::vector<bool> isCurveSegment(data.lines().size(), false);
	for (const CurveSegmentSource& source : data.curveSegments())
	{
		if (source.lineIndex < isCurveSegment.size())
			isCurveSegment[source.lineIndex] = true;
	}
	for (size_t i = 0; i < data.lines().size(); ++i)
	{
		if (isCurveSegment[i])
			continue;
		const DxfLine& line = data.lines()[i];
		logger->trace(
			"[import={}] converted LINE id={} start=({}, {}, {}) end=({}, {}, {})",
			importId, line.getId(),
			line.start().x(), line.start().y(), line.start().z(),
			line.end().x(), line.end().y(), line.end().z());
	}

	// Each original curve gets one INFO summary. Its individual generated
	// segments remain TRACE and can be expanded by filtering parent_id.
	const std::vector<CurveSegmentSource>& curveSegments = data.curveSegments();
	for (size_t i = 0; i < curveSegments.size();)
	{
		const CurveSegmentSource& first = curveSegments[i];
		size_t end = i + 1;
		while (end < curveSegments.size() &&
			curveSegments[end].parentType == first.parentType &&
			curveSegments[end].parentId == first.parentId)
		{
			++end;
		}

		logger->info(
			"[import={}] converted_curve type={} parent_id={} segment_count={}",
			importId, entityTypeName(first.parentType), first.parentId, end - i);

		for (size_t j = i; j < end; ++j)
		{
			const CurveSegmentSource& source = curveSegments[j];
			if (source.lineIndex >= data.lines().size())
				continue;
			const DxfLine& line = data.lines()[source.lineIndex];
			logger->trace(
				"[import={}] curve_segment type={} parent_id={} segment_index={} line_id={} start=({}, {}, {}) end=({}, {}, {})",
				importId, entityTypeName(source.parentType), source.parentId,
				source.segmentIndex, line.getId(),
				line.start().x(), line.start().y(), line.start().z(),
				line.end().x(), line.end().y(), line.end().z());
		}

		i = end;
	}

	logEntities(logger, importId, "converted", data.circles(),
		[](const std::shared_ptr<spdlog::logger>& log, const std::string& id,
		   const std::string& tag, size_t, const DxfCircle& circle) {
			log->trace(
				"[import={}] {} CIRCLE id={} center=({}, {}, {}) radius={}",
				id, tag, circle.getId(),
				circle.center().x(), circle.center().y(), circle.center().z(),
				circle.radius());
		});
}
