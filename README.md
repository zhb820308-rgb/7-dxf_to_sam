# DXF to SAM — 船舶 CAD 图纸导入 SAM 插件

将 AutoCAD DXF 格式的二维船舶 CAD 图纸自动导入 SAM (Ship Analysis Model) 三维建模平台的插件，支持直线、圆、圆弧、多段线、椭圆、样条曲线及块/INSERT 等常见图元。

## 核心流程

```
DXF 文件 → DxfParser (libdxfrw) → DxfData (原始几何数据)
  → ConversionEngine (曲线离散 + 坐标平移) → SamData
  → SamBuilder (SAM 草图创建) → SAM 场景
```

## 功能特性

- **多图元支持**：LINE、CIRCLE、ARC、LWPOLYLINE（含凸度圆弧）、ELLIPSE、SPLINE、POINT
- **Block/INSERT 展开**：支持嵌套块引用（含循环检测）、非均匀缩放、旋转、矩形阵列
- **曲线离散化**：基于 OpenCASCADE，将圆弧、椭圆弧、B 样条曲线离散为线段，公差可配置（默认 0.01）
- **基点偏移**：支持用户指定 CAD 原点在 SAM 坐标系中的位置 (X/Y/Z)
- **图层过滤**：通过多选下拉框选择忽略指定图层
- **导入日志**：每次导入生成独立日志文件（`logs/imports/dxf_import_<id>.log`），支持自动轮转与过期清理
- **日志查看器**：内置树形日志浏览器，支持按级别过滤与实体搜索
- **进度回调与取消**：长时导入可中途取消
- **自动图纸尺寸**：根据导入几何包围盒自动计算 Sketch SheetSize（2.4 倍外扩，最小 200.0）
- **独立草图**：每次导入创建带时间戳的独立草图（如 `DxfImport_1712345678901`）

## 技术栈

| 层次     | 技术                                     |
| -------- | ---------------------------------------- |
| 语言     | C++17                                    |
| 构建系统 | CMake ≥ 4.4，MSVC (Windows x64)         |
| GUI 框架 | Qt 5.12.6 (Widgets, Core, Gui)           |
| DXF 解析 | libdxfrw（开源 C++ 库，支持 R12–R2018） |
| 几何内核 | OpenCASCADE (OCCT) 7.x                   |
| 日志     | spdlog 1.15.x + fmt                      |
| 测试框架 | Google Test 1.15.2                       |
| SAM 平台 | SAM SDK（内嵌 Python 2.7）               |

## 构建

### 依赖

构建前需准备以下依赖，并通过 CMake 变量或环境变量指定路径：

| 依赖       | CMake 变量           | 说明                                    |
| ---------- | -------------------- | --------------------------------------- |
| SAM SDK    | `LIBS_SAMSDK_ROOT` | SAM 平台 SDK 根目录                     |
| SAM 安装   | `LIBS_SAM_ROOT`    | SAM 安装根目录                          |
| libdxfrw   | `LIBDXFRW_ROOT`    | libdxfrw 库根目录                       |
| spdlog     | `SPDLOG_ROOT`      | spdlog 安装目录（未设置时回退到 vcpkg） |
| Python 2.7 | `LIBS_PYTHON_ROOT` | SAM 内嵌 Python 路径                    |

### 编译

```bash
mkdir build && cd build
cmake .. -G "Visual Studio 17 2022" -A x64 \
  -DLIBS_SAMSDK_ROOT="path/to/SAMSDK" \
  -DLIBS_SAM_ROOT="path/to/SAM" \
  -DLIBDXFRW_ROOT="path/to/libdxfrw" \
  -DSPDLOG_ROOT="path/to/spdlog"
cmake --build . --config Release
```

### 构建产物

| 目标                | 类型               | 输出路径                                    | 用途                                        |
| ------------------- | ------------------ | ------------------------------------------- | ------------------------------------------- |
| `Example1`        | .pyd (Python 扩展) | `bin/Release/Example1.pyd`                | 核心 DLL：解析、转换、草图创建、Python 绑定 |
| `Example1Toolset` | .dll (Qt 插件)     | `bin/Release/SAM.Pre.Example1Toolset.dll` | GUI 插件：菜单、导入对话框、日志查看器      |

## 使用方法

### GUI 方式

1. 打开 SAM，点击 **Tools** 菜单
2. 选择 **Import DXF**
3. 浏览选择 DXF 文件，可选设置基点坐标 (X/Y/Z)、公差、忽略图层
4. 点击 **OK** 开始导入

### Python 脚本方式

在 SAM 的 Python 脚本控制台中：

```python
session.journal('Example1').importDxf(r'D:\dxf_files\drawing.dxf')
```

## 项目结构

```
7-dxf_to_sam/
├── CMakeLists.txt                          # 根构建文件
├── src/                                     # 源代码
│   ├── Example1/                            # 核心模块（.pyd）
│   └── Example1Toolset/                     # GUI 插件（.dll）
├── test/                                    # 单元测试与集成测试
└── bin/Release/                             # 构建产物
    ├── Example1.pyd                         # 核心 DLL（Python 扩展）
    └── SAM.Pre.Example1Toolset.dll          # GUI 插件（Qt 插件）
```

## 运行测试

```bash
cd build
ctest --output-on-failure -C Release
```

## 支持的 DXF 图元

| 图元类型       | 状态        | 说明                                     |
| -------------- | ----------- | ---------------------------------------- |
| LINE           | ✅ 完全支持 | 直接映射                                 |
| CIRCLE         | ✅ 完全支持 | 直接映射                                 |
| ARC            | ✅ 完全支持 | 离散为线段（OCCT）                       |
| LWPOLYLINE     | ✅ 完全支持 | 含凸度圆弧离散                           |
| ELLIPSE        | ✅ 完全支持 | 参数采样离散（OCCT）                     |
| SPLINE         | ✅ 完全支持 | GCPnts_TangentialDeflection 离散（OCCT） |
| BLOCK / INSERT | ✅ 完全支持 | 嵌套展开、阵列、变换                     |
