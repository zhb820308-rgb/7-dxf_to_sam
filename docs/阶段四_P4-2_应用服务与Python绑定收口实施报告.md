# 阶段四 P4-2：应用服务与 Python 绑定收口实施报告

## 1. 实施结论

P4-2 已完成代码实现、Release 构建和自动化测试，当前状态为：

> 统一请求对象、参数默认值、导入预检和 Python/GUI 入口已完成收口；非法数值、非法模式和缺失 FE 名称会在读取 DXF 前返回；等待用户执行 SAM GUI/Python 兼容性验收。未执行本批次 Git 提交或推送。

## 2. 修改前的问题

1. `DxfImportRequest` 定义在编排器头文件中，数据对象与流程入口耦合。
2. Python 绑定创建请求后，还要从两个转换器中分别读取默认容差。
3. GUI 中重复写有 `0.01`、`0.05`、`1e-12`、`1000`、`100000`、`500000` 和 `-1`。
4. 参数校验发生在解析前，但模式和 FE 名称校验发生在 DXF 解析后。
5. `Example1PytModule.h` 的说明仍只描述 Sketch 流程。

## 3. 主要实现

### 3.1 统一默认值

新增：

```text
src/Example1/DxfImportDefaults.h
```

集中定义：

| 参数 | 默认值 |
|---|---:|
| 默认曲线容差 | `0.01` |
| Large/Unlimited 推荐曲线容差 | `0.05` |
| 节点合并容差 | `1e-6` |
| 最小曲线容差 | `1e-12` |
| 最大曲线容差 | `1000` |
| Small 输出上限 | `100000` |
| Large 输出上限 | `500000` |
| Unlimited | `-1` |

以下模块现在引用同一来源：

- Python 请求默认值；
- Sketch 转换器默认容差；
- FE 转换器默认节点合并容差；
- 参数校验；
- SAM GUI Profile；
- SAM GUI 容差输入范围。

### 3.2 独立请求对象

新增：

```text
src/Example1/DxfImportRequest.h
```

`DxfImportRequest` 不再定义在编排器中。构造默认请求时已自动获得正确的曲线容差、节点合并容差和 Small 输出上限。

### 3.3 导入预检服务

新增：

```text
src/Example1/DxfImportPreflight.h
src/Example1/DxfImportPreflight.cpp
```

预检顺序：

```text
数值与 Profile 校验
→ 导入模式校验
→ FE Model/Part 名称校验
→ 计算最终输出上限
→ 才允许读取 DXF
```

预检结果包含：

```text
valid
mode
outputLimit
stage
detail
message
```

### 3.4 编排器顺序调整

以前：

```text
数值校验
→ 解析 DXF
→ 模式/FE 名称校验
→ 转换和创建
```

现在：

```text
统一预检
→ 图层参数解析
→ 解析 DXF
→ 按预检模式转换和创建
```

因此，非法 `importMode` 或缺失 FE 名称不会再读取大文件后才报错。

### 3.5 Python 绑定收口

`Example1PytModule::importDxf()` 现在只负责：

1. 创建具有规范默认值的 `DxfImportRequest`；
2. 读取必填和可选 Python 参数；
3. 调用 `runDxfImport()`；
4. 将结构化结果转换为既有 Python 返回值。

绑定层不再依赖 `ConversionEngine` 和 `FeConversionEngine` 来获取默认值。

Python 返回兼容性保持不变：

- 成功返回创建数量；
- 失败或取消返回 `None`。

### 3.6 GUI 参数统一

SAM GUI 现在使用统一常量设置：

- Small、Large、Unlimited 的实际值；
- 默认容差；
- Profile 切换后的推荐容差；
- 容差输入验证范围。

GUI 与后端不再分别维护同一批数字。

## 4. 涉及文件

### 4.1 新增

```text
src/Example1/DxfImportDefaults.h
src/Example1/DxfImportRequest.h
src/Example1/DxfImportPreflight.h
src/Example1/DxfImportPreflight.cpp
docs/阶段四_P4-2_应用服务与Python绑定收口实施报告.md
```

### 4.2 修改

```text
src/Example1/CMakeLists.txt
src/Example1/ConversionEngine.cpp
src/Example1/ConversionEngine.h
src/Example1/FeConversionEngine.cpp
src/Example1/FeConversionEngine.h
src/Example1/DxfImportValidation.cpp
src/Example1/DxfImportValidation.h
src/Example1/DxfParser.cpp
src/Example1/DxfImportOrchestrator.h
src/Example1/DxfImportOrchestrator.cpp
src/Example1/Example1PytModule.h
src/Example1/Example1PytModule.cpp
src/Example1Toolset/Example1DXFImportDialog.cpp
test/test_dxf_import_mode.cpp
test/test_dxf_import_validation.cpp
```

## 5. 自动化测试

新增/增强用例覆盖：

- 默认请求使用规范默认值；
- 省略模式时默认选择 Sketch；
- 默认输出上限为 Small；
- 非法模式在预检阶段失败；
- FE 模式依次检查 Model 和 Part 名称；
- 数值校验在模式校验之前执行；
- 原有模式大小写兼容行为保持不变。

## 6. 自动化验证结果

| 验证项 | 结果 |
|---|---|
| VS2017 x64 Release 构建 | 通过 |
| `Example1.pyd` | 生成成功 |
| `SAM.Pre.Example1Toolset.dll` | 生成成功 |
| CTest | 14/14 通过 |
| `DxfImportMode`/Preflight 测试 | 通过 |
| `npm.cmd run check` | 通过 |
| Node 测试 | 8/8 通过 |
| `git diff --check` | 通过 |

构建警告仍来自 SAM Python 2.7 和 Qtitan 第三方头文件，本批次未引入新的编译错误。

## 7. 代码审查结论

### Correctness

- 默认参数值与修改前一致。
- 模式/名称校验提前，但成功导入路径没有改变。
- Python 返回约定没有改变。

### Readability

- 请求、默认值和预检职责已经独立命名。
- Python 绑定不再了解转换器默认值。

### Architecture

依赖方向为：

```text
GUI/Python 入口
→ DxfImportRequest
→ DxfImportPreflight
→ DxfImportOrchestrator
→ Parser / Converter / BuildService
```

### Security

- 非法输入更早被拒绝。
- 未增加网络、密钥、动态执行或第三方依赖。

### Performance

- 非法模式和缺失 FE 名称不再触发 DXF 文件解析。
- 正常导入只增加一次轻量预检，没有改变几何循环。

审查结论：未发现新的 Critical/Required 问题；等待 SAM 入口兼容性验收。

## 8. 用户测试方法

### 8.1 自动化测试

```powershell
cmake --build build_stage4_vs2017 --config Release --parallel 2
ctest --test-dir build_stage4_vs2017 -C Release --output-on-failure
ctest --test-dir build_stage4_vs2017 -C Release `
  -R "DxfImportMode" --output-on-failure
npm.cmd run check
npm.cmd test
git diff --check
```

预期：

- Release 构建成功；
- CTest 14/14 通过；
- `DxfImportMode` 测试通过；
- Node 8/8 通过。

### 8.2 GUI 默认值

部署最新：

```text
bin/Release/Example1.pyd
bin/Release/SAM.Pre.Example1Toolset.dll
```

重启 SAM 并打开 DXF Import。

检查：

- [ ] 默认 Profile 为 Small。
- [ ] 默认容差为 `0.01`。
- [ ] 切换 Large 后容差变为 `0.05`。
- [ ] 切换 Unlimited 后容差保持/变为 `0.05`。
- [ ] 切回 Small 后容差变回 `0.01`。
- [ ] Sketch 模式下 Model/Part 输入框禁用。
- [ ] FE 模式下 Model/Part 输入框启用。

### 8.3 Python 默认参数兼容

在 SAM Python 控制台执行：

```python
import Example1
path = r'D:\shixiSoftware\Homework\7-dxf_to_sam\example\block_test_minimal.dxf'
result = Example1.importDxf(path, 0.0, 0.0, 0.0)
print(result)
```

预期：

- 不传任何可选参数时仍按 Sketch 导入；
- 返回 `3`；
- 生成 3 条直线；
- 日志显示 `curve_tolerance=0.01`、`max_output_entities=100000`。

### 8.4 非法模式在读文件前拒绝

故意使用不存在的文件，同时传入非法模式：

```python
result = Example1.importDxf(
    r'D:\not-exist\large-file.dxf',
    0.0, 0.0, 0.0,
    importMode='Mesh')
print(result)
```

预期：

- 返回 `None`；
- 日志错误阶段是 `validate_mode`；
- 错误内容是 unsupported `importMode 'Mesh'`；
- 不应先报文件不存在或 DXF 读取失败；
- 不创建任何 Sketch/Part。

### 8.5 FE 名称在读文件前校验

```python
result = Example1.importDxf(
    r'D:\not-exist\large-file.dxf',
    0.0, 0.0, 0.0,
    importMode='FiniteElement',
    modelName='',
    partName='')
print(result)
```

预期：

- 返回 `None`；
- 先提示缺少 `modelName`；
- 不报 DXF 文件读取失败；
- 不创建 Part。

### 8.6 正常 FE 导入

```text
文件：example/block_test_minimal.dxf
Profile：Small drawing
Mode：Finite Element
Model：Model-1
Part：P4_2_Defaults_FE
Base：0, 0, 0
Tolerance：0.01
```

预期：

- 导入成功；
- 生成 5 个节点和 3 个 Truss；
- 日志中的 Profile 上限和容差与界面一致。

## 9. 验收记录

| 项目 | 结果 | 备注 |
|---|---|---|
| Release 构建 | 通过 | 自动化已完成 |
| CTest 14/14 | 通过 | 自动化已完成 |
| Node 8/8 | 通过 | 自动化已完成 |
| GUI Profile 默认值 | 待用户验收 | |
| GUI 容差联动 | 待用户验收 | |
| Python 省略可选参数 | 待用户验收 | |
| 非法模式提前失败 | 待用户验收 | |
| FE 名称提前失败 | 待用户验收 | |
| 正常 FE 导入 | 待用户验收 | |
| Git 提交/推送 | 未执行 | 等待用户确认 |

验收通过后回复：

```text
P4-2 通过
```

收到确认后，才会提交并上传本批次允许的文件，然后开始 P4-3。
