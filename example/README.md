# DXF 样例入口

项目的所有 DXF 已统一存放在 [`../tests/fixtures/dxf/`](../tests/fixtures/dxf/)。本目录只保留
这份入口说明，不再保存数据副本，避免同一图纸以不同名字重复维护。

## 如何选择

| 目标 | 建议文件 |
|---|---|
| 最快验证一次导入 | [`circle.dxf`](../tests/fixtures/dxf/basic_element/circle.dxf) |
| 最小 BLOCK/INSERT | [`minimal_block_insert.dxf`](../tests/fixtures/dxf/block/minimal_block_insert.dxf) |
| 椭圆与样条 | [`ellipse.dxf`](../tests/fixtures/dxf/basic_element/ellipse.dxf)、[`spline_standard.dxf`](../tests/fixtures/dxf/basic_element/spline_standard.dxf) |
| 阵列与大量展开 | [`spline_profile_array_stress.dxf`](../tests/fixtures/dxf/function/spline_profile_array_stress.dxf) |
| 大型块展开 | [`ship_block_expansion_stress.dxf`](../tests/fixtures/dxf/block/ship_block_expansion_stress.dxf) |
| 大型船舶图纸 | [`ship_full_scale_stress.dxf`](../tests/fixtures/dxf/ship/ship_full_scale_stress.dxf) |

完整的分类、用途和维护规则见
[`../tests/fixtures/dxf/README.md`](../tests/fixtures/dxf/README.md) 与
[`../docs/08-样例与测试数据.md`](../docs/08-样例与测试数据.md)。

## 几何基线

几何基线脚本直接读取统一数据源：

```powershell
npm.cmd run test:geometry-baseline
```

指纹变化不必然表示错误，但必须结合实现变更检查实体数量、边界和代表性几何后才能更新。
