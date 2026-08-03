#pragma once

#include <memory>
#include <string>

namespace spdlog {
class logger;
}

// ============================================================================
// Sketch 中间数据 SamData
// ============================================================================
#ifndef SamData_h
#define SamData_h

#include "DxfData.h"
#include <cstddef>
#include <vector>

/// Describes which original curve produced a converted line segment.
/// lineIndex refers to the same DxfLine stored in SamData::lines(), so this
/// metadata does not duplicate or alter the geometry used to build SAM.
struct CurveSegmentSource {
    EntityType parentType;
    int parentId;
    size_t segmentIndex;
    size_t lineIndex;
};

/**
 * @brief Sketch 转换阶段的中间结果，可直接交给 SamBuilder/skcGeomFactory。
 *
 * 为什么不让 ConversionEngine 直接调用 SAM？因为这个纯数据对象切断了算法与 SDK，
 * 转换可独立单元测试，也能在真正修改 SAM 数据库之前完整验证输出预算和错误。
 * 坐标已加上用户基点；复杂曲线已离散为 line，原生 circle 仍保留为 circle。
 */
class SamData {
public:
    SamData() = default;

    // ======== read-only access ========
    const std::vector<DxfLine>&   lines()   const { return m_lines; }
    const std::vector<DxfCircle>& circles() const { return m_circles; }
    const std::vector<CurveSegmentSource>& curveSegments() const { return m_curveSegments; }

    // ======== modifiers ========
    void addLine(const DxfLine& line)   { m_lines.push_back(line); }
    void addCircle(const DxfCircle& c)  { m_circles.push_back(c); }
    void addCurveSegment(EntityType parentType, int parentId,
                         size_t segmentIndex, const DxfLine& line) {
        m_lines.push_back(line);
        // 花括号是聚合初始化，按 struct 字段声明顺序一次填入四个值。
        CurveSegmentSource source = {
            parentType, parentId, segmentIndex, m_lines.size() - 1
        };
        m_curveSegments.push_back(source);
    }

    void clear() {
        m_lines.clear();
        m_circles.clear();
        m_curveSegments.clear();
        m_errorCode = DxfImportErrorCode::None;
        m_errorMessage.clear();
    }

    DxfImportErrorCode errorCode() const { return m_errorCode; }
    QString errorMessage() const { return m_errorMessage; }
    void setError(DxfImportErrorCode code, const QString& message) {
        m_errorCode = code;
        m_errorMessage = message;
    }

private:
    std::vector<DxfLine>   m_lines;
    std::vector<DxfCircle> m_circles;
    std::vector<CurveSegmentSource> m_curveSegments;
    DxfImportErrorCode m_errorCode = DxfImportErrorCode::None;
    QString m_errorMessage;
};

#endif

// ============================================================================
// Sketch 纯转换器
// ============================================================================
#ifndef ConversionEngine_h
#define ConversionEngine_h


/**
 * @brief 将保留 DXF 语义的 `DxfData` 转成 Sketch 可写的 `SamData`。
 *
 * 它过滤非法实体、把用户基点加到坐标、让 GeometryUtils 离散圆弧/椭圆/样条，
 * 并在产生过多线段时停止。该类不 include SAM SDK，属于可测试的纯转换层。
 */
class ConversionEngine {
public:
    ConversionEngine() = default;

    /// @brief Convert DXF entities to SAM-ready geometry.
    /// @param dxfData   Source DXF entity container.
    /// @param baseX     Translation offset X.
    /// @param baseY     Translation offset Y.
    /// @param baseZ     Translation offset Z.
    /// @param tolerance Curve tessellation sagitta tolerance.
    /// @param outData   [out] Resulting SAM-compatible entity container.
    /// @return true if at least one valid entity was converted.
    bool convert(const DxfData& dxfData,
                 double baseX, double baseY, double baseZ,
                 double tolerance,
                 SamData& outData,
                 std::size_t maxOutputEntities = 100000) const;

    /// @brief Default bulge/chord-height tolerance for curve tessellation.
    static double defaultBulgeTolerance()
    {
        return DxfImportDefaults::kCurveTolerance;
    }

private:
    /// @brief Translate a point by the given offsets.
    static DxfPoint translate(const DxfPoint& pt,
                              double bx, double by, double bz);
};

#endif

// ============================================================================
// Sketch Builder 抽象接口
// ============================================================================
class ISamImportBuilder
{
public:
    // 通过接口指针销毁派生对象时，需要虚析构保证派生析构函数也被调用。
    virtual ~ISamImportBuilder() = default;

    // `= 0` 表示纯虚函数；测试和两种真实 Builder 会各自实现这些步骤。
    virtual ImportBuildResult beginImport(
        const QString& modelName = QStringLiteral("Model-1")) = 0;
    virtual ImportBuildResult createLines(
        const std::vector<DxfLine>& lines) = 0;
    virtual ImportBuildResult createCircles(
        const std::vector<DxfCircle>& circles) = 0;
    virtual ImportBuildResult commit() = 0;
    virtual ImportBuildResult rollback() = 0;
};

namespace DxfImportBuildService {

ImportBuildResult buildSamSketch(
    const SamData& samData,
    ISamImportBuilder& builder);

} // namespace DxfImportBuildService

/// 记录 Sketch 转换结果；属于 Sketch Core，不让公共运行层依赖 SamData。
void logConvertedSamData(
    const std::shared_ptr<spdlog::logger>& logger,
    const std::string& importId,
    const SamData& data);
