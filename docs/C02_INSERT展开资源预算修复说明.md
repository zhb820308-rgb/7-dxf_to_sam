# C-02：INSERT 展开资源预算修复说明

## 1. 文档目的

本文说明 C-02 的问题成因、风险、修复方案、实现边界和验证结果，供后续代码审查、维护及回归测试使用。

修复日期：2026-08-01
修复状态：代码完成；自动化验证通过，待 SAM GUI 大图人工验收
关联报告：[阶段二代码审查报告](阶段二_代码审查报告.md)

## 2. 问题背景

DXF 的 `INSERT` 用于把一个 `BLOCK` 放置到模型空间或另一个块中。一个 `INSERT` 还可以通过以下字段表达矩形阵列：

- `rows`：阵列行数。
- `columns`：阵列列数。
- `rowSpacing`：行间距。
- `columnSpacing`：列间距。

因此，一个 INSERT 需要创建的块实例数为：

```text
instances = rows × columns
```

例如，`rows = 1000`、`columns = 1000` 时，一个 INSERT 就会生成 1,000,000 个块实例。如果块内还有嵌套 INSERT，实例数量还会继续累积，最终生成的点和线可能远多于块实例数。

修复前，C++ 和 Web 两条 DXF 处理路径都会直接按照文件中的行列数进入双层循环，并持续向输出容器追加实体。递归深度限制只能阻止无限向下递归，无法阻止单层大阵列或多层阵列产生巨量输出。

## 3. 风险分析

不可信或损坏的 DXF 可以使用异常大的行列数触发以下问题：

1. 双层循环长时间占用 CPU，导致 SAM 或浏览器无响应。
2. 展开结果持续写入数组或容器，造成内存耗尽。
3. 嵌套阵列使工作量快速放大，单纯限制递归深度仍不足以保护系统。
4. 如果在超限前已经写入部分实体，调用方可能误用不完整结果，造成静默数据错误。
5. 直接计算超大整数的 `rows × columns`，可能先发生整数溢出，再得到错误的安全判断。

该问题同时影响可用性、正确性、安全性和性能，因此在阶段二审查中被标记为 Critical。

## 4. 修复目标

本次修复遵循以下原则：

- 在进入大循环之前拒绝明显超限的阵列。
- 限制单个阵列，也限制整个文件解析期间的累计展开量。
- Web 与 C++ 提供小图纸、大图纸和 Unlimited 三档；Unlimited 只取消最终输出数量上限。
- 使用不会先发生乘法溢出的判断方式。
- 超限时明确失败，不静默截断结果。
- C++ 失败后不向调用方暴露部分展开数据。
- 正常 DXF 的几何坐标、顺序和类型保持不变。

## 5. 统一资源上限

| 资源 | Web | C++ | 说明 |
| --- | ---: | ---: | --- |
| 单个 INSERT 阵列实例 | 100,000 | 100,000 | 限制单次 `rows × columns` |
| 累计展开块实例 | 100,000 | 100,000 | 包括模型空间 INSERT 和嵌套 INSERT |
| 受预算保护的输出 | 100,000 / 500,000 / Unlimited | 100,000 / 500,000 / Unlimited | 小图纸 / 大图纸 / Unlimited；覆盖模型空间、展开及 Sketch/FE 最终输出 |
| 最大嵌套深度 | 8 | 32 | 保留两端原有深度策略，并将越界改为明确失败 |

Web 与 C++ 使用相同的档位语义。有限档位下，C++ Parser 先限制展平后的中间
实体，Sketch 转换再限制线和圆，FE 转换限制节点与单元之和。Unlimited 将最终
输出上限设为平台可表示的最大值，但仍保留单阵列、累计块实例、嵌套深度和
预分配容量限制，避免“无限档位”绕过结构性输入保护。

## 6. 安全乘法检查

修复没有先执行乘法再比较，而是使用除法检查：

```text
如果 rows > maxInstances / columns，则阵列必然超限
```

只有检查通过后才计算：

```text
count = rows × columns
```

这样可以避免 `rows × columns` 在固定宽度整数中先溢出，再绕过上限判断。

Web 端还要求 `rows` 和 `columns` 同时满足：

- 是安全整数；
- 大于或等于 1；
- 单个阵列乘积不超过 100,000。

## 7. C++ 实现

主要实现位于 `src/Example1/DxfParser.cpp`。

### 7.1 ExpansionBudget

新增内部 `ExpansionBudget`，在一次解析过程中累计记录：

```cpp
struct ExpansionBudget {
    std::uint64_t blockInstances;
    std::size_t entities;
    std::size_t maxOutputEntities;
    QString error;
};
```

它提供两类消费操作：

- `consumeArray()`：检查单个阵列和累计块实例。
- `consumeEntities()`：检查展开产生的实体和离散线段总量。

### 7.2 错误传播

块展开相关函数由无返回值改为返回 `bool`，包括：

- `addTransformedSegments()`
- `expandCurveGroup()`
- `expandInsertArray()`
- `expandSingleBlock()`
- `expandBlocks()`

任一层发现预算超限后立即返回 `false`，错误逐层传播到 `DxfParser::parseFile()`，不再继续展开剩余实体。

### 7.3 失败回滚

解析器发现展开失败时执行：

```cpp
outData.clear();
outData.setError(DxfImportErrorCode::ExpansionLimit, error);
return false;
```

因此调用方得到的状态为：

- `parseFile()` 返回 `false`；
- `DxfData::isValid()` 为 `false`；
- `entityCount()` 为 `0`；
- `errorMessage()` 包含明确的展开限制错误。
- `errorCode()` 为稳定的结构化错误码。

这保证了部分展开结果不会被误认为有效模型。

### 7.4 递归状态清理

递归展开使用局部守卫维护 `visiting` 集合。即使中途因预算超限提前返回，当前块名也会从集合中移除，避免错误路径污染递归状态。

## 8. Web 实现

主要实现位于 `page/dxf.js`。

### 8.1 限制配置

新增只读配置：

```javascript
const EXPANSION_PROFILES = Object.freeze({
  small: { maxOutputEntities: 100000, defaultTolerance: 0.01 },
  large: { maxOutputEntities: 500000, defaultTolerance: 0.05 },
  unlimited: { maxOutputEntities: Infinity, defaultTolerance: 0.05 }
});
```

### 8.2 检查时机

Web 离散过程在以下位置消费预算：

- 进入 INSERT 双层循环前调用 `consumeInsertArray()`。
- 添加点之前调用 `consumeOutput(1)`。
- 添加折线线段前按 `points.length - 1` 调用 `consumeOutput()`。
- 访问深度超过 8 时立即失败。

输出预算覆盖点、直线、圆弧离散线段、椭圆离散线段、样条离散线段和块展开结果，因此无法通过“少量块实例、复杂块内容”绕过保护。

### 8.3 结构化错误

超限时 Web 抛出 `Error`，并设置：

```javascript
error.code = "DXF_EXPANSION_LIMIT";
```

调用方不会获得 `discretize()` 的部分返回值，可以通过错误码区分资源限制错误与普通解析错误。

## 9. 执行流程

```text
读取 INSERT 行列数
        │
        ▼
检查正整数和安全乘法
        │
        ▼
检查单个阵列实例上限
        │
        ▼
检查累计块实例上限
        │
        ▼
逐个展开块实例
        │
        ▼
每次输出前检查实体预算
        │
   ┌────┴────┐
   │         │
预算充足    任一预算超限
   │         │
继续展开    立即失败，不返回部分模型
```

## 10. 自动化测试

### 10.1 C++

新增测试夹具：

- `test/data/block_array_limit.dxf`

该文件声明 100,001 个阵列实例，用于确认解析器在进入长循环前立即失败。

测试 `oversized_insert_array_is_rejected_without_partial_output` 验证：

- `parseFile()` 返回 `false`；
- 数据对象无效；
- 输出实体数量为 0；
- 错误信息包含 `limit`。

### 10.2 Web

新增 `tests/dxf-expansion-limits.test.js`，覆盖：

1. `2 × 3` 正常阵列生成 6 条线。
2. `100001 × 1` 在循环前被拒绝。
3. 非整数行数 `1.5` 被拒绝。
4. 阵列实例未超限、但块内容导致输出超过 100,000 时被拒绝。
5. 大图纸档允许超过 100,000 的输出，并在超过 500,000 时拒绝。
6. 超限错误具有 `DXF_EXPANSION_LIMIT` 错误码。

## 11. 验证结果

本次修复完成后执行：

```powershell
npm.cmd run check
npm.cmd test
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

结果：

- Node.js 语法检查通过。
- Node.js 全部测试通过。
- Release 构建成功。
- CTest 8/8 通过。
- `git diff --check` 通过。

阶段一三个 Web 基准输入的几何指纹均保持不变：

| 文件                         | SHA-256                                                              |
| ---------------------------- | -------------------------------------------------------------------- |
| `block_test_minimal.dxf`   | `E9E68AA652D2A17D2365B559FEA2F6B2027784686C3689471E72D3B53EEDAB19` |
| `slock_spline_zhenlie.dxf` | `143835F9F0EB5EC2770626D8669C81CFBCD36D2EFAF76CA2F21FD5928F62B9AD` |
| `ship2_block_test.dxf`     | `8962F01D707AA1EF8E27A6D7BE06D53FAF49C6FE3550054AF9713FA3157973F9` |

这表明正常基准文件的输出坐标、顺序、图层、来源类型和不支持类型没有因资源预算修复发生变化。

### 11.1 C++ 最终输出保护

- `parseFile()` 用模型空间实体数初始化预算，避免原始实体绕过 Parser 上限。
- `ConversionEngine` 对 Sketch 最终的线和圆计数。
- `FeConversionEngine` 对最终节点与单元之和计数。
- 任一阶段超限均清空半成品，并返回 `ExpansionLimit` 或 `ConversionLimit`。
- GUI 与 Python 入口接受 100,000、500,000 和 `-1`；`-1` 映射为 Unlimited，
  仅取消 Parser/Sketch/FE 最终实体数上限。
- 自动化测试覆盖模型空间加 INSERT 越界、Sketch 离散越界、FE 输出越界，
  以及失败后半成品清空。

## 12. 兼容性与维护说明

1. 正常输入只增加常数时间的计数和比较，不改变几何算法。
2. 有限档位下，超过限制的文件会明确失败，这是预期的安全行为变化。
3. 不应通过简单调高常量处理真实的大文件需求。若业务确实需要超过 100,000 个输出实体，应先评估流式处理、取消机制、进度报告和内存模型。
4. 修改限制值时，必须同步检查 C++、Web、服务端实体限制、测试和本说明文档。
5. 后续可把两端限制提取到共享配置，并统一 C++ 与 Web 的嵌套深度策略。
6. SAM GUI 中的 Sketch/FE 人工验收仍应作为发布前验收步骤执行。
7. Unlimited 是用户显式选择的资源保护降级模式，只应用于可信图纸；导入时应
   监控内存和响应时间，无法接受资源耗尽风险时必须使用有限档位。

## 13. 结论

C-02 的单阵列、嵌套实例和安全乘法始终受保护；有限档位下，Web 最终输出、
C++ 模型空间与 Sketch/FE 最终输出均纳入预算，失败路径不会暴露半成品。
Unlimited 是对最终数量上限的显式选择性退出，不适用于不可信输入。代码和
自动化验证已经完成；在 SAM GUI 使用有限档位边界及 Unlimited 实际大图验收后，
即可完成发布级关闭。
