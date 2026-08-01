# DxfData 数据结构说明

> 本文档描述 `src/Example1/DxfData.h` 中所有 DXF 导入数据结构的字段含义、设计原因、合法性校验规则及相互关系。

---

## 目录

1. [概述](#1-概述)
2. [EntityType 枚举](#2-entitytype-枚举)
3. [DxfEntity（抽象基类）](#3-dxfentity抽象基类)
4. [DxfPoint（点）](#4-dxfpoint点)
5. [DxfLine（直线）](#5-dxfline直线)
6. [DxfCircle（圆）](#6-dxfcircle圆)
7. [DxfArc（圆弧）](#7-dxfarc圆弧)
8. [DxfEllipse（椭圆/椭圆弧）](#8-dxfellipse椭圆椭圆弧)
9. [DxfLWPolyline（轻量多段线）](#9-dxflwpolyline轻量多段线)
10. [DxfSpline（B 样条曲线）](#10-dxfsplineb-样条曲线)
11. [SplineKind / SplineConstruction](#11-splinekind--splineconstruction)
12. [DxfEntityStats（实体统计）](#12-dxfentitystats实体统计)
13. [InsertInfo（插入变换信息）](#13-insertinfo插入变换信息)
14. [DxfBlock（块定义）](#14-dxfblock块定义)
15. [DxfData（顶层容器）](#15-dxfdata顶层容器)
16. [Block 展开与嵌套 INSERT](#16-block-展开与嵌套-insert)
17. [总体数据流](#17-总体数据流)

---

## 1. 概述

`DxfData.h` 是整个 DXF 导入流程的**核心数据层**。所有从 DXF 文件读取的几何实体都按该文件中定义的结构存储，后续由转换引擎（普通 SAM 模式 / FE 有限元模式）和几何工具类读取并做离散化或建模。

设计原则：

- **保真优先**：尽量保留 DXF 原始参数（圆弧参数、椭圆参数方程、B 样条控制点/节点/权重、bulge 等），不提前离散。
- **类型分组存储**：不使用 `vector<Base*>` + `dynamic_cast`，而是按实体类型独立 `vector`，提高性能和代码可读性。
- **延迟离散**：离散化（tessellation）在 `GeometryUtils` 中按需进行，精度由容差参数控制，可适配不同转换模式。

---

## 2. EntityType 枚举

```cpp
enum class EntityType {
    Point,
    Line,
    Circle,
    Arc,
    LWPolyline,
    Ellipse,
    Spline,
};
```

| 枚举值 | 对应 DXF 实体 | 对应的 struct/class |
|---|---|---|
| `Point` | `POINT` | `DxfPoint` |
| `Line` | `LINE` | `DxfLine` |
| `Circle` | `CIRCLE` | `DxfCircle` |
| `Arc` | `ARC` | `DxfArc` |
| `LWPolyline` | `LWPOLYLINE` | `DxfLWPolyline` |
| `Ellipse` | `ELLIPSE` | `DxfEllipse` |
| `Spline` | `SPLINE` | `DxfSpline` |

---

## 3. DxfEntity（抽象基类）

```cpp
class DxfEntity {
public:
    DxfEntity(EntityType entityType);
    virtual ~DxfEntity() = default;

    int getId() const { return id; }
    EntityType getType() const { return type; }

    virtual bool isValid() const = 0;
private:
    int id;
    EntityType type;
};
```

每个实体都有一个全局自增 ID 和类型枚举。`isValid()` 由各派生类重写，用于在导入后、展开后、转换前过滤无效数据。

---

## 4. DxfPoint（点）

```cpp
class DxfPoint : public DxfEntity {
public:
    DxfPoint();
    DxfPoint(double ix, double iy, double iz);

    double x() const;
    double y() const;
    double z() const;

    void setX(double v);
    void setY(double v);
    void setZ(double v);

    bool isValid() const override;
};
```

| 字段 | 含义 |
|---|---|
| `m_x, m_y, m_z` | 三维坐标 |

合法条件：三个坐标均为有限值（`std::isfinite`）。

`DxfPoint` 被 Line 的端点、Circle 的圆心等直接存储为成员变量，也在 LWPolyline 和 Spline 等结构中以 `vector` 形式存储。

---

## 5. DxfLine（直线）

```cpp
class DxfLine : public DxfEntity {
public:
    DxfLine();
    DxfLine(const DxfPoint& start, const DxfPoint& end);

    const DxfPoint& start() const;
    const DxfPoint& end()   const;

    bool isValid() const override;
};
```

| 字段 | 含义 |
|---|---|
| `m_start` | 起点 |
| `m_end` | 终点 |

合法条件：起点和终点均有合法坐标，且不重合。

---

## 6. DxfCircle（圆）

```cpp
class DxfCircle : public DxfEntity {
public:
    DxfCircle();
    DxfCircle(const DxfPoint& center, double radius);

    const DxfPoint& center() const;
    double radius() const;

    bool isValid() const override;
};
```

| 字段 | 含义 |
|---|---|
| `m_center` | 圆心 |
| `m_radius` | 半径 |

合法条件：圆心坐标合法，半径 > 0 且有限。

---

## 7. DxfArc（圆弧）

```cpp
class DxfArc : public DxfEntity {
public:
    DxfArc();
    DxfArc(const DxfPoint& center, double radius,
           double startAngle, double endAngle, bool isCCW);

    const DxfPoint& center()    const;
    double radius()             const;
    double startAngle()         const;
    double endAngle()           const;
    bool   isCCW()              const;

    bool isValid() const override;
};
```

| 字段 | 含义 |
|---|---|
| `m_center` | 圆心 |
| `m_radius` | 半径 |
| `m_startAngle` | 起始角度（弧度） |
| `m_endAngle` | 结束角度（弧度） |
| `m_isCCW` | 参数增大方向是否为逆时针 |

合法条件：圆心坐标合法，半径 > 0 且有限，起止角度有限且不相等。

> **与圆的区别**：圆弧是圆的一段，用起止角度限定范围。与椭圆弧的区别是圆弧的轴比固定为 1。

---

## 8. DxfEllipse（椭圆/椭圆弧）

```cpp
class DxfEllipse : public DxfEntity {
public:
    DxfEllipse();
    DxfEllipse(const DxfPoint& center, const DxfPoint& majorAxisEnd,
               double ratio, double startParam, double endParam, bool isCCW);

    const DxfPoint& center()       const;
    const DxfPoint& majorAxisEnd() const;
    double ratio()                 const;
    double startParam()            const;
    double endParam()              const;
    bool   isCCW()                 const;

    bool isValid() const override;
};
```

| 字段 | 含义 |
|---|---|
| `m_center` | 椭圆中心 `C` |
| `m_majorAxisEnd` | **相对中心的**长轴端点向量 `A`（不是世界坐标） |
| `m_ratio` | 短轴与长轴之比 `b/a` |
| `m_startParam` | 起始参数 `t0`（弧度），用于椭圆弧 |
| `m_endParam` | 结束参数 `t1`（弧度） |
| `m_isCCW` | 参数增大方向 |

### 参数方程

```text
短轴向量 B = rotate90°(A) × ratio

P(t) = C + A·cos(t) + B·sin(t)
```

### 为什么 majorAxisEnd 是相对向量？

DXF 对 ELLIPSE 的定义中 `secPoint`（轴端点）是相对于中心的。这样设计的好处是：

- 一个 `DxfPoint` 同时携带长轴**长度**（模长 `a`）和**方向**（倾角），无需额外存储 `majorRadius` 和 `rotation`；
- 完全对应 DXF 原始数据模型；
- 离散化时直接代入参数方程，无需重复计算方向角。

合法条件：长轴向量长度 > 0，轴比 > 0 且有限，起止参数有限且不相等。

---

## 9. DxfLWPolyline（轻量多段线）

```cpp
class DxfLWPolyline : public DxfEntity {
public:
    DxfLWPolyline();
    DxfLWPolyline(const std::vector<DxfPoint>& vertices,
                  const std::vector<double>& bulges,
                  bool closed, double constZ = 0.0);

    const std::vector<DxfPoint>& vertices()   const;
    const std::vector<double>&   bulges()     const;
    bool   isClosed() const;
    double constZ()   const;
    int    vertexCount() const;

    bool isValid() const override;
};
```

| 字段 | 含义 |
|---|---|
| `m_vertices` | 顶点序列（世界坐标） |
| `m_bulges` | 每段圆弧的 bulge 值（见下方说明） |
| `m_closed` | 是否闭合 |
| `m_constZ` | 整条多段线的统一高程 |

### bulge 定义

```text
bulge = tan(θ / 4)
```

其中 `θ` 是圆弧段的圆心角（带符号）：

- `bulge == 0`：直线段
- `bulge > 0`：逆时针弯曲
- `bulge < 0`：顺时针弯曲
- `|bulge|` 越大，弯曲越厉害

### bulges 与 vertices 的数量关系

| 是否闭合 | 段数 | `m_bulges.size()` |
|---|---|---|
| 开放 | `N - 1` | `N - 1` |
| 闭合 | `N` | `N`（最后一个是闭合段） |

合法条件：顶点数 ≥ 2，bulges 数量与闭合策略匹配，所有 bulge 值有限。

---

## 10. DxfSpline（B 样条曲线）

```cpp
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

    // 控制点方式
    const std::vector<DxfPoint>& controlPoints() const;
    const std::vector<double>&   knots()          const;
    const std::vector<double>&   weights()        const;
    int    degree()      const;
    bool   isRational()  const;  // (flags & 4) != 0
    bool   isPeriodic()  const;  // (flags & 2) != 0
    bool   isClosed()    const;  // (flags & 1) != 0

    // 拟合点方式（fallback）
    const std::vector<DxfPoint>& fitPoints() const;

    // 起止切线
    double tgStartX/Y/Z() const;
    double tgEndX/Y/Z()   const;

    SplineKind kind() const;
    bool isValid() const override;
};
```

| 字段 | 含义 | 用途 |
|---|---|---|
| `m_ctrlPts` | 控制点 `Pi` | 控制点多边形，定义 B 样条形状；通常不在曲线上 |
| `m_knots` | 节点向量 `U` | 决定每段基函数的参数区间 |
| `m_degree` | 次数 `p` | 通常 `p=3` 为三次 B 样条 |
| `m_weights` | 权重 `wi` | 仅 `isRational() == true` 时有效，产生 NURBS |
| `m_fitPts` | 拟合点 | 当控制点数据不完整时的降级备选 |
| `m_flags` | 位标志 | bit0=closed, bit1=periodic, bit2=rational |
| `tg*` | 首尾切线向量 | 拟合插值时约束首尾方向 |

### flags 位定义

| 掩码 | 方法 | 含义 |
|---|---|---|
| `1` | `isClosed()` | 几何闭合 |
| `2` | `isPeriodic()` | 参数周期化（比 closed 有更强的连续性约束） |
| `4` | `isRational()` | 有理 B 样条（NURBS），必须使用 weights |

### 两个分支：控制点优先，拟合点兜底

```cpp
if (hasControlData) {
    curve = buildCurveFromControlData(spline);  // 标准 B-Spline / NURBS
} else if (hasFitData) {
    curve = buildCurveFromFitPoints(spline);    // OCCT GeomAPI_Interpolate
}
```

控制点合法的条件：

- `ncontrol > degree`
- `knots.size() == ncontrol + degree + 1`
- 节点非递减
- 如有理，所有权重 > 0

拟合点合法的条件：

- `fitPoints.size() >= 2`

### 起止切向量

只在拟合点路径且曲线不为周期时使用：

```cpp
if (tgStart.Magnitude() > threshold && tgEnd.Magnitude() > threshold)
    interpolator.Load(tgStart, tgEnd);
```

它们不是位置，是方向向量（一阶导数），使插值曲线在首尾有明确走向。

---

## 11. SplineKind / SplineConstruction

```cpp
enum class SplineConstruction {
    ControlBased,   // 由控制点/节点定义
    FitBased,       // 由拟合点插值重建
};

struct SplineKind {
    SplineConstruction construction;
    bool rational;
    bool periodic;
    bool closed;
};
```

用于统计和日志分类，不影响几何计算本身。

---

## 12. DxfEntityStats（实体统计）

```cpp
struct DxfEntityStats {
    std::size_t lines = 0;
    std::size_t lwPolylines = 0;
    std::size_t circles = 0;
    std::size_t arcs = 0;
    std::size_t ellipses = 0;
    std::map<SplineKind, std::size_t> splineKinds;

    std::size_t splineCount() const;   // 各类样条总数
    std::size_t curveCount() const;    // 含曲线属性的实体总数
};
```

用于导入日志记录各实体数量，便于调试和文件分析。记录的是纯模型空间实体（不包含隐藏在 Block 定义中尚未展开的实体）。

---

## 13. InsertInfo（插入变换信息）

```cpp
struct InsertInfo {
    std::string blockName;
    double insertX = 0, insertY = 0, insertZ = 0;
    double scaleX  = 1, scaleY  = 1, scaleZ  = 1;
    double angle   = 0;  // radians
    int    colCount = 1, rowCount = 1;
    double colSpace = 0, rowSpace = 0;
};
```

| 字段 | 含义 |
|---|---|
| `blockName` | 引用的 BLOCK 名称 |
| `insertX/Y/Z` | 插入位置（世界坐标） |
| `scaleX/Y/Z` | X/Y/Z 方向的缩放因子 |
| `angle` | 绕 Z 轴旋转角（弧度） |
| `colCount/rowCount` | 阵列列数/行数 |
| `colSpace/rowSpace` | 阵列列间距/行间距 |

变换公式（模型空间）：

```text
P_world = T_insert × R_angle × S_scale × (P_local - Base_block)
```

---

## 14. DxfBlock（块定义）

```cpp
class DxfBlock {
public:
    const std::string& name()  const;
    double baseX/Y/Z() const;

    void setName(const std::string& n);
    void setBase(double x, double y, double z);

    // 块内各类型实体
    const std::vector<DxfPoint>&      points()      const;
    const std::vector<DxfLine>&       lines()       const;
    const std::vector<DxfCircle>&     circles()     const;
    const std::vector<DxfArc>&        arcs()        const;
    const std::vector<DxfLWPolyline>& lwPolylines() const;
    const std::vector<DxfEllipse>&    ellipses()    const;
    const std::vector<DxfSpline>&     splines()     const;

    // 嵌套 INSERT
    const std::vector<InsertInfo>& inserts() const;
    void addInsert(const InsertInfo& ins);
};
```

| 字段 | 含义 |
|---|---|
| `m_name` | 块名，供 INSERT 按名称引用 |
| `m_baseX/Y/Z` | 块局部坐标的基准点（插入锚点） |
| 各实体数组 | 块内定义的几何图元（局部坐标） |
| `m_inserts` | 块内部引用其他块的 INSERT |

### 设计特点

- `DxfBlock` 和 `DxfData` 有相似的实体容器结构，但 `DxfBlock` 代表**局部坐标系中的模板**，实体坐标尚未变换到世界坐标。
- 块不立即展开：先收集所有 `BLOCK` 定义到 `unordered_map`，所有模型空间 `INSERT` 存到 `vector`，解析完成后再统一递归展开。
- 嵌套 INSERT 通过 `m_inserts` 支持，展开器递归时合成外层与内层的变换矩阵。

---

## 15. DxfData（顶层容器）

```cpp
class DxfData {
public:
    // modifiers
    void addPoint/Line/Circle/Arc/LWPolyline/Ellipse/Spline(...);
    void addGeneratedLine(const DxfLine& line);   // 离散化生成的线段
    void reserveLines/Points/LWPolylines/Splines(size_t count);
    void clear();

    // accessors
    const std::vector<DxfPoint/Line/...>& points/lines/...() const;
    const DxfEntityStats& entityStats() const;

    int entityCount() const;             // 全部实体总数
    int sketchEntityCount() const;       // Sketch 转换路径支持的实体数
    int feEntityCount() const;           // FE 转换路径支持的实体数

    // error handling
    QString errorMessage() const;
    void setErrorMessage(const QString& msg);
    bool isValid() const;
    void setValid(bool v);
};
```

这是最终交给转换引擎的**扁平化数据容器**。其中的实体全部是模型空间坐标（Block 已展开并入）。

`DxfData` 中混合存储两类实体：

1. **原始实体**：从 DXF 直接解析的模型空间实体；
2. **展开实体**：从 Block 通过 INSERT 展开后的模型空间实体；
3. **离散化线段**：通过 `addGeneratedLine()` 添加的 tessellation 结果。

---

## 16. Block 展开与嵌套 INSERT

展开过程位于 `DxfParser.cpp`：

1. **收集阶段**：
   - 每个 `BLOCK` 存入 `m_blocks[name]`
   - 模型空间的 `INSERT` 存入 `m_modelSpaceInserts`
   - 块内部的 `INSERT` 存入 `m_currentBlock->inserts()`

2. **展开阶段**：
   ```cpp
   expandBlocks(outputData, m_blocks, m_modelSpaceInserts, tolerance);
   ```

3. **展开一层的规则**：
   - 均匀 XY 缩放 → 保留原始实体类型（圆仍是圆）
   - 非均匀缩放 → 先离散再变换（圆变椭圆拆成线段）
   - 支持阵列 INSERT（colCount × rowCount）

4. **嵌套 INSERT 的变换合成**：
   ```cpp
   composed.scale = outer.scale × inner.scale
   composed.angle = outer.angle + inner.angle
   ```

5. **循环引用保护**：`visiting` 集合确保每个块在单条展开路径上最多出现一次。

---

## 17. 总体数据流

```text
DXF 文件
  │
  ▼
libdxfrw 解析 (DRW_* 回调)
  │
  ▼
DxfReader / DxfParser
  │
  ├── 模型空间实体 → 直接写入 DxfData
  │
  ├── BLOCK 定义   → 存入 unordered_map<string, DxfBlock>
  │
  └── INSERT       → 存入模型空间 INSERT 列表 / 块内 INSERT 列表
  │
  ▼
expandBlocks() ── 递归展开所有 INSERT → 实体写入 DxfData
  │
  ▼
DxfData（扁平化，所有实体坐标为世界坐标）
  │
  ├── ConversionEngine   → tessellateAll() → SamBuilder → SAM 模型
  │
  └── FeConversionEngine → 转为 FeData → PythonFiniteElementBuilder → FE 模型
```

---

*文档生成自 `src/Example1/DxfData.h`，覆盖所有数据结构、字段含义、设计决策及合法条件。*