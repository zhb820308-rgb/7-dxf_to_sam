# DXF 最小回归夹具

这里存放由自动化测试直接引用的小型 DXF 文件。它们通常只保留复现某个解析、BLOCK/INSERT 展开或异常输入问题所需的最少内容，不用于人工展示。

| 文件 | 覆盖场景 |
|---|---|
| `block_array_limit.dxf` | INSERT 阵列展开预算上限 |
| `block_array_rotated.dxf` | 旋转阵列 |
| `block_layer_inheritance.dxf` | 块与图层继承语义 |
| `block_mirrored_curves.dxf` | 镜像块中的曲线 |
| `block_nested_nonuniform.dxf` | 嵌套块与非均匀缩放 |
| `block_rotated_curves.dxf` | 旋转块中的曲线 |
| `block_stats_nonuniform.dxf` | 非均匀缩放下的统计 |
| `invalid_entities_only.dxf` | 仅含无效实体的容错路径 |
| `point_only.dxf` | 仅 POINT 实体的最小输入 |

这些文件已同步到统一测试库的 `regression/` 分类。文件名和当前路径保持不变，以免破坏 C++ 与 JavaScript 测试引用。
