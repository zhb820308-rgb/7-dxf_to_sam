# DXF 导入新增 Finite Element 模式 — 后端方案分析

> 编写日期: 2026-07-31
> 本文基于源码只读分析，不包含代码修改，仅评估如何复用现有后端功能实现"导入 DXF 为节点和杆单元（Truss）"的需求。

---

## 1. 总体架构

整个导入流程可以扩展为：

```text
DXF 文件
  ↓
DxfParser（复用）
  ↓
DxfData（复用）
  ↓
ConversionEngine（复用或扩展）
  ↓
┌──────────────────────┬────────────────────────┐
│ Sketch 模式          │ Finite Element 模式    │
│ SamData              │ FeData（新增）         │
│ SamBuilder           │ FiniteElementBuilder   │
│ 创建草图              │ 创建 Part、节点、Truss │
└──────────────────────┴────────────────────────┘
```

其中：

- `DxfParser` 继续负责 DXF 文件解析；
- `DxfData` 继续作为原始几何数据；
- `ConversionEngine` 继续负责坐标偏移、曲线离散和几何转换；
- Sketch 模式继续调用现在的 `SamBuilder`；
- Finite Element 模式新增一个有限元创建适配器；
- GUI 暂时不实现，只扩展 `Example1.importDxf()` 的后端参数。

---

## 2. 当前代码中可以直接复用的部分

### 2.1 DXF 解析逻辑可以完全复用

当前 `DxfParser` 已经能够解析：LINE、POINT、CIRCLE、ARC、LWPOLYLINE、ELLIPSE、SPLINE、BLOCK / INSERT、图层过滤、坐标变换、曲线离散。

Finite Element 模式复用 `DxfParser::parse() → DxfData`，两种模式使用同一套 DXF 几何来源。

### 2.2 图层过滤可以复用

当前 `importDxf()` 已经接收 `ignoreLayers` 参数。FE 模式继续使用相同的图层过滤逻辑。

### 2.3 坐标偏移可以复用

当前转换逻辑为 `输出坐标 = DXF 坐标 + baseX/Y/Z`，FE 模式保持一致，确保同一个 DXF 文件转换成 Sketch 和 FE 后坐标一致。

### 2.4 曲线离散逻辑可以复用

当前已有 `GeometryUtils::tessellateArc()` / `tessellateLWPolyline()` / `tessellateEllipse()` / `tessellateSpline()`。

对 FE 模式（统一转换为折线，CIRCLE 也要离散）：
- LINE → 一个 Truss
- ARC → 多个 Truss
- LWPOLYLINE → 多个 Truss
- ELLIPSE → 多个 Truss
- SPLINE → 多个 Truss
- CIRCLE → 圆周离散为多个 Truss

FE 模式不应保留 Circle 对象，因为有限元杆单元只需要节点和连接关系。

---

## 3. 新增关键数据结构

现有的 `SamData` 只包含 `m_lines` 和 `m_circles`，适合创建 Sketch 但不适合 FE。

建议新增：

```cpp
struct FeNode
{
    int id;
    double x;
    double y;
    double z;
};

struct FeTruss
{
    int id;
    int startNodeId;
    int endNodeId;
};

class FeData
{
public:
    const std::vector<FeNode>& nodes() const;
    const std::vector<FeTruss>& trusses() const;

    int addOrGetNode(double x, double y, double z, double tolerance);
    void addTruss(int startNodeId, int endNodeId);
    void clear();

private:
    std::vector<FeNode> m_nodes;
    std::vector<FeTruss> m_trusses;
};
```

不建议把节点和单元塞进 `SamData`，原因：
- `SamData` 当前语义是 Sketch 创建数据；
- Sketch 的 Circle 在 FE 中没有对应对象；
- FE 需要显式拓扑关系；
- 后续可能增加 Beam、Shell、Solid 等类型；
- 分开 `SamData` 和 `FeData`，后续更容易维护。

---

## 4. 有限元转换核心逻辑

### 4.1 线段端点需要合并成节点

不能简单对每条线的起点和终点各创建一个节点，否则相连线段会产生重复节点。建议使用坐标容差进行节点去重：如果两个点的距离 <= `nodeMergeTolerance`，则认为是同一个节点。第一版使用空间哈希或简单线性查找。

### 4.2 DXF POINT 的处理

当前代码明确跳过了 POINT。FE 模式建议把 DXF POINT 转为 FE Node（无连接关系时只生成 Node，不生成 Truss）。

### 4.3 CIRCLE 需要离散化

FE 模式把圆周离散成折线，首尾闭合：`P0 → P1 → P2 → ... → Pn → P0`。

### 4.4 曲线离散线段直接转 Truss

```cpp
for (const DxfLine& line : convertedLines)
{
    int startNode = feData.addOrGetNode(
        line.start().x(), line.start().y(), line.start().z(),
        nodeMergeTolerance);
    int endNode = feData.addOrGetNode(
        line.end().x(), line.end().y(), line.end().z(),
        nodeMergeTolerance);
    if (startNode == endNode) continue;
    feData.addTruss(startNode, endNode);
}
```

---

## 5. 后端接口扩展

当前 Python 模块只有一个入口 `omuPrimitive* importDxf(omuArguments& args)`。

建议新增参数：

```text
importMode   // "Sketch" 或 "FiniteElement"
partName     // 仅 importMode=FiniteElement 时使用
```

建议的枚举：

```cpp
enum class DxfImportMode
{
    Sketch = 0,
    FiniteElement = 1
};
```

Python 入口逻辑：

```python
if importMode == Sketch:
    # 现有 SamBuilder 流程
elif importMode == FiniteElement:
    # 新增 FeBuilder 流程
else:
    # 返回不支持的导入模式
```

FiniteElement 模式下 `partName` 不能为空；Sketch 模式下 `partName` 忽略。

---

## 6. 建议新增的类

### 6.1 FeData

| 项目 | 内容 |
|---|---|
| 职责 | 保存有限元节点和 Truss 单元，节点去重，拓扑管理 |
| 依赖 | 不依赖 SAM SDK，可单元测试 |
| 建议文件 | `src/Example1/FeData.h`，`src/Example1/FeData.cpp` |

### 6.2 FiniteElementConversionEngine

**方式一（推荐第一阶段）**：在 `ConversionEngine` 中新增方法

```cpp
bool ConversionEngine::convertToFiniteElement(
    const DxfData&, double baseX/Y/Z, double tolerance,
    double nodeMergeTolerance, FeData& outData) const;
```

**方式二（后续拆分）**：新增独立 `FeConversionEngine` 类。

### 6.3 FiniteElementBuilder

职责：

```
创建 Part → 创建 orphan mesh → 添加节点
→ 创建 Truss 单元 → 插入 Part Repository
→ Replace 数据库 → 切换 PART 显示 → 刷新场景
```

建议文件：`src/Example1/FiniteElementBuilder.h` / `.cpp`

接口设计：

```cpp
class FiniteElementBuilder
{
public:
    using ProgressCallback = std::function<bool(const QString& stage, int current, int total)>;

    FiniteElementBuilder();
    ~FiniteElementBuilder();

    bool beginImport(const QString& modelName, const QString& partName);
    int createNodes(const std::vector<FeNode>& nodes);
    int createTrusses(const std::vector<FeTruss>& trusses);
    bool commit();
    void rollback();

    void setProgressCallback(const ProgressCallback& callback);
    const QString& partName() const;
    const QString& lastError() const;
    int createdCount() const;
};
```

`FiniteElementBuilder` 和 `SamBuilder` 平行存在。

---

## 7. 从 SDK 已经确认的内容

SDK 路径：`D:\shixi_CAE\SAMSDK`

### 7.1 Part 对象

```cpp
ptoKPart::ptoKPart(bdoTheory theory, const QString& name);
```

头文件：`include/ptoKPart.h`

说明 SAM 存在正式的 Part 对象，构造时需要 `bdoTheory` 和名称。

### 7.2 Mesh 节点数据

```cpp
bmeNodeData::AppendNode(float x, float y, float z);
bmeNodeData::AppendNode(float x, float y, float z, int label);
```

头文件：`include/bmeNodeData.h`

### 7.3 Mesh 单元数据

```cpp
bmeElementClass::ConstructObject(int numElements, const QString& elTypLabel, int* connectivity);
```

以及 `bmeElementData::AddElements()` / `AppendClass()`。

头文件：`include/bmeElementClass.h`，`include/bmeElementData.h`

### 7.4 网格编辑器创建单元接口

```cpp
gmeMeshEditor::CreateElement(bmeMesh*, gmeElementTypeConst, cowListInt& nodes, int inst_id, ...);
gmeMeshEditor::CreateElements(...);
gmeMeshEditor::CreateElementsV2(...);
```

头文件：`include/gmeMeshEditor.h`

这是创建 Truss 单元最可能的正式接口。

---

## 8. 尚不能确定的 SDK 接口（需要向 SAM 团队确认）

### 8.1 Part 创建流程

需要确认：

```text
1. 如何获取当前 Model 的 Part Repository？
2. 如何创建 ptoKPart？
3. 如何设置 Part 的 bdoTheory？
4. 如何把 ptoKPart 插入 Model？
5. 如何调用 Replace() 保存？
```

可能相关的头文件：`ptoKPart.h`, `ptoKPartRepository.h`, `ptoKPartShortcut.h`, `ptoKUtils.h`, `ptsKPartCmd.h`, `ptsKUtils.h`。

### 8.2 Mesh 创建和挂载

需要确认：

```text
1. 如何创建 orphan mesh (bmeMesh / gmeMesh)？
2. 如何将 bmeMesh 挂载到 ptoKPart？
3. 如何创建 ftrFeatureList？
4. 如何获取或设置 Part 的 mesh？
```

### 8.3 节点创建方式

建议确认是否推荐 `mesh->GetNodeData().AppendNode(x, y, z, label)`，或者通过其他接口创建。

### 8.4 Truss 对应的 gmeElementTypeConst

这是最关键的问题，需要确认：

```text
1. 两节点杆单元对应的 gmeElementTypeConst 枚举值是什么？
2. 二维杆单元和三维杆单元分别是什么类型？
3. gme_TRUSS / gme_LINE / gme_BEAM？
4. CreateElement 的 elemType 参数应该传什么？
5. 两节点 Truss 是否必须指定分析类型或理论类型？
```

### 8.5 Part 提交和场景刷新

Sketch 当前使用 `basBasis::Instance()->Replace(mdb)`。FE 可能还需要：将 Part 插入 Part Repository、设置当前 Part、切换到 `omu_PART`、清理并刷新场景、设置主对象路径、触发 Mesh/Part PDO 重建。

---

## 9. 建议向 SAM 团队查询的最小代码片段

最理想的方式是让 SAM 团队提供以下最小完整示例：

```cpp
// 1. 创建 Part
// 2. 创建 orphan mesh
// 3. 添加两个节点
// 4. 添加一个两节点 Truss
// 5. 保存到当前 Model
// 6. 刷新场景
```

如果能拿到这段代码，后端的 `FiniteElementBuilder` 可以直接实现。

---

## 10. 建议的实现顺序

### 第一阶段：与 SAM 无关的数据转换

先完成 `DxfData → FeData` 的纯逻辑转换，可单元测试。测试覆盖：

- LINE 转节点和 Truss
- 相邻线段共享节点
- 坐标偏移
- ARC / LWPOLYLINE / ELLIPSE / SPLINE 离散
- CIRCLE 离散闭合
- DXF POINT 转独立节点
- 零长度线跳过
- 节点合并容差
- 重复单元处理

### 第二阶段：SAM Part 创建适配器

确认 SDK 接口后，实现 `FiniteElementBuilder::beginImport()`。

### 第三阶段：写入节点和 Truss

实现 `FeData → bmeNodeData + gmeMeshEditor::CreateElements()`。

### 第四阶段：接入 Example1.importDxf()

扩展参数 `mode` 和 `partName`，解析后按 mode 分流。

### 第五阶段：日志和统计

FE 模式输出：

```text
partName
nodeCount
trussCount
skippedZeroLengthCount
mergedNodeCount
```

---

## 11. 需要注意的问题

### 11.1 节点合并容差不要等于曲线离散容差

建议分开 `curveTolerance` 和 `nodeMergeTolerance`。第一版可以暂时使用 `nodeMergeTolerance = curveTolerance * 0.1`。

### 11.2 空间接近但实际不相连的点

默认严格合并；后续可以考虑允许用户输入节点合并容差。

### 11.3 闭合曲线的重复单元

闭合曲线首尾点可能通过容差合并，需要避免产生 `Node A → Node A` 的无效单元。

### 11.4 当前 SamBuilder 的取消逻辑需要修正

当前代码没有及时检查 `-1`。FE Builder 不应复制这个问题，应改为：

```cpp
int count = builder.createNodes(...);
if (count < 0) { rollback; return -1; }
count = builder.createTrusses(...);
if (count < 0) { rollback; return -1; }
```

---

## 12. 最终建议

最稳妥的实施方案：

```text
不改 DXF 解析层
不改 GUI
保留 SamData 和 SamBuilder
新增 FeData
新增 FE 转换逻辑
新增 FiniteElementBuilder
扩展 Example1.importDxf(mode, partName)
```

真正需要从 SAM SDK 或团队确认的核心接口只有三类：

1. **创建 Part 并插入 Part Repository 的标准流程**
2. **创建/挂载 orphan mesh 的标准流程**
3. **`gmeMeshEditor::CreateElement/CreateElements` 对应 Truss 的元素类型和调用方式**

拿到完整示例后，后端功能可以按上述结构实现，不需要改动现有 DXF 解析和 Sketch 导入逻辑。
