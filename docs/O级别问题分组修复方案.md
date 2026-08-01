# O 级别问题分组修复方案

## 1. 文档目的

本文将阶段二代码审查中的 6 项 Optional 问题整理为可独立实施、验证和回退的
修复批次。O 系列不阻断当前功能发布，但会影响后续维护成本、构建效率、路径兼容性
和大型图纸性能，因此应在 Critical 与 Required 问题完成代码修复后分阶段实施。

关联文档：[阶段二代码审查报告](阶段二_代码审查报告.md)

制定日期：2026-08-01
当前基线：`example-dxf-plugin` 分支，提交 `b329d21`

## 2. 当前复查结论

阶段二列出的 6 项 O 级问题目前仍然有效：

| 编号 | 问题 | 当前证据 | 状态 |
| --- | --- | --- | --- |
| O-01 | 错误类型仍依赖字符串比较 | Sketch 和 FE 路径仍比较 `import canceled by user`、`already exists` 等文本 | 待处理 |
| O-02 | 多个文件职责过大 | `page/app.js`、`server.js`、`DxfParser.cpp`、`Example1PytModule.cpp` 等仍为大型多职责文件 | 待处理 |
| O-03 | CMake 使用目录级配置和 `GLOB` | Example1 与 Toolset 仍使用全局 include/link/definition 和 `file(GLOB)` | 待处理 |
| O-04 | 测试重复编译生产源码 | 多个测试目标重复编译 `GeometryUtils.cpp`、`DxfData.cpp`、`ConversionEngine.cpp` | 待处理 |
| O-05 | DXF 路径使用本地 8 位编码 | `DxfParser::parseFile()` 仍通过 `toLocal8Bit()` 传递路径 | 待处理 |
| O-06 | 前端图层统计重复扫描实体 | 每个图层分别过滤全部点和线，复杂度约为 `O(L × N)` | 待处理 |

其中：

- `L` 为图层数量。
- `N` 为离散后的点和线总数。
- O-01 已有局部 `BuildStatus`，但状态来源尚未完全结构化，不能视为关闭。

## 3. 分组原则

1. 行为修复、性能优化和结构拆分分别提交。
2. 先处理小范围、容易验证的问题，再拆分大型文件。
3. 每个提交必须保持 Release 构建和现有测试通过。
4. 文件拆分必须减少同时需要理解的概念，不能只把代码移动到更多文件。
5. 不在 O 系列重构中改变 DXF 几何、SAM 图层、预算或事务语义。
6. 不引入新的第三方依赖，除非现有技术栈无法解决且经过单独评审。

## 4. 推荐执行顺序

| 顺序 | 修复组 | 包含问题 | 核心目标 | 主要原因 |
| ---: | --- | --- | --- | --- |
| 0 | OG0 基线准备 | 前置工作 | 建立可回退、可比较的稳定基线 | 防止用户改动或未提交文件混入 |
| 1 | OG1 图层统计 | O-06 | 将图层计数改为单次遍历 | 范围小、收益明确、风险低 |
| 2 | OG2 结构化状态 | O-01 | 消除字符串控制流程 | 为后续导入模块拆分建立边界 |
| 3 | OG3 测试核心库 | O-04 | 生产和测试链接同一实现 | 减少重复编译并稳定目标边界 |
| 4 | OG4 CMake 目标化 | O-03 | 清除目录级依赖泄漏 | 依赖 OG3 建立的库边界 |
| 5 | OG5 Unicode 路径 | O-05 | 支持非系统代码页路径 | 需要单独验证 libdxfrw 能力 |
| 6 | OG6 职责拆分 | O-02 | 拆分 C++、Web 和 Node 大文件 | 最后执行以利用前面形成的边界 |

依赖关系：

```text
OG0 基线准备
    │
    ├── OG1 图层统计
    │
    ├── OG2 结构化状态 ──────────┐
    │                            │
    ├── OG3 测试核心库 → OG4 CMake 目标化
    │                            │
    ├── OG5 Unicode 路径         │
    │                            │
    └────────────────────────────┴→ OG6 职责拆分
```

## 5. OG0：基线准备

### 5.1 目标

保证 O 系列可以逐组验证、提交和回退，并避免无关工作区内容进入重构提交。

### 5.2 实施步骤

1. 检查当前分支和工作区：

```powershell
git status --short --branch
git log --oneline -5
```

2. 单独处理或保留用户已有的 G6 文档修改和 `.claude/`，不得混入 O 系列提交。
3. 推送或确认当前功能基线提交 `b329d21`。
4. 创建独立分支：

```powershell
git switch -c refactor/o-series
```

5. 执行并记录初始门禁：

```powershell
npm.cmd run check
npm.cmd test
ctest --test-dir build -C Release --output-on-failure
cmake --build build --config Release
```

### 5.3 通过条件

- 工作区中的无关改动已明确隔离。
- Node、CTest 和 Release 构建全部通过。
- 当前提交号、构建环境和测试结果已记录。

## 6. OG1：O-06 前端图层统计优化

### 6.1 问题

`page/app.js` 当前对每个图层分别扫描全部点和线。实体数量和图层数量同时增长时，
刷新图层面板的成本约为：

```text
O(图层数 × 实体数)
```

Unlimited 档位允许更大的最终输出，因此该问题的实际影响会更加明显。

### 6.2 实施步骤

1. 添加单次遍历函数，例如 `buildLayerCounts()`。
2. 一次扫描 `state.points` 和 `state.lines`，建立 `Map<layer, count>`。
3. 按 SAM 规则处理有效图层与祖先 INSERT 控制层：
   - 实体属于一个有效图层；
   - 祖先 INSERT 图层仍可以控制该实体显示；
   - 同一实体不能在同一图层重复计数。
4. 图层面板直接读取计数映射，不再逐层调用 `filter()`。
5. 仅在实体集合或图层关系变化时重建映射。
6. 保持图层隐藏、选择和渲染行为不变。

### 6.3 测试

- 普通实体图层计数。
- 块内图层 `0` 的 INSERT 图层继承。
- 多层嵌套 INSERT 控制层。
- 一个控制层和有效图层同名时不重复计数。
- IGNORE、KEEP、PARENT、Fixed 等现有 SAM 图层回归文件。
- 大量实体、多图层情况下的刷新耗时对比。

### 6.4 建议提交

```text
perf(web): index layer counts in one pass
```

## 7. OG2：O-01 结构化构建状态

### 7.1 问题

当前代码已有局部 `BuildStatus`，但 Builder 仍主要返回整数或布尔值，并由调用方
比较英文错误文本判断取消、开始失败和名称冲突。修改提示文案可能改变程序控制流。

### 7.2 目标模型

建议定义共享状态：

```cpp
enum class BuildStatus
{
    Success,
    Canceled,
    BeginFailed,
    CreateFailed,
    CommitFailed,
    RollbackFailed
};

struct BuildResult
{
    BuildStatus status;
    int createdCount;
    QString message;
};
```

### 7.3 实施步骤

1. 先为当前字符串分类行为增加锁定测试。
2. 让 `SamBuilder` 返回结构化状态，不再只返回负数和 `lastError()`。
3. 让 `PythonFiniteElementBuilder` 使用相同的状态模型。
4. Sketch 与 FE 编排只根据枚举分支。
5. 日志和对话框继续使用 `message`，但文案不参与状态判断。
6. 删除以下形式的程序逻辑：

```cpp
error == QStringLiteral("import canceled by user")
error.contains(QStringLiteral("already exists"))
```

7. 保留 G5 已实现的事务回滚、幂等和取消语义。

### 7.4 测试

- begin 失败。
- 创建节点、线、圆或单元失败。
- 用户取消。
- commit 失败。
- rollback 失败以及再次回滚。
- 成功提交后禁止回滚。
- 修改错误文案后状态测试仍通过。

### 7.5 建议提交

```text
refactor(import): return structured build status
```

## 8. OG3：O-04 测试公共核心库

### 8.1 问题

`test/CMakeLists.txt` 将相同生产源码重复加入多个测试目标，导致完整构建反复编译
相同文件，并可能使生产目标和测试目标使用不同编译配置。

### 8.2 实施步骤

1. 建立不依赖 SAM Repository 的静态库，例如 `Example1Core`。
2. 初始纳入以下可测试实现：

```text
DxfData.cpp
GeometryUtils.cpp
ConversionEngine.cpp
FeData.cpp
FeConversionEngine.cpp
```

3. 根据 libdxfrw 依赖边界决定 `DxfParser.cpp`：
   - 纳入同一核心库并公开 libdxfrw 链接依赖；或
   - 建立单独的 `Example1DxfParser` 静态库。
4. 生产插件和测试目标链接相同静态库。
5. 删除测试目标中的 `TESTABLE_SOURCES` 重复展开。
6. 验证库的 Qt、OCCT、libdxfrw 依赖可见性正确。
7. 对比修改前后的全量构建时间和实际编译次数。

### 8.3 通过条件

- 每个生产 `.cpp` 在一次配置中只有一个主要编译目标。
- 测试使用的实现与生产插件一致。
- 所有测试和正式插件目标构建成功。

### 8.4 建议提交

```text
build(test): share Example1 core library
```

## 9. OG4：O-03 CMake 目标化

### 9.1 实施步骤

1. 用 `target_include_directories()` 替换目录级 `include_directories()`。
2. 用目标链接或 `target_link_directories()` 替换全局 `link_directories()`。
3. 用 `target_compile_definitions()` 替换 `add_definitions()`。
4. 用明确源文件列表替换 `file(GLOB)`。
5. 正确标注 `PRIVATE`、`PUBLIC` 和 `INTERFACE` 传播范围。
6. 保持 SAM、SAMSDK、libdxfrw、spdlog 和 OCCT 路径可配置。
7. 不增加开发机绝对路径。
8. 删除旧构建目录后执行一次全新配置，防止缓存掩盖依赖问题。

### 9.2 验证

```powershell
cmake -S . -B build-clean -DEXAMPLE1_BUILD_TESTS=ON
cmake --build build-clean --config Release
ctest --test-dir build-clean -C Release --output-on-failure
```

还应验证：

- 新增源文件时配置行为明确。
- 可选 ContainerShip 目标关闭时不参与构建。
- 测试关闭时不下载或构建 Google Test。

### 9.3 建议提交

```text
build(cmake): scope Example1 target settings
```

## 10. OG5：O-05 Unicode DXF 路径

### 10.1 问题

当前解析入口通过 `QString::toLocal8Bit()` 将路径交给 libdxfrw。在 Windows 系统
代码页无法表示文件名时，路径可能被替换或损坏。

### 10.2 实施步骤

1. 先创建中文、日文、扩展拉丁字符和空格路径测试。
2. 确认 libdxfrw 当前版本是否支持：
   - 宽字符路径；
   - 已打开文件流；
   - Windows Unicode 文件句柄。
3. 优先使用库原生 Unicode 或流接口。
4. 不得仅把 `toLocal8Bit()` 改为 `toUtf8()`；Windows 文件 API 不一定按 UTF-8
   解释窄字符路径。
5. 如果 libdxfrw 无法直接支持，设计受控临时 ASCII 路径方案：
   - 使用唯一临时目录和文件名；
   - 复制前后验证文件；
   - 解析结束自动清理；
   - 日志仍记录原始路径；
   - 清理失败不得覆盖主要解析错误。
6. 在真实 SAM 环境完成中文目录和中文文件名验收。

### 10.3 测试

- 中文目录、中文文件名。
- 日文和扩展字符。
- 带空格与括号的路径。
- 文件不存在、无权限和临时复制失败。
- 成功和失败后均无临时文件泄漏。

### 10.4 建议提交

```text
fix(dxf): support Unicode import paths
```

## 11. OG6：O-02 大文件职责拆分

大文件拆分最后执行，并拆成多个独立提交。每次只移动一种职责，不同时改变业务
行为、算法或输出格式。

### 11.1 拆分 `DxfParser.cpp`

建议边界：

```text
DxfReaderCallbacks    libdxfrw 回调与原始数据接收
DxfEntityValidation  原始实体校验与规范化
DxfTransform         点、向量和仿射变换
DxfBlockExpansion    INSERT、阵列、递归与预算
DxfParser            文件入口与结果编排
```

要求：G2 仿射语义、G3 图层继承、G4 数值防御和 C-02 预算必须保持不变。

### 11.2 拆分 `Example1PytModule.cpp`

建议保留：

- Python 参数解析。
- 调用导入服务。
- 将结果转换为 Python 返回值。

建议移出：

- 文件和参数校验。
- Parser、Converter、Builder 编排。
- Sketch/FE 模式选择。
- 取消、失败和回滚状态传播。
- 日志生命周期和摘要格式化。

OG2 的结构化状态应成为模块边界，不再复制错误分类逻辑。

### 11.3 拆分 `page/app.js`

在不引入新框架和构建器的前提下，建议拆为：

```text
state.js              页面状态与快照
viewport.js           坐标变换、缩放和适配
renderer.js           画布渲染
selection.js          选择、框选和编辑
import-controller.js  文件导入与离散化
agent-controller.js   Agent 请求和结果应用
app.js                初始化与模块编排
```

OG1 形成的图层索引应放在状态或图层模块中，不得重新引入多次全量扫描。

### 11.4 拆分 `server.js`

建议拆为：

```text
router                 HTTP 路由与响应
security               Origin、CSRF 和安全头
static-server          静态资源服务
provider-controller    上游模型请求编排
geometry-tools         Agent 几何工具
server                 监听配置与生命周期
```

要求：G6 的 API Key、固定端点、重定向、超时和响应大小限制不得弱化。

### 11.5 拆分门禁

每个文件拆分提交必须满足：

- 仅移动职责，不改变行为。
- `git diff --check` 通过。
- Node 和 CTest 全部通过。
- 正式插件 Release 构建通过。
- 三个阶段一 Web 基准几何指纹不变。
- SAM 图层数量和 Sketch/FE 创建结果不变。

建议拆成至少四个提交：

```text
refactor(dxf): split parser responsibilities
refactor(import): extract import orchestration
refactor(web): split editor responsibilities
refactor(server): split HTTP responsibilities
```

## 12. 统一验证门禁

### 12.1 每个提交

```powershell
npm.cmd run check
npm.cmd test
ctest --test-dir build -C Release --output-on-failure
git diff --check
```

涉及 C++ 或 CMake 时增加：

```powershell
cmake --build build --config Release --target Example1 Example1Toolset
```

### 12.2 每个修复组完成后

- 审查正确性、可读性、架构、安全性和性能。
- 核对提交中没有混入无关文件。
- 对比关键几何数量、坐标、图层和包围盒。
- 更新阶段二报告中对应 O 项状态。

### 12.3 最终 SAM 验收

至少覆盖：

1. 小型精确图纸：核对坐标、方向、尺寸和数量。
2. 综合图纸：覆盖图层、块、嵌套、阵列、镜像和复杂曲线。
3. 大型船舶图纸：检查耗时、内存、取消响应和连续导入稳定性。
4. 中文路径图纸：Sketch 与 FE 双模式。
5. Small、Large、Unlimited 三个图纸规模档位。

## 13. 完成检查表

- [ ] OG0：稳定基线已建立，无关工作区修改已隔离。
- [ ] OG1/O-06：图层统计改为一次遍历并保持 SAM 图层语义。
- [ ] OG2/O-01：程序控制流不再依赖错误文案。
- [ ] OG3/O-04：测试与生产链接共享核心实现。
- [ ] OG4/O-03：CMake 使用目标级配置和明确源文件列表。
- [ ] OG5/O-05：Unicode 路径通过自动化和 SAM 人工验收。
- [ ] OG6/O-02：主要大文件按职责拆分且复杂度实际下降。
- [ ] 所有 Node 和 C++ 自动化测试通过。
- [ ] Release 正式目标构建成功。
- [ ] 阶段一几何基线保持一致。
- [ ] Sketch/FE、图层、事务、预算和 Agent 安全语义未退化。

## 14. 提交数量建议

O 系列建议拆成 8～10 个提交：

1. O-06 图层统计。
2. O-01 结构化状态。
3. O-04 公共核心库。
4. O-03 Example1 CMake 目标化。
5. O-03 Toolset 与测试 CMake 目标化。
6. O-05 Unicode 路径。
7. O-02 Parser 拆分。
8. O-02 导入编排拆分。
9. O-02 Web 拆分。
10. O-02 Server 拆分。

不建议把全部 O 系列压缩为一个大提交，也不建议在文件拆分过程中顺便修改几何、
事务或安全行为。
