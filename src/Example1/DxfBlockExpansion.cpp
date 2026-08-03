#include "DxfBlockExpansion.h"

#include "DxfData.h"
#include "DxfParser.h"
#include "GeometryUtils.h"

#include <QDebug>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <unordered_set>
#include <utility>

namespace {

constexpr std::uint64_t kMaxArrayInstancesPerInsert = 100000;
constexpr std::uint64_t kMaxExpandedBlockInstances = 100000;
constexpr std::size_t kMaxReserveEntities = 500000;

struct ExpansionBudget
{
    // 三种预算各防一种“展开爆炸”：单个阵列过大、累计块实例过多、最终实体过多。
    std::uint64_t blockInstances = 0;
    std::size_t entities = 0;
    std::size_t maxOutputEntities =
        DxfImportDefaults::kDefaultMaxOutputEntities;
    DxfBlockExpansionStatus status = DxfBlockExpansionStatus::Success;
    QString error;

    explicit ExpansionBudget(
        std::size_t initialEntities = 0,
        std::size_t outputLimit =
            DxfImportDefaults::kDefaultMaxOutputEntities)
        : entities(initialEntities), maxOutputEntities(outputLimit)
    {
    }

    bool consumeArray(int rows, int columns, const std::string& blockName)
    {
        const std::uint64_t rowCount = static_cast<std::uint64_t>(
            std::max(1, rows));
        const std::uint64_t columnCount = static_cast<std::uint64_t>(
            std::max(1, columns));
        // 用除法形式预先检查乘法溢出；不能先算 rowCount*columnCount 再判断。
        if (rowCount > kMaxArrayInstancesPerInsert / columnCount) {
            status = DxfBlockExpansionStatus::ArrayInstanceLimit;
            error = QStringLiteral(
                "INSERT expansion limit exceeded for block '%1': %2 rows x %3 columns")
                .arg(QString::fromStdString(blockName))
                .arg(rowCount)
                .arg(columnCount);
            return false;
        }
        const std::uint64_t count = rowCount * columnCount;
        if (blockInstances > kMaxExpandedBlockInstances - count) {
            status = DxfBlockExpansionStatus::BlockInstanceLimit;
            error = QStringLiteral(
                "INSERT expansion limit exceeded: more than %1 block instances")
                .arg(kMaxExpandedBlockInstances);
            return false;
        }
        blockInstances += count;
        return true;
    }

    bool consumeEntities(std::size_t count)
    {
        if (count > maxOutputEntities
            || entities > maxOutputEntities - count) {
            status = DxfBlockExpansionStatus::OutputEntityLimit;
            error = QStringLiteral(
                "INSERT expansion limit exceeded: more than %1 output entities")
                .arg(static_cast<qulonglong>(maxOutputEntities));
            return false;
        }
        entities += count;
        return true;
    }
};

const std::string& resolveEffectiveLayer(
    const std::string& sourceLayer,
    const std::string& inheritedLayer)
{
    // DXF 规则：块内实体位于 layer 0 时继承 INSERT 的图层；显式非 0 图层保持自身。
    static const std::string defaultLayer("0");
    if (sourceLayer.empty() || sourceLayer == "0") {
        return inheritedLayer.empty() ? defaultLayer : inheritedLayer;
    }
    return sourceLayer;
}

bool isIgnoredLayer(
    const std::string& layer,
    const std::set<std::string>& ignoredLayers)
{
    return ignoredLayers.count(layer) != 0;
}

bool addTransformedSegments(
    DxfData& output,
    const std::vector<DxfLine>& segments,
    const Transform2D& transform,
    const std::string& effectiveLayer,
    ExpansionBudget& budget)
{
    if (!budget.consumeEntities(segments.size())) return false;
    output.reserveLines(output.lines().size() + segments.size());
    for (const DxfLine& segment : segments) {
        if (!segment.isValid()) continue;
        DxfLine transformed(
            transform.apply(segment.start()),
            transform.apply(segment.end()));
        transformed.setLayer(effectiveLayer);
        output.addGeneratedLine(transformed);
    }
    return true;
}

template<typename Entity>
void recordGeneratedEntity(DxfData& output, const Entity& entity)
{
    output.recordGeneratedEntity(entity.getType());
}

void recordGeneratedEntity(DxfData& output, const DxfSpline& spline)
{
    output.recordGeneratedSpline(spline.kind());
}

template<typename Entity, typename Tessellate, typename Preserve>
bool expandCurveGroup(
    DxfData& output,
    const std::vector<Entity>& entities,
    const Transform2D& transform,
    const std::string& insertLayer,
    const std::set<std::string>& ignoredLayers,
    double tolerance,
    bool preserveOriginal,
    ExpansionBudget& budget,
    Tessellate tessellate,
    Preserve preserve)
{
    // 这是泛型“曲线组”算法。Entity 可为 Arc/Polyline/Ellipse/Spline；调用者分别传入
    // tessellate 和 preserve 两个策略 lambda，公共的图层、过滤、统计、预算只写一次。
    for (const Entity& entity : entities) {
        if (!entity.isValid()) continue;
        const std::string& effectiveLayer = resolveEffectiveLayer(
            entity.layer(), insertLayer);
        if (isIgnoredLayer(effectiveLayer, ignoredLayers)) continue;
        recordGeneratedEntity(output, entity);
        if (preserveOriginal) {
            if (!budget.consumeEntities(1)) return false;
            preserve(output, entity, transform, effectiveLayer);
        } else {
            const std::vector<DxfLine> segments = tessellate(
                entity, tolerance);
            if (!addTransformedSegments(
                    output, segments, transform, effectiveLayer, budget)) {
                return false;
            }
        }
    }
    return true;
}

bool expandSingleBlock(
    DxfData& output,
    const DxfBlock& block,
    const Transform2D& transform,
    const std::string& insertLayer,
    const std::set<std::string>& ignoredLayers,
    double tolerance,
    const std::unordered_map<std::string, DxfBlock>& blocks,
    int depth,
    std::unordered_set<std::string>& visiting,
    ExpansionBudget& budget);

bool expandInsertArray(
    DxfData& output,
    const DxfBlock& block,
    const InsertInfo& insert,
    const Transform2D& parentTransform,
    const std::string& insertLayer,
    const std::set<std::string>& ignoredLayers,
    double tolerance,
    const std::unordered_map<std::string, DxfBlock>& blocks,
    int depth,
    std::unordered_set<std::string>& visiting,
    ExpansionBudget& budget)
{
    const int columnCount = std::max(1, insert.colCount);
    const int rowCount = std::max(1, insert.rowCount);
    if (!budget.consumeArray(rowCount, columnCount, block.name())) return false;

    // 阵列行列间距定义在 INSERT 的局部方向上，因此先按插入角旋转成父坐标增量。
    const double cosAngle = std::cos(insert.angle);
    const double sinAngle = std::sin(insert.angle);
    const double columnDx = cosAngle * insert.colSpace;
    const double columnDy = sinAngle * insert.colSpace;
    const double rowDx = -sinAngle * insert.rowSpace;
    const double rowDy = cosAngle * insert.rowSpace;

    const std::size_t instances = static_cast<std::size_t>(columnCount)
        * rowCount;
    // reserve 是性能优化，不是业务数量承诺。预留容量也必须封顶，否则恶意阵列即使随后
    // 被预算拒绝，也可能先申请巨量内存。lambda 捕获实例数和值、budget 引用。
    const auto clampReserve = [instances, &budget](
        std::size_t current, std::size_t perInstance) {
        const std::size_t limit = std::min(
            budget.maxOutputEntities, kMaxReserveEntities);
        if (current >= limit || perInstance == 0) return current;
        const std::size_t boundedInstances = std::min(instances, limit);
        const std::size_t remaining = limit - current;
        if (boundedInstances > remaining / perInstance) return limit;
        return current + boundedInstances * perInstance;
    };
    output.reserveLines(clampReserve(
        output.lines().size(), block.lines().size()));
    output.reservePoints(clampReserve(
        output.points().size(), block.points().size()));
    output.reserveLWPolylines(clampReserve(
        output.lwPolylines().size(), block.lwPolylines().size()));
    output.reserveSplines(clampReserve(
        output.splines().size(), block.splines().size()));

    InsertInfo currentInsert = insert;
    double baseX = insert.insertX;
    double baseY = insert.insertY;
    for (int row = 0; row < rowCount; ++row) {
        double currentX = baseX;
        double currentY = baseY;
        for (int column = 0; column < columnCount; ++column) {
            currentInsert.insertX = currentX;
            currentInsert.insertY = currentY;
            // 局部矩阵包含块基点平移、缩放、旋转和插入平移；与父矩阵相乘得到世界
            // 变换。嵌套时必须做矩阵组合，不能把角度/缩放简单相加。
            const Transform2D localTransform = Transform2D::fromInsert(
                currentInsert,
                block.baseX(), block.baseY(), block.baseZ());
            const Transform2D worldTransform =
                parentTransform.composedWith(localTransform);
            if (!expandSingleBlock(
                    output, block, worldTransform, insertLayer,
                    ignoredLayers, tolerance, blocks, depth, visiting,
                    budget)) {
                return false;
            }
            currentX += columnDx;
            currentY += columnDy;
        }
        baseX += rowDx;
        baseY += rowDy;
    }
    return true;
}

bool expandSingleBlock(
    DxfData& output,
    const DxfBlock& block,
    const Transform2D& transform,
    const std::string& insertLayer,
    const std::set<std::string>& ignoredLayers,
    double tolerance,
    const std::unordered_map<std::string, DxfBlock>& blocks,
    int depth,
    std::unordered_set<std::string>& visiting,
    ExpansionBudget& budget)
{
    static const int kMaxExpandDepth = 32;
    if (depth > kMaxExpandDepth) {
        qWarning() << "[BlockExpand] max depth" << kMaxExpandDepth
                   << "exceeded at block:" << block.name().c_str();
        budget.status = DxfBlockExpansionStatus::DepthLimit;
        budget.error = QStringLiteral(
            "INSERT expansion depth limit exceeded at block '%1'")
            .arg(QString::fromStdString(block.name()));
        return false;
    }

    // visiting 保存当前递归调用栈中的块名。若 A→B→A，再遇到 A 就是环；它与“同一个
    // 块在图中合法插入多次”不同，后者在返回上一层时会从 visiting 移除。
    if (visiting.count(block.name())) {
        qWarning() << "[BlockExpand] cycle detected for block:"
                   << block.name().c_str() << "- skipping recursion";
        return true;
    }
    visiting.insert(block.name());
    // 局部 RAII Guard 确保任何 return 路径都会移除当前块名，避免手写多处 erase。
    struct VisitingGuard
    {
        std::unordered_set<std::string>& names;
        std::string name;
        ~VisitingGuard() { names.erase(name); }
    } guard{visiting, block.name()};

    // 只有平面相似变换（旋转/平移/等比缩放/镜像）仍把圆映射成圆。非均匀缩放会把
    // 圆变成椭圆，所以必须先在局部坐标按公差离散，再逐点应用完整矩阵。
    const bool preserveRoundCurves = transform.isPlanarSimilarity();

    for (const DxfPoint& point : block.points()) {
        if (!point.isValid()) continue;
        const std::string& layer = resolveEffectiveLayer(
            point.layer(), insertLayer);
        if (isIgnoredLayer(layer, ignoredLayers)) continue;
        if (!budget.consumeEntities(1)) return false;
        DxfPoint transformed = transform.apply(point);
        transformed.setLayer(layer);
        output.recordGeneratedEntity(EntityType::Point);
        output.addGeneratedPoint(transformed);
    }

    for (const DxfLine& line : block.lines()) {
        if (!line.isValid()) continue;
        const std::string& layer = resolveEffectiveLayer(
            line.layer(), insertLayer);
        if (isIgnoredLayer(layer, ignoredLayers)) continue;
        if (!budget.consumeEntities(1)) return false;
        DxfLine transformed(
            transform.apply(line.start()), transform.apply(line.end()));
        transformed.setLayer(layer);
        output.recordGeneratedEntity(EntityType::Line);
        output.addGeneratedLine(transformed);
    }

    for (const DxfCircle& circle : block.circles()) {
        if (!circle.isValid()) continue;
        const std::string& layer = resolveEffectiveLayer(
            circle.layer(), insertLayer);
        if (isIgnoredLayer(layer, ignoredLayers)) continue;
        output.recordGeneratedEntity(EntityType::Circle);
        if (preserveRoundCurves) {
            const DxfPoint center = transform.apply(circle.center());
            const double radius = circle.radius() * transform.planarScale();
            if (radius > 0.0) {
                if (!budget.consumeEntities(1)) return false;
                DxfCircle transformed(center, radius);
                transformed.setLayer(layer);
                output.addGeneratedCircle(transformed);
            }
        } else {
            const std::vector<DxfLine> segments = GeometryUtils::tessellateArc(
                DxfArc(circle.center(), circle.radius(),
                       0.0, 2.0 * M_PI, true),
                tolerance);
            if (!addTransformedSegments(
                    output, segments, transform, layer, budget)) {
                return false;
            }
        }
    }

    if (!expandCurveGroup(
            output, block.arcs(), transform, insertLayer, ignoredLayers,
            tolerance, preserveRoundCurves, budget,
            GeometryUtils::tessellateArc,
            [](DxfData& out, const DxfArc& arc, const Transform2D& current,
               const std::string& layer) {
                DxfArc transformed(
                    current.apply(arc.center()),
                    arc.radius() * current.planarScale(),
                    current.applyAngle(arc.startAngle()),
                    current.applyAngle(arc.endAngle()),
                    current.reversesOrientation()
                        ? !arc.isCCW() : arc.isCCW());
                transformed.setLayer(layer);
                out.addGeneratedArc(transformed);
            })) {
        return false;
    }

    if (!expandCurveGroup(
            output, block.lwPolylines(), transform, insertLayer,
            ignoredLayers, tolerance, preserveRoundCurves, budget,
            GeometryUtils::tessellateLWPolyline,
            [](DxfData& out, const DxfLWPolyline& polyline,
               const Transform2D& current, const std::string& layer) {
                std::vector<DxfPoint> vertices;
                vertices.reserve(polyline.vertices().size());
                for (const DxfPoint& vertex : polyline.vertices()) {
                    vertices.push_back(current.apply(vertex));
                }
                std::vector<double> bulges = polyline.bulges();
                // 镜像会改变顺/逆时针方向；bulge 符号也必须反转，否则圆弧凸向错误。
                if (current.reversesOrientation()) {
                    for (double& bulge : bulges) bulge = -bulge;
                }
                DxfLWPolyline transformed(
                    vertices, bulges, polyline.isClosed(),
                    current.applyZ(polyline.constZ()));
                transformed.setLayer(layer);
                out.addGeneratedLWPolyline(transformed);
            })) {
        return false;
    }

    if (!expandCurveGroup(
            output, block.ellipses(), transform, insertLayer,
            ignoredLayers, tolerance, preserveRoundCurves, budget,
            GeometryUtils::tessellateEllipse,
            [](DxfData& out, const DxfEllipse& ellipse,
               const Transform2D& current, const std::string& layer) {
                const DxfPoint center = current.apply(ellipse.center());
                const DxfPoint majorAxis = current.applyVector(
                    ellipse.majorAxisEnd());
                const bool reflected = current.reversesOrientation();
                DxfEllipse transformed(
                    center, majorAxis, ellipse.ratio(),
                    reflected ? -ellipse.startParam() : ellipse.startParam(),
                    reflected ? -ellipse.endParam() : ellipse.endParam(),
                    reflected ? !ellipse.isCCW() : ellipse.isCCW());
                transformed.setLayer(layer);
                out.addGeneratedEllipse(transformed);
            })) {
        return false;
    }

    if (!expandCurveGroup(
            output, block.splines(), transform, insertLayer,
            ignoredLayers, tolerance, preserveRoundCurves, budget,
            GeometryUtils::tessellateSpline,
            [](DxfData& out, const DxfSpline& spline,
               const Transform2D& current, const std::string& layer) {
                std::vector<DxfPoint> controlPoints;
                controlPoints.reserve(spline.controlPoints().size());
                for (const DxfPoint& point : spline.controlPoints()) {
                    controlPoints.push_back(current.apply(point));
                }
                std::vector<DxfPoint> fitPoints;
                fitPoints.reserve(spline.fitPoints().size());
                for (const DxfPoint& point : spline.fitPoints()) {
                    fitPoints.push_back(current.apply(point));
                }
                // 切向量表示方向，不是位置，所以只能应用线性部分，不能附加平移。
                const DxfPoint startTangent = current.applyVector(
                    spline.tgStartX(), spline.tgStartY(), spline.tgStartZ());
                const DxfPoint endTangent = current.applyVector(
                    spline.tgEndX(), spline.tgEndY(), spline.tgEndZ());
                DxfSpline transformed(
                    controlPoints, spline.knots(), spline.weights(), fitPoints,
                    spline.degree(), spline.flags(),
                    startTangent.x(), startTangent.y(), startTangent.z(),
                    endTangent.x(), endTangent.y(), endTangent.z());
                transformed.setLayer(layer);
                out.addGeneratedSpline(std::move(transformed));
            })) {
        return false;
    }

    for (const InsertInfo& nested : block.inserts()) {
        const std::string nestedLayer = resolveEffectiveLayer(
            nested.layer, insertLayer);
        if (isIgnoredLayer(nestedLayer, ignoredLayers)) continue;
        const auto found = blocks.find(nested.blockName);
        if (found == blocks.end()) {
            qWarning() << "[BlockExpand] nested INSERT references unknown block:"
                       << nested.blockName.c_str() << "- skipped";
            continue;
        }
        if (!expandInsertArray(
                output, found->second, nested, transform, nestedLayer,
                ignoredLayers, tolerance, blocks, depth + 1, visiting,
                budget)) {
            return false;
        }
    }
    return true;
}

bool expandBlocks(
    DxfData& output,
    const std::unordered_map<std::string, DxfBlock>& blocks,
    const std::vector<InsertInfo>& inserts,
    const std::set<std::string>& ignoredLayers,
    double tolerance,
    ExpansionBudget& budget)
{
    std::unordered_set<std::string> visiting;
    for (const InsertInfo& insert : inserts) {
        const std::string insertLayer = resolveEffectiveLayer(
            insert.layer, "0");
        if (isIgnoredLayer(insertLayer, ignoredLayers)) continue;
        const auto found = blocks.find(insert.blockName);
        if (found == blocks.end()) {
            qWarning() << "[BlockExpand] INSERT references unknown block:"
                       << insert.blockName.c_str() << "- skipped";
            continue;
        }
        if (!expandInsertArray(
                output, found->second, insert, Transform2D(), insertLayer,
                ignoredLayers, tolerance, blocks, 0, visiting, budget)) {
            return false;
        }
    }
    return true;
}

} // namespace

DxfBlockExpansionResult expandDxfBlocks(
    const DxfBlockExpansionRequest& request)
{
    ExpansionBudget budget(
        request.initialEntities, request.maxOutputEntities);
    const bool expanded = expandBlocks(
        request.output,
        request.blocks,
        request.inserts,
        request.ignoredLayers,
        request.curveTolerance,
        budget);
    if (expanded) {
        return DxfBlockExpansionResult{};
    }
    return DxfBlockExpansionResult{budget.status, budget.error};
}
