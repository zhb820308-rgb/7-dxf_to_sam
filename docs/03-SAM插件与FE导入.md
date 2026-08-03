# SAM 插件与 FE 导入

## 1. 入口

构建后有两个必须配套部署的产物：

| 产物 | 作用 |
|---|---|
| `Example1.pyd` | SAM Python 模块、DXF 编排、转换和构建实现 |
| `SAM.Pre.Example1Toolset.dll` | Qt 菜单、导入对话框和日志查看器 |

重启 SAM 后可以从 `File → Import → DXF...` 打开导入对话框。工具菜单中的
`DXFimport log` 打开日志查看器。

## 2. 导入参数

GUI 和 Python 绑定最终构造同一个 `DxfImportRequest`：

| 参数 | 默认值 | 说明 |
|---|---:|---|
| `filePath` | 必填 | DXF 文件路径 |
| `baseX/baseY/baseZ` | `0` | 加到输出坐标的基点偏移 |
| `curveTolerance` | `0.01` | 曲线弦高公差，范围 `1e-12`～`1000` |
| `ignoreLayers` | 空 | 逗号分隔的忽略图层 |
| `importMode` | 空，即 Sketch | `Sketch` 或 `FiniteElement`，不区分大小写 |
| `modelName` | 空 | FE 模式必填，目标模型必须存在 |
| `partName` | 空 | FE 模式必填，新 Part 名称 |
| `nodeMergeTolerance` | `1e-6` | FE 邻近节点合并公差 |
| `maxOutputEntities` | `100000` | 只接受 `100000`、`500000` 或 `-1` |

Python/SAM 方法名为 `Example1.importDxf`。底层参数顺序是文件路径和 X/Y/Z，其他参数
为可选命名参数。GUI 使用 `omuMethodCall("Example1", "importDxf", args)` 调用同一入口。
成功返回创建数量，失败返回空并通过日志/界面报告原因。

## 3. 图纸档位

| 档位 | 最终输出上限 | GUI 默认曲线公差 |
|---|---:|---:|
| Small | 100,000 | 0.01 |
| Large | 500,000 | 0.05 |
| Unlimited | 无最终数量上限 | 0.05 |

Unlimited 只取消最终输出数量限制，不取消 INSERT 单阵列、累计实例和递归深度保护。

## 4. Sketch 模式

```text
DxfData
 → ConversionEngine
 → SamData(lines, circles)
 → SamBuilder.beginImport
 → createLines / createCircles
 → commit
```

每次导入创建带时间戳的独立 Sketch。提交时重新 Fetch 最新 MDB，避免长时间几何构建
覆盖并发的模型变化；Repository Replace 成功后才进入已提交状态。场景刷新和 PDO 重建
在持久化提交之后执行，显示失败不会错误撤销已经完整保存的数据。

## 5. Finite Element 模式

```text
DxfData
 → FeConversionEngine
 → FeData(nodes, trusses)
 → PythonFiniteElementBuilder.beginImport(model, part)
 → createNodes / createTrusses
 → commit
```

FE 模式要求 `modelName` 和 `partName` 非空。节点先创建，Truss 后创建；节点 ID 与
单元引用在提交前完成校验。Python 命令中的模型名和 Part 名经过字符串转义。

邻近节点使用三维空间索引按 `nodeMergeTolerance` 合并；重复或零长度 Truss 被跳过并计入统计。

## 6. 事务和回滚

Sketch 与 FE Builder 共用状态机：

```text
Idle → Preparing → Writing → Committing → Committed
                    └────────→ RollingBack → RolledBack
```

规则：

- begin 失败时不进入创建阶段；
- 任一创建阶段失败或取消时停止后续阶段；
- commit 失败执行补偿删除；
- rollback 可重试且对已回滚状态幂等；
- 已提交事务不能再回滚；
- 原始失败和 rollback 失败分别保存，避免清理错误覆盖根因；
- Builder 析构时如果仍拥有资源，会再次尝试清理。

## 7. 进度和取消

解析/转换和创建阶段通过回调更新进度。GUI 事件处理有单次 25 ms 上限，创建大批几何时
按批次检查取消，避免每个实体都触发 UI 事件。取消结果使用独立状态，不通过错误文本猜测。

取消后的验收要求：

- 不提交部分 Sketch 或 FE Part；
- Repository 中不存在同名半成品；
- 同名重试可以成功；
- 已有 Repository 对象不受影响。

## 8. 日志

日志优先写入 SAM 应用目录：

```text
logs/imports/dxf_import_<id>.log
logs/dxf_import_errors.log
```

- 每次导入使用独立 ID 和独立日志，最多保留最近 50 份；
- 错误汇总日志按 10 MiB 轮转，保留 5 份；
- 如果应用目录不可写，日志模块回退到用户可写位置；
- 日志查看器按批次读取快照，可停止加载、筛选级别和实体，并安全关闭重开。

## 9. 人工验收

发布前至少覆盖：

1. 小型精确图：核对坐标、方向和创建数量；
2. 块/阵列/镜像/曲线综合图：比较 Sketch 与 FE 轮廓；
3. 大型船舶图：检查完成、取消、内存和连续导入；
4. 中文、日文、扩展字符和空格路径；
5. 失败/取消后同名重试及既有对象保护。

推荐输入见[样例与测试数据](08-样例与测试数据.md)。
