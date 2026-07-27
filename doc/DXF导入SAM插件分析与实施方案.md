# DXF 导入 SAM 草图插件：需求分析与实施方案

## 1. 文档目的

本文基于以下资料整理：

- `doc/ai修改设计报告ver1.4.pdf`
- `doc/SAM V4.0 二次开发手册(1).pdf`
- 当前工程 `src/Example1`、`src/Example1Toolset` 及相关阶段性源码

当前项目的核心交付物是：

> 读取 ASCII DXF 文件中的几何图元，完成必要的坐标处理和曲线离散化，调用 SAM 草图接口，在 SAM 中创建可保存、可编辑的几何草图。

当前不以自动有限元分析为目标。

---

## 2. 核心结论

当前工程已经完成了 **SAM 插件框架和草图创建最小验证**，但还没有完成 **DXF 文件解析到 SAM 草图** 的完整业务闭环。

### 已经具备的能力

- 已有 `Example1PytModule`、`Example1Utils`、`SAMExample1Fragment` 插件结构；
- 已经实际使用 `skcSketch` 创建 SAM 草图；
- 已经实际使用 `skcGeomFactory::CreateLine()` 创建草图直线；
- 已经使用 `gmlSketchRepository` 将草图写入模型数据库；
- `Example1PytModule.cpp` 已经引入 `libdxfrw.h`；
- 已有 `Example1Toolset` Qt/SAM 工具集框架；
- 已有 `kefKLine` 临时几何渲染器；
- OCC 最小验证已经完成；
- 顶层 CMake 已配置 SAM、Qt 和 OCCT 相关路径。

### 当前缺少的能力

- DXF 文件打开和前置检查；
- libdxfrw 解析回调和图元收集；
- `DxfData`、`SamData` 等中间数据模型；
- DXF 图元到 SAM 草图图元的转换；
- ARC、SPLINE、ELLIPSE 等曲线的 OCC 离散化接入；
- 基点平移；
- DXF 导入菜单和窗口；
- 导入统计、日志、异常处理和重复导入验证。

当前状态可以概括为：

```text
SAM 插件和草图接口验证完成
DXF → SAM 草图业务管线尚未打通
```

---

## 3. 项目范围重新定义

### 3.1 当前目标

```text
DXF 文件
  → 几何图元提取
  → 数据清洗
  → 基点/坐标处理
  → 曲线离散化
  → SAM 草图创建
  → 场景刷新和统计反馈
```

### 3.2 当前明确不做

以下内容不属于当前 MVP：

- 自动创建材料；
- 自动创建梁、壳、实体截面；
- 自动分配物理属性；
- 自动划分有限元网格；
- 自动创建分析步；
- 自动施加载荷和边界条件；
- 自动创建 Job 并提交求解；
- 自动读取 HDF5 结果；
- 自动后处理和输出云图。

这些属于后续“几何到有限元分析”阶段，不是当前“图纸导入 SAM 草图”的验收条件。

### 3.3 图元范围

| DXF 图元 | 处理方式 | 优先级 |
|---|---|---|
| `LINE` | 直接创建 SAM 草图直线 | MVP |
| `CIRCLE` | 直接创建 SAM 草图圆 | MVP |
| `LWPOLYLINE` | 根据顶点逐段生成直线 | 第二阶段 |
| `ARC` | 离散为点列，再生成线段 | 第二阶段 |
| `SPLINE` | 使用 OCC 离散为点列，再生成线段 | 第三阶段 |
| `ELLIPSE` | 使用 OCC 参数采样，再生成线段 | 第三阶段 |
| `BLOCK` | 当前不解析，要求用户预先炸开 | 不在范围 |
| 二进制 DXF | 当前不支持 | 不在范围 |

---

## 4. 需求报告与当前源码对照

### 4.1 `Example1PytModule`

文件：

```text
src/Example1/Example1PytModule.h
src/Example1/Example1PytModule.cpp
```

类定义：

```cpp
class Example1PytModule : public pyoModule
```

当前注册了：

```cpp
calcArea
createLine
```

`createLine()` 已经验证了完整的 SAM 草图创建流程：

```text
获取 mdb
  → 获取 Model-1 的 Sketch Repository
  → 创建 XY 轴草图
  → 设置草图 ID 和显示尺寸
  → 创建 skcGeomFactory
  → CreateLine
  → 使用 gmlSketchWrapper 包装草图
  → 插入草图仓库
  → Replace(mdb)
  → 更新场景和当前草图状态
```

当前已经确认的直线接口形式：

```cpp
geometryFactory.CreateLine(
    gslPoint(x1, y1, z1),
    gslPoint(x2, y2, z2),
    skc_FOREGROUND,
    false);
```

这说明 SAM 草图创建能力已经被当前工程真实验证，而不是仅停留在需求报告层面。

### 4.2 `Example1Utils`

文件：

```text
src/Example1/Example1Utils.h
src/Example1/Example1Utils.cpp
```

主要负责：

- 创建 `Example1PytModule`；
- 创建 `SAMExample1Fragment`；
- 注册 `kefKLine` 类型；
- 通过 `iniPythonModuleRegistrar` 注册初始化和回收；
- 提供 `initExample1()` 动态库入口。

DXF 导入应复用这套插件生命周期。

### 4.3 `SAMExample1Fragment`

当前 Fragment 注册了：

```cpp
drawExample
getNodeLocation
```

`drawExample()` 通过 `kefKLine` 绘制临时几何；`getNodeLocation()` 演示从 Part 网格中读取节点坐标。

DXF 导入的最终目标是 SAM 草图，而不是有限元网格，因此主流程更适合由独立的 DXF 解析器、几何转换器和草图适配器实现，再由 `Example1PytModule` 暴露入口。

### 4.4 `kefKLine`

`kefKLine` 继承自：

```cpp
class kefKLine : public gdyNonPageGeomEditor
```

它把顶点、颜色和段 ID 保存在内存容器中，然后通过渲染器绘制点、线、三角形、四边形和多边形。

它适合：

- 调试 DXF 解析结果；
- 预览 OCC 离散化后的折线；
- 验证坐标和几何方向。

它不适合作为正式 DXF 导入的最终存储方式，因为它不是 SAM 草图数据库对象。正式导入应使用：

```text
skcSketch + skcGeomFactory + gmlSketchRepository
```

### 4.5 `Example1Toolset`

当前工具集已经完成菜单注册和 SAM 命令发送的基本验证：

```cpp
SAMMenu* testMenu = new SAMMenu(this, tr("&Test"));
SAMMenuCommand* example1Cmd =
    new SAMMenuCommand(this, testMenu, tr("&Example1"));
```

点击菜单后，当前代码会发送：

```cpp
import Example1
Example1.createLine()
```

这说明 GUI 到 Python 模块的调用链已经有样例。后续可以将其扩展为：

```text
选择 DXF
  → 输入基点和容差
  → 发送 Example1.importDxf(...)
```

---

## 5. SAM 二次开发手册对应关系

SAM 手册给出的 C++ 插件通常包括：

1. 继承 `pyoModule` 的 Python 模块类；
2. 初始化和回收工具类；
3. 继承 SAM Fragment 的业务类；
4. 使用 CMake 构建动态库；
5. 在 SAM 启动环境中加载 `.pyd`；
6. 通过 Python/Qt 界面调用动态库命令。

当前工程已经采用这套结构。

对于本项目，推荐遵循：

```text
DXF 解析层
  → 几何转换层
  → SAM 草图对接层
  → Python 模块入口
  → Qt 工具集
```

解析层不应直接调用 SAM API。这样可以在不启动 SAM 的情况下测试 DXF 解析，也能降低 SAM SDK 变化对业务层的影响。

---

## 6. 推荐总体架构

```text
Qt 工具集
  → 选择 DXF、输入基点和容差
  → Example1PytModule::importDxf
  → DxfParser（libdxfrw）
  → DxfData
  → GeometryConverter
  → SamData
  → SamSketchAdapter
  → skcSketch / skcGeomFactory
  → 写入 mdb、刷新场景
```

建议在 `src/Example1` 中逐步增加：

```text
src/Example1/
├── Example1PytModule.h/.cpp
├── Example1Utils.h/.cpp
├── SAMExample1Fragment.h/.cpp
├── DxfData.h
├── DxfParser.h/.cpp
├── GeometryTypes.h
├── GeometryConverter.h/.cpp
├── CurveDiscretizer.h/.cpp
└── SamSketchAdapter.h/.cpp
```

职责划分：

| 模块 | 负责内容 | 不负责内容 |
|---|---|---|
| `DxfParser` | 文件读取、libdxfrw 回调、原始图元收集 | SAM API、草图提交 |
| `DxfData` | 保存 DXF 原始图元 | 文件读取 |
| `CurveDiscretizer` | ARC/SPLINE/ELLIPSE 转点列 | SAM 数据库 |
| `GeometryConverter` | 基点平移、合法性检查、统一输出 | GUI |
| `SamSketchAdapter` | 创建草图、创建图元、提交数据库、刷新场景 | DXF 解析 |
| `Example1PytModule` | 暴露命令、组织主流程 | 复杂底层算法 |
| `Example1Toolset` | 菜单和用户输入 | 直接操作内核对象 |

---

## 7. 建议数据模型

### 7.1 基础几何类型

```cpp
struct Point3D
{
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

struct Line3D
{
    Point3D start;
    Point3D end;
};

struct Circle3D
{
    Point3D center;
    double radius = 0.0;
};

struct Arc3D
{
    Point3D center;
    double radius = 0.0;
    double startAngle = 0.0;
    double endAngle = 0.0;
};

struct Polyline3D
{
    std::vector<Point3D> vertices;
    bool closed = false;
};
```

虽然 DXF 通常是二维图纸，但内部建议统一使用三维点，默认 `z = 0`。

### 7.2 解析结果和 SAM 就绪结果

```cpp
struct DxfData
{
    std::vector<Point3D> points;
    std::vector<Line3D> lines;
    std::vector<Circle3D> circles;
    std::vector<Arc3D> arcs;
    std::vector<Polyline3D> polylines;
    // spline / ellipse 根据 libdxfrw 回调参数补充
};

struct SamData
{
    std::vector<Point3D> points;
    std::vector<Line3D> lines;
    std::vector<Circle3D> circles;
};
```

`DxfData` 保留 DXF 原始类型；`SamData` 只保留当前 SAM 对接层需要的点、线和完整圆。

---

## 8. 图元转换规则

### 8.1 LINE

```text
DXF 起点/终点
  → 基点平移
  → 合法性检查
  → SamData::lines
  → CreateLine
```

零长度直线应跳过并记录警告。

### 8.2 CIRCLE

完整圆不应先离散化，应保留为圆：

```text
DXF 圆心和半径
  → 圆心基点平移
  → CreateCircle
```

这样精度更高，草图对象数量更少。

当前源码只直接验证了 `CreateLine()` 的签名；`CreateCircle()` 的真实参数必须以当前 SAM SDK 头文件为准，不能仅依据报告中的抽象描述。

### 8.3 ARC

如果当前草图 API 没有可用的圆弧接口：

```text
圆弧
  → 生成采样点列
  → 相邻点两两组成线段
  → 批量 CreateLine
```

必须正确处理：

- DXF 角度单位；
- 逆时针方向；
- 跨越 0° 的圆弧；
- 首尾点；
- 闭合属性。

### 8.4 LWPOLYLINE

普通多段线按相邻顶点连接；闭合多段线补充最后一个顶点到第一个顶点的线段。

如果存在 `bulge`，则对应段实际是圆弧，不能简单忽略，应转换为圆弧后离散。

### 8.5 SPLINE 和 ELLIPSE

建议使用 OCC 生成曲线，再按容差输出点列，最后转换为线段。接入时需要确认：

- 样条控制点、节点向量、阶次和权重；
- 参数范围和闭合属性；
- 椭圆主轴方向、长短半轴和参数区间；
- 离散后的最大几何偏差。

---

## 9. 坐标和基点处理

推荐定义：

```text
SAM坐标 = DXF坐标 - DXF基点
```

例如：

```text
DXF基点 = (1000, 2000, 0)
DXF点   = (1010, 2025, 0)
SAM点   = (10, 25, 0)
```

MVP 先实现把用户指定的 DXF 基点移动到 SAM 原点。以后若需要将结果放到 SAM 的任意目标位置，再扩展为：

```text
SAM坐标 = DXF坐标 - DXF基点 + SAM目标点
```

必须区分“被减去的 DXF 基点”和“场景中要放置的目标点”。

---

## 10. SAM 草图适配器

建议新增：

```cpp
class SamSketchAdapter
{
public:
    bool createSketch(const SamData& data,
                      const QString& sketchName,
                      QString& errorMessage);
};
```

内部复用当前 `createLine()` 已验证的流程：

```text
获取 mdb
  → 获取 Sketch Repository
  → 创建新草图
  → 创建 skcGeomFactory
  → 遍历 SamData 创建线和圆
  → 插入草图仓库
  → Replace(mdb)
  → 设置当前草图 PDO
  → Rebuild
  → 刷新场景
```

建议所有图元创建成功后再统一插入仓库和提交数据库，避免导入失败留下半成品对象。

草图名称可以继续采用时间戳，但要检查名称冲突。草图 ID 不能简单假定 `Size()+1` 永远安全，应依据当前 SDK 的仓库规则确认。

---

## 11. 插件和 GUI 接口

建议在 `Example1PytModule` 中注册：

```cpp
omuPrimitive* importDxf(omuArguments& args);
```

参数可以包括：

```text
filePath
baseX
baseY
baseZ
tolerance
```

调用关系：

```text
Qt GUI
  → importDxf(filePath, basePoint, tolerance)
  → DxfParser
  → GeometryConverter
  → SamSketchAdapter
  → 返回统计结果
```

GUI 只负责：

- 文件选择；
- 基点和容差输入；
- 调用 `importDxf`；
- 显示成功、失败和统计结果。

GUI 不应直接操作 `skcSketch` 内部对象。

MVP 界面只需要：

1. “导入 DXF”菜单；
2. 文件选择框；
3. 基点 X/Y/Z 输入；
4. 导入按钮；
5. 成功/失败提示；
6. 图元数量统计。

---

## 12. 日志和异常处理

必须安全处理：

- 文件不存在或不可读；
- 空文件；
- 二进制 DXF；
- 损坏记录；
- 未知实体；
- 零长度直线；
- 零半径圆；
- 样条点数不足；
- 极端坐标；
- 中文路径；
- 重复导入。

文件级错误应终止本次导入，但不能导致 SAM 崩溃。图元级错误应记录警告、跳过当前图元并继续处理后续图元。

建议统计：

```text
原始图元数量
成功创建数量
离散生成线段数量
跳过数量及原因
创建草图名称
总耗时
```

---

## 13. 分阶段实施计划

### 阶段 0：接口确认

- 确认 libdxfrw 版本和回调签名；
- 确认 `CreateCircle()` 的真实签名；
- 确认 CMake 中 DXF 和 OCC 库的链接方式；
- 确认草图对象和 ID 的仓库规则。

### 阶段 1：MVP 解析

只实现 ASCII DXF 的 `LINE`、`CIRCLE`：

- 文件检查；
- 解析回调；
- `DxfData`；
- 图元数量统计；
- 独立解析测试。

### 阶段 2：MVP 草图导入

- 创建新的 SAM 草图；
- LINE → `CreateLine`；
- CIRCLE → `CreateCircle`；
- 写入 mdb；
- 刷新场景。

### 阶段 3：GUI 接入

- 菜单项；
- 文件选择；
- 基点输入；
- 导入按钮；
- 错误提示和统计。

### 阶段 4：复杂图元

依次实现：

1. `LWPOLYLINE`；
2. `ARC`；
3. 带 bulge 的多段线；
4. `SPLINE`；
5. `ELLIPSE`；
6. OCC 容差控制。

### 阶段 5：质量和性能

- 空文件和损坏文件；
- 连续导入；
- 大文件；
- 名称和 ID 冲突；
- 日志；
- 必要时异步解析和分块提交。

---

## 14. 测试方案

建议准备以下测试文件：

| 文件 | 内容 | 验证重点 |
|---|---|---|
| `test_line.dxf` | 仅直线 | 基础解析和建草图 |
| `test_circle.dxf` | 仅圆 | 圆心、半径和圆接口 |
| `test_arc.dxf` | 圆弧 | 离散化、方向和首尾 |
| `test_polyline.dxf` | 多段线 | 顶点和闭合 |
| `test_spline.dxf` | 样条 | OCC 精度 |
| `test_mixed.dxf` | 混合图元 | 完整管线 |
| `test_empty.dxf` | 空文件 | 稳定性 |
| `test_bad.dxf` | 损坏文件 | 容错能力 |

端到端测试应检查：

- 图元数量；
- 位置和方向；
- 圆心、半径；
- 曲线首尾；
- 基点平移；
- 草图名称；
- 多次导入；
- SAM 已有对象是否受影响。

---

## 15. 关键风险

### 15.1 libdxfrw 回调版本差异

不能仅依据需求报告中的回调名称编写代码，必须以当前 libdxfrw 版本头文件为准。

### 15.2 `CreateCircle()` 尚未直接验证

当前源码只证明了 `CreateLine()` 的调用形式，圆接口需要先读取 SAM SDK 声明再实现。

### 15.3 OCC 采样方向和误差

必须验证：

- 坐标平面；
- 角度方向；
- 闭合首尾；
- 离散容差；
- 大坐标下的数值稳定性。

### 15.4 大量线段导致草图过重

复杂曲线可能生成大量线段。MVP 先同步实现，确认正确性后再考虑后台解析和主线程批量提交。

### 15.5 解析线程与 SAM 内核线程

推荐后续采用：

```text
后台线程：文件读取、DXF 解析、曲线离散化
主线程：创建草图、调用 SAM API、场景刷新
```

MVP 阶段可以先同步实现，优先保证正确性。

---

## 16. 最终判断

当前工程的正确定位是：

> 运行在 SAM 内部的 DXF 几何导入插件，将 CAD 图纸中的几何转换成 SAM 草图。

当前已完成：

```text
插件加载机制
Python 模块
SAM 草图创建
CreateLine
DXF 库接入入口
Qt 工具集框架
OCC 最小验证
```

下一步应实现：

```text
DXF 文件解析
  → DxfData
  → 基点处理和曲线离散化
  → SamData
  → skcSketch / skcGeomFactory
  → 新草图写入 SAM
```

最小可行版本应先实现 ASCII DXF 的 `LINE` 和 `CIRCLE` 导入，再逐步扩展曲线、多段线、统计、日志和大文件优化。

自动有限元分析不属于当前目标。