# DXF 演示与集成样例

本目录保存项目长期维护的 28 个 DXF 样例。文件直接放在目录根部，是为了保持现有 C++/JavaScript 测试、几何基线脚本和 SAM GUI 验收文档中的路径稳定。

## 如何选择

| 目标 | 建议文件 |
|---|---|
| 最快验证一次导入 | `circle.dxf` 或 `block_test_minimal.dxf` |
| 基础直线/圆弧/闭合轮廓 | `line.dxf`、`line_and_circle_closed.dxf`、`square.dxf` |
| 椭圆与样条 | `elipse.dxf`、`spline.dxf`、`nurbs_quarter_circle.dxf` |
| BLOCK/INSERT 变换 | `block_spline.dxf`、`ship2_new_block.dxf` |
| 阵列与大量展开 | `slock_spline_zhenlie.dxf` |
| 大型船舶图纸 | `ship2.dxf`、`ship2_block_test.dxf`、`ship4_2.dxf` |

## 文件分类

### 基础图元与曲线（15）

`circle.dxf`、`circle_coner.dxf`、`elipse.dxf`、`half_circle_up.dxf`、`part_circle_down.dxf`、`line.dxf`、`line1.dxf`、`line_and_circle_closed.dxf`、`pline_half_circle.dxf`、`nurbs_quarter_circle.dxf`、`spline.dxf`、`spline_2.dxf`、`spline_close.dxf`、`spline_fit_only.dxf`、`square.dxf`。

### BLOCK/INSERT（4）

`block_spline.dxf`、`block_test_minimal.dxf`、`ship2_block_test.dxf`、`ship2_new_block.dxf`。

### CAD 操作与压力场景（7）

`many_merge_points_lines.dxf`、`slock1.dxf`、`slock1_2.dxf`、`slock1_5.dxf`、`slock_daojiao.dxf`、`slock_daojiao_zhenlie.dxf`、`slock_spline_zhenlie.dxf`。

### 船舶集成图（2）

`ship2.dxf`、`ship4_2.dxf`。

## 自动验证中的固定样例

几何基线脚本固定检查以下三个代表性输入：

| 文件 | 覆盖范围 |
|---|---|
| `block_test_minimal.dxf` | 最小 BLOCK/INSERT 展开 |
| `slock_spline_zhenlie.dxf` | 样条轮廓和密集阵列 |
| `ship2_block_test.dxf` | 大型船舶图纸与块展开 |

运行：

```powershell
npm.cmd run test:geometry-baseline
```

指纹变化不必然表示错误，但必须结合实现变更解释并人工检查边界、数量和代表性几何后，才能更新基线。

## 与完整测试库的关系

完整的 55 文件测试库位于相邻 OpenOLTranSim 工作区的 `Community-plugins/io-plugins/DXFtoSam/dxf_test/`。两处同内容文件通过 SHA-256 对应，完整映射和分类见 [`../docs/08-样例与测试数据.md`](../docs/08-样例与测试数据.md)。该外部目录不是本项目的运行依赖。

针对单一缺陷的更小输入放在 [`../test/data/`](../test/data/README.md)，不应为了展示用途复制到本目录。
