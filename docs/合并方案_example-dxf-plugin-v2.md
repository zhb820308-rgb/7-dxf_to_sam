# 合并方案：example-dxf-plugin ← example-dxf-plugin-v2

**日期**: 2026-07-29
**目标**: 将 `origin/example-dxf-plugin-v2` 的样条曲线支持合并到 `example-dxf-plugin`

---

## 一、分支概况

```
共同祖先: 1638108 (fix: address code review findings + add test infrastructure)

example-dxf-plugin (当前)            example-dxf-plugin-v2 (来源)
─────────────────────────────        ─────────────────────────────
d883820 refactor: importDxf          7727d56 添加样条曲线支持和测试DXF文件
  拆分+模板+P3修复                      86eadb3 DXF导入SAM插件：完整代码
d5fe005 Merge dxf-addlog              616bc3e refactor: defer curve tessellation
4f39d5d 添加设置精度功能                 fb4736b docs:更新数据接口文档
caa8565 Add DXF import logging          ...
33f58e9 Merge dxf-addlog
```

---

## 二、冲突文件清单

### 2.1 直接冲突（git 标记 <<<<<<）

| 文件                            | v2 侧                                | 当前侧                                  | 冲突原因             |
| ------------------------------- | ------------------------------------ | --------------------------------------- | -------------------- |
| `CMakeLists.txt`              | 添加 OCCT 路径定义                   | 改 SAM 路径 + 添加 spdlog               | 相邻行修改           |
| `src/Example1/CMakeLists.txt` | 添加 OCCT include/lib、C++17、TK* 库 | 添加 Qt5Gui、spdlog、fmt DLL 复制       | 相邻行修改           |
| `src/Example1/DxfParser.h`    | `addSpline` stub `{}`→`;`     | 整个`DxfReader` 类移入 .cpp           | 同一文件完全不同内容 |
| `src/Example1/DxfParser.cpp`  | 添加 ~380 行 spline 代码             | 完全重构：`DxfReader`→匿名 namespace | 同一文件完全不同内容 |

### 2.2 无冲突但需关注

| 文件                        | 说明                              |
| --------------------------- | --------------------------------- |
| `src/Example1/DebugLog.h` | v2 新增，合并后需要存在           |
| `example/OCCT/`           | v2 新增 OCCT 库头文件 (~800 .hxx) |
| `example/*.dxf`           | v2 新增测试用 DXF 文件            |
| `.gitignore`              | v2 修改                           |

---

## 三、合并策略总览

```
Phase 1: git merge origin/example-dxf-plugin-v2
           │
           ▼ (4 个冲突文件)
Phase 2: 手动解决冲突
           │
           ├─ 2.1 CMakeLists.txt (根)         → 取双方并集
           ├─ 2.2 CMakeLists.txt (Example1)   → 取双方并集
           ├─ 2.3 DxfParser.h                 → 用当前版本 (17行)
           └─ 2.4 DxfParser.cpp               → 手动合并
                    │
                    ▼
Phase 3: 架构重构 — spline 代码归位
           │
           ├─ 3.1 DxfData.h/.cpp   → 新增 DxfSpline 实体类
           ├─ 3.2 DxfParser.cpp    → addSpline() 只存原始数据
           ├─ 3.3 GeometryUtils    → 新增 tessellateSpline()
           ├─ 3.4 ConversionEngine → convert() 加 spline 调度
           └─ 3.5 清理             → 删除 SplinePoint3、remove calculateSegmentCount
                    │
                    ▼
Phase 4: 构建验证
           │
           ├─ cmake --build
           └─ ctest
```

---

## 四、Phase 2：冲突解决详解

### 4.1 `CMakeLists.txt`（根目录）

**原则**: 取双方并集

```cmake
# === 当前侧保留 ===
set(LIBS_SAM_ROOT "D:/shixiSoftware/SAM" CACHE PATH ...)
set(LIBS_SAMSDK_ROOT "D:/shixiSoftware/OpenOLTranSim-main/SAMSDK" CACHE PATH ...)
set(LIBDXFRW_ROOT "D:/shixiSoftware/Homework/env/libdxfrw" CACHE PATH ...)
set(SPDLOG_ROOT "D:/vcpkg/installed/x64-windows" CACHE PATH ...)
set(LIBS_OCCT_ROOT ${LIBS_SAMSDK_ROOT}/ThridPartys/OCCT)          # 保留
set(spdlog_DIR "${SPDLOG_ROOT}/share/spdlog")
set(fmt_DIR "${SPDLOG_ROOT}/share/fmt")
find_package(spdlog CONFIG REQUIRED)

# === v2 侧新增 ===
set(SAM_OCCT_ROOT "D:/shixi_CAE/example/example/OCCT")
set(SAM_OCCT_INCLUDE_DIR "${SAM_OCCT_ROOT}/inc")
set(SAM_OCCT_LIB_DIR "${SAM_OCCT_ROOT}/win64/vc14/lib")
set(SAM_OCCT_BIN_DIR "${SAM_OCCT_ROOT}/win64/vc14/bin")
```

### 4.2 `src/Example1/CMakeLists.txt`

**原则**: 取双方并集

| 来自 | 内容                                                     |
| ---- | -------------------------------------------------------- |
| v2   | `include_directories(${SAM_OCCT_INCLUDE_DIR})`         |
| v2   | `link_directories(${SAM_OCCT_LIB_DIR})`                |
| v2   | `CXX_STANDARD 17` + `CXX_STANDARD_REQUIRED ON`       |
| v2   | 链接`TKernel TKMath TKG2d TKG3d TKGeomBase TKGeomAlgo` |
| v2   | 格式化修正（tab→space）                                 |
| 当前 | `include_directories(${LIBS_QT5_ROOT}/include/QtGui)`  |
| 当前 | 链接`Qt5Gui spdlog::spdlog`                            |
| 当前 | POST_BUILD 复制 spdlog/fmt DLL                           |
| 当前 | 保留`LIBS_TCMALLOC_ROOT` 和 `libtcmalloc_minimal`    |

### 4.3 `src/Example1/DxfParser.h`

**原则**: 完全使用当前版本

当前头文件已是最优形态（17 行，`DxfReader` 在 .cpp 匿名 namespace）。v2 的 `addSpline` stub 改动无意义——整个类都不在头文件了。

```
结果: 头文件不变。后续 Phase 3 新增 DxfSpline 时也只在 DxfData.h 中声明。
```

### 4.4 `src/Example1/DxfParser.cpp`

**原则**: 以当前结构为骨架，注入 v2 的 spline 功能

当前结构 (`src/Example1/DxfParser.cpp`):

```
#include + 匿名 namespace {
    class DxfReader : public DRW_Interface {
        // 5 个已实现的回调 + ~40 个 stub
    };
    // addLine, addCircle, addArc, addEllipse, addLWPolyline 实现
}  // namespace
DxfParser::parseFile() { ... }
```

**从 v2 注入的内容：**

| 优先级    | 注入项                            | 位置                                 |
| --------- | --------------------------------- | ------------------------------------ |
| ✅ 必需   | `#include "DebugLog.h"`         | include 区                           |
| ✅ 必需   | OCCT includes (12 个)             | include 区                           |
| ✅ 必需   | `DxfReader::addSpline()` 声明   | 类定义内（替换 stub`{}` → `;`） |
| ✅ 必需   | `DxfReader::addSpline()` 实现   | 功能实现区                           |
| ❌ 跳过   | `calculateSegmentCount()`       | 与 GeometryUtils 重复，不引入        |
| ⚠️ 暂留 | `SplinePoint3` + 4 个离散化函数 | 留到 Phase 3 迁移到 GeometryUtils    |

> v2 的 `addSpline()` 当前会直接离散化成线段放入 `m_data`。Phase 3 会改为只存原始数据到 `DxfSpline`，离散化延迟到 `ConversionEngine`。

---

## 五、Phase 3：架构重构 — Spline 代码归位

### 5.1 当前问题

v2 在 `DxfParser::addSpline()` 中同时做了三件事：

1. 解析 DXF spline 数据
2. 构建 OCCT B-spline 曲线
3. 离散化成线段

这违反了现有管道架构：

```
正确: Parser(存原始数据) → Engine(调GeometryUtils离散化) → SamData
错误: Parser(存原始数据+离散化) → 丢失原始几何信息
```

### 5.2 目标架构

```
DxfParser::addSpline()
  → 验证数据
  → 存储到 DxfData::addSpline(DxfSpline)     ← 只存原始数据

ConversionEngine::convert()
  → for (spline : dxfData.splines())
      addSegments(GeometryUtils::tessellateSpline(spline, tolerance))

GeometryUtils::
  tessellateSpline(DxfSpline, tolerance) → vector<DxfLine>
    ├─ buildCurveFromControlData()    // 控制点 → OCCT Geom_BSplineCurve
    ├─ buildCurveFromFitPoints()      // 拟合点 → 插值 B-spline
    ├─ discretizeByDeflection()       // B-spline → 采样点 → DxfLine 段
    └─ 闭合性检查 + 端点 snap
```

### 5.3 具体改动

#### A. DxfData.h — 新增 `DxfSpline` 实体类

```cpp
// ---- DxfSpline ----
// B-spline curve: control points, knots, weights, fit points

class DxfSpline : public DxfEntity {
public:
    DxfSpline();
    DxfSpline(const std::vector<DxfPoint>& ctrlPts,
              const std::vector<double>& knots,
              const std::vector<double>& weights,
              const std::vector<DxfPoint>& fitPts,
              int degree, int flags,
              double tgStartX, double tgStartY, double tgStartZ,
              double tgEndX, double tgEndY, double tgEndZ);

    const std::vector<DxfPoint>& controlPoints() const;
    const std::vector<double>&   knots()          const;
    const std::vector<double>&   weights()        const;
    const std::vector<DxfPoint>& fitPoints()       const;
    int    degree()      const;
    int    flags()        const;    // bit 0=closed, 1=periodic, 2=rational
    bool   isRational()  const;
    bool   isPeriodic()  const;
    bool   isClosed()    const;
    // tangents
    double tgStartX() const; double tgStartY() const; double tgStartZ() const;
    double tgEndX()   const; double tgEndY()   const; double tgEndZ()   const;

    bool isValid() const override;

private:
    std::vector<DxfPoint> m_ctrlPts;
    std::vector<double>   m_knots;
    std::vector<double>   m_weights;
    std::vector<DxfPoint> m_fitPts;
    int    m_degree = 0;
    int    m_flags  = 0;
    double m_tgStartX = 0, m_tgStartY = 0, m_tgStartZ = 0;
    double m_tgEndX   = 0, m_tgEndY   = 0, m_tgEndZ   = 0;
};
```

`DxfData` 容器新增：

```cpp
void addSpline(const DxfSpline& spline);
const std::vector<DxfSpline>& splines() const;
```

#### B. DxfParser.cpp — `addSpline()` 简化

只做验证 + 存储，不离散化：

```cpp
void DxfReader::addSpline(const DRW_Spline* data)
{
    // 1. 验证（复用 v2 的验证逻辑）
    if (!data) return;
    // ... 验证控制点/拟合点/节点/权重 ...

    // 2. 搬运数据到 DxfSpline
    std::vector<DxfPoint> ctrlPts;
    for (int i = 0; i < data->ncontrol; ++i)
        ctrlPts.push_back(DxfPoint(/* ... */));
    // ... 同样搬运 knots, weights, fitPts ...

    // 3. 存储（不离散化）
    m_data.addSpline(DxfSpline(ctrlPts, knots, weights, fitPts,
                                data->degree, data->flags, ...));
}
```

#### C. GeometryUtils.h/.cpp — 新增 `tessellateSpline()`

将 v2 的 4 个离散化函数从 `DxfParser.cpp` 的匿名 namespace 迁移到 `GeometryUtils` namespace：

| v2 原函数                       | GeometryUtils 中名称     | 说明                             |
| ------------------------------- | ------------------------ | -------------------------------- |
| `discretizeByDeflection()`    | 同名                     | `SplinePoint3` → `DxfPoint` |
| `buildCurveFromControlData()` | 同名                     | 不变                             |
| `buildCurveFromFitPoints()`   | 同名                     | 不变                             |
| `buildSplineOCCT()`           | →`tessellateSpline()` | 改名，统一入口                   |

公开接口：

```cpp
namespace GeometryUtils {
    /// Discretize a B-spline curve into line segments via OCCT.
    /// Uses chord-height adaptive sampling for smooth curves.
    std::vector<DxfLine> tessellateSpline(const DxfSpline& spline,
                                          double tolerance);
}
```

**关键改动**: 所有 `SplinePoint3` → `DxfPoint`，直接输出 `DxfPoint` 采样点。

#### D. ConversionEngine.cpp — 调度 spline 离散化

在 `convert()` 末尾加一个 for 循环（与 arc/lwpolyline/ellipse 并列）：

```cpp
// --- splines (OCCT B-spline 构建 + 离散化 + 平移) ---
for (const DxfSpline& spline : dxfData.splines()) {
    if (!spline.isValid()) continue;
    addSegments(GeometryUtils::tessellateSpline(spline, tolerance));
}
```

#### E. 清理

| 删除项                                  | 原因                                                 |
| --------------------------------------- | ---------------------------------------------------- |
| `SplinePoint3` struct                 | 用`DxfPoint` 替代                                  |
| `calculateSegmentCount()`             | 与`GeometryUtils::calculateArcSegmentCount()` 重复 |
| 4 个 spline 函数在 DxfParser.cpp 的定义 | 已迁移到 GeometryUtils                               |
| `DxfParser.cpp` 的 OCCT includes      | 移到 GeometryUtils.cpp                               |
| `DxfParser.cpp` 中 spline 离散化逻辑  | 只保留验证+存储                                      |

---

## 六、文件变更汇总

| 文件                                  | Phase 2 (合并)              | Phase 3 (重构)             | 最终变更    |
| ------------------------------------- | --------------------------- | -------------------------- | ----------- |
| `CMakeLists.txt` (根)               | 合并 OCCT 路径 + spdlog     | —                         | 双方并集    |
| `src/Example1/CMakeLists.txt`       | 合并 OCCT + Qt5Gui + spdlog | —                         | 双方并集    |
| `src/Example1/DxfParser.h`          | 用当前版本 (17行)           | —                         | 不变        |
| `src/Example1/DxfParser.cpp`        | 注入 addSpline()            | 简化：只存不散             | ~40 行新增  |
| `src/Example1/DxfData.h`            | —                          | 新增`DxfSpline` 类       | ~50 行新增  |
| `src/Example1/DxfData.cpp`          | —                          | 新增实现                   | ~40 行新增  |
| `src/Example1/GeometryUtils.h`      | —                          | 新增`tessellateSpline()` | 1 行声明    |
| `src/Example1/GeometryUtils.cpp`    | —                          | 新增 4 个离散化函数        | ~280 行新增 |
| `src/Example1/ConversionEngine.cpp` | —                          | 加 spline 调度循环         | ~5 行新增   |
| `src/Example1/DebugLog.h`           | v2 新文件                   | —                         | 引入        |
| `example/OCCT/`                     | v2 新目录                   | —                         | 引入        |
| `example/*.dxf`                     | v2 新文件                   | —                         | 引入        |
| `.gitignore`                        | v2 修改                     | —                         | 保留        |

---

## 七、测试验证

```
cd build
cmake --build . --config Release --target ALL_BUILD
ctest --test-dir build -C Release

预期: 100% tests passed out of 6
```

如需验证 spline 功能，使用 v2 附带的样条曲线 DXF 测试文件。

---

## 八、风险与回滚

| 风险                  | 缓解                                                                   |
| --------------------- | ---------------------------------------------------------------------- |
| OCCT 路径不存在       | Phase 2 构建前确认`D:/shixi_CAE/example/example/OCCT` 或设为对应路径 |
| C++17 编译错误        | v2 已设置`CXX_STANDARD 17`，当前代码 C++14 兼容 C++17                |
| DebugLog.h 依赖缺失   | v2 包含此文件，确认存在                                                |
| DxfSpline 序列化/测试 | 现有测试不依赖 spline，新增代码不影响旧测试                            |

**回滚**: `git merge --abort` 可撤销 Phase 1 的 merge。Phase 3 在 merge 完成后是独立提交，可单独 revert。

---

*Generated with Claude Code on 2026-07-29*
