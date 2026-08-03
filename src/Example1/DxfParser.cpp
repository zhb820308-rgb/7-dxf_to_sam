// ============================================================================
// 解析主入口
// ============================================================================
#include "DxfParser.h"

#include "DxfBlockExpansion.h"
#include "DxfInputFile.h"
#include "DxfData.h"

#include <libdxfrw.h>

bool DxfParser::parseFile(
    const QString& filePath,
    DxfData& outData,
    double curveTolerance,
    const std::set<std::string>& ignoredLayers,
    std::size_t maxOutputEntities)
{
    // 先用默认构造的新对象覆盖旧值，保证重复使用 outData 时不会混入上次结果。
    outData = DxfData();
    if (filePath.isEmpty()) {
        outData.setError(
            DxfImportErrorCode::InvalidArgument,
            QStringLiteral("DXF file is empty"));
        return false;
    }
    if (!DxfNumeric::isWithinInclusive(
            curveTolerance,
            DxfImportValidation::kMinimumCurveTolerance,
            DxfImportValidation::kMaximumCurveTolerance)) {
        outData.setError(
            DxfImportErrorCode::InvalidArgument,
            QStringLiteral(
                "curveTolerance is outside the supported range"));
        return false;
    }
    if (maxOutputEntities == 0) {
        outData.setError(
            DxfImportErrorCode::InvalidArgument,
            QStringLiteral("maxOutputEntities must be greater than zero"));
        return false;
    }

    // libdxfrw 的旧接口接收本地窄字符路径。DxfInputFile 负责 Unicode 路径适配，
    // 必要时创建仅在对象生命周期内存在的临时副本（RAII）。
    DxfInputFile inputFile(filePath);
    if (!inputFile.prepare()) {
        outData.setError(
            DxfImportErrorCode::ReadFailed,
            inputFile.errorMessage());
        return false;
    }

    // dxfRW 是 libdxfrw 的读取器；reader 实现 DRW_Interface。read() 遇到每个
    // LINE/CIRCLE/... 时会反向调用 reader.addLine/addCircle，而不是返回一个大对象。
    dxfRW dxf(inputFile.encodedPath().constData());
    DxfReaderCallbacks reader;
    reader.setIgnoredLayers(ignoredLayers);
    if (!dxf.read(&reader, true)) {
        outData.setError(
            DxfImportErrorCode::ReadFailed,
            QStringLiteral("Failed to read DXF file. Error code: %1")
                .arg(static_cast<int>(dxf.getError())));
        return false;
    }

    const std::size_t initialEntities = reader.data().entityCount();
    if (initialEntities > maxOutputEntities) {
        outData.setError(
            DxfImportErrorCode::ExpansionLimit,
            QStringLiteral("DXF output limit exceeded: more than %1 entities")
                .arg(static_cast<qulonglong>(maxOutputEntities)));
        return false;
    }

    // takeData() 使用 move 语义把可能很大的 vector 所有权转交出来，避免深拷贝。
    outData = reader.takeData();

    // BLOCK 是可复用定义，INSERT 是“在某位置按某比例放一份 BLOCK”。这里递归展开
    // 成实际模型空间实体，并执行嵌套深度、阵列数量和总输出预算保护。
    const DxfBlockExpansionResult expansion = expandDxfBlocks({
        outData,
        reader.blocks(),
        reader.modelSpaceInserts(),
        ignoredLayers,
        curveTolerance,
        initialEntities,
        maxOutputEntities});
    if (!expansion.succeeded()) {
        QString message = expansion.message;
        if (message.isEmpty()) {
            message = QStringLiteral("INSERT expansion failed");
        }
        outData.clear();
        outData.setError(
            DxfImportErrorCode::ExpansionLimit,
            message);
        return false;
    }

    // `const &` 只借用统计对象，既不允许修改，也不复制整张 map。
    const DxfEntityStats& stats = outData.entityStats();
    const std::size_t usable =
        stats.acceptedEntities + stats.generatedEntities;
    if (usable == 0 || outData.entityCount() == 0) {
        outData.setValid(false);
        outData.setError(
            DxfImportErrorCode::NoSupportedEntities,
            QStringLiteral(
                "DXF was read successfully, but no valid supported entities were found (%1 rejected).")
                .arg(stats.rejectedEntities));
        return false;
    }

    outData.setValid(true);
    return true;
}

// ============================================================================
// libdxfrw 回调
// ============================================================================


#include <drw_entities.h>
#include <drw_objects.h>

#include <optional>
#include <utility>

void DxfReaderCallbacks::setIgnoredLayers(
    const std::set<std::string>& ignoredLayers)
{
    m_ignoredLayers = ignoredLayers;
}

DxfData DxfReaderCallbacks::takeData()
{
    // std::move 把 m_data 内部容器的资源转交给返回值。调用后本对象仍可析构，
    // 但 m_data 处于“有效但内容未指定”的已移动状态，不应继续当旧数据使用。
    return std::move(m_data);
}

bool DxfReaderCallbacks::isLayerIgnored(const DRW_Entity& entity) const
{
    if (m_ignoredLayers.empty()) return false;
    return m_ignoredLayers.count(entity.layer) != 0;
}

bool DxfReaderCallbacks::shouldSkipDuringRead(const DRW_Entity& entity) const
{
    return m_currentBlock == nullptr && isLayerIgnored(entity);
}

void DxfReaderCallbacks::addLine(const DRW_Line& data)
{
    if (shouldSkipDuringRead(data)) return;
    const DxfLine line = DxfEntityValidation::makeLine(data);
    if (m_currentBlock) {
        m_currentBlock->addLine(line);
    } else {
        m_data.addLine(line);
    }
}

void DxfReaderCallbacks::addCircle(const DRW_Circle& data)
{
    if (shouldSkipDuringRead(data)) return;
    const DxfCircle circle = DxfEntityValidation::makeCircle(data);
    if (m_currentBlock) {
        m_currentBlock->addCircle(circle);
    } else {
        m_data.addCircle(circle);
    }
}

void DxfReaderCallbacks::addArc(const DRW_Arc& data)
{
    if (shouldSkipDuringRead(data)) return;
    // optional 表示“可能有一个 DxfArc，也可能没有”。非法半径/角度会返回空，
    // 比返回魔法值或裸空指针更清楚；`if (!arc)` 检查是否有值，`*arc` 取出值。
    const std::optional<DxfArc> arc = DxfEntityValidation::makeArc(data);
    if (!arc) return;
    if (m_currentBlock) {
        m_currentBlock->addArc(*arc);
    } else {
        m_data.addArc(*arc);
    }
}

void DxfReaderCallbacks::addEllipse(const DRW_Ellipse& data)
{
    if (shouldSkipDuringRead(data)) return;
    const std::optional<DxfEllipse> ellipse =
        DxfEntityValidation::makeEllipse(data);
    if (!ellipse) return;
    if (m_currentBlock) {
        m_currentBlock->addEllipse(*ellipse);
    } else {
        m_data.addEllipse(*ellipse);
    }
}

void DxfReaderCallbacks::addLWPolyline(const DRW_LWPolyline& data)
{
    if (shouldSkipDuringRead(data)) return;
    const std::optional<DxfLWPolyline> polyline =
        DxfEntityValidation::makeLWPolyline(data);
    if (!polyline) return;
    if (m_currentBlock) {
        m_currentBlock->addLWPolyline(*polyline);
    } else {
        m_data.addLWPolyline(*polyline);
    }
}

void DxfReaderCallbacks::addSpline(const DRW_Spline* data)
{
    if (!data || shouldSkipDuringRead(*data)) return;
    std::optional<DxfSpline> spline =
        DxfEntityValidation::makeSpline(*data);
    if (!spline) return;
    if (m_currentBlock) {
        m_currentBlock->addSpline(std::move(*spline));
    } else {
        m_data.addSpline(std::move(*spline));
    }
}

void DxfReaderCallbacks::addPoint(const DRW_Point& data)
{
    if (shouldSkipDuringRead(data)) return;
    const DxfPoint point = DxfEntityValidation::makePoint(data);
    if (m_currentBlock) {
        m_currentBlock->addPoint(point);
    } else {
        m_data.addPoint(point);
    }
}

void DxfReaderCallbacks::addBlock(const DRW_Block& data)
{
    // addBlock 之后到 endBlock 之前，后续实体回调都属于这个块定义。
    DxfBlock block;
    block.setName(data.name);
    block.setBase(data.basePoint.x, data.basePoint.y, data.basePoint.z);
    if (block.name() == "*Model_Space"
        || block.name() == "*Paper_Space"
        || block.name() == "*Paper_Space0") {
        m_currentBlock = nullptr;
        return;
    }

    // unordered_map 的 []：键不存在时先创建值，再赋值；随后保存元素地址作为
    // “当前块”。这里只在读取阶段使用，且项目不在持有指针时主动 rehash/清空容器。
    m_blocks[block.name()] = block;
    m_currentBlock = &m_blocks[block.name()];
}

void DxfReaderCallbacks::endBlock()
{
    m_currentBlock = nullptr;
}

void DxfReaderCallbacks::addInsert(const DRW_Insert& data)
{
    // INSERT 自身不含完整几何，只记录块名、平移、缩放、旋转及阵列参数。
    // DxfBlockExpansion 稍后才会组合嵌套变换并生成模型空间实体。
    InsertInfo insert;
    insert.blockName = data.name;
    insert.layer = data.layer;
    insert.insertX = data.basePoint.x;
    insert.insertY = data.basePoint.y;
    insert.insertZ = data.basePoint.z;
    insert.scaleX = data.xscale;
    insert.scaleY = data.yscale;
    insert.scaleZ = data.zscale;
    insert.angle = data.angle;
    insert.colCount = data.colcount;
    insert.rowCount = data.rowcount;
    insert.colSpace = data.colspace;
    insert.rowSpace = data.rowspace;

    if (m_currentBlock) {
        m_currentBlock->addInsert(insert);
    } else {
        m_modelSpaceInserts.push_back(insert);
    }
}

void DxfReaderCallbacks::addLayer(const DRW_Layer& data)
{
    m_allLayers.insert(data.name);
}

// ============================================================================
// 实体合法性校验
// ============================================================================


#include <algorithm>
#include <cmath>
#include <vector>

namespace DxfEntityValidation {

DxfLine makeLine(const DRW_Line& source)
{
    DxfLine line(
        DxfPoint(source.basePoint.x, source.basePoint.y, source.basePoint.z),
        DxfPoint(source.secPoint.x, source.secPoint.y, source.secPoint.z));
    line.setLayer(source.layer);
    return line;
}

DxfCircle makeCircle(const DRW_Circle& source)
{
    DxfCircle circle(
        DxfPoint(source.basePoint.x, source.basePoint.y, source.basePoint.z),
        source.radious);
    circle.setLayer(source.layer);
    return circle;
}

std::optional<DxfArc> makeArc(const DRW_Arc& source)
{
    const DRW_Coord center = source.basePoint;
    const double radius = source.radious;
    if (!DxfNumeric::areFinite(center.x, center.y, center.z)) {
        return std::nullopt;
    }
    if (!DxfNumeric::isPositiveFinite(radius)) {
        return std::nullopt;
    }
    if (!DxfNumeric::isFinite(source.staangle)
        || !DxfNumeric::isFinite(source.endangle)) {
        return std::nullopt;
    }

    DxfArc arc(DxfPoint(center.x, center.y, center.z), radius,
               source.staangle, source.endangle, source.isccw);
    arc.setLayer(source.layer);
    return arc;
}

std::optional<DxfEllipse> makeEllipse(const DRW_Ellipse& source)
{
    const double majorX = source.secPoint.x;
    const double majorY = source.secPoint.y;
    const double majorLength = std::sqrt(
        majorX * majorX + majorY * majorY);
    if (majorLength <= 0.0) {
        return std::nullopt;
    }
    if (!DxfNumeric::isPositiveFinite(source.ratio)) {
        return std::nullopt;
    }

    DxfEllipse ellipse(
        DxfPoint(source.basePoint.x, source.basePoint.y, source.basePoint.z),
        DxfPoint(source.secPoint.x, source.secPoint.y, source.secPoint.z),
        source.ratio, source.staparam, source.endparam, source.isccw);
    ellipse.setLayer(source.layer);
    return ellipse;
}

std::optional<DxfLWPolyline> makeLWPolyline(const DRW_LWPolyline& source)
{
    const int vertexCount = std::min(
        source.vertexnum, static_cast<int>(source.vertlist.size()));
    if (vertexCount < 2) {
        return std::nullopt;
    }

    const bool isClosed = (source.flags & 1) != 0;
    std::vector<DxfPoint> vertices;
    std::vector<double> bulges;
    vertices.reserve(vertexCount);
    bulges.reserve(isClosed ? vertexCount : vertexCount - 1);

    for (int i = 0; i < vertexCount; ++i) {
        const DRW_Vertex2D& vertex = *source.vertlist[i];
        vertices.push_back(DxfPoint(vertex.x, vertex.y, 0.0));
    }

    const int bulgeCount = isClosed ? vertexCount : vertexCount - 1;
    for (int i = 0; i < bulgeCount; ++i) {
        bulges.push_back(source.vertlist[i]->bulge);
    }

    DxfLWPolyline polyline(vertices, bulges, isClosed, 0.0);
    polyline.setLayer(source.layer);
    return polyline;
}

std::optional<DxfSpline> makeSpline(const DRW_Spline& source)
{
    const bool isRational = (source.flags & 4) != 0;
    const bool hasControlData = source.ncontrol > 0
        && static_cast<int>(source.controllist.size()) >= source.ncontrol
        && source.degree >= 1
        && source.ncontrol > source.degree;
    const bool hasFitData = source.nfit >= 2
        && static_cast<int>(source.fitlist.size()) >= source.nfit;
    if (!hasControlData && !hasFitData) {
        return std::nullopt;
    }

    std::vector<DxfPoint> controlPoints;
    std::vector<double> knots;
    std::vector<double> weights;
    if (hasControlData) {
        const int controlCount = source.ncontrol;
        const int expectedKnotCount = controlCount + source.degree + 1;
        if (static_cast<int>(source.knotslist.size()) != expectedKnotCount) {
            return std::nullopt;
        }

        knots.reserve(expectedKnotCount);
        for (int i = 0; i < expectedKnotCount; ++i) {
            const double knot = source.knotslist[i];
            if (!DxfNumeric::isFinite(knot)
                || (i > 0 && knot < knots.back())) {
                return std::nullopt;
            }
            knots.push_back(knot);
        }

        controlPoints.reserve(controlCount);
        for (int i = 0; i < controlCount; ++i) {
            const auto& point = source.controllist[i];
            if (!point
                || !DxfNumeric::areFinite(point->x, point->y, point->z)) {
                return std::nullopt;
            }
            controlPoints.push_back(DxfPoint(point->x, point->y, point->z));
        }

        if (isRational) {
            if (static_cast<int>(source.weightlist.size()) < controlCount) {
                return std::nullopt;
            }
            weights.reserve(controlCount);
            for (int i = 0; i < controlCount; ++i) {
                const double weight = source.weightlist[i];
                if (!DxfNumeric::isPositiveFinite(weight)) {
                    return std::nullopt;
                }
                weights.push_back(weight);
            }
        }
    }

    std::vector<DxfPoint> fitPoints;
    fitPoints.reserve(source.nfit);
    for (int i = 0; i < source.nfit; ++i) {
        const auto& point = source.fitlist[i];
        if (point
            && DxfNumeric::areFinite(point->x, point->y, point->z)) {
            fitPoints.push_back(DxfPoint(point->x, point->y, point->z));
        }
    }

    DxfSpline spline(
        std::move(controlPoints), std::move(knots), std::move(weights),
        std::move(fitPoints), source.degree, source.flags,
        source.tgStart.x, source.tgStart.y, source.tgStart.z,
        source.tgEnd.x, source.tgEnd.y, source.tgEnd.z);
    spline.setLayer(source.layer);
    return spline;
}

DxfPoint makePoint(const DRW_Point& source)
{
    DxfPoint point(
        source.basePoint.x, source.basePoint.y, source.basePoint.z);
    point.setLayer(source.layer);
    return point;
}

} // namespace DxfEntityValidation

// ============================================================================
// 坐标变换
// ============================================================================


Transform2D Transform2D::fromInsert(const InsertInfo& insert,
                                    double baseX,
                                    double baseY,
                                    double baseZ)
{
    Transform2D transform;
    const double cosAngle = std::cos(insert.angle);
    const double sinAngle = std::sin(insert.angle);
    transform.m00 = cosAngle * insert.scaleX;
    transform.m01 = -sinAngle * insert.scaleY;
    transform.m10 = sinAngle * insert.scaleX;
    transform.m11 = cosAngle * insert.scaleY;
    transform.tx = insert.insertX
        - transform.m00 * baseX - transform.m01 * baseY;
    transform.ty = insert.insertY
        - transform.m10 * baseX - transform.m11 * baseY;
    transform.scaleZ = insert.scaleZ;
    transform.offsetZ = insert.insertZ - insert.scaleZ * baseZ;
    return transform;
}

Transform2D Transform2D::composedWith(const Transform2D& child) const
{
    Transform2D result;
    result.m00 = m00 * child.m00 + m01 * child.m10;
    result.m01 = m00 * child.m01 + m01 * child.m11;
    result.m10 = m10 * child.m00 + m11 * child.m10;
    result.m11 = m10 * child.m01 + m11 * child.m11;
    result.tx = m00 * child.tx + m01 * child.ty + tx;
    result.ty = m10 * child.tx + m11 * child.ty + ty;
    result.scaleZ = scaleZ * child.scaleZ;
    result.offsetZ = scaleZ * child.offsetZ + offsetZ;
    return result;
}

DxfPoint Transform2D::apply(double x, double y, double z) const
{
    return DxfPoint(m00 * x + m01 * y + tx,
                    m10 * x + m11 * y + ty,
                    scaleZ * z + offsetZ);
}

DxfPoint Transform2D::apply(const DxfPoint& point) const
{
    return apply(point.x(), point.y(), point.z());
}

DxfPoint Transform2D::applyVector(double x, double y, double z) const
{
    return DxfPoint(m00 * x + m01 * y,
                    m10 * x + m11 * y,
                    scaleZ * z);
}

DxfPoint Transform2D::applyVector(const DxfPoint& vector) const
{
    return applyVector(vector.x(), vector.y(), vector.z());
}

double Transform2D::determinant() const
{
    return m00 * m11 - m01 * m10;
}

bool Transform2D::reversesOrientation() const
{
    return determinant() < 0.0;
}

bool Transform2D::isPlanarSimilarity() const
{
    const double firstLength2 = m00 * m00 + m10 * m10;
    const double secondLength2 = m01 * m01 + m11 * m11;
    const double dot = m00 * m01 + m10 * m11;
    const double scale = std::max({1.0, firstLength2, secondLength2});
    return firstLength2 > 1e-24
        && std::fabs(firstLength2 - secondLength2) <= 1e-9 * scale
        && std::fabs(dot) <= 1e-9 * scale;
}

double Transform2D::planarScale() const
{
    return std::sqrt(m00 * m00 + m10 * m10);
}

double Transform2D::applyAngle(double angle) const
{
    const double rotation = std::atan2(m10, m00);
    return reversesOrientation() ? rotation - angle : rotation + angle;
}

double Transform2D::applyZ(double z) const
{
    return scaleZ * z + offsetZ;
}
