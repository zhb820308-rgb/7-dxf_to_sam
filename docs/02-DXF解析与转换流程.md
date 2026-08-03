# DXF 解析与转换流程

## 1. 总览

```text
文件路径
 → DxfInputFile
 → libdxfrw / DxfReaderCallbacks
 → 顶层实体、BLOCK 定义、INSERT 列表
 → DxfBlockExpansion
 → DxfData
 → Sketch 或 FE 转换
```

`DxfParser::parseFile()` 成功表示至少存在可接受的输入实体且展开未超限。失败时
`DxfData` 携带结构化错误码和消息，不允许把部分输出继续提交。

## 2. 输入与实体模型

活动数据模型支持：

| DXF 类型 | `DxfData` 表示 | 后续处理 |
|---|---|---|
| `POINT` | `DxfPoint` | Sketch 当前不创建点；FE 创建节点 |
| `LINE` | `DxfLine` | Sketch 线或 FE Truss |
| `CIRCLE` | `DxfCircle` | Sketch 圆；FE 离散为 Truss |
| `ARC` | `DxfArc` | 按弦高公差离散 |
| `LWPOLYLINE` | `DxfLWPolyline` | 直线段直接输出，bulge 段离散 |
| `ELLIPSE` | `DxfEllipse` | 参数曲线离散 |
| `SPLINE` | `DxfSpline` | OCCT B 样条构造和自适应离散 |
| `BLOCK/INSERT` | `DxfBlock` / `InsertInfo` | 展开为带有效图层的普通几何 |

`TEXT`、`MTEXT`、`HATCH`、`SOLID`、`VIEWPORT` 等当前不转为 Sketch/FE 几何。
解析器会跳过或统计不支持/无效实体，避免它们被误认为可提交输出。

所有几何公共合法性检查都会拒绝 NaN/Inf、非正半径、退化线、无效样条数据等输入。

## 3. Unicode 文件路径

libdxfrw 接口使用窄字符串。`DxfInputFile` 在 Windows 代码页无法表示原路径时创建
受控临时目录和 ASCII 暂存副本，解析结束后通过对象生命周期自动清理。解析器和 GUI
图层扫描共享这一路径适配，避免两条入口行为不同。

## 4. BLOCK/INSERT 展开

每个 INSERT 使用二维仿射矩阵表示：

```text
父变换 × 插入平移 × 旋转 × 缩放 × 块基点平移
```

嵌套 INSERT 通过矩阵乘法组合，不把旋转和缩放重新拆成近似参数。实现同时处理：

- 非零块基点；
- 行列阵列偏移；
- 统一和非均匀缩放；
- X/Y/双轴镜像；
- Z 平移和缩放；
- 循环块、未知块和最大深度；
- layer 0 继承 INSERT 有效图层，显式子图层保持自身语义。

当平面变换保持圆形时，可保留圆和圆弧参数；非均匀缩放会先按公差离散，再对线段
应用完整矩阵。镜像会翻转圆弧方向和多段线 bulge 符号。椭圆主轴和样条切向量按向量
变换，不能附加平移。

## 5. 曲线离散

曲线公差表示曲线与折线之间允许的最大弦高偏差：

- 默认：`0.01`；
- Large/Unlimited 默认：`0.05`；
- 接受范围：`1e-12` 到 `1000`；
- 圆弧离散同时限制单段最大角度；
- 样条优先使用控制点、节点和权重；只有拟合点时走拟合曲线路径；
- 首尾足够接近的闭合样条会吸附闭合；
- 零长度离散段不会进入最终输出。

## 6. Sketch 转换

`ConversionEngine`：

1. 验证基点、公差和输出上限；
2. 对实体应用基点平移；
3. 保留直线和圆；
4. 把圆弧、多段线、椭圆和样条离散为线；
5. 生成 `SamData`；
6. 超限或没有可创建几何时返回错误并清空部分输出。

## 7. FE 转换

`FeConversionEngine` 把点和离散线段转换为：

- `FeNode`：从 `0` 开始的连续节点 ID；
- `FeTruss`：连接两个不同节点，端点顺序规范化；
- 空间索引：在 `nodeMergeTolerance` 内复用已有节点；
- Truss 索引：忽略方向检测重复单元。

默认节点合并公差为 `1e-6`。取消、输出超限或转换失败时清空 `FeData`，不会把部分
节点或单元交给 Builder。

## 8. Web 几何指纹

几何指纹用于判断 Web 解析和离散行为是否变化。脚本按确定顺序序列化：

- 点的 `x、y、layer、sourceType`；
- 线的 `x1、y1、x2、y2、layer、sourceType`；
- 不支持实体统计。

随后计算 SHA-256。坐标、顺序、图层、来源类型或不支持实体变化都会改变指纹。
指纹变化不必然是错误，但必须有明确原因和坐标级测试。

运行方式：

```powershell
npm.cmd run test:geometry-baseline
```
