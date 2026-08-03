# DXF 测试用例说明

本目录是项目中 DXF 文件的唯一数据源，用于验证 DXF 解析、几何转换、BLOCK/INSERT 展开、
异常输入处理以及大型图纸导入的正确性与稳定性。测试文件按用途分为五类，共 55 个 DXF。
代码、测试、脚本和教程应直接引用这里的文件，不要在 `example/` 或其他目录建立内容副本。

## 目录结构

```text
tests/fixtures/dxf/
├── basic_element/   # 基础图元与曲线
├── block/           # BLOCK、INSERT 与嵌套块
├── function/        # CAD 操作、复杂组合与压力场景
├── regression/      # 针对具体缺陷的最小回归用例
└── ship/            # 真实船舶图纸与大型集成测试
```

## 1. `basic_element/`：基础图元测试

这组文件用于确认解析器能够正确读取基础几何实体，并保留坐标、半径、角度、闭合状态、节点、控制点、拟合点和权重等关键数据。

| 文件 | 主要内容 | 测试用途 |
|---|---|---|
| `circle.dxf` | 单个 `CIRCLE` | 验证圆心、半径及整圆转换。 |
| `rounded_corner_arc_lines.dxf` | `ARC` 与直线组合 | 验证带圆角的转角轮廓以及圆弧与直线的连接。 |
| `ellipse.dxf` | 两个 `ELLIPSE` | 验证椭圆中心、长短轴、参数范围和离散化。 |
| `arc_half_circle.dxf` | 单个半圆 `ARC` | 验证半圆的起止角、方向和端点。 |
| `multi_line_segments.dxf` | 多条 `LINE` | 验证多线段图形的读取、坐标和端点连接。 |
| `closed_lines_arc.dxf` | 三条直线与一个圆弧 | 验证直线和圆弧组成的闭合轮廓。 |
| `line_corner.dxf` | 三条相连直线 | 验证折线转角、公共端点和线段连接关系。 |
| `mixed_lines_lwpolyline.dxf` | 多条直线与一个 `LWPOLYLINE` | 验证直线密集轮廓及直线与轻量多段线混合解析。 |
| `nurbs_quarter_circle.dxf` | 有理 `SPLINE` 四分之一圆 | 验证 NURBS 阶次、节点、权重及有理曲线离散化。 |
| `arc_partial.dxf` | 单个局部 `ARC` | 验证非半圆圆弧的角度范围、方向和端点。 |
| `lwpolyline_half_circle.dxf` | 带凸度的 `LWPOLYLINE` | 验证由多段线凸度表示的半圆弧。 |
| `lwpolyline_mixed_segments.dxf` | 含直线段和弧段的 `LWPOLYLINE` | 验证轻量多段线顶点、凸度及复合路径。 |
| `spline_standard.dxf` | 单个标准 `SPLINE` | 验证样条阶次、控制点、节点向量和曲线离散化。 |
| `spline_variant.dxf` | 样条曲线变体 | 验证不同控制点和节点配置下的样条解析。 |
| `spline_closed.dxf` | 闭合 `SPLINE` | 验证闭合标志、首尾连续性和闭合曲线离散化。 |
| `spline_fit_only.dxf` | 仅提供拟合点的 `SPLINE` | 验证缺少常规控制点数据时的拟合点解析路径。 |
| `lwpolyline_closed_square.dxf` | 闭合 `LWPOLYLINE` 正方形 | 验证闭合多段线、顶点顺序和首尾封闭。 |

## 2. `block/`：块定义与引用测试

这组文件用于验证 `BLOCK` 定义、`INSERT` 引用、嵌套展开、插入点、旋转、缩放、图层语义，以及块内曲线和大型图纸中的块处理。

| 文件 | 主要内容 | 测试用途 |
|---|---|---|
| `mixed_entities_block_inserts.dxf` | 多种基础图元组成的块和 8 个 `INSERT` | 验证圆、圆弧、椭圆、直线、多段线和样条进入块后能正确展开。 |
| `nested_block_inserts.dxf` | 块中再次引用其他块 | 验证嵌套块的递归展开、组合变换和终止条件。 |
| `block_curves_transform.dxf` | 包含样条、椭圆的块及多个引用 | 验证块内曲线在插入、旋转或缩放后的几何结果。 |
| `minimal_block_insert.dxf` | 线段块和单个 `INSERT` | 提供最小 BLOCK/INSERT 输入，验证最基础的块定义与展开链路。 |
| `ship_block_expansion_stress.dxf` | 大型船舶图纸、多个块及大量多段线 | 对复杂图纸中的块展开数量、几何完整性、性能和资源控制进行压力测试。 |
| `ship_new_block_integration.dxf` | 船舶图纸中的新增块定义和引用 | 验证大型既有图纸加入新块后，解析结果和其他实体不受影响。 |

## 3. `function/`：CAD 操作与复杂场景测试

这组文件来自常见 CAD 编辑结果，用于验证镜像、缩放、阵列、倒角/圆角、重复点合并以及多级组合操作后的 DXF 数据。

| 文件 | 主要内容 | 测试用途 |
|---|---|---|
| `merge_points_stress.dxf` | 216 条直线和 200 个点 | 压测大量相同或邻近端点的合并、去重和线段连接逻辑。 |
| `mirror_mixed_entities.dxf` | 镜像后的基础图元和块引用 | 验证镜像变换后的坐标、方向、顶点顺序和块缩放符号。 |
| `rounded_profile.dxf` | 带倒角/圆角的组合轮廓和块引用 | 验证轮廓转角处理及圆弧、样条、椭圆与直线的组合。 |
| `rounded_profile_array.dxf` | 倒角/圆角轮廓的阵列 | 验证组合轮廓经阵列复制后产生的多个 `INSERT` 及其变换。 |
| `spline_profile_array_stress.dxf` | 含样条轮廓的密集阵列 | 验证大量块引用、样条离散化以及阵列展开的性能和稳定性。 |
| `linear_profile_block_inserts.dxf` | 线段块和多个块引用 | 验证同一线性轮廓通过多个 `INSERT` 组合后的展开结果。 |
| `linear_profile_block_variant.dxf` | 线性块轮廓的几何布局变体 | 用于对比不同插入位置或组合方式下的块变换结果。 |
| `simplified_linear_profile.dxf` | 简化的线性轮廓变体 | 验证块定义存在但实例化几何较少时的解析和统计行为。 |
| `scale_mixed_entities.dxf` | 整体缩放后的混合图元和块 | 验证缩放对坐标、半径、曲线控制数据和 `INSERT` 比例的影响。 |
| `array_block_instances.dxf` | 矩形或环形阵列生成的多个块引用 | 验证阵列数量、插入位置、旋转角度及重复实例展开。 |

## 4. `regression/`：最小回归测试

这组文件只保留复现特定问题所需的最少实体，适合在单元测试中快速运行。修改解析器、块展开或几何转换代码后，应优先执行这些用例。

| 文件 | 主要内容 | 测试用途 |
|---|---|---|
| `block_array_limit.dxf` | 带阵列参数的单个块引用 | 验证阵列展开预算和数量上限，防止异常参数造成过量实例。 |
| `block_array_rotated.dxf` | 带旋转的阵列块 | 验证阵列偏移与旋转组合时的实例坐标。 |
| `block_layer_inheritance.dxf` | 多层块、多个图层和 4 个引用 | 验证块内 layer 0、显式图层及嵌套引用的图层继承规则。 |
| `block_mirrored_curves.dxf` | 镜像缩放下的圆弧和多段线块 | 验证负比例缩放后的圆弧方向、凸度符号和顶点顺序。 |
| `block_nested_nonuniform.dxf` | 两层嵌套块和非均匀缩放 | 验证嵌套变换矩阵组合以及非均匀缩放的处理策略。 |
| `block_rotated_curves.dxf` | 含圆弧、椭圆和样条的旋转块 | 验证不同曲线实体应用块旋转后的坐标和参数。 |
| `block_stats_nonuniform.dxf` | 非均匀缩放块中的多种实体 | 验证展开前后实体统计、过滤数量和转换结果的一致性。 |
| `invalid_entities_only.dxf` | 仅含无效或不可转换实体数据 | 验证错误数值、无效几何被跳过，并正确报告“无有效实体”。 |
| `point_only.dxf` | 两个 `POINT` | 验证只含点实体时的解析、统计和无可绘制几何处理。 |

## 5. `ship/`：大型船舶图纸测试

这组文件用于集成、兼容性、性能和内存稳定性测试，覆盖多图层、大量实体、文字、标注、填充、复杂多段线、样条及块引用。

| 文件 | 主要内容 | 测试用途 |
|---|---|---|
| `ship_basic_mixed_entities.dxf` | 约 1 MB 的基础船舶图纸 | 验证大量直线、圆弧、文字、标注、实体填充和少量样条的综合导入。 |
| `ship_polyline_block_dense.dxf` | 约 3.6 MB、含大量 `LWPOLYLINE` 和块引用 | 验证多段线密集型船舶图纸的解析性能、块处理和几何完整性。 |
| `ship_hatch_polyline_mixed.dxf` | 约 1.1 MB、包含 `HATCH` 和传统 `POLYLINE` | 验证填充边界、传统多段线、块和混合实体的兼容性。 |
| `ship_full_scale_stress.dxf` | 约 10.6 MB 的完整大型综合图纸 | 作为最高负载用例，验证大型文件的耗时、内存、稳定性和完整导入能力。 |
| `ship_full_scale_segment_01.dxf` | 大型综合图纸的第 1 个分段 | 在较小数据量下独立验证大型图纸第 1 分段的实体组合。 |
| `ship_full_scale_segment_02.dxf` | 大型综合图纸的第 2 个分段 | 独立验证第 2 分段中的多图层、文字、标注和几何实体。 |
| `ship_full_scale_segment_03.dxf` | 大型综合图纸的第 3 个分段 | 独立验证第 3 分段的解析结果，便于定位完整图导入差异。 |
| `ship_full_scale_segment_04.dxf` | 大型综合图纸的第 4 个分段 | 独立验证第 4 分段的实体统计、边界范围和转换完整性。 |
| `ship_full_scale_segment_05.dxf` | 大型综合图纸的第 5 个分段 | 独立验证第 5 分段中的复杂多段线、块和文字数据。 |
| `ship_full_scale_segment_06.dxf` | 大型综合图纸的第 6 个分段 | 独立验证第 6 分段的兼容性、性能和异常实体处理。 |
| `ship_full_scale_segment_07.dxf` | 大型综合图纸的第 7 个分段 | 独立验证第 7 分段，并与完整大型综合图纸的导入结果交叉检查。 |
| `ship_alternate_design.dxf` | 另一型约 2 MB 的综合船舶图纸 | 验证不同图纸风格、图层组织和实体组合下的兼容性。 |
| `ship_medium_integration.dxf` | 约 1.4 MB 的简化船舶图纸 | 用于中等规模的快速集成回归，覆盖填充、块、多段线、文字和样条。 |

## 建议测试顺序

1. 先运行 `regression/`，快速确认已修复问题没有复发。
2. 再运行 `basic_element/`，确认各类几何实体能正确解析和转换。
3. 运行 `block/` 与 `function/`，验证组合变换、块展开和复杂操作。
4. 最后运行 `ship/`，检查真实大型图纸下的正确性、性能和稳定性。

测试结果建议至少记录：解析是否成功、各类型实体数量、跳过/失败实体数量、展开后几何数量、边界范围、处理耗时和峰值内存。
