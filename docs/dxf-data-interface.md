# DXF 数据接口文档

> 本文档描述了基于 `libdxfrw` 的 DXF 文件解析、数据模型及其在 SAM 平台中的集成接口。

---

## 目录

1. [整体架构](#1-整体架构)
2. [数据模型 — DxfData](#2-数据模型--dxfdata)
   - 2.1 [实体类型枚举](#21-实体类型枚举-entitytype)
   - 2.2 [DxfEntity 基类](#22-dxfentity-抽象基类)
   - 2.3 [DxfPoint](#23-dxfpoint)
   - 2.4 [DxfLine](#24-dxfline)
   - 2.5 [DxfCircle](#25-dxfcircle)
   - 2.6 [DxfData 容器](#26-dxfdata-容器)
3. [解析器 — DxfParser](#3-解析器--dxfparser)
   - 3.1 [DxfReader](#31-dxfreader)
   - 3.2 [DxfParser](#32-dxfparser)
4. [Python 绑定 — Example1PytModule](#4-python-绑定--example1pytmodule)
   - 4.1 [importDxf 方法](#41-importdxf-方法)
5. [Python 端使用示例](#5-python-端使用示例)
6. [构建与依赖](#6-构建与依赖)
7. [扩展指南](#7-扩展指南)

---

## 1. 整体架构

```
┌─────────────────────────────────────────────────┐
│                  Python 脚本层                    │
│   session.journal('Example1').importDxf(path)    │
└──────────────────────┬──────────────────────────┘
                       │
┌──────────────────────▼──────────────────────────┐
│           Example1PytModule (pyoModule)           │
│    importDxf() → 解析 + 建草图 + 场景展示          │
└──────────────────────┬──────────────────────────┘
                       │
┌──────────────────────▼──────────────────────────┐
│              DxfParser                           │
│    parseFile(filePath, outData)                  │
└──────────────────────┬──────────────────────────┘
                       │
┌──────────────────────▼──────────────────────────┐
│              DxfReader                           │
│   (继承 DRW_Interface, 接收 libdxfrw 回调)        │
└──────────────────────┬──────────────────────────┘
                       │
┌──────────────────────▼──────────────────────────┐
│            DxfData (数据容器)                     │
│   m_points / m_lines / m_circles                 │
└──────────────────────┬──────────────────────────┘
                       │
┌──────────────────────▼──────────────────────────┐
│            libdxfrw (第三方 DXF 解析库)            │
│   dxfRW::read() → 回调 DxfReader 各 addXxx()     │
└─────────────────────────────────────────────────┘
```

**数据流：** DXF 文件 → `libdxfrw` 解析 → `DxfReader` 回调 → 填充 `DxfData` → `DxfParser` 返回 → `Example1PytModule` 消费（创建 SAM 草图几何）。

---

## 2. 数据模型 — DxfData

数据模型定义在 `DxfData.h` / `DxfData.cpp` 中，采用类层次结构表示 DXF 实体。

### 2.1 实体类型枚举 EntityType

```cpp
enum class EntityType {
    Point,   // 点
    Line,    // 线段
    Circle   // 圆
};
```

用于运行时标识 `DxfEntity` 的具体子类类型。

### 2.2 DxfEntity 抽象基类

所有 DXF 实体的基类，提供**自动递增 ID**。

```cpp
class DxfEntity {
public:
    DxfEntity(EntityType entityType);
    virtual ~DxfEntity() = default;

    int getId() const;          // 获取唯一标识 ID（全局自增）
    EntityType getType() const; // 获取实体类型

    virtual bool isValid() const = 0;  // 子类实现有效性检查

private:
    int id;
    EntityType type;
};
```

| 方法 | 说明 |
|------|------|
| `getId()` | 返回全局自增整数 ID（从 1 开始，每次构造递增） |
| `getType()` | 返回 `EntityType` 枚举值 |
| `isValid()` | 纯虚函数，子类各自实现 |

### 2.3 DxfPoint

表示三维空间中的一个点坐标。

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

    bool isValid() const override;  // 三轴均为有限浮点数
};
```

| 成员 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `x`, `y`, `z` | `double` | `0.0` | 三维坐标 |

**有效性规则：** `std::isfinite(x) && std::isfinite(y) && std::isfinite(z)`。

### 2.4 DxfLine

表示一条由起点和终点确定的线段。

```cpp
class DxfLine : public DxfEntity {
public:
    DxfLine();
    DxfLine(const DxfPoint& start, const DxfPoint& end);

    const DxfPoint& start() const;  // 起点
    const DxfPoint& end()   const;  // 终点

    bool isValid() const override;   // 起点终点均有效且不重合
};
```

| 成员 | 类型 | 说明 |
|------|------|------|
| `m_start` | `DxfPoint` | 起点 |
| `m_end` | `DxfPoint` | 终点 |

**有效性规则：** 起点和终点均 `isValid()`，且至少有一个坐标分量不同（非零长度线）。

### 2.5 DxfCircle

表示一个由圆心和半径确定的圆。

```cpp
class DxfCircle : public DxfEntity {
public:
    DxfCircle();
    DxfCircle(const DxfPoint& center, double radius);

    const DxfPoint& center() const;  // 圆心
    double radius()          const;  // 半径

    bool isValid() const override;   // 圆心有效、半径有限且 > 0
};
```

| 成员 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `m_center` | `DxfPoint` | `(0,0,0)` | 圆心坐标 |
| `m_radius` | `double` | `0.0` | 半径 |

**有效性规则：** 圆心 `isValid()` 且 `std::isfinite(radius) && radius > 0.0`。

### 2.6 DxfData 容器

顶层数据容器，管理三种实体的集合和解析状态。

```cpp
class DxfData {
public:
    DxfData() = default;

    // --- 修改器 ---
    void addPoint(const DxfPoint& pt);
    void addLine(const DxfLine& line);
    void addCircle(const DxfCircle& circle);
    void clear();

    // --- 访问器 ---
    const std::vector<DxfPoint>&  points()  const;
    const std::vector<DxfLine>&   lines()   const;
    const std::vector<DxfCircle>& circles() const;

    int entityCount() const;  // lines.size() + circles.size()

    // --- 错误/状态 ---
    QString errorMessage() const;
    void setErrorMessage(const QString& msg);

    bool isValid() const;
    void setValid(bool v);
};
```

| 方法 | 返回值 | 说明 |
|------|--------|------|
| `addPoint(pt)` | `void` | 添加一个点实体 |
| `addLine(line)` | `void` | 添加一条线段 |
| `addCircle(circle)` | `void` | 添加一个圆 |
| `clear()` | `void` | 清空所有实体、错误消息，重置 `isValid=false` |
| `points()` | `const std::vector<DxfPoint>&` | 获取所有点的只读引用 |
| `lines()` | `const std::vector<DxfLine>&` | 获取所有线段的只读引用 |
| `circles()` | `const std::vector<DxfCircle>&` | 获取所有圆的只读引用 |
| `entityCount()` | `int` | 返回 `lines().size() + circles().size()` |
| `errorMessage()` | `QString` | 获取错误消息 |
| `setErrorMessage(msg)` | `void` | 设置错误消息 |
| `isValid()` | `bool` | 获取解析成功标志（默认 `false`） |
| `setValid(v)` | `void` | 设置解析成功标志 |

> **注意：** `entityCount()` 目前仅统计线段和圆，**不包含点**。

---

## 3. 解析器 — DxfParser

解析器定义在 `DxfParser.h` / `DxfParser.cpp` 中，基于 `libdxfrw` 库实现 DXF 文件读取。

### 3.1 DxfReader

`DxfReader` 继承自 `DRW_Interface`（`libdxfrw` 定义的回调接口），注册接收解析事件的回调。

```cpp
class DxfReader : public DRW_Interface {
public:
    DxfData m_data;  // 存储解析结果

    // 已实现回调
    void addLine(const DRW_Line& data) override;
    void addCircle(const DRW_Circle& data) override;
    void addLWPolyline(const DRW_LWPolyline& data) override;

    // 其他回调均为空实现（支持后续扩展）
};
```

**已实现的回调处理：**

| 回调 | 实体 | 处理逻辑 |
|------|------|----------|
| `addLine` | LINE | 将 `DRW_Line` 的 basePoint / secPoint 转为 `DxfLine` 并添加到 `m_data` |
| `addCircle` | CIRCLE | 将 `DRW_Circle` 的 basePoint / radious 转为 `DxfCircle` 并添加到 `m_data` |
| `addLWPolyline` | LWPOLYLINE | 将轻量多段线的相邻顶点分解为多条 `DxfLine`；闭合多段线首尾相连；跳过含弧形段（bulge ≠ 0）的顶点 |

**LWPOLYLINE 分解规则：**

1. 顶点数 < 2 → 跳过
2. 遍历 `i = 0` 到 `numVerts - 2`，取 `v1 = vertlist[i]`, `v2 = vertlist[i+1]`
3. 若 `v1.bulge != 0` 或 `v2.bulge != 0` → 跳过（弧形段暂不支持）
4. 否则将其作为直线段 `DxfLine(v1 → v2)` 加入
5. 若为闭合多段线 (`flags & 1`)，连接最后一个顶点到第一个顶点

### 3.2 DxfParser

解析入口类，提供单一静态风格的接口。

```cpp
class DxfParser {
public:
    bool parseFile(const QString& filePath, DxfData& outData);
};
```

#### `parseFile`

```cpp
bool DxfParser::parseFile(const QString& filePath, DxfData& outData);
```

| 参数 | 类型 | 说明 |
|------|------|------|
| `filePath` | `const QString&` | DXF 文件路径 |
| `outData` | `DxfData&` | [输出] 解析结果容器 |

| 返回值 | 含义 |
|--------|------|
| `true` | 解析成功，且至少包含一条 LINE 实体 |
| `false` | 解析失败或文件中没有 LINE 实体 |

**处理流程：**

1. 检查路径是否为空 → 设置错误消息并返回 `false`
2. 转码为本地编码 → 创建 `dxfRW` 对象
3. 调用 `dxf.read(&reader, true)` → 若失败，提取错误码并设置错误消息
4. 将 `reader.m_data` 移到输出 → 设置 `isValid = true`
5. 检查 `lines().empty()` → 若无 LINE 实体，设置警告消息并返回 `false`
6. 返回 `true`

**可能出现的错误消息：**

| 错误消息 | 触发条件 |
|----------|----------|
| `"DXF file is empty"` | 文件路径为空字符串 |
| `"Failed to read DXF file. Error code: %1"` | `dxf.read()` 返回 false |
| `"DXF was read successfully, but no LINE entities were found."` | 成功读取但无 LINE 实体 |

---

## 4. Python 绑定 — Example1PytModule

DXF 导入功能通过 `Example1PytModule` 注册为 SAM 的 Python 可调用方法。

### 4.1 `importDxf` 方法

```python
session.journal('Example1').importDxf(filePath)
```

| 参数 | 类型 | 说明 |
|------|------|------|
| `filePath` | `str` | DXF 文件的完整路径 |

| 返回值 | 类型 | 含义 |
|--------|------|------|
| `int` | `omuPrimNumber` | 成功导入的有效线段数；失败返回 `None`/`0` |

**内部流程（四阶段）：**

1. **阶段 1 — DXF 解析**
   - 调用 `DxfParser::parseFile()` 解析文件
   - 解析失败 → 返回 `nullptr`（Python 端收到 `None`）

2. **阶段 2 — 创建草图 + 生成几何**
   - 在 `Model-1` 中创建名为 `DxfImport_<timestamp>` 的草图
   - 遍历 `data.lines()`，对每条有效线段调用 `geometryFactory.CreateLine()`
   - 自动跳过无效或零长度的线段
   - 无有效线段 → 返回 `0`

3. **阶段 3 — 提交数据库**
   - 将草图包装为 `gmlSketchWrapper` 并插入草图仓库
   - 提交 `basBasis` 变更
   - 清空撤销栈

4. **阶段 4 — 场景展示**
   - 切换到 PART 场景视图
   - 获取当前场景，刷新显示
   - 设置草图 PDO 并重建
   - 设置主对象路径以便 GUI 交互

---

## 5. Python 端使用示例

### 基本导入

```python
session.journal('Example1').importDxf(r'D:\dxf_files\drawing.dxf')
```

### 完整工作流

```python
# 导入 DXF
count = session.journal('Example1').importDxf(r'C:\model\part.dxf')

if count is None:
    print("DXF 文件解析失败！")
elif count == 0:
    print("DXF 文件无有效线段")
else:
    print(f"成功导入 {count} 条线段")
```

### 注意事项

- 草图名称包含时间戳，每次导入创建新的独立草图
- 支持实体：**LINE**、**CIRCLE**、**LWPOLYLINE**（分解为线段）
- LWPOLYLINE 的**弧形段**（bulge ≠ 0）当前被跳过
- CIRCLE 实体虽可解析，但 Python 绑定侧目前仅遍历 `lines()` 创建直线几何
- 导入后自动切换到 PART 视图并清空撤销栈

---

## 6. 构建与依赖

### 依赖库

| 依赖 | 说明 | 链接变量 |
|------|------|----------|
| `libdxfrw` | 第三方 C++ DXF 解析库 | `LIBDXFRW_INCLUDE_DIR` / `LIBDXFRW_LIBRARY` / `LIBDXFRW_RUNTIME` |
| `Qt5Core` | Qt 5 核心库 | `${LIBS_QT5_ROOT}` |
| SAM SDK | SAM 平台 SDK | `${LIBS_SAMSDK_ROOT}` |

### 构建输出

- 目标名称：`Example1`
- 后缀：`.pyd`（Python 动态库）
- 构建后自动拷贝 `dxfrw.dll` 到输出目录

### 关键 CMake 配置

```cmake
add_library(Example1 SHARED ${TARGET_SRC})
set_target_properties(Example1 PROPERTIES SUFFIX ".pyd")
target_link_libraries(Example1 Qt5Core ${LIBDXFRW_LIBRARY} ...)
add_custom_command(TARGET Example1 POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${LIBDXFRW_RUNTIME}" "$<TARGET_FILE_DIR:Example1>"
)
```

---

## 7. 扩展指南

### 添加新的实体类型

1. 在 `EntityType` 枚举中新增类型（如 `Arc`）
2. 新建继承 `DxfEntity` 的子类，实现 `isValid()`
3. 在 `DxfData` 中添加对应的 `std::vector<T>` 成员和 `addXxx()` / `xxx()` 方法
4. 在 `DxfReader` 中实现对应的 `addXxx(const DRW_Xxx&)` 回调
5. 若需暴露给 Python，在 `Example1PytModule::importDxf()` 中消费

### 支持 LWPOLYLINE 弧形段

当前弧形段（bulge ≠ 0）被跳过。如需支持，可在 `DxfReader::addLWPolyline()` 中根据 bulge 值计算弧的圆心、半径和角度范围，生成 `DxfArc` 或将其细分折线段。

### 导出功能

`DxfReader` 已声明 `writeHeader`、`writeEntities` 等回调方法，可通过 `dxfRW::write()` 实现 DXF 文件导出。
