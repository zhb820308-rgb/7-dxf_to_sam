#pragma once

// ============================================================================
// DxfEntityValidation
// ============================================================================

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

// ============================================================================
// DxfTransform
// ============================================================================


/// Complete affine transform from block-local coordinates to world space:
///   x' = m00*x + m01*y + tx
///   y' = m10*x + m11*y + ty
///   z' = scaleZ*z + offsetZ
///
/// Transforms compose without decomposing back into scale and rotation, so
/// nested shear and mirror combinations retain their exact affine semantics.
struct Transform2D
{
    double m00 = 1.0;
    double m01 = 0.0;
    double m10 = 0.0;
    double m11 = 1.0;
    double tx = 0.0;
    double ty = 0.0;
    double scaleZ = 1.0;
    double offsetZ = 0.0;

    static Transform2D fromInsert(const InsertInfo& insert,
                                  double baseX,
                                  double baseY,
                                  double baseZ);

    /// Return this x child: child is applied first, then this transform.
    Transform2D composedWith(const Transform2D& child) const;

    DxfPoint apply(double x, double y, double z) const;
    DxfPoint apply(const DxfPoint& point) const;
    DxfPoint applyVector(double x, double y, double z = 0.0) const;
    DxfPoint applyVector(const DxfPoint& vector) const;

    double determinant() const;
    bool reversesOrientation() const;
    bool isPlanarSimilarity() const;
    double planarScale() const;
    double applyAngle(double angle) const;
    double applyZ(double z) const;
};

// ============================================================================
// DxfReaderCallbacks
// ============================================================================


#include <drw_interface.h>

#include <set>
#include <string>
#include <unordered_map>
#include <vector>

/**
 * @brief libdxfrw 的回调接收器（Adapter/适配器）。
 *
 * libdxfrw 只认识自己的 `DRW_Line`、`DRW_Circle` 等类型。本类继承它规定的
 * `DRW_Interface`，把外部对象转换成项目自己的 DxfLine/DxfCircle/...，从而把
 * 第三方库隔离在解析边界。`override` 表示这些函数覆盖基类虚函数，read() 会通过
 * 基类指针动态分派到本类实现。
 */
class DxfReaderCallbacks : public DRW_Interface
{
public:
    void setIgnoredLayers(const std::set<std::string>& ignoredLayers);

    const DxfData& data() const { return m_data; }
    DxfData takeData();
    const std::unordered_map<std::string, DxfBlock>& blocks() const
    {
        return m_blocks;
    }
    const std::vector<InsertInfo>& modelSpaceInserts() const
    {
        return m_modelSpaceInserts;
    }
    const std::set<std::string>& allLayers() const { return m_allLayers; }

    void addLine(const DRW_Line& data) override;
    void addCircle(const DRW_Circle& data) override;
    void addArc(const DRW_Arc& data) override;
    void addEllipse(const DRW_Ellipse& data) override;
    void addLWPolyline(const DRW_LWPolyline& data) override;
    void addSpline(const DRW_Spline* data) override;
    void addPoint(const DRW_Point& data) override;

    void addBlock(const DRW_Block& data) override;
    void endBlock() override;
    void addInsert(const DRW_Insert& data) override;
    void addLayer(const DRW_Layer& data) override;

    // DRW_Interface 要求实现许多回调。本项目暂不支持的实体给出空实现，表示“读取但
    // 不输出几何”。它们不是忘记写；新增支持时应从这里补验证和 DxfData 类型。
    void addHeader(const DRW_Header*) override {}
    void addLType(const DRW_LType&) override {}
    void addDimStyle(const DRW_Dimstyle&) override {}
    void addVport(const DRW_Vport&) override {}
    void addTextStyle(const DRW_Textstyle&) override {}
    void addAppId(const DRW_AppId&) override {}
    void setBlock(const int) override {}
    void addRay(const DRW_Ray&) override {}
    void addXline(const DRW_Xline&) override {}
    void addPolyline(const DRW_Polyline&) override {}
    void addKnot(const DRW_Entity&) override {}
    void addTrace(const DRW_Trace&) override {}
    void add3dFace(const DRW_3Dface&) override {}
    void addSolid(const DRW_Solid&) override {}
    void addMText(const DRW_MText&) override {}
    void addText(const DRW_Text&) override {}
    void addDimAlign(const DRW_DimAligned*) override {}
    void addDimLinear(const DRW_DimLinear*) override {}
    void addDimRadial(const DRW_DimRadial*) override {}
    void addDimDiametric(const DRW_DimDiametric*) override {}
    void addDimAngular(const DRW_DimAngular*) override {}
    void addDimAngular3P(const DRW_DimAngular3p*) override {}
    void addDimOrdinate(const DRW_DimOrdinate*) override {}
    void addLeader(const DRW_Leader*) override {}
    void addHatch(const DRW_Hatch*) override {}
    void addViewport(const DRW_Viewport&) override {}
    void addImage(const DRW_Image*) override {}
    void linkImage(const DRW_ImageDef*) override {}
    void addComment(const char*) override {}
    void addPlotSettings(const DRW_PlotSettings*) override {}
    void writeHeader(DRW_Header&) override {}
    void writeBlocks() override {}
    void writeBlockRecords() override {}
    void writeEntities() override {}
    void writeLTypes() override {}
    void writeLayers() override {}
    void writeTextstyles() override {}
    void writeVports() override {}
    void writeDimstyles() override {}
    void writeObjects() override {}
    void writeAppId() override {}

private:
    bool isLayerIgnored(const DRW_Entity& entity) const;
    bool shouldSkipDuringRead(const DRW_Entity& entity) const;

    DxfData m_data;
    std::unordered_map<std::string, DxfBlock> m_blocks;
    std::vector<InsertInfo> m_modelSpaceInserts;
    // 非空表示当前回调正在读取 BLOCK 定义，实体应存入块而不是模型空间。
    // 指针只借用 m_blocks 中元素，不负责 delete。
    DxfBlock* m_currentBlock = nullptr;
    std::set<std::string> m_ignoredLayers;
    std::set<std::string> m_allLayers;
};

// ============================================================================
// DxfParser
// ============================================================================
#ifndef DxfParser_h
#define DxfParser_h

#include <QString>
#include <cstddef>

/**
 * @brief DXF 文件解析门面：把磁盘文件转换成与 SAM 无关的 `DxfData`。
 *
 * 真正读取 DXF 文本的是第三方 libdxfrw；`DxfReaderCallbacks` 接收其回调；随后
 * `DxfBlockExpansion` 展开 BLOCK/INSERT。这里只负责编排这三步和统一错误检查。
 *
 * 解析阶段尽量保留圆、圆弧、样条等原始语义，不立刻全部折成线段。普通曲线的
 * 离散推迟到 ConversionEngine，才能统一使用用户选择的 tolerance。
 */
class DxfParser {
public:
    /// Parse a DXF file into a DxfData container.
    /// Block expansion does coordinate transform only; discretization
    /// is deferred to ConversionEngine (which uses the user's tolerance).
    /// @param  filePath       absolute or relative path to .dxf file
    /// @param  outData        [out] 输出参数；成功时装入实体，失败时装入错误信息
    /// @param  curveTolerance discretization tolerance for non-uniform scaled block entities
    /// @param  ignoredLayers  layers to filter after resolving INSERT layer inheritance
    /// @return true on success; on failure outData.errorMessage() is set
    bool parseFile(const QString& filePath, DxfData& outData,
                   double curveTolerance = DxfImportDefaults::kCurveTolerance,
                   const std::set<std::string>& ignoredLayers = {},
                   std::size_t maxOutputEntities =
                       DxfImportDefaults::kDefaultMaxOutputEntities);
};

#endif
