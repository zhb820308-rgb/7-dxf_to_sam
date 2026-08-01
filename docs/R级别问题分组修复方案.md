w

# R 级别问题分组修复方案

## 1. 目的

本文将阶段二代码审查发现的 12 项 Required 问题按依赖关系划分为 6 个
可独立实施、验证和回退的修复组。原始问题证据见
[阶段二代码审查报告](阶段二_代码审查报告.md)。

基准提交：`4b48230`
当前审查提交：`698c46b`
制定日期：2026-08-01
当前状态：G1、G2、H1、G3、G4、G6 已关闭；G5 代码完成、待 SAM GUI 验收

## 2. 分组原则

分组遵循以下规则：

1. 先建立能发现回归的测试，再修改高风险生产逻辑。
2. 使用同一基础抽象的问题放在同一组，例如仿射矩阵和方向向量。
3. 几何正确性、事务原子性和 Agent 安全分别提交，避免互相干扰。
4. 每组必须保持项目可构建、可测试，并可以单独回退。
5. 功能修复与纯文件拆分、命名清理等重构分开。
6. C-02 的 C++ 最终输出预算已随 budget 分支合并完成；SAM GUI 大图验收前仍避免导入不可信大文件。

## 3. 分组总览

| 顺序 | 修复组                  | 包含问题         | 核心目标                       | 主要风险                     |
| ---: | ----------------------- | ---------------- | ------------------------------ | ---------------------------- |
|    1 | G1 测试基础（已完成）   | R-11             | 建立可移植、坐标级回归测试     | 测试继续漏报几何错误         |
|    2 | G2 几何变换（已完成）   | R-01、R-02、R-04 | 统一点、向量和嵌套仿射变换     | 静默几何错误                 |
|    3 | G3 图层语义（已完成）   | R-03             | 正确实现 INSERT 图层继承与过滤 | 忽略图层失效                 |
|    4 | G4 输入合法性（已完成） | R-06、R-09、R-10 | 在数据边界拒绝极端值和无效实体 | CPU 阻塞、异常和错误成功状态 |
|    5 | G5 事务回滚（待验收）   | R-05、R-12       | 保证 FE/Sketch 提交失败原子性  | 半成品 Part 或草图残留       |
|    6 | G6 Agent 安全（已完成） | R-07、R-08       | 消除持久密钥和无界上游响应     | 密钥泄露、请求长期占用       |

推荐执行顺序：

```text
G1 测试基础（已完成；C-02 代码随后补完）
        ↓
G2 几何变换（已完成）
        ↓
H1 样条右值统计热修复（已完成）
        ↓
G3 图层语义（已完成）
        ↓
G4 输入合法性（已完成）
        ↓
G5 事务回滚

G6 Agent 安全（已完成）
```

## 4. G1：测试基础与坐标级断言

**状态：已完成（2026-08-01）。**

### 4.1 包含问题

- R-11：解析测试不可移植且关键断言过弱。

### 4.2 原始问题

`test/test_parser.cpp` 原先把测试目录写死为当前开发机的绝对路径。部分测试只
检查解析成功或数量大于零，多文件测试还可能跳过缺失文件。这些断言无法发现
旋转角、主轴、镜像方向和 Z 坐标错误。

### 4.3 实施内容

1. 由 CMake 通过编译定义注入测试数据路径和示例目录。
2. 删除源码中的开发机绝对路径。
3. 文件缺失使用 `ASSERT_TRUE` 立即失败，不允许 `continue`。
4. 建立坐标比较辅助函数，统一绝对/相对容差。
5. 添加可执行的非零块基点和旋转阵列夹具，检查精确线段坐标与顺序。
6. 添加嵌套非均匀缩放和旋转 ARC/ELLIPSE 的正确结果规格；修复前规格可捕获错误，G2 完成后已启用。
7. 镜像、SPLINE 切向量和 LWPOLYLINE Z 的精确规格已随 G2 实现补充并启用。

### 4.4 验收标准

- 从仓库根目录和 `build/` 目录运行 Parser/CTest 均通过：已验证。
- 测试源码不包含开发机绝对路径：已验证。
- 测试数据缺失、解析失败和转换失败时明确失败：已实现。
- 非零块基点和旋转阵列使用精确坐标断言：已实现并通过。
- 两项 G2 正确结果规格修复前显式运行均失败；G2 完成后已移除 `DISABLED_` 并通过。

### 4.5 建议提交

```text
test(parser): make geometry fixtures portable
```

## 5. G2：仿射变换与曲线语义

**状态：已完成（2026-08-01）。**

### 5.1 包含问题

- R-01：嵌套非均匀缩放使用错误的参数组合。
- R-02：块内曲线没有完整应用变换。
- R-04：FE 椭圆破坏相对主轴向量。

### 5.2 原始问题

当前 `Transform2D` 能减少单个 INSERT 中重复的三角函数和基点计算，但嵌套
INSERT 仍把缩放相乘、角度相加，无法表达非均匀缩放和旋转组合产生的剪切。
曲线保留路径也没有完整处理方向、切向量、角度和 Z 平移。

### 5.3 目标结构

建立一个完整的仿射变换对象，至少提供：

```cpp
DxfPoint transformPoint(const DxfPoint& point) const;
DxfPoint transformVector(const DxfPoint& vector) const;
AffineTransform composedWith(const AffineTransform& child) const;
double determinant() const;
bool preservesAngles() const;
bool hasUniformScale() const;
```

语义要求：

- 点使用线性部分和平移。
- 向量只使用线性部分，不使用平移。
- 嵌套 INSERT 使用矩阵乘法，不重新分解为角度和缩放。
- 阵列偏移进入相同的局部坐标变换。
- 使用行列式判断镜像后方向是否翻转。

### 5.4 曲线处理规则

| 类型       | 可以保留原类型的条件             | 一般仿射变换处理                 |
| ---------- | -------------------------------- | -------------------------------- |
| Circle     | 统一非零缩放和刚体旋转           | 转为 Ellipse 或先离散            |
| Arc        | 保角变换，半径保持正值           | 先离散后变换                     |
| Ellipse    | 能准确更新主轴、比例和方向       | 无法证明时先离散                 |
| LWPOLYLINE | 直线段或保角变换                 | 含 bulge 时先离散                |
| Spline     | 控制点、拟合点和切向量全部可变换 | 保留节点和权重，变换所有向量数据 |

### 5.5 FE 椭圆

FE 转换中：

```text
中心 = center + base
主轴 = majorAxisVector
```

主轴是相对向量，不能先加基点再减去中心。若后续引入缩放或旋转，应使用
`transformVector()`。

### 5.6 验收标准

- 外层 X 缩放 2、Y 缩放 1，内层旋转 90° 的坐标与矩阵计算一致。
- 单轴镜像后的圆弧方向正确，半径始终为正。
- INSERT 旋转后椭圆主轴同步旋转。
- 样条首尾点和切向量同时正确。
- 非零块基点和非零 Z 插入后的 LWPOLYLINE 高度正确。
- Sketch 与 FE 对同一椭圆的离散坐标在容差内一致。

### 5.7 建议提交拆分

```text
refactor(dxf): represent inserts with affine transforms
fix(dxf): apply transforms to preserved curves
fix(fe): preserve ellipse major-axis vectors
```

G2 可以使用同一分支连续提交，但每个提交后都必须保持测试通过。

### 5.8 实际完成内容

1. `Transform2D` 增加完整矩阵组合、点/向量/Z 变换、行列式和相似变换判断。
2. 嵌套 INSERT 与阵列实例直接组合父子矩阵，不再相乘缩放、相加角度。
3. 修复圆弧方向与角度、椭圆主轴、LWPOLYLINE bulge/Z、样条切向量。
4. 非相似的一般仿射变换继续采用“源曲线离散后变换”，避免错误保留曲线类型。
5. FE 椭圆仅平移中心，主轴保持相对向量语义。
6. 新增并启用嵌套、旋转、镜像、Z、样条切向量和 FE 椭圆回归测试。

### 5.9 G3 前置门禁：样条右值统计热修复

**状态：已完成（2026-08-01）。**

提交 `c3d8555` 为 SPLINE 解析增加移动语义后，`DxfData::addSpline(DxfSpline&&)`
先把对象移动进容器，再调用移动后对象的 `kind()` 更新统计。未提前计算过
`kind()` 的控制型样条，其控制点和节点容器移动后为空，可能被统计为拟合型。

该回归不属于 R-10 的公共不变量修复，也不重新打开 G2。实际完成内容：

1. 移动前保存 `SplineKind`，移动后使用保存值更新统计。
2. 添加右值控制型和拟合型样条统计测试，并证明修复前测试失败。
3. 保留移动构造优化；移除没有改变 O(1) 渐近复杂度的 `m_kind` 可变缓存。
4. 新测试在修复前稳定失败、修复后通过。
5. Release 完整构建、CTest 7/7、Node 语法检查和四组 Node 测试均通过。

## 6. G3：INSERT 图层继承与过滤

**状态：已完成（2026-08-01）。**

### 6.1 包含问题

- R-03：忽略图层对 INSERT 和块内 layer 0 不生效。

### 6.2 DXF 图层规则

- 块内实体使用显式非零图层时，保持自身图层。
- 块内实体位于 layer 0 时，继承 INSERT 的有效图层。
- 嵌套 INSERT 的 layer 0 继续继承外层有效图层。
- 得到最终有效图层后，再执行 ignoredLayers 过滤。

### 6.3 实施内容

1. 先添加普通实体、INSERT、块内 layer 0、显式图层和两层嵌套的失败规格。
2. 在 `InsertInfo` 中保存 INSERT 图层。
3. 在块实体数据或展开上下文中保留原始图层。
4. 为展开函数增加显式图层上下文，统一计算继承后的有效图层。
5. 删除只在解析回调阶段过滤、导致无法表达继承的分支，并按有效图层过滤。
6. 日志记录原始图层和最终有效图层；该可观测性修改与核心过滤逻辑分开提交。

### 6.4 验收标准

- 普通实体忽略图层行为保持不变。
- 被忽略图层上的 INSERT 不生成任何继承实体。
- 块内 layer 0 正确继承 INSERT 图层。
- 块内显式图层不被 INSERT 图层覆盖。
- 两层嵌套的 layer 0 继承结果正确。

### 6.5 建议提交

```text
fix(dxf): apply INSERT layer inheritance
```

### 6.6 实际完成内容

1. `DxfEntity` 与 `InsertInfo` 保存原始图层，默认图层为 `0`。
2. 模型空间普通实体仍可立即过滤；块定义内实体延迟到展开阶段过滤。
3. 顶层和嵌套 INSERT 使用显式有效图层上下文；layer 0 逐层继承，显式非零图层保持不变。
4. 被忽略图层上的 INSERT 整体跳过，不消费展开预算，也不生成部分实体。
5. 保留曲线和离散生成线均保存最终有效图层。
6. 新增 `block_layer_inheritance.dxf`，覆盖普通实体、顶层 INSERT、两层继承、显式图层和忽略 layer 0。
7. 原始/最终图层的详细日志作为独立可观测性增强，不阻断 R-03 关闭。
8. Web 保存实体有效图层和祖先 INSERT 控制层；列表计数、高亮、隐藏及 Agent 按层匹配统一采用 SAM 的受影响图元语义。
9. Release 完整构建、CTest 7/7、Node 语法检查和五组 Node 测试均通过。

## 7. G4：数值边界与实体不变量

**状态：已完成（2026-08-01）。**

### 7.1 包含问题

- R-06：极端角度和段数计算不安全。
- R-09：无效实体会被计为解析成功。
- R-10：样条公共合法性检查不完整。

### 7.2 角度和段数

1. 所有角度、半径、容差和中间差值先检查 `std::isfinite()`。
2. 使用 `std::fmod()` 常数时间归一化角度。
3. 在浮点域限制段数上限后，再转换为整数。
4. 明确 Inf、NaN、DBL_MAX 附近数值的错误结果。

### 7.3 有效实体统计

建议把统计拆成：

```text
sourceEntities   原始支持实体数
acceptedEntities 通过不变量检查的实体数
rejectedEntities 被拒绝的实体数及原因
generatedEntities 展开或离散产生的实体数
```

`parseFile()` 的成功条件使用 `acceptedEntities + generatedEntities`，不能只检查
容器中是否曾经追加对象。

### 7.4 样条不变量

提交 `c3d8555` 只优化解析期容器移动与分类，不补全公共合法性检查，R-10
仍保持打开。不要用 Parser 回调中的局部校验替代 `DxfSpline::isValid()` 的
类型边界校验。

`DxfSpline::isValid()` 至少检查：

- degree 合法；
- 控制点和拟合点全部有限；
- 节点值有限且非递减；
- 节点数量符合 degree 和控制点数量；
- 有理样条权重数量与控制点一致；
- 权重有限，并符合 OCCT 接口要求；
- 切向量全部有限。

### 7.5 验收标准

- DBL_MAX 附近角度在常数时间内失败或归一化。
- 无穷角度差不会转换为整数。
- 只含零长度线、零半径圆或非有限坐标的文件解析失败。
- 权重不足、乱序节点、非有限控制点和错误 degree 的样条稳定失败。
- 合法的大角度和有理样条输出不发生回归。

### 7.6 建议提交拆分

```text
fix(geometry): bound angle normalization
fix(dxf): reject invalid parsed entities
fix(spline): enforce public invariants
```

### 7.7 实际完成内容

1. `normalizeSweep()` 使用 `std::fmod()` 将有限角度归一化到单圈有向范围，非有限输入返回 NaN，不再按圈数循环。
2. `calculateArcSegmentCount()` 在任何浮点到整数转换前检查有限性，并在浮点域将输出限制到 10,000。
3. `DxfData` 的公共添加入口拒绝无效实体，统计 `sourceEntities`、`acceptedEntities`、`rejectedEntities`、`generatedEntities` 和拒绝原因。
4. INSERT 展开和曲线离散产物通过独立生成入口记录，不再与模型空间原生有效实体混计。
5. `parseFile()` 以 `acceptedEntities + generatedEntities` 和实际输出共同判定成功；仅含无效实体的文件返回失败及拒绝数量。
6. `DxfSpline::isValid()` 统一检查 OCCT degree 上限、全部点和切向量有限、节点数量/顺序/重数、以及有理权重数量和值。
7. 新增 `invalid_entities_only.dxf` 和 DBL_MAX、Inf、NaN、错误 degree、乱序节点、权重不足/非正等回归测试。
8. Release 全量构建、CTest 7/7、五组 Node 测试和 diff 检查通过。

## 8. G5：FE/Sketch 事务与回滚

**状态：代码与自动化验证已完成（2026-08-01），待 SAM GUI 人工验收。**

### 8.1 包含问题

- R-05：FE rollback 保留半成品 Part。
- R-12：Sketch commit/rollback 不是失败原子操作。

### 8.2 事务状态机

两个 Builder 应共享明确的状态语义：

```text
Idle
→ Preparing
→ Writing
→ Committing
→ Committed

任一步失败或取消：
→ RollingBack
→ RolledBack
```

每个状态必须定义：

- 当前资源所有者；
- 已经写入仓库的对象；
- 可以执行的下一步；
- 失败时的补偿操作；
- rollback 是否可以重复调用。

### 8.3 FE 实施策略

优先验证 SAM 是否提供稳定的删除 Part API。如果可用，在任何失败或取消后删除
目标 Part。如果不可用，采用临时唯一名称创建，全部成功后再发布或重命名，并
为无法清理的异常提供明确恢复步骤。

### 8.4 Sketch 实施策略

1. 区分内存中草图、仓库中草图和场景显示对象的所有权。
2. 仓库插入成功后记录可补偿句柄。
3. `Replace()` 或后续步骤失败时执行补偿删除。
4. 如果场景刷新不影响数据完整性，将其移出事务成功判定。
5. 返回结构化提交结果，避免依赖错误文案判断状态。

### 8.5 验收标准

- 节点批次、单元批次、显示阶段和用户取消后没有半成品 Part。
- 同名 Part 可以在失败后立即重试。
- Sketch 仓库插入、Replace、场景获取和 PDO 重建分别失败时，仓库状态与返回状态一致。
- rollback 连续调用两次不会产生新的错误或重复删除。
- 必须完成 SAM GUI 人工验收。

### 8.6 建议提交拆分

```text
fix(fe): remove partial parts on rollback
fix(sketch): make repository commit atomic
```

### 8.7 实施结果

1. 新增共享 `ImportTransaction`，实现 Preparing、Writing、Committing、Committed、
   RollingBack 和 RolledBack 状态及可重试、幂等 rollback。
2. FE rollback 删除半成品 Part 并通过 Repository 二次验证；创建命令失败也执行补偿。
3. Sketch 检查 Insert 结果，Replace 异常后可补偿删除；场景/PDO 刷新移出提交判定。
4. 新增 6 个事务测试，Release 构建、CTest 8/8、Node 测试脚本 4/4 全部通过。
5. 详细设计、证据和人工验收项见 [G5事务回滚修复说明](G5事务回滚修复说明.md)。

## 9. G6：Agent 密钥与上游请求安全

**状态：已完成（2026-08-01）。**

### 9.1 包含问题

- R-07：API Key 被持久化到 localStorage。
- R-08：上游模型请求没有超时和响应体上限。

### 9.2 密钥生命周期

1. `saveAgentSettings()` 不再接收或序列化 `apiKey`。
2. API Key 只保存在当前页面内存或服务端环境变量中。
3. 页面刷新后 Key 输入框必须为空。
4. 清理已有 localStorage 数据中的 `apiKey` 字段。
5. UI 明确说明密钥不会持久保存。

### 9.3 上游请求边界

1. 使用 `AbortController` 设置总超时。
2. 超时后主动取消 fetch，并映射为 HTTP 504。
3. 读取响应流时累计字节数，超过上限立即取消。
4. 上游非成功状态和超大响应映射为 502。
5. 保持 C-01 已建立的固定端点、同源、CSRF 和禁止重定向规则。

### 9.4 验收标准

- 保存配置和刷新页面后 localStorage 中不存在密钥。
- 模拟永不响应的上游，服务在规定时间内返回 504。
- 模拟超大 JSON 响应，服务在达到上限时停止读取并返回 502。
- 超时和超大响应不会泄露 API Key 或响应正文。
- C-01 安全测试继续通过。

### 9.5 建议提交拆分

```text
fix(agent): keep API keys out of localStorage
fix(agent): bound upstream requests
```

### 9.6 实施结果

1. 设置持久化改为 provider/model/apiUrl 白名单，旧 apiKey 自动迁移删除。
2. 页面初始化清空 Key，UI 明确说明密钥不持久化。
3. 上游请求提取为独立模块，增加 30 秒全过程超时和 4 MiB 解压后流式响应上限。
4. 上游非成功、超限和无效 JSON 映射为 502，超时映射为 504；正文和 Key 不进入错误消息。
5. 新增 Agent 边界测试；详细说明见
   [G6 Agent 安全修复说明](G6_Agent密钥与上游请求安全修复说明.md)。

## 10. 每组统一质量门禁

每个修复组完成后执行：

```powershell
npm.cmd run check
npm.cmd test
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

涉及 Web 或 DXF 几何的组还必须重新比较阶段一三个几何指纹。涉及 Sketch/FE
事务的 G5 必须完成 SAM GUI 人工验收。

关闭一个 Required 问题必须同时满足：

- 修复代码已提交；
- 原问题能够由新增测试复现；
- 新增测试在修复后通过；
- 完整自动化测试通过；
- 文档中的状态、位置和验证证据已更新；
- 没有把失败改成静默跳过或部分成功。

## 11. 分支与提交建议

建议每组使用独立分支：

```text
required/g1-test-foundation
required/g2-affine-transform
required/g3-layer-inheritance
required/g4-input-validation
required/g5-transaction-rollback
required/g6-agent-boundaries
```

G2、G4、G5、G6 内部可以拆为多个小提交，但不应把不同组压缩到一个提交中。
每组合并后再从最新目标分支创建下一组分支。

## 12. 完成检查表

- [X] G1：R-11 已关闭。
- [X] G2：R-01、R-02、R-04 已关闭。
- [X] H1：样条右值统计回归已关闭。
- [X] G3：R-03 已关闭。
- [X] G4：R-06、R-09、R-10 已关闭。
- [X] G5：代码与自动化验证已完成；SAM GUI 验收后关闭 R-05、R-12。
- [X] G6：R-07、R-08 已关闭。
- [X] C-02 C++ 最终转换预算已补完，待 SAM GUI 大图验收。
- [X] G1 完成时所有自动化检查通过。
- [X] 阶段二报告状态和验证证据已同步。
