# DxfParser 解析流程说明

本文档说明 `src/Example1/DxfParser.cpp` 的职责、主要数据结构、DXF 回调、块展开以及 `parseFile()` 的完整流程。

## 1. 总体职责

`DxfParser.cpp` 使用 libdxfrw 读取 DXF 文件，并将库中的 `DRW_*` 数据转换为项目内部的 `DxfData`、`DxfBlock`、`InsertInfo` 和各类 `DxfEntity`。

核心流程是：

```text
DXF 文件
  -> libdxfrw
  -> DxfReader 回调
  -> 模型空间实体 / BLOCK 定义 / INSERT 引用
  -> Block 展开与坐标变换
  -> 扁平化 DxfData
```

解析阶段尽量保留圆弧、椭圆、LWPOLYLINE 和 Spline 的原始参数；只有在块的非均匀缩放会改变曲线类型时，才按容差离散成线段。

## 2. DxfReader

`DxfReader` 是匿名命名空间中的 `DRW_Interface` 子类，仅在当前源文件使用。

```cpp
class DxfReader : public DRW_Interface {
    DxfData m_data;
    std::unordered_map<std::string, DxfBlock> m_blocks;
    std::vector<InsertInfo> m_modelSpaceInserts;
    DxfBlock* m_currentBlock = nullptr;
    std::set<std::string> m_ignoredLayers;
    std::set<std::string> m_allLayers;
};
```

### 成员含义

| 成员 | 作用 |
|---|---|
| `m_data` | 保存模型空间中直接出现的实体，后续也接收展开后的实体 |
| `m_blocks` | 按块名保存所有 BLOCK 定义 |
| `m_modelSpaceInserts` | 保存模型空间中的 INSERT |
| `m_currentBlock` | 指示当前是否正在解析某个 BLOCK；为空表示模型空间 |
| `m_ignoredLayers` | 需要跳过的图层集合 |
| `m_allLayers` | 收集文件中出现的图层名 |

实体回调根据 `m_currentBlock` 决定将实体放入 `m_data` 还是当前 `DxfBlock`。

## 3. 图层过滤

```cpp
bool isLayerIgnored(const DRW_Entity& ent) const {
    if (m_ignoredLayers.empty()) return false;
    return m_ignoredLayers.count(ent.layer) != 0;
}
```

模型空间普通实体在回调阶段直接检查并过滤。BLOCK 内实体不能在此时直接按
原始图层过滤，因为 layer 0 必须继承 INSERT 的有效图层；它们会保留原始图层，
延迟到块展开阶段处理。

块展开使用以下规则：

1. 顶层 INSERT 使用自身图层；被忽略时整个块引用跳过。
2. 嵌套 INSERT 位于 layer 0 时继承父 INSERT 的有效图层，否则使用自身图层。
3. 块内实体位于 layer 0 时继承当前 INSERT 的有效图层，显式非零图层保持不变。
4. 解析出最终有效图层后执行过滤；保留或离散生成的实体均记录该有效图层。

`addLayer()` 只负责记录所有图层名称，不改变实体解析结果。

## 4. 坐标变换：transformPoint

块中的实体保存在块局部坐标系。INSERT 提供基点、缩放、旋转和插入位置，转换公式为：

```text
P_world = T_insert * R_z(angle) * S * (P_local - P_blockBase)
```

代码按以下顺序执行：

1. 减去 BLOCK 基点；
2. 分别乘以 `scaleX/Y/Z`；
3. 绕 Z 轴旋转 `angle`；
4. 加上 `insertX/Y/Z`。

```cpp
double x = pt.x() - blockBaseX;
double y = pt.y() - blockBaseY;
double z = pt.z() - blockBaseZ;
x *= ins.scaleX;
y *= ins.scaleY;
z *= ins.scaleZ;
```

旋转部分为：

```text
x' = x cos(a) - y sin(a)
y' = x sin(a) + y cos(a)
```

该函数只负责一个点的数学变换，不负责输出和实体类型判断。

## 5. 离散线段写入

`addTransformedSegments()` 接收已经由 `GeometryUtils` 离散得到的线段：

```cpp
for (const DxfLine& seg : segments) {
    if (!seg.isValid()) continue;
    DxfPoint s = transformPoint(seg.start(), ins, baseX, baseY, baseZ);
    DxfPoint e = transformPoint(seg.end(), ins, baseX, baseY, baseZ);
    output.addGeneratedLine(DxfLine(s, e));
}
```

它会：

- 预留输出线段容量，减少扩容；
- 跳过无效线段；
- 对线段两个端点分别应用 INSERT 变换；
- 通过 `addGeneratedLine()` 标记这些线段是曲线离散产物，而不是原始 DXF LINE。

## 6. 均匀缩放判断

```cpp
static bool isScaleUniformXY(const InsertInfo& ins) {
    return std::fabs(ins.scaleX - ins.scaleY) < 1e-9
        && std::fabs(ins.scaleX) > 1e-12;
}
```

这个函数判断 XY 平面内是否近似等比例缩放：

- `scaleX` 与 `scaleY` 差值小于 `1e-9`，视为相同；
- 缩放绝对值不能接近零，避免退化。

均匀缩放下，圆仍是圆、圆弧仍是圆弧，曲线可以保留原始实体类型；非均匀缩放下，圆可能变为椭圆，任意曲线形状也会改变，因此先离散再变换。

注意：这个判断只针对 XY。圆的保留还额外要求 `scaleZ` 与 XY 缩放一致。

## 7. 曲线展开模板

`expandCurveGroup()` 用模板统一处理 Arc、LWPolyline、Ellipse 和 Spline：

```text
实体有效？
  否 -> 跳过
  是 -> uniformXY ? preserve() : tessellate() + addTransformedSegments()
```

`preserve()` 负责变换实体参数并保持原类型；`tessellate()` 负责按容差离散；非均匀缩放路径会先调用 `recordGeneratedEntity()` 记录源实体类型。

Spline 有单独的重载统计逻辑，通过 `SplineKind` 记录控制点/拟合点、有理、周期和闭合属性。

## 8. Block 和 INSERT 展开

### 8.1 addBlock / endBlock

`addBlock()` 创建 `DxfBlock`，记录名称和基点，并将 `m_currentBlock` 指向该块。布局块 `*Model_Space`、`*Paper_Space` 和 `*Paper_Space0` 被跳过。

`endBlock()` 将 `m_currentBlock` 置空，恢复模型空间解析上下文。

### 8.2 addInsert

`addInsert()` 从 `DRW_Insert` 复制以下信息：

```text
块名、插入位置、XYZ 缩放、旋转角、行列数、行列间距
```

如果当前在 BLOCK 内，INSERT 保存到该块的 `m_inserts`，形成嵌套块；否则保存到 `m_modelSpaceInserts`，等待统一展开。

### 8.3 expandInsertArray

INSERT 支持行列阵列。函数将 `rowCount * colCount` 个实例逐一生成。阵列偏移先在 INSERT 局部坐标中计算，再根据 INSERT 旋转角转到世界方向：

```text
offset = R_z(angle) * (col * colSpace, row * rowSpace)
```

`rowCount`、`colCount` 小于 1 时按 1 处理。

### 8.4 composeInsertTransform

嵌套 INSERT 通过外层和内层变换组合：

- 内层插入点先经过外层变换；
- XYZ 缩放因子相乘；
- 旋转角相加；
- 阵列信息使用内层 INSERT 的信息。

当前实现明确适用于标准 INSERT 和均匀缩放。嵌套非均匀缩放、旋转和镜像严格来说需要完整仿射矩阵，不能始终用“缩放相乘、角度相加”表达；源代码注释也指出了这一边界。

### 8.5 expandSingleBlock

该函数展开一个 BLOCK 实例，并按实体类型处理：

| 实体 | 均匀缩放 | 非均匀缩放 |
|---|---|---|
| Point | 变换后保留 | 变换后保留 |
| Line | 变换端点后保留 | 变换端点后保留 |
| Circle | 保留 Circle | 离散成线段 |
| Arc | 保留 Arc | 离散成线段 |
| LWPolyline | 变换顶点后保留 | 离散成线段 |
| Ellipse | 变换中心和轴向量后保留 | 离散成线段 |
| Spline | 变换控制点/拟合点后保留 | 离散成线段 |

函数还负责：

- 使用 `reserve*()` 预留容量；
- 跳过无效实体；
- 检查未知嵌套块并发出警告；
- 递归展开嵌套 INSERT；
- 使用 `kMaxExpandDepth = 32` 防止过深递归；
- 使用 `visiting` 检测 BLOCK 循环引用。

## 9. 实体回调

模型空间实体回调遵循：

```text
图层过滤 -> 读取 DRW 数据 -> 构造 DxfEntity -> 写入当前 Block 或 m_data
```

块定义内实体则遵循“读取并保留原始图层 → 写入当前 Block → INSERT 展开时解析
有效图层并过滤”。

### addLine

读取 `basePoint` 和 `secPoint`，构造 `DxfLine`。

### addCircle

读取圆心和 `radious`。`radious` 是 libdxfrw 中的原字段拼写。

### addArc

读取圆心、半径、起止角度和方向，并额外校验坐标、半径和角度是否有限。

### addEllipse

字段映射为：

```text
basePoint             -> center
secPoint              -> majorAxisEnd
ratio                 -> ratio
staparam/endparam     -> startParam/endParam
isccw                 -> isCCW
```

同时检查长轴长度大于零、轴比为正。

### addLWPolyline

读取顶点 X/Y 和每个顶点的 bulge。`flags & 1` 表示闭合：

- 开放多段线：bulge 数量为 `N - 1`；
- 闭合多段线：bulge 数量为 `N`，最后一个描述最后顶点到第一个顶点的段。

当前实现将 LWPOLYLINE 顶点 Z 和 `constZ` 设为 `0.0`。

### addSpline

Spline 同时支持控制点定义和拟合点定义。控制点数据完整时优先保留控制点、节点、权重和次数；否则允许至少两个拟合点作为备用。

控制点路径检查：

```text
ncontrol > degree
knots.size() == ncontrol + degree + 1
节点有限且非递减
有理 Spline 的权重有限且 > 0
```

还会读取控制点、节点、权重、拟合点、flags 以及首尾切向量，构造 `DxfSpline`。

### addPoint

直接读取三维坐标并构造 `DxfPoint`。

## 10. parseFile

```cpp
bool DxfParser::parseFile(const QString& filePath,
                          DxfData& outData,
                          double curveTolerance,
                          const std::set<std::string>& ignoredLayers)
```

执行步骤：

1. 清空输出对象：`outData = DxfData()`；
2. 检查文件路径非空；
3. 检查 `curveTolerance` 有限且大于零；
4. 创建 libdxfrw `dxfRW` 对象；
5. 创建 `DxfReader` 并设置忽略图层；
6. 调用 `dxf.read(&reader, true)`；
7. 将模型空间数据移动到 `outData`；
8. 调用 `expandBlocks()` 展开所有模型空间 INSERT；
9. 检查 `acceptedEntities + generatedEntities` 和最终输出实体数；
10. 设置 `isValid` 并返回结果。

失败情况包括：

- 文件路径为空；
- 容差无效；
- libdxfrw 读取失败；
- 成功读取但没有任何通过公共不变量检查或成功生成的实体。

失败时通过 `DxfData::setErrorMessage()` 返回错误原因和拒绝实体数量。实体统计
区分源实体、接受实体、拒绝实体与 BLOCK/离散生成实体，解析成功不会再由曾经
追加但无效的对象触发。

## 11. 最终数据流

```text
parseFile()
  |
  +-- libdxfrw.read()
  |     |
  |     +-- 模型空间实体 -> reader.m_data
  |     +-- BLOCK         -> reader.m_blocks
  |     +-- 模型空间 INSERT -> reader.m_modelSpaceInserts
  |     +-- 嵌套 INSERT   -> DxfBlock::m_inserts
  |
  +-- outData = move(reader.m_data)
  |
  +-- expandBlocks()
  |     +-- transformPoint()
  |     +-- 保留实体或按容差离散
  |     +-- 递归展开嵌套块
  |
  +-- outData.setValid(true)
```

## 12. 当前实现的边界

1. 只实现了 POINT、LINE、CIRCLE、ARC、ELLIPSE、LWPOLYLINE 和 SPLINE；文字、标注、HATCH、3DFACE 等回调为空。
2. LWPOLYLINE 当前没有从 `DRW_LWPolyline` 读取 elevation，统一使用 `z = 0`。
3. 非均匀缩放会将曲线离散成线段，输出不再保留原始曲线类型。
4. 嵌套非均匀缩放与旋转的组合使用简化的缩放相乘和角度相加，复杂仿射情形需要矩阵表示才能完全准确。
5. 块展开存在最大递归深度和循环引用保护，异常块会跳过并通过 `qWarning()` 输出警告。

## 13. 总结

`DxfParser` 采用“**回调收集、延迟展开、按需离散、最终扁平化**”的架构：

- `DxfReader` 负责把 libdxfrw 数据转换为内部对象；
- `DxfBlock` 和 `InsertInfo` 保留 BLOCK/INSERT 的复用结构；
- `transformPoint` 完成块坐标到世界坐标的变换；
- 均匀缩放尽量保留曲线类型，非均匀缩放则离散后变换；
- `parseFile` 最终输出可供 Sketch 和 FE 转换路径使用的 `DxfData`。
