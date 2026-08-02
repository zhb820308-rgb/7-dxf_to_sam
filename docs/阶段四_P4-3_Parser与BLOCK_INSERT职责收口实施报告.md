# 阶段四 P4-3：Parser 与 BLOCK/INSERT 职责收口实施报告

## 1. 实施结论

P4-3 已完成代码实现、Release 构建和自动化回归，当前状态为：

> `DxfParser` 继续只编排文件读取和结果状态；BLOCK/INSERT 展开改用显式请求对象和结构化结果；仿射变换、递归、循环跳过、图层继承、阵列及预算算法未改变。等待用户验收，尚未提交或推送本批次 Git。

## 2. 修改前的问题

前期已经完成 Parser 文件拆分，但块展开入口仍为：

```cpp
bool expandDxfBlocks(
    DxfData& output,
    const BlockMap& blocks,
    const Inserts& inserts,
    const IgnoredLayers& ignoredLayers,
    double tolerance,
    std::size_t initialEntities,
    std::size_t maxOutputEntities,
    QString& error);
```

存在以下边界问题：

1. 8 个位置参数容易错位，输入、输出和配置没有明确分组。
2. 失败只返回 `false + QString`，调用方无法区分具体的资源边界。
3. Parser 默认值中仍重复写有 `0.01` 和 `100000`。
4. BLOCK 曲线镜像、旋转、Z 传播及 Sketch/FE 一致性测试尚未登记到 CTest。

## 3. 主要实现

### 3.1 显式块展开请求

新增 `DxfBlockExpansionRequest`，统一表达：

- 输出 `DxfData`；
- BLOCK 定义表；
- 模型空间 INSERT 列表；
- 忽略图层集合；
- 曲线容差；
- 初始实体数；
- 最大输出实体数。

Parser 只负责组装请求并调用块展开模块，不接触展开器内部的递归状态、访问集合或预算对象。

### 3.2 结构化展开结果

新增 `DxfBlockExpansionResult` 和 `DxfBlockExpansionStatus`：

| 状态 | 含义 |
|---|---|
| `Success` | 展开成功 |
| `ArrayInstanceLimit` | 单个 INSERT 行列阵列超限 |
| `BlockInstanceLimit` | 嵌套展开累计块实例超限 |
| `OutputEntityLimit` | 展开后累计实体超限 |
| `DepthLimit` | 嵌套递归深度超限 |

结果同时保留原有可读错误消息。Parser 对外继续映射为既有的
`DxfImportErrorCode::ExpansionLimit`，因此 Python、GUI 和日志错误语义保持兼容。

### 3.3 默认值单一来源

以下默认值改为引用 `DxfImportDefaults`：

- Parser 默认曲线容差；
- Parser 默认输出上限；
- 块展开预算默认输出上限；
- 相关测试使用的默认容差和预算。

### 3.4 保持既有几何算法

本批次没有修改：

- `Transform2D` 仿射矩阵构建和矩阵组合；
- 嵌套非均匀缩放、旋转和镜像处理；
- 圆、圆弧、椭圆、多段线和样条变换；
- 非相似变换下的曲线离散算法；
- INSERT 图层继承和忽略图层行为；
- 阵列坐标计算；
- 循环引用在首次重复块处停止的既有行为；
- Parser 失败后清除部分输出的行为。

## 4. 测试增强

### 4.1 结构化预算结果

`DxfBlockExpansion` 测试现在直接验证：

- 单个阵列超限返回 `ArrayInstanceLimit`；
- 嵌套累计实例超限返回 `BlockInstanceLimit`；
- 实体预算超限返回 `OutputEntityLimit`；
- 递归深度超限返回 `DepthLimit`；
- 精确预算边界仍成功；
- Parser 深度失败仍清空部分结果并可继续复用；
- 相互循环引用仍在首次重复处停止，不无限递归。

### 4.2 坐标级 BLOCK 曲线回归

新增并登记 `DxfTransformedCurves`，覆盖：

- 镜像加旋转后的 ARC、ELLIPSE、LWPOLYLINE、SPLINE 精确参数；
- X/Y 各种负缩放组合及 bulge 方向；
- 嵌套非均匀缩放加旋转后的精确端点、包围盒和 Z 坐标；
- 同一块展开结果转换为 Sketch 与 FE 后的线段一致性；
- 样条首尾坐标和来源追踪。

## 5. 涉及文件

### 5.1 修改

```text
src/Example1/DxfBlockExpansion.h
src/Example1/DxfBlockExpansion.cpp
src/Example1/DxfParser.h
src/Example1/DxfParser.cpp
test/CMakeLists.txt
test/test_dxf_block_expansion.cpp
```

### 5.2 新增

```text
test/test_dxf_transformed_curves.cpp
docs/阶段四_P4-3_Parser与BLOCK_INSERT职责收口实施报告.md
```

## 6. 自动化验证结果

构建目录：`build_stage4_vs2017`，配置：Visual Studio 2017 x64 Release。

| 检查 | 结果 |
|---|---|
| Release 构建 | 通过，插件和全部测试目标生成成功 |
| `Parser + DxfBlockExpansion + DxfTransformedCurves` | 3/3 通过 |
| 全量 CTest | 14/14 通过 |
| `npm.cmd run check` | 通过 |
| `npm.cmd test` | 8/8 通过 |
| `git diff --check` | 通过，仅有既有行尾转换提示 |

编译器仍报告既有 Python 2.7 头文件 `register` 警告，没有新增编译错误。

Web 解析和离散源码本批次没有改动，Web 全量测试通过；阶段三的三个 Web 几何指纹不受本批次 C++ 接口收口影响。

## 7. 代码审查结论

按正确性、兼容性、资源边界、可维护性和测试充分性检查本批次差异：

- 未发现阻塞验收的问题；
- 没有新增第二套仿射变换实现；
- 预算仍由一次展开共享，没有按嵌套层重新计数；
- Parser 对外方法签名和错误码保持兼容；
- 失败时 Parser 仍清除部分输出；
- 没有修改曲线支持范围或离散策略。

保留的既有语义：未知块引用会跳过并记录警告；循环块引用会在首次重复处停止，而不是把整个文件判为失败。

## 8. 用户验收步骤

### 8.1 自动化验收

在仓库根目录执行：

```powershell
cmake --build build_stage4_vs2017 --config Release
ctest --test-dir build_stage4_vs2017 -C Release -R "Parser|DxfBlockExpansion|DxfTransformedCurves" --output-on-failure
ctest --test-dir build_stage4_vs2017 -C Release --output-on-failure
npm.cmd run check
npm.cmd test
```

预期：

- 专项测试 `3/3` 通过；
- 全量 CTest `14/14` 通过；
- Node 语法检查通过；
- Node 测试 `8/8` 通过。

### 8.2 SAM 最小图精确验收

部署最新产物并重启 SAM：

```text
bin/Release/Example1.pyd
bin/Release/SAM.Pre.Example1Toolset.dll
```

导入：

```text
example\block_test_minimal.dxf
Profile: Small drawing
Mode: Sketch
Base X/Y/Z: 0 / 0 / 0
Curve tolerance: 0.01
```

检查必须得到 3 条直线：

```text
(1000,1000,0) -> (1100,1000,0)
(400,300,0)   -> (410,300,0)
(400,300,0)   -> (400,310,0)
```

包围范围必须为：

```text
Min = (400, 300)
Max = (1100, 1000)
```

### 8.3 SAM 阵列与曲线验收

分别用 Sketch 和 Finite Element 导入：

```text
example\slock_spline_zhenlie.dxf
Profile: Large drawing
Curve tolerance: 0.05
FE Model: Model-1
FE Part: P4_3_slock_FE
```

检查：

- [ ] 两种模式均导入成功，SAM 不崩溃、不长时间失去响应。
- [ ] 块、阵列、镜像、椭圆和样条均可见。
- [ ] 没有整体错位、异常超长线或明显缺失。
- [ ] Sketch 与 FE 的整体轮廓一致。
- [ ] 日志没有深度或预算错误。

### 8.4 预算错误快速验收

自动化测试已经覆盖构造的超限阵列和超深嵌套。人工只需确认：

- [ ] 正常样例不会误报 `INSERT expansion limit exceeded`。
- [ ] Large 图纸使用 Small Profile 超限时，导入失败且不留下部分 Sketch/Part。
- [ ] 失败后改用 Large Profile 可以立即重试成功。

## 9. 验收判定

满足以下条件即可回复“通过”：

- [ ] 自动化专项 3/3、全量 CTest 14/14、Node 8/8 均通过；
- [ ] 最小图 3 条线的数量和坐标准确；
- [ ] 阵列曲线图 Sketch/FE 轮廓一致；
- [ ] 正常导入无预算误报，失败不残留部分对象；
- [ ] 未发现 BLOCK/INSERT 坐标、镜像、Z 坐标或图层回归。

收到“通过”后，再提交并推送 P4-3。
