// ============================================================================
// 公共 DXF 领域数据
// ============================================================================
#include "DxfData.h"
#include <atomic>
#include <cmath>

// ========================================================================
//  DxfEntity
// ========================================================================

static std::atomic<int> s_nextId{1};

bool SplineKind::operator<(const SplineKind& other) const
{
    if (construction != other.construction)
        return construction < other.construction;
    if (rational != other.rational)
        return rational < other.rational;
    if (periodic != other.periodic)
        return periodic < other.periodic;
    return closed < other.closed;
}

std::size_t DxfEntityStats::splineCount() const
{
    std::size_t count = 0;
    for (const auto& entry : splineKinds)
        count += entry.second;
    return count;
}

std::size_t DxfEntityStats::curveCount() const
{
    return circles + arcs + ellipses + splineCount();
}

DxfEntity::DxfEntity(EntityType entityType)
    : id(s_nextId++)
    , type(entityType)
{
}

// ========================================================================
//  DxfPoint
// ========================================================================

DxfPoint::DxfPoint()
    : DxfEntity(EntityType::Point)
    , m_x(0.0), m_y(0.0), m_z(0.0)
{
}

DxfPoint::DxfPoint(double ix, double iy, double iz)
    : DxfEntity(EntityType::Point)
    , m_x(ix), m_y(iy), m_z(iz)
{
}

bool DxfPoint::isValid() const
{
    return DxfNumeric::areFinite(m_x, m_y, m_z);
}

// ========================================================================
//  DxfLine
// ========================================================================

DxfLine::DxfLine()
    : DxfEntity(EntityType::Line)
{
}

DxfLine::DxfLine(const DxfPoint& start, const DxfPoint& end)
    : DxfEntity(EntityType::Line)
    , m_start(start)
    , m_end(end)
{
}

bool DxfLine::isValid() const
{
    if (!m_start.isValid() || !m_end.isValid())
        return false;
    // reject zero-length line
    return m_start.x() != m_end.x()
        || m_start.y() != m_end.y()
        || m_start.z() != m_end.z();
}

// ========================================================================
//  DxfCircle
// ========================================================================

DxfCircle::DxfCircle()
    : DxfEntity(EntityType::Circle)
{
}

DxfCircle::DxfCircle(const DxfPoint& center, double radius)
    : DxfEntity(EntityType::Circle)
    , m_center(center)
    , m_radius(radius)
{
}

bool DxfCircle::isValid() const
{
    return m_center.isValid() && DxfNumeric::isPositiveFinite(m_radius);
}

// ========================================================================
//  DxfArc
// ========================================================================

DxfArc::DxfArc()
    : DxfEntity(EntityType::Arc)
{
}

DxfArc::DxfArc(const DxfPoint& center, double radius,
               double startAngle, double endAngle, bool isCCW)
    : DxfEntity(EntityType::Arc)
    , m_center(center)
    , m_radius(radius)
    , m_startAngle(startAngle)
    , m_endAngle(endAngle)
    , m_isCCW(isCCW)
{
}

bool DxfArc::isValid() const
{
    return m_center.isValid()
        && DxfNumeric::isPositiveFinite(m_radius)
        && DxfNumeric::isFinite(m_startAngle)
        && DxfNumeric::isFinite(m_endAngle)
        && (m_startAngle != m_endAngle);
}

// ========================================================================
//  DxfEllipse
// ========================================================================

DxfEllipse::DxfEllipse()
    : DxfEntity(EntityType::Ellipse)
{
}

DxfEllipse::DxfEllipse(const DxfPoint& center, const DxfPoint& majorAxisEnd,
                       double ratio, double startParam, double endParam, bool isCCW)
    : DxfEntity(EntityType::Ellipse)
    , m_center(center)
    , m_majorAxisEnd(majorAxisEnd)
    , m_ratio(ratio)
    , m_startParam(startParam)
    , m_endParam(endParam)
    , m_isCCW(isCCW)
{
}

bool DxfEllipse::isValid() const
{
    const double majorLen = std::hypot(
        m_majorAxisEnd.x(), m_majorAxisEnd.y());
    return m_center.isValid()
        && m_majorAxisEnd.isValid()
        && DxfNumeric::isPositiveFinite(majorLen)
        && DxfNumeric::isPositiveFinite(m_ratio)
        && DxfNumeric::isFinite(m_startParam)
        && DxfNumeric::isFinite(m_endParam)
        && (m_startParam != m_endParam);
}

// ========================================================================
//  DxfLWPolyline
// ========================================================================

DxfLWPolyline::DxfLWPolyline()
    : DxfEntity(EntityType::LWPolyline)
{
}

DxfLWPolyline::DxfLWPolyline(const std::vector<DxfPoint>& vertices,
                             const std::vector<double>& bulges,
                             bool closed, double constZ)
    : DxfEntity(EntityType::LWPolyline)
    , m_vertices(vertices)
    , m_bulges(bulges)
    , m_closed(closed)
    , m_constZ(constZ)
{
}

bool DxfLWPolyline::isValid() const
{
    if (m_vertices.size() < 2) return false;
    for (const DxfPoint& v : m_vertices) {
        if (!v.isValid()) return false;
    }
    // bulges size should match vertex count (last bulge for closing segment if closed)
    if (m_closed) {
        if (static_cast<int>(m_bulges.size()) != static_cast<int>(m_vertices.size()))
            return false;
    } else {
        if (static_cast<int>(m_bulges.size()) != static_cast<int>(m_vertices.size()) - 1)
            return false;
    }
    for (double b : m_bulges) {
        if (!DxfNumeric::isFinite(b)) return false;
    }
    return DxfNumeric::isFinite(m_constZ);
}

// ========================================================================
//  DxfData
// ========================================================================

void DxfData::clear()
{
    m_points.clear();
    m_lines.clear();
    m_circles.clear();
    m_arcs.clear();
    m_lwPolylines.clear();
    m_ellipses.clear();
    m_splines.clear();
    m_inserts.clear();
    m_entityStats = DxfEntityStats();
    m_errorMessage.clear();
    m_errorCode = DxfImportErrorCode::None;
    m_isValid = false;
}

void DxfData::reserveLines(std::size_t count)
{
    if (m_lines.capacity() < count)
        m_lines.reserve(count);
}

void DxfData::reservePoints(std::size_t count)
{
    if (m_points.capacity() < count)
        m_points.reserve(count);
}

void DxfData::reserveLWPolylines(std::size_t count)
{
    if (m_lwPolylines.capacity() < count)
        m_lwPolylines.reserve(count);
}

void DxfData::reserveSplines(std::size_t count)
{
    if (m_splines.capacity() < count)
        m_splines.reserve(count);
}

void DxfData::addPoint(const DxfPoint& pt)
{
    ++m_entityStats.sourceEntities;
    if (!pt.isValid()) {
        recordRejected("invalid point");
        return;
    }
    m_points.push_back(pt);
    ++m_entityStats.points;
    recordAccepted();
}

void DxfData::addLine(const DxfLine& line)
{
    ++m_entityStats.sourceEntities;
    if (!line.isValid()) {
        recordRejected("invalid line");
        return;
    }
    m_lines.push_back(line);
    ++m_entityStats.lines;
    recordAccepted();
}

void DxfData::addCircle(const DxfCircle& circle)
{
    ++m_entityStats.sourceEntities;
    if (!circle.isValid()) {
        recordRejected("invalid circle");
        return;
    }
    m_circles.push_back(circle);
    ++m_entityStats.circles;
    recordAccepted();
}

void DxfData::addArc(const DxfArc& arc)
{
    ++m_entityStats.sourceEntities;
    if (!arc.isValid()) {
        recordRejected("invalid arc");
        return;
    }
    m_arcs.push_back(arc);
    ++m_entityStats.arcs;
    recordAccepted();
}

void DxfData::addLWPolyline(const DxfLWPolyline& poly)
{
    ++m_entityStats.sourceEntities;
    if (!poly.isValid()) {
        recordRejected("invalid lwpolyline");
        return;
    }
    m_lwPolylines.push_back(poly);
    ++m_entityStats.lwPolylines;
    recordAccepted();
}

void DxfData::addEllipse(const DxfEllipse& ellipse)
{
    ++m_entityStats.sourceEntities;
    if (!ellipse.isValid()) {
        recordRejected("invalid ellipse");
        return;
    }
    m_ellipses.push_back(ellipse);
    ++m_entityStats.ellipses;
    recordAccepted();
}

void DxfData::addSpline(const DxfSpline& spline)
{
    ++m_entityStats.sourceEntities;
    if (!spline.isValid()) {
        recordRejected("invalid spline");
        return;
    }
    m_splines.push_back(spline);
    ++m_entityStats.splineKinds[spline.kind()];
    recordAccepted();
}

void DxfData::addSpline(DxfSpline&& spline)
{
    const SplineKind kind = spline.kind();
    ++m_entityStats.sourceEntities;
    if (!spline.isValid()) {
        recordRejected("invalid spline");
        return;
    }
    m_splines.push_back(std::move(spline));
    ++m_entityStats.splineKinds[kind];
    recordAccepted();
}

void DxfData::addGeneratedPoint(const DxfPoint& point)
{
    if (!point.isValid()) {
        recordRejected("invalid generated point");
        return;
    }
    m_points.push_back(point);
    ++m_entityStats.generatedEntities;
}

void DxfData::addGeneratedLine(const DxfLine& line)
{
    if (!line.isValid()) {
        recordRejected("invalid generated line");
        return;
    }
    m_lines.push_back(line);
    ++m_entityStats.generatedEntities;
}

void DxfData::addGeneratedCircle(const DxfCircle& circle)
{
    if (!circle.isValid()) {
        recordRejected("invalid generated circle");
        return;
    }
    m_circles.push_back(circle);
    ++m_entityStats.generatedEntities;
}

void DxfData::addGeneratedArc(const DxfArc& arc)
{
    if (!arc.isValid()) {
        recordRejected("invalid generated arc");
        return;
    }
    m_arcs.push_back(arc);
    ++m_entityStats.generatedEntities;
}

void DxfData::addGeneratedLWPolyline(const DxfLWPolyline& poly)
{
    if (!poly.isValid()) {
        recordRejected("invalid generated lwpolyline");
        return;
    }
    m_lwPolylines.push_back(poly);
    ++m_entityStats.generatedEntities;
}

void DxfData::addGeneratedEllipse(const DxfEllipse& ellipse)
{
    if (!ellipse.isValid()) {
        recordRejected("invalid generated ellipse");
        return;
    }
    m_ellipses.push_back(ellipse);
    ++m_entityStats.generatedEntities;
}

void DxfData::addGeneratedSpline(DxfSpline&& spline)
{
    if (!spline.isValid()) {
        recordRejected("invalid generated spline");
        return;
    }
    m_splines.push_back(std::move(spline));
    ++m_entityStats.generatedEntities;
}

void DxfData::recordRejected(const char* reason)
{
    ++m_entityStats.rejectedEntities;
    ++m_entityStats.rejectionReasons[reason];
}

void DxfData::recordAccepted()
{
    ++m_entityStats.acceptedEntities;
}

void DxfData::recordGeneratedEntity(EntityType sourceType)
{
    ++m_entityStats.sourceEntities;
    switch (sourceType) {
    case EntityType::Point:
        ++m_entityStats.points;
        break;
    case EntityType::Line:
        ++m_entityStats.lines;
        break;
    case EntityType::Circle:
        ++m_entityStats.circles;
        break;
    case EntityType::Arc:
        ++m_entityStats.arcs;
        break;
    case EntityType::LWPolyline:
        ++m_entityStats.lwPolylines;
        break;
    case EntityType::Ellipse:
        ++m_entityStats.ellipses;
        break;
    default:
        break;
    }
}

void DxfData::recordGeneratedSpline(const SplineKind& kind)
{
    ++m_entityStats.sourceEntities;
    ++m_entityStats.splineKinds[kind];
}

// ========================================================================
//  DxfSpline
// ========================================================================

DxfSpline::DxfSpline()
    : DxfEntity(EntityType::Spline)
{
}

DxfSpline::DxfSpline(const std::vector<DxfPoint>& ctrlPts,
                     const std::vector<double>& knots,
                     const std::vector<double>& weights,
                     const std::vector<DxfPoint>& fitPts,
                     int degree, int flags,
                     double tgStartX, double tgStartY, double tgStartZ,
                     double tgEndX, double tgEndY, double tgEndZ)
    : DxfEntity(EntityType::Spline)
    , m_ctrlPts(ctrlPts)
    , m_knots(knots)
    , m_weights(weights)
    , m_fitPts(fitPts)
    , m_degree(degree)
    , m_flags(flags)
    , m_tgStartX(tgStartX), m_tgStartY(tgStartY), m_tgStartZ(tgStartZ)
    , m_tgEndX(tgEndX), m_tgEndY(tgEndY), m_tgEndZ(tgEndZ)
{
}

DxfSpline::DxfSpline(std::vector<DxfPoint>&& ctrlPts,
                     std::vector<double>&& knots,
                     std::vector<double>&& weights,
                     std::vector<DxfPoint>&& fitPts,
                     int degree, int flags,
                     double tgStartX, double tgStartY, double tgStartZ,
                     double tgEndX, double tgEndY, double tgEndZ)
    : DxfEntity(EntityType::Spline)
    , m_ctrlPts(std::move(ctrlPts))
    , m_knots(std::move(knots))
    , m_weights(std::move(weights))
    , m_fitPts(std::move(fitPts))
    , m_degree(degree)
    , m_flags(flags)
    , m_tgStartX(tgStartX), m_tgStartY(tgStartY), m_tgStartZ(tgStartZ)
    , m_tgEndX(tgEndX), m_tgEndY(tgEndY), m_tgEndZ(tgEndZ)
{
}

SplineKind DxfSpline::kind() const
{
    const bool hasControlData =
        static_cast<int>(m_ctrlPts.size()) > m_degree
        && static_cast<int>(m_knots.size()) >=
           static_cast<int>(m_ctrlPts.size()) + m_degree + 1;

    SplineKind result;
    result.construction = hasControlData
        ? SplineConstruction::ControlBased
        : SplineConstruction::FitBased;
    result.rational = isRational();
    result.periodic = isPeriodic();
    result.closed = isClosed();
    return result;
}

bool DxfSpline::isValid() const
{
    constexpr int maxOcctDegree = 25;
    if (m_degree < 1 || m_degree > maxOcctDegree)
        return false;

    for (const DxfPoint& point : m_ctrlPts)
        if (!point.isValid()) return false;
    for (const DxfPoint& point : m_fitPts)
        if (!point.isValid()) return false;
    for (std::size_t i = 0; i < m_knots.size(); ++i) {
        if (!DxfNumeric::isFinite(m_knots[i])) return false;
        if (i > 0 && m_knots[i] < m_knots[i - 1]) return false;
    }
    for (double weight : m_weights)
        if (!DxfNumeric::isPositiveFinite(weight)) return false;

    const bool hasCtrl = m_ctrlPts.size() > static_cast<std::size_t>(m_degree);
    const bool hasFit = m_fitPts.size() >= 2;
    if (!hasCtrl && !hasFit) return false;

    if (hasCtrl) {
        constexpr double knotTolerance = 1.0e-12;
        if (m_knots.size() != m_ctrlPts.size()
            + static_cast<std::size_t>(m_degree) + 1)
            return false;
        if (std::abs(m_knots.front() - m_knots.back()) <= knotTolerance)
            return false;
        for (std::size_t begin = 0; begin < m_knots.size(); ) {
            std::size_t end = begin + 1;
            while (end < m_knots.size()
                   && std::abs(m_knots[end] - m_knots[begin]) <= knotTolerance)
                ++end;
            const std::size_t multiplicity = end - begin;
            const bool boundary = begin == 0 || end == m_knots.size();
            const std::size_t maximum = static_cast<std::size_t>(
                boundary ? m_degree + 1 : m_degree);
            if (multiplicity > maximum) return false;
            begin = end;
        }
        if (isRational()) {
            if (m_weights.size() != m_ctrlPts.size()) return false;
        }
    } else if (isRational()) {
        return false;
    }

    // Validate tangents are finite
    if (!DxfNumeric::areFinite(m_tgStartX, m_tgStartY, m_tgStartZ))
        return false;
    if (!DxfNumeric::areFinite(m_tgEndX, m_tgEndY, m_tgEndZ))
        return false;

    return true;
}

// ============================================================================
// 导入配置
// ============================================================================

#include <QStringList>

std::set<std::string> parseIgnoredDxfLayers(const QString& layerText)
{
    std::set<std::string> ignoredLayers;
    if (layerText.isEmpty())
        return ignoredLayers;

    // SkipEmptyParts 跳过连续逗号产生的空项；trimmed() 再去掉图层名前后的空格。
    const QStringList parts = layerText.split(',', QString::SkipEmptyParts);
    for (const QString& part : parts)
    {
        const std::string layer = part.trimmed().toStdString();
        if (!layer.empty())
            ignoredLayers.insert(layer);
    }
    return ignoredLayers;
}

// ============================================================================
// 导入结果
// ============================================================================

namespace {

DxfImportOutcomeStatus outcomeStatus(ImportBuildStatus status)
{
    switch (status)
    {
    case ImportBuildStatus::Canceled:
        return DxfImportOutcomeStatus::Canceled;
    case ImportBuildStatus::BeginFailed:
        return DxfImportOutcomeStatus::BeginFailed;
    case ImportBuildStatus::CreateFailed:
        return DxfImportOutcomeStatus::CreateFailed;
    case ImportBuildStatus::CommitFailed:
        return DxfImportOutcomeStatus::CommitFailed;
    case ImportBuildStatus::RollbackFailed:
        return DxfImportOutcomeStatus::RollbackFailed;
    case ImportBuildStatus::Success:
        break;
    }
    return DxfImportOutcomeStatus::Succeeded;
}

DxfImportErrorCode outcomeErrorCode(ImportBuildStatus status)
{
    switch (status)
    {
    case ImportBuildStatus::Canceled:
        return DxfImportErrorCode::Canceled;
    case ImportBuildStatus::BeginFailed:
        return DxfImportErrorCode::BeginFailed;
    case ImportBuildStatus::CreateFailed:
        return DxfImportErrorCode::CreateFailed;
    case ImportBuildStatus::CommitFailed:
        return DxfImportErrorCode::CommitFailed;
    case ImportBuildStatus::RollbackFailed:
        return DxfImportErrorCode::RollbackFailed;
    case ImportBuildStatus::Success:
        break;
    }
    return DxfImportErrorCode::None;
}

} // namespace

DxfImportOutcome DxfImportOutcome::success(int created)
{
    DxfImportOutcome result;
    result.status = DxfImportOutcomeStatus::Succeeded;
    result.createdCount = created;
    return result;
}

DxfImportOutcome DxfImportOutcome::failure(
    DxfImportOutcomeStatus failureStatus,
    DxfImportErrorCode failureCode,
    const QString& failureStage,
    const QString& failureMessage,
    int created)
{
    DxfImportOutcome result;
    result.status = failureStatus;
    result.errorCode = failureCode;
    result.stage = failureStage;
    result.message = failureMessage;
    result.createdCount = created;
    return result;
}

DxfImportOutcome DxfImportOutcome::fromBuildFailure(
    const ImportBuildResult& buildResult,
    const QString& failureStage,
    const QString& failureMessage)
{
    DxfImportOutcome result = failure(
        outcomeStatus(buildResult.status),
        outcomeErrorCode(buildResult.status),
        failureStage,
        failureMessage,
        buildResult.createdCount);
    if (buildResult.rollbackAttempted)
    {
        result.rollbackStatus = buildResult.rollbackSucceeded
            ? DxfImportRollbackStatus::Succeeded
            : DxfImportRollbackStatus::Failed;
        result.rollbackMessage = buildResult.rollbackMessage;
    }
    else if (buildResult.status == ImportBuildStatus::RollbackFailed)
    {
        result.rollbackStatus = DxfImportRollbackStatus::Failed;
        result.rollbackMessage = buildResult.message;
    }
    return result;
}

// ============================================================================
// 公共参数校验
// ============================================================================


#include <limits>

namespace DxfImportValidation {

Result validate(
    double baseX,
    double baseY,
    double baseZ,
    double curveTolerance,
    double nodeMergeTolerance,
    int maxOutputEntities)
{
    if (!DxfNumeric::areFinite(baseX, baseY, baseZ))
    {
        return Result{
            false,
            0,
            " invalid_base_coordinates",
            QStringLiteral(
                "[importDxf] ERROR: base coordinates must be finite")};
    }
    if (!DxfNumeric::isWithinInclusive(
            curveTolerance, kMinimumCurveTolerance, kMaximumCurveTolerance))
    {
        return Result{
            false,
            0,
            " invalid_curveTolerance",
            QString("[importDxf] ERROR: curveTolerance %1 is outside [%2, %3]")
                .arg(curveTolerance)
                .arg(kMinimumCurveTolerance)
                .arg(kMaximumCurveTolerance)};
    }
    if (!DxfNumeric::isNonNegativeFinite(nodeMergeTolerance))
    {
        return Result{
            false,
            0,
            " invalid_nodeMergeTolerance",
            QString("[importDxf] ERROR: invalid nodeMergeTolerance %1")
                .arg(nodeMergeTolerance)};
    }
    if (maxOutputEntities != static_cast<int>(kSmallDrawingEntityLimit)
        && maxOutputEntities != kLargeDrawingEntityLimit
        && maxOutputEntities != DxfImportDefaults::kUnlimitedOutputEntities)
    {
        return Result{
            false,
            0,
            " error_code=INVALID_ARGUMENT invalid_maxOutputEntities",
            QStringLiteral(
                "[importDxf] ERROR [INVALID_ARGUMENT]: invalid maxOutputEntities")};
    }

    const std::size_t outputLimit = maxOutputEntities < 0
        ? std::numeric_limits<std::size_t>::max()
        : static_cast<std::size_t>(maxOutputEntities);
    return Result{true, outputLimit, std::string(), QString()};
}

} // namespace DxfImportValidation

DxfImportModeResult selectDxfImportMode(
    const QString& importMode,
    const QString& modelName,
    const QString& partName)
{
    const bool finiteElement = importMode.compare(
        QStringLiteral("FiniteElement"), Qt::CaseInsensitive) == 0;
    const bool sketch = importMode.isEmpty() || importMode.compare(
        QStringLiteral("Sketch"), Qt::CaseInsensitive) == 0;
    if (!sketch && !finiteElement)
    {
        return DxfImportModeResult{
            false,
            DxfImportMode::Sketch,
            "validate_mode",
            " mode=\"" + importMode.toLocal8Bit().toStdString() + "\"",
            QString("[importDxf] ERROR: unsupported importMode '%1'")
                .arg(importMode)};
    }

    if (!finiteElement)
    {
        return DxfImportModeResult{
            true, DxfImportMode::Sketch, "validate_mode",
            std::string(), QString()};
    }

    if (modelName.isEmpty() || partName.isEmpty())
    {
        const QString missingName = modelName.isEmpty()
            ? QStringLiteral("modelName")
            : QStringLiteral("partName");
        return DxfImportModeResult{
            false,
            DxfImportMode::FiniteElement,
            "validate_params",
            " missing=\"" + missingName.toLocal8Bit().toStdString() + "\"",
            QString("[importDxf] ERROR: FE mode requires '%1'")
                .arg(missingName)};
    }

    return DxfImportModeResult{
        true, DxfImportMode::FiniteElement, "validate_mode",
        std::string(), QString()};
}

DxfImportPreflightResult validateDxfImportRequest(
    const DxfImportRequest& request)
{
    // 第一步只验证数值范围，并把 -1（不限最终输出）转换为 size_t 最大值。
    const DxfImportValidation::Result parameters =
        DxfImportValidation::validate(
            request.baseX,
            request.baseY,
            request.baseZ,
            request.curveTolerance,
            request.nodeMergeTolerance,
            request.maxOutputEntities);
    if (!parameters.valid)
    {
        return DxfImportPreflightResult{
            false,
            DxfImportMode::Sketch,
            0,
            "validate_params",
            parameters.detail,
            parameters.message};
    }

    // 第二步判断走 Sketch 还是 FE，并在 FE 模式下检查模型名、部件名。
    const DxfImportModeResult mode = selectDxfImportMode(
        request.importMode, request.modelName, request.partName);
    if (!mode.valid)
    {
        return DxfImportPreflightResult{
            false,
            mode.mode,
            0,
            mode.stage,
            mode.detail,
            mode.message};
    }

    return DxfImportPreflightResult{
        true,
        mode.mode,
        parameters.outputLimit,
        std::string(),
        std::string(),
        QString()};
}
