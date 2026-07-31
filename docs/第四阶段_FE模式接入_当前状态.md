# Example1 DXF 导入 FiniteElement 模式 — 当前最终状态

> 最后更新：2026-07-31
> 工作目录：`D:\shixi_CAE\example`
> 新 AI 窗口应先阅读本文再继续开发。

---

## 1. 最终结论

**DXF 导入为 FE Part（节点 + T3D2 Truss）已实现，并已在 SAMCAE 内嵌 Python 与 SAM GUI 中验收通过。**

当前生产路径不是 C++ 直接操作 `ptoKPartRepository::Insert()`，而是由 C++ 调用 SAM kernel 内部 Python 解释器，执行已验证的官方 Python 建模 API：

```text
DXF
→ DxfParser
→ DxfData
→ FeConversionEngine
→ FeData
→ PythonFiniteElementBuilder
→ mdl.Part()
→ p.createNode()
→ p.Element(... samConstants.TRUSS ...)
→ Viewport.setValues(displayedObject=p)
→ Viewport.view.fitView()
```

官方 Python 路径会正确更新：

- `mdb.models[modelName].parts`；
- Model Tree / Part Manager；
- 顶部 `Model:`、`Part:` 下拉框；
- 当前 Part 上下文；
- Viewport；
- 节点和 T3D2 杆单元显示。

---

## 2. 已验收结果

### 2.1 最小 3 节点、2 Truss Part

测试：

```python
import Example1

Example1.createTestTrussPart(
    modelName='Model-1',
    partName='Python_FE_Test'
)
```

SAM 实测输出：

```text
1 node created, create node completed.   # 共 3 次
1 elements of type TRUSS be created       # 共 2 次
Create element completed.
1
```

数据验证：

```python
p = mdb.models['Model-1'].parts['Python_FE_Test']
print(len(p.nodes))
print(len(p.elements))
for el in p.elements:
    print(el.label, el.type, el.connectivity)
```

实测：

```text
3
2
(1, T3D2, (0, 1))
(2, T3D2, (1, 2))
```

### 2.2 DXF 导入 FE Part

测试：

```python
result = Example1.importDxf(
    r'D:\shixi_CAE\example\example\line.dxf',
    0.0, 0.0, 0.0,
    importMode='FiniteElement',
    modelName='Model-1',
    partName='DXF_Python_Line'
)
print(result)
```

实测：

```text
22 次：1 node created, create node completed.
22 次：1 elements of type TRUSS be created
返回值：44
```

即：

```text
22 节点 + 22 个 T3D2 Truss = 44
```

---

## 3. 对外 Python API

### 3.1 Sketch 模式（原有功能）

```python
Example1.importDxf(r'D:\path\drawing.dxf', 0.0, 0.0, 0.0)
```

默认：

```text
importMode='Sketch'
```

路径：

```text
DxfParser → ConversionEngine → SamData → SamBuilder
```

### 3.2 FiniteElement 模式（当前生产路径）

```python
Example1.importDxf(
    r'D:\path\drawing.dxf',
    0.0, 0.0, 0.0,
    importMode='FiniteElement',
    modelName='Model-1',
    partName='MyFEPart',
    nodeMergeTolerance=1.0e-6
)
```

| 参数 | 默认值 | 含义 |
|---|---:|---|
| `filePath` | 必填 | DXF 文件路径 |
| `baseX/Y/Z` | 必填 | 输出坐标 = DXF 坐标 + base |
| `curveTolerance` | 现有默认值 | 曲线离散容差 |
| `ignoreLayers` | `''` | 逗号分隔的忽略图层 |
| `importMode` | `'Sketch'` | `'Sketch'` / `'FiniteElement'` |
| `modelName` | `''` | FE 模式必填，通常为 `'Model-1'` |
| `partName` | `''` | FE 模式必填且在该 Model 内必须唯一 |
| `nodeMergeTolerance` | `1e-6` | 节点合并容差 |

成功返回：

```text
节点数 + Truss 数
```

失败返回：`None`。

---

## 4. 当前生产实现

### 4.1 `PythonFiniteElementBuilder`

文件：

```text
src\Example1\PythonFiniteElementBuilder.h
src\Example1\PythonFiniteElementBuilder.cpp
```

这是当前 FE 生产 Builder。它通过参考工程 `ContainerShipSection` 使用过的 kernel-side API 执行 Python：

```cpp
pytInterpreterRole::Instance().RunCommand(command, false);
```

创建 Part：

```python
import samConstants
_example1_fe_model = mdb.models['Model-1']
_example1_fe_part = _example1_fe_model.Part(name='MyFEPart')
```

创建节点：

```python
_example1_fe_part.createNode(x=..., y=..., z=...)
```

创建 Truss：

```python
_example1_fe_part.Element(
    nodes=(
        _example1_fe_part.nodes[startNodeId],
        _example1_fe_part.nodes[endNodeId]),
    elemShape=samConstants.TRUSS,
    intersectNodes=False)
```

显示并适配视图：

```python
_example1_fe_vp = session.viewports['Viewport: 1']
_example1_fe_vp.setValues(displayedObject=_example1_fe_part)
_example1_fe_vp.view.fitView()
```

`Example1.importDxf(..., importMode='FiniteElement')` 和 `Example1.createTestTrussPart()` 都使用该 Builder。

### 4.2 `FeConversionEngine` 与 `FeData`

文件：

```text
src\Example1\FeConversionEngine.h/.cpp
src\Example1\FeData.h/.cpp
```

转换规则：

| DXF 图元 | FE 输出 |
|---|---|
| `POINT` | 单独节点，无单元 |
| `LINE` | 两节点 + 一个 Truss |
| `CIRCLE` | 闭合离散 Truss 链 |
| `ARC` / `LWPOLYLINE` / `ELLIPSE` / `SPLINE` | 离散线段 + Trusses |

特性：坐标偏移、节点合并、零长度过滤、重复 Truss 过滤均已实现。

### 4.3 `FiniteElementBuilder`（保留，非生产路径）

文件：

```text
src\Example1\FiniteElementBuilder.h/.cpp
```

该 C++ SDK 对照实现已验证：

```text
omeMesh + mesSetMesh
AppendNode
CreateElementsV2(TRUSS)
```

`omeMesh + mesSetMesh` 来自参考工程：

```text
D:\shixi_CAE\example\ContainerShipSection\CSSFragment.cpp:361-423
```

它解决了 orphan mesh 在 Viewport 不绘制的问题。

但 C++ 直接 `ptoKPartRepository::Insert()` 不会让新 Part 进入 Part Manager / 顶部 Part 下拉框，所以该类**不再作为生产创建 Part 路径**。保留它用于 SDK 对照和回退研究，勿删除。

---

## 5. 已验证的 SAM Python 事实

参考：

```text
D:\xwechat_files\wxid_glbd6f4u09f622_272e\msg\file\2026-07\sam_python_verified_commands.md
```

本项目额外实测：

```python
import samConstants
hasattr(samConstants, 'TRUSS')  # True
hasattr(samConstants, 'BEAM')   # True
```

以下官方 API 在 SAMCAE 内嵌 Python 中已验证：

```python
mdl = mdb.Model(name='PythonTrussProbeModel')
p = mdl.Part(name='PythonTrussProbePart')
p.createNode(x=0.0, y=0.0, z=0.0)
p.createNode(x=10.0, y=0.0, z=0.0)
p.Element(
    nodes=(p.nodes[0], p.nodes[1]),
    elemShape=samConstants.TRUSS,
    intersectNodes=False)
```

输出：

```text
1 elements of type TRUSS be created
Create element completed.
(1, T3D2, (0, 1))
```

---

## 6. 当前限制

### 6.1 失败后的 Part 回滚

`PythonFiniteElementBuilder` 在 Part 创建后发生节点/单元失败时，不调用未经验证的删除 API；因此可能留下部分创建的同名 Part。

当前处理：人工删除同名 Part 后重试。

后续需要验证稳定删除路线，例如：

```python
del mdb.models['Model-1'].parts['PartName']
```

### 6.2 固定 Viewport 名

当前使用：

```python
session.viewports['Viewport: 1']
```

这是当前验收环境中的 Viewport 名称。其他环境如果名称不同，需要改为获取当前 Viewport 或将其参数化。

### 6.3 性能

当前对每一个节点和 Truss 单独执行一次 `RunCommand()`。优点：错误定位准确，保留逐项进度与取消。缺点：大 DXF 可能较慢。

批量优化需要先在 SAMCAE 中验证多行/批量 Python API，不要先猜测接口。

### 6.4 GUI 对话框尚未支持 FE 参数

`src\Example1Toolset\Example1DXFImportDialog.*` 当前只发送 Sketch 参数：

```text
filePath, baseX, baseY, baseZ, curveTolerance, ignoreLayers
```

目前 FE 通过 Python 控制台调用。后续 GUI 需要增加：

```text
Import Mode: Sketch / FiniteElement
Model Name
Part Name
Node Merge Tolerance
```

这项完成后必须重新编译 `SAM.Pre.Example1Toolset.dll`。

---

## 7. 核心文件

```text
D:\shixi_CAE\example\
├── src\Example1\
│   ├── Example1PytModule.h/.cpp             # Python 入口、模式分流
│   ├── PythonFiniteElementBuilder.h/.cpp    # 当前 FE 生产路径
│   ├── FiniteElementBuilder.h/.cpp          # C++ SDK 对照路径，非生产
│   ├── FeData.h/.cpp                        # 节点/Truss 数据、去重、统计
│   ├── FeConversionEngine.h/.cpp            # DxfData → FeData
│   ├── DxfParser.h/.cpp                     # DXF 解析
│   ├── ConversionEngine.h/.cpp              # Sketch 转换
│   ├── SamBuilder.h/.cpp                    # Sketch 建模
│   └── DxfImportLogger.h/.cpp               # 日志
├── src\Example1Toolset\                    # GUI DLL，暂未支持 FE 参数
├── example\                                # DXF 样例
├── bin\Release\Example1.pyd                # 部署文件
└── build\                                  # CMake 构建目录
```

---

## 8. 构建与部署

```powershell
cmake -S D:\shixi_CAE\example -B D:\shixi_CAE\example\build
cmake --build D:\shixi_CAE\example\build --config Release --target Example1
```

当前成功构建产物：

```text
D:\shixi_CAE\example\bin\Release\Example1.pyd
最后成功编译：2026-07-31 14:57:39
```

部署：完全退出 SAM → 覆盖实际加载的 `Example1.pyd` → 重启 SAM → 使用未重复的 Part 名测试。

---

## 9. 下一步

| 优先级 | 任务 | 状态 |
|---|---|---|
| P0 | 最小 FE Part、节点、T3D2、DXF FE 导入、SAM GUI 生命周期 | **已通过** |
| P1 | GUI 对话框增加 FE 模式、Model/Part/节点合并容差 | 未开始 |
| P1 | 验证并实现 Python Builder 失败时安全删除 Part | 未开始 |
| P2 | 批量化 Python 建模提升大型 DXF 性能 | 未开始 |
| P2 | Material、Truss Section、截面积 | 未开始 |
| P3 | Assembly、BC、Load、Step、Job | 未开始 |

---

## 10. 新 AI 接手约束

1. **不要**把生产 FE 路径改回 C++ `ptoKPartRepository::Insert()`；该路线不更新 Part Manager。
2. **不要**在 kernel/Python 扩展中调用 GUI 的 `cmdGCommandDeliveryRole::SendCommand()`；此前会崩溃。
3. 当前生产关键是：`mdl.Part()`、`createNode()`、`Element(... samConstants.TRUSS, intersectNodes=False)`。
4. 新增 `.cpp` 后，CMake `file(GLOB ...)` 不会自动更新已有工程；必须重新运行 `cmake -S ... -B ...` 再构建。
