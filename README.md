# DXF to SAM

DXF to SAM 是一个面向船舶 CAD 图纸的 DXF 导入与检查项目，包含两套入口：

- C++/Qt 插件：把 DXF 导入 SAM，输出 Sketch 或有限元 Part；
- Web Studio：在浏览器中解析、离散、检查和编辑 DXF，并可通过本地服务调用图形 Agent。

当前实现支持 `POINT`、`LINE`、`CIRCLE`、`ARC`、`LWPOLYLINE`、`ELLIPSE`、
`SPLINE` 和 `BLOCK/INSERT`。文字、填充等不参与几何输出的实体会被记录或跳过。

## 从这里开始

| 想了解的内容 | 文档 |
|---|---|
| 项目组成和模块关系 | [系统架构](docs/01-系统架构.md) |
| DXF 如何变成 Sketch/FE 几何 | [DXF 解析与转换流程](docs/02-DXF解析与转换流程.md) |
| SAM GUI、Python 入口和事务 | [SAM 插件与 FE 导入](docs/03-SAM插件与FE导入.md) |
| 浏览器编辑器和图形 Agent | [Web 编辑器与图形 Agent](docs/04-Web编辑器与图形Agent.md) |
| 配置、编译、部署和测试 | [构建部署与测试](docs/05-构建部署与测试.md) |
| 资源限制、安全边界和排错 | [安全限制与故障排查](docs/06-安全限制与故障排查.md) |
| 当前完成情况和主要变更 | [当前状态与主要变更](docs/07-当前状态与主要变更.md) |
| 55 个完整测试样例 | [样例与测试数据](docs/08-样例与测试数据.md) |

完整目录见 [docs/README.md](docs/README.md)。

## 核心链路

```text
DXF
 └─ DxfParser + libdxfrw
     ├─ DxfData
     ├─ BLOCK/INSERT 展开与仿射变换
     └─ 图层过滤、数值校验和资源预算
          ├─ ConversionEngine → SamData → SamBuilder → Sketch
          └─ FeConversionEngine → FeData → PythonFiniteElementBuilder → FE Part
```

Web 链路独立于 SAM 插件：

```text
浏览器 page/
 ├─ 本地解析、离散、编辑和导出
 └─ POST /api/agent/process
      └─ Node 本地服务 → AI 提供商 → 受约束的几何工具
```

## 快速构建

项目面向 Windows x64，并依赖 SAM SDK 的既有 ABI。已验证的生成器是 Visual Studio
2017。需要 CMake 4.4、SAM/SAMSDK、libdxfrw、spdlog，以及仓库内配置的 OCCT。

```powershell
cmake -S . -B build -G "Visual Studio 15 2017" -A x64 `
  -DEXAMPLE1_BUILD_TESTS=ON `
  -DLIBS_SAM_ROOT="D:/path/to/SAM" `
  -DLIBS_SAMSDK_ROOT="D:/path/to/SAMSDK" `
  -DLIBDXFRW_ROOT="D:/path/to/libdxfrw" `
  -DSPDLOG_ROOT="D:/path/to/spdlog"
cmake --build build --config Release --parallel 4
```

主要产物：

```text
bin/Release/Example1.pyd
bin/Release/SAM.Pre.Example1Toolset.dll
```

依赖变量、默认路径和部署方式见[构建部署与测试](docs/05-构建部署与测试.md)。

## 运行测试

```powershell
npm.cmd run check
npm.cmd test
ctest --test-dir build -C Release --output-on-failure
```

`npm test` 同时验证三份基准 DXF 的输入哈希、点线数量、包围盒和几何指纹。

## 启动 Web Studio

需要 Node.js 18 或更高版本，无需安装 npm 运行时依赖：

```powershell
npm.cmd start
```

然后访问 `http://127.0.0.1:8080`。如果 8080 被占用且没有显式设置 `PORT`，服务会
尝试 18080。只做本地编辑时也可以直接打开 `page/index.html`；AI Agent 必须连接本地服务。

## SAM 中使用

部署两个 Release 产物并重启 SAM 后，可以通过 `File → Import → DXF...` 打开导入
对话框。对话框支持：

- Sketch / Finite Element 两种模式；
- 基点 X/Y/Z；
- 曲线弦高公差；
- 忽略图层；
- Small、Large、Unlimited 三种输出档位；
- FE 模式的模型名和 Part 名。

日志查看器位于 `Tools → DXFimport log`。

## 目录

```text
src/Example1/          C++ 解析、转换、Sketch/FE 构建和 Python 绑定
src/Example1Toolset/   Qt 导入对话框、菜单和日志查看器
page/                  浏览器 DXF Studio
server/                本地 Agent HTTP 服务
test/                  C++ Google Test 与最小 DXF 夹具
tests/                 Node 回归测试
example/               项目内稳定样例和几何基线输入
docs/                  当前项目文档
```
