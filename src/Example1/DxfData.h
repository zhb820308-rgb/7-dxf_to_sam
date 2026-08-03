#pragma once

// ============================================================================
// DxfImportConfig
// ============================================================================

#include <QString>

#include <cstddef>
#include <set>
#include <string>

/**
 * @file DxfData.h
 * @brief DXF 导入的“输入配置”总入口。
 *
 * 这个文件合并了原来的默认值、请求 DTO、导入模式和忽略图层配置。它们都回答
 * 同一个问题：“用户希望怎样导入？”，放在一起比四处寻找小头文件更容易理解。
 */

namespace DxfImportDefaults {

// constexpr 表示编译期常量。统一放在这里，GUI、解析器和转换器会使用同一套默认值。
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

/// 用户选择的两种导入路线：生成 Sketch，或生成有限元节点/杆单元。
enum class DxfImportMode
{
    Sketch,
    FiniteElement
};

/**
 * @brief 一次导入的全部输入参数（DTO，数据传输对象）。
 *
 * `struct` 的成员默认 public。这里故意只放数据，不放 UI 或解析逻辑，使 Qt
 * 对话框、Python 入口和测试都能构造同一种请求。`= ...` 是类内默认初始化：
 * 调用者没填写字段时，自动使用右侧默认值。
 */
struct DxfImportRequest
{
    QString filePath;
    double baseX = 0.0;
    double baseY = 0.0;
    double baseZ = 0.0;
    double curveTolerance = DxfImportDefaults::kCurveTolerance;
    QString ignoredLayers;
    QString importMode;
    QString modelName;
    QString partName;
    double nodeMergeTolerance = DxfImportDefaults::kNodeMergeTolerance;
    int maxOutputEntities = DxfImportDefaults::kDefaultMaxOutputEntities;
};

/// 把界面上的逗号分隔图层文本整理成便于查询的集合。
std::set<std::string> parseIgnoredDxfLayers(const QString& layerText);

// ============================================================================
// DxfImportResult
// ============================================================================


/**
 * @file DxfData.h
 * @brief 汇总导入流程中跨层传递的错误码和结果值对象。
 *
 * 这些类型原来散落在 DxfImportError、ImportBuildResult 和 DxfImportOutcome 中。
 * 它们都用于表达“上一步做得怎么样”，因此合并后可以沿着一份文件看完整的
 * 错误传播链：底层错误码 -> Builder 阶段结果 -> 对外导入结果。
 */

enum class DxfImportErrorCode
{
    None = 0,
    InvalidArgument,
    ReadFailed,
    ExpansionLimit,
    ConversionLimit,
    NoSupportedEntities,
    ConversionFailed,
    Canceled,
    BeginFailed,
    CreateFailed,
    CommitFailed,
    RollbackFailed
};

enum class ImportBuildStatus
{
    Success,
    Canceled,
    BeginFailed,
    CreateFailed,
    CommitFailed,
    RollbackFailed
};

/**
 * Builder 单步操作的返回值。它不只返回 true/false，还保留创建数量和回滚结果，
 * 这样清理失败不会覆盖最初的业务错误。
 */
struct ImportBuildResult
{
    ImportBuildStatus status = ImportBuildStatus::Success;
    int createdCount = 0;
    QString message;
    bool rollbackAttempted = false;
    bool rollbackSucceeded = false;
    QString rollbackMessage;

    bool succeeded() const { return status == ImportBuildStatus::Success; }

    static ImportBuildResult success(int created = 0)
    {
        // C++17 聚合返回：编译器按字段顺序构造一个临时 ImportBuildResult。
        return {
            ImportBuildStatus::Success,
            created,
            QString(),
            false,
            false,
            QString()};
    }

    static ImportBuildResult failure(
        ImportBuildStatus failureStatus,
        const QString& error,
        int created = 0)
    {
        return {
            failureStatus,
            created,
            error,
            false,
            false,
            QString()};
    }

    void recordRollback(const ImportBuildResult& cleanup)
    {
        rollbackAttempted = true;
        rollbackSucceeded = cleanup.succeeded();
        rollbackMessage = cleanup.message;
    }
};

namespace DxfImportBuildDetail {

/**
 * @brief 某一步创建或提交失败后，统一执行 Builder 的补偿回滚。
 *
 * 这里使用函数模板，是因为 Sketch Builder 与 FE Builder 的具体类型不同，但都遵守
 * `rollback() -> ImportBuildResult` 这一最小协议。模板让公共层不必依赖任何一种业务
 * Builder。failure 按值传入是有意的：函数只修改自己的结果副本，再把完整错误返回。
 */
template <typename Builder>
ImportBuildResult rollbackAfterFailure(
    Builder& builder,
    ImportBuildResult failure,
    int previouslyCreated = 0)
{
    failure.createdCount += previouslyCreated;
    const ImportBuildResult cleanup = builder.rollback();
    // 保留原始失败，同时记录回滚是否成功，避免“清理失败”覆盖真正的业务错误。
    failure.recordRollback(cleanup);
    return failure;
}

} // namespace DxfImportBuildDetail

enum class DxfImportOutcomeStatus
{
    Succeeded,
    Canceled,
    ValidationFailed,
    ParseFailed,
    ConvertFailed,
    BeginFailed,
    CreateFailed,
    CommitFailed,
    RollbackFailed
};

enum class DxfImportRollbackStatus
{
    NotAttempted,
    Succeeded,
    Failed
};

/// Python/SAM 入口最终拿到的完整导入结果。
struct DxfImportOutcome
{
    DxfImportOutcomeStatus status = DxfImportOutcomeStatus::ValidationFailed;
    DxfImportErrorCode errorCode = DxfImportErrorCode::None;
    QString stage;
    QString message;
    int createdCount = 0;
    DxfImportRollbackStatus rollbackStatus =
        DxfImportRollbackStatus::NotAttempted;
    QString rollbackMessage;

    bool succeeded() const
    {
        return status == DxfImportOutcomeStatus::Succeeded;
    }

    bool canceled() const
    {
        return status == DxfImportOutcomeStatus::Canceled;
    }

    static DxfImportOutcome success(int createdCount);

    static DxfImportOutcome failure(
        DxfImportOutcomeStatus status,
        DxfImportErrorCode errorCode,
        const QString& stage,
        const QString& message,
        int createdCount = 0);

    static DxfImportOutcome fromBuildFailure(
        const ImportBuildResult& buildResult,
        const QString& stage,
        const QString& message);
};

// ============================================================================
// DxfNumeric
// ============================================================================

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

// ============================================================================
// DxfData
// ============================================================================
#ifndef DxfData_h
#define DxfData_h

#include <map>
#include <utility>
#include <vector>

// 本文件是“领域数据模型”：只描述 DXF 实体和容器，不依赖 SAM SDK。
// 可以先把 class 理解成“C struct + 与数据绑定的函数 + 访问权限”。.h 声明别人
// 可以怎样使用对象，DxfData.cpp 则实现较长的成员函数。

// ======== InsertInfo — BLOCK/INSERT 的实例化参数 ========

struct InsertInfo {
    std::string blockName;
    std::string layer = "0";
    double insertX = 0, insertY = 0, insertZ = 0;
    double scaleX  = 1, scaleY  = 1, scaleZ  = 1;
    double angle   = 0;  // radians
    int    colCount = 1, rowCount = 1;
    double colSpace = 0, rowSpace = 0;
};

// ======== EntityType ========

// enum class 是有作用域的枚举，使用时必须写 EntityType::Line；它不会像 C enum
// 那样把 Line 这个普通名字泄漏到全局作用域，也不会随意隐式转成 int。
enum class EntityType {
    Point,
    Line,
    Circle,
    Arc,
    LWPolyline,
    Ellipse,
    Spline,
};

enum class SplineConstruction {
    ControlBased,
    FitBased,
};

struct SplineKind {
    SplineConstruction construction = SplineConstruction::ControlBased;
    bool rational = false;
    bool periodic = false;
    bool closed = false;

    bool operator<(const SplineKind& other) const;
};

struct DxfEntityStats {
    std::size_t sourceEntities = 0;
    std::size_t acceptedEntities = 0;
    std::size_t rejectedEntities = 0;
    std::size_t generatedEntities = 0;
    std::map<std::string, std::size_t> rejectionReasons;
    std::size_t points = 0;
    std::size_t lines = 0;
    std::size_t lwPolylines = 0;
    std::size_t circles = 0;
    std::size_t arcs = 0;
    std::size_t ellipses = 0;
    std::map<SplineKind, std::size_t> splineKinds;

    std::size_t splineCount() const;
    std::size_t curveCount() const;
};

// ======== DxfEntity（抽象基类）========

/**
 * 所有项目内 DXF 实体的共同父类，保存 id、类型和图层。
 * `isValid() = 0` 是纯虚函数，因此不能直接创建 DxfEntity，只能创建实现它的
 * DxfLine/DxfCircle 等派生类。`virtual` 允许通过基类指针调用派生类版本（多态）。
 */
class DxfEntity {
public:
    DxfEntity(EntityType entityType);
    // 基类析构函数必须是 virtual，才能通过 DxfEntity* 安全删除派生对象。
    // `= default` 要求编译器生成默认实现。
    virtual ~DxfEntity() = default;

    int getId() const { return id; }
    EntityType getType() const { return type; }
    // 前一个 const 表示返回只读引用，避免复制字符串；末尾 const 表示本函数不修改对象。
    const std::string& layer() const { return m_layer; }
    void setLayer(const std::string& layerName) { m_layer = layerName; }

    virtual bool isValid() const = 0;

private:
    int id;
    EntityType type;
    std::string m_layer = "0";
};

// ======== DxfPoint ========

class DxfPoint : public DxfEntity {
public:
    DxfPoint();
    DxfPoint(double ix, double iy, double iz);

    double x() const { return m_x; }
    double y() const { return m_y; }
    double z() const { return m_z; }

    void setX(double v) { m_x = v; }
    void setY(double v) { m_y = v; }
    void setZ(double v) { m_z = v; }

    // override 明确覆盖父类纯虚函数；若签名不匹配，编译器会报错。
    bool isValid() const override;

private:
    double m_x = 0.0;
    double m_y = 0.0;
    double m_z = 0.0;
};

// ======== DxfLine ========

class DxfLine : public DxfEntity {
public:
    DxfLine();
    DxfLine(const DxfPoint& start, const DxfPoint& end);

    const DxfPoint& start() const { return m_start; }
    const DxfPoint& end()   const { return m_end; }

    bool isValid() const override;

private:
    DxfPoint m_start;
    DxfPoint m_end;
};

// ======== DxfCircle ========

class DxfCircle : public DxfEntity {
public:
    DxfCircle();
    DxfCircle(const DxfPoint& center, double radius);

    const DxfPoint& center() const { return m_center; }
    double radius()          const { return m_radius; }

    bool isValid() const override;

private:
    DxfPoint m_center;
    double m_radius = 0.0;
};

// ======== DxfArc ========
/// Circular arc defined by center, radius, start/end angles (radians), and direction.

class DxfArc : public DxfEntity {
public:
    DxfArc();
    DxfArc(const DxfPoint& center, double radius,
           double startAngle, double endAngle, bool isCCW);

    const DxfPoint& center()      const { return m_center; }
    double radius()               const { return m_radius; }
    double startAngle()           const { return m_startAngle; }
    double endAngle()             const { return m_endAngle; }
    bool   isCCW()                const { return m_isCCW; }

    bool isValid() const override;

private:
    DxfPoint m_center;
    double m_radius      = 0.0;
    double m_startAngle  = 0.0;
    double m_endAngle    = 0.0;
    bool   m_isCCW       = true;
};

// ======== DxfEllipse ========
/// Elliptical arc defined by center, major axis endpoint (relative to center),
/// axis ratio, parameter range, and direction.

class DxfEllipse : public DxfEntity {
public:
    DxfEllipse();
    DxfEllipse(const DxfPoint& center, const DxfPoint& majorAxisEnd,
               double ratio, double startParam, double endParam, bool isCCW);

    const DxfPoint& center()       const { return m_center; }
    const DxfPoint& majorAxisEnd() const { return m_majorAxisEnd; }
    double ratio()                 const { return m_ratio; }
    double startParam()            const { return m_startParam; }
    double endParam()              const { return m_endParam; }
    bool   isCCW()                 const { return m_isCCW; }

    bool isValid() const override;

private:
    DxfPoint m_center;
    DxfPoint m_majorAxisEnd;  ///< Major axis endpoint relative to center.
    double m_ratio       = 1.0;
    double m_startParam  = 0.0;
    double m_endParam    = 0.0;
    bool   m_isCCW       = true;
};

// ======== DxfLWPolyline ========
/// Lightweight polyline defined by vertex list and per-segment bulge values,
/// preserving raw geometry for tessellation.

class DxfLWPolyline : public DxfEntity {
public:
    DxfLWPolyline();
    DxfLWPolyline(const std::vector<DxfPoint>& vertices,
                  const std::vector<double>& bulges,
                  bool closed, double constZ = 0.0);

    const std::vector<DxfPoint>& vertices() const { return m_vertices; }
    const std::vector<double>&   bulges()   const { return m_bulges; }
    bool   isClosed() const { return m_closed; }
    double constZ()   const { return m_constZ; }
    int    vertexCount() const { return static_cast<int>(m_vertices.size()); }

    bool isValid() const override;

private:
    std::vector<DxfPoint> m_vertices;
    std::vector<double>   m_bulges;    ///< bulge[i] describes arc segment from vertex i to vertex i+1
    bool   m_closed = false;
    double m_constZ = 0.0;
};

// ======== DxfSpline ========
// B-spline curve: control points, knots, weights, fit points.
// flags bit 0=closed, bit 1=periodic, bit 2=rational.

class DxfSpline : public DxfEntity {
public:
    DxfSpline();
    DxfSpline(const std::vector<DxfPoint>& ctrlPts,
              const std::vector<double>& knots,
              const std::vector<double>& weights,
              const std::vector<DxfPoint>& fitPts,
              int degree, int flags,
              double tgStartX, double tgStartY, double tgStartZ,
              double tgEndX, double tgEndY, double tgEndZ);
    /// Move-assemble constructor: transfers ownership of the input containers,
    /// avoiding one layer of deep copy in the hot SPLINE parse path.
    // `&&` 是右值引用，配合 std::move 转移 vector 的缓冲区所有权，避免大样条数据
    // 在解析热路径中被完整复制。传入对象之后不应再依赖其旧内容。
    DxfSpline(std::vector<DxfPoint>&& ctrlPts,
              std::vector<double>&& knots,
              std::vector<double>&& weights,
              std::vector<DxfPoint>&& fitPts,
              int degree, int flags,
              double tgStartX, double tgStartY, double tgStartZ,
              double tgEndX, double tgEndY, double tgEndZ);

    const std::vector<DxfPoint>& controlPoints() const { return m_ctrlPts; }
    const std::vector<double>&   knots()          const { return m_knots; }
    const std::vector<double>&   weights()        const { return m_weights; }
    const std::vector<DxfPoint>& fitPoints()       const { return m_fitPts; }
    int    degree()      const { return m_degree; }
    int    flags()        const { return m_flags; }
    // DXF 把多个布尔状态压在 flags 的不同二进制位中；按位与 `&` 用来检查某一位。
    bool   isRational()  const { return (m_flags & 4) != 0; }
    bool   isPeriodic()  const { return (m_flags & 2) != 0; }
    bool   isClosed()    const { return (m_flags & 1) != 0; }
    double tgStartX() const { return m_tgStartX; }
    double tgStartY() const { return m_tgStartY; }
    double tgStartZ() const { return m_tgStartZ; }
    double tgEndX()   const { return m_tgEndX; }
    double tgEndY()   const { return m_tgEndY; }
    double tgEndZ()   const { return m_tgEndZ; }
    SplineKind kind() const;

    bool isValid() const override;

private:
    std::vector<DxfPoint> m_ctrlPts;
    std::vector<double>   m_knots;
    std::vector<double>   m_weights;
    std::vector<DxfPoint> m_fitPts;
    int    m_degree    = 0;
    int    m_flags     = 0;
    double m_tgStartX  = 0.0, m_tgStartY  = 0.0, m_tgStartZ  = 0.0;
    double m_tgEndX    = 0.0, m_tgEndY    = 0.0, m_tgEndZ    = 0.0;
};

// ======== DxfData (container) ========

/**
 * @brief 一次 DXF 解析结果的所有者。
 *
 * 每种实体放在独立 `std::vector` 中。vector 类似 Python list，但元素类型固定且
 * 连续存储；容器自动管理内存。解析器填充它，转换器只读它，转换成功后编排器
 * `clear()` 以尽早释放大图数据。
 */
class DxfData {
public:
    DxfData() = default;

    // ======== modifiers ========
    void addPoint(const DxfPoint& pt);
    void addLine(const DxfLine& line);
    void addCircle(const DxfCircle& circle);
    void addArc(const DxfArc& arc);
    void addLWPolyline(const DxfLWPolyline& poly);
    void addEllipse(const DxfEllipse& ellipse);
    void addSpline(const DxfSpline& spline);
    // 同名不同参数叫函数重载：左值版本复制，右值版本移动。
    void addSpline(DxfSpline&& spline);
    void addGeneratedPoint(const DxfPoint& point);
    void addGeneratedLine(const DxfLine& line);
    void addGeneratedCircle(const DxfCircle& circle);
    void addGeneratedArc(const DxfArc& arc);
    void addGeneratedLWPolyline(const DxfLWPolyline& poly);
    void addGeneratedEllipse(const DxfEllipse& ellipse);
    void addGeneratedSpline(DxfSpline&& spline);
    void recordGeneratedEntity(EntityType sourceType);
    void recordGeneratedSpline(const SplineKind& kind);
    void addInsert(const InsertInfo& ins) { m_inserts.push_back(ins); }
    void reserveLines(std::size_t count);
    void reservePoints(std::size_t count);
    void reserveLWPolylines(std::size_t count);
    void reserveSplines(std::size_t count);
    void clear();

    // ======== accessors ========
    // 返回 const 引用：调用者能遍历但不能改容器，同时避免复制成千上万个实体。
    const std::vector<DxfPoint>&       points()       const { return m_points; }
    const std::vector<DxfLine>&        lines()        const { return m_lines; }
    const std::vector<DxfCircle>&      circles()      const { return m_circles; }
    const std::vector<DxfArc>&         arcs()         const { return m_arcs; }
    const std::vector<DxfLWPolyline>&  lwPolylines()  const { return m_lwPolylines; }
    const std::vector<DxfEllipse>&     ellipses()     const { return m_ellipses; }
    const std::vector<DxfSpline>&      splines()      const { return m_splines; }
    const std::vector<InsertInfo>&     inserts()      const { return m_inserts; }
    const DxfEntityStats&             entityStats()  const { return m_entityStats; }

    /// Number of all supported DXF entities, including standalone POINTs.
    std::size_t entityCount() const {
        return m_points.size() + m_lines.size() + m_circles.size() +
               m_arcs.size() + m_lwPolylines.size() + m_ellipses.size() +
               m_splines.size();
    }

    /// Number of entities supported by the Sketch conversion path.
    int sketchEntityCount() const {
        // static_cast<int> 是显式类型转换。size() 返回无符号 size_t，而旧 SAM 接口
        // 常用 int；显式转换让这种边界一目了然。
        return static_cast<int>(
            m_lines.size() + m_circles.size() +
            m_arcs.size() + m_lwPolylines.size() + m_ellipses.size() +
            m_splines.size());
    }

    /// Number of entities supported by the finite-element conversion path.
    int feEntityCount() const { return entityCount(); }

    // ======== error / validity ========
    QString errorMessage() const { return m_errorMessage; }
    void setErrorMessage(const QString& msg) { m_errorMessage = msg; }
    DxfImportErrorCode errorCode() const { return m_errorCode; }
    void setError(DxfImportErrorCode code, const QString& msg) {
        m_errorCode = code;
        m_errorMessage = msg;
    }

    bool isValid() const { return m_isValid; }
    void setValid(bool v) { m_isValid = v; }

private:
    void recordRejected(const char* reason);
    void recordAccepted();

    std::vector<DxfPoint>       m_points;
    std::vector<DxfLine>        m_lines;
    std::vector<DxfCircle>      m_circles;
    std::vector<DxfArc>         m_arcs;
    std::vector<DxfLWPolyline>  m_lwPolylines;
    std::vector<DxfEllipse>     m_ellipses;
    std::vector<DxfSpline>      m_splines;
    std::vector<InsertInfo>     m_inserts;
    DxfEntityStats m_entityStats;
    QString m_errorMessage;
    DxfImportErrorCode m_errorCode = DxfImportErrorCode::None;
    bool m_isValid = false;
};

// ======== DxfBlock — named block definition (parsed, not yet expanded) ========

/**
 * @brief 尚未展开的命名块定义。
 *
 * 它保存块内局部坐标实体和嵌套 INSERT；真正的模型空间实体由
 * DxfBlockExpansion 按插入点/缩放/旋转递归生成。
 */
class DxfBlock {
public:
    DxfBlock() = default;

    const std::string& name()  const { return m_name; }
    double baseX() const { return m_baseX; }
    double baseY() const { return m_baseY; }
    double baseZ() const { return m_baseZ; }

    void setName(const std::string& n)    { m_name = n; }
    void setBase(double x, double y, double z) { m_baseX = x; m_baseY = y; m_baseZ = z; }

    const std::vector<DxfPoint>&      points()      const { return m_points; }
    const std::vector<DxfLine>&       lines()       const { return m_lines; }
    const std::vector<DxfCircle>&     circles()     const { return m_circles; }
    const std::vector<DxfArc>&        arcs()        const { return m_arcs; }
    const std::vector<DxfLWPolyline>& lwPolylines() const { return m_lwPolylines; }
    const std::vector<DxfEllipse>&    ellipses()    const { return m_ellipses; }
    const std::vector<DxfSpline>&     splines()     const { return m_splines; }

    void addPoint(const DxfPoint& pt)           { if (pt.isValid()) m_points.push_back(pt); }
    void addLine(const DxfLine& line)           { if (line.isValid()) m_lines.push_back(line); }
    void addCircle(const DxfCircle& circle)     { if (circle.isValid()) m_circles.push_back(circle); }
    void addArc(const DxfArc& arc)              { if (arc.isValid()) m_arcs.push_back(arc); }
    void addLWPolyline(const DxfLWPolyline& p)  { if (p.isValid()) m_lwPolylines.push_back(p); }
    void addEllipse(const DxfEllipse& e)        { if (e.isValid()) m_ellipses.push_back(e); }
    void addSpline(const DxfSpline& s)          { if (s.isValid()) m_splines.push_back(s); }
    // std::move 本身不搬数据，它把 s 标记成可转移对象，vector 随后接管其内部资源。
    void addSpline(DxfSpline&& s)               { if (s.isValid()) m_splines.push_back(std::move(s)); }

    // --- Nested INSERTs ---
    void addInsert(const InsertInfo& ins) { m_inserts.push_back(ins); }
    const std::vector<InsertInfo>& inserts() const { return m_inserts; }

private:
    std::string m_name;
    double m_baseX = 0.0, m_baseY = 0.0, m_baseZ = 0.0;
    std::vector<DxfPoint>      m_points;
    std::vector<DxfLine>       m_lines;
    std::vector<DxfCircle>     m_circles;
    std::vector<DxfArc>        m_arcs;
    std::vector<DxfLWPolyline> m_lwPolylines;
    std::vector<DxfEllipse>    m_ellipses;
    std::vector<DxfSpline>     m_splines;
    std::vector<InsertInfo>    m_inserts;
};

#endif // DxfData_h

// ============================================================================
// DxfImportValidation
// ============================================================================




/**
 * @file DxfData.h
 * @brief 导入执行前的全部校验：数值、模式以及整份请求预检。
 *
 * 阅读顺序是 validate() -> selectDxfImportMode() ->
 * validateDxfImportRequest()。最后一个函数把前两个步骤串成 Orchestrator 可直接使用
 * 的预检结果。原来的 Mode 和 Preflight 小文件已合并到这里。
 */

namespace DxfImportValidation {

// 保留这些别名是为了让校验和界面提示都从同一命名空间读取限制值。
constexpr std::size_t kSmallDrawingEntityLimit =
    DxfImportDefaults::kSmallDrawingEntityLimit;
constexpr int kLargeDrawingEntityLimit =
    DxfImportDefaults::kLargeDrawingEntityLimit;
constexpr double kMinimumCurveTolerance =
    DxfImportDefaults::kMinimumCurveTolerance;
constexpr double kMaximumCurveTolerance =
    DxfImportDefaults::kMaximumCurveTolerance;

struct Result
{
    bool valid = false;
    std::size_t outputLimit = 0;
    std::string detail;
    QString message;
};

Result validate(
    double baseX,
    double baseY,
    double baseZ,
    double curveTolerance,
    double nodeMergeTolerance,
    int maxOutputEntities);

} // namespace DxfImportValidation

struct DxfImportModeResult
{
    bool valid = false;
    DxfImportMode mode = DxfImportMode::Sketch;
    const char* stage = "validate_mode";
    std::string detail;
    QString message;
};

DxfImportModeResult selectDxfImportMode(
    const QString& importMode,
    const QString& modelName,
    const QString& partName);

struct DxfImportPreflightResult
{
    bool valid = false;
    DxfImportMode mode = DxfImportMode::Sketch;
    std::size_t outputLimit = 0;
    std::string stage;
    std::string detail;
    QString message;
};

DxfImportPreflightResult validateDxfImportRequest(
    const DxfImportRequest& request);
