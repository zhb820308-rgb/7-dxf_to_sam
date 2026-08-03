# Web 编辑器与图形 Agent

## 1. 运行方式

Web Studio 使用原生 HTML、CSS 和 JavaScript，不依赖前端框架。只做本地 DXF 编辑时
可以直接打开 `page/index.html`；使用 Agent 时在项目根目录运行：

```powershell
npm.cmd start
```

访问 `http://127.0.0.1:8080`。未显式设置 `PORT` 且 8080 不可用时自动尝试 18080。

## 2. 浏览器能力

- 读取不超过 50 MB 的 ASCII DXF；
- 解析 POINT、LINE、CIRCLE、ARC、ELLIPSE、LWPOLYLINE、POLYLINE、SPLINE、INSERT；
- 展开 DIMENSION 关联块并保留标注来源，识别 LEADER；
- 按弦高公差离散曲线；
- 显示图层、来源类型、点线数量和世界坐标；
- 缩放、平移、框选、多选、拖动、编辑坐标、添加和删除点线；
- 整体平移、旋转、缩放、图层修改和最多 50 步撤销/重做；
- 从原始 DXF 重新离散，不受当前显示过滤影响；
- 导出 ASCII DXF R2000，结果为 POINT/LINE，不恢复原曲线或 BLOCK 结构。

## 3. 页面模块

脚本按 `index.html` 中的顺序装载，共享 `DXFStudioApp`：

| 模块 | 说明 |
|---|---|
| `dxf.js` | 无 DOM 依赖的解析、离散和导出核心 |
| `agent-settings.js` | Agent 配置白名单持久化，不保存密钥 |
| `state.js` | 状态、DOM 查询、快照和通用 UI 工具 |
| `viewport.js` | 世界/屏幕坐标、缩放和平移 |
| `renderer.js` | Canvas 绘制 |
| `selection.js` | 命中、框选和拖动 |
| `import-controller.js` | 文件读取、重离散和导出 |
| `agent-controller.js` | 安全会话、Agent 请求、预览和应用 |
| `ui-controller.js` | 图层/类型列表和属性面板 |
| `editor-controller.js` | 工具、按钮、键盘和拖放事件 |
| `app.js` | 初始化各模块 |

## 4. 本地服务 API

| 方法和路径 | 用途 |
|---|---|
| `GET /api/health` | 服务状态、默认模型和服务端密钥是否配置 |
| `GET /api/session` | 获取当前进程随机生成的 CSRF Token |
| `POST /api/agent/process` | 运行几何 Agent |
| `GET/HEAD /*` | 提供 `page/` 静态文件 |

请求和响应都是 JSON。未知非 GET/HEAD 方法返回 405；静态路径经过规范化并限制在 `page/` 内。

## 5. AI 提供商

支持：

- OpenAI Responses；
- OpenAI 兼容 Chat Completions；
- Anthropic Messages。

服务端环境变量：

```text
OPENAI_API_KEY
OPENAI_MODEL
OPENAI_BASE_URL
ANTHROPIC_API_KEY
ANTHROPIC_BASE_URL
PORT
```

浏览器也可以在单次请求中提交 API Key 和自定义端点。浏览器只缓存 provider、model 和
apiUrl；API Key 不写入 `localStorage`，页面加载和清除配置时都会清空密钥输入框。

## 6. Agent 工具

Agent 不能运行任意脚本，只能调用严格参数定义的工具：

| 工具 | 用途 |
|---|---|
| `inspect_geometry` | 分页检查实体和真实 ID |
| `measure_geometry` | 数量、边界和选择摘要 |
| `find_annotation_lines` | 查找原生标注、引线和候选标注线 |
| `transform_geometry` | 平移、旋转和缩放 |
| `cleanup_geometry` | 网格吸附、去重和删除零长度线 |
| `delete_geometry` | 删除明确范围内的实体 |
| `copy_geometry` | 复制并平移实体 |
| `set_geometry_layer` | 修改图层 |
| `add_geometry` | 添加点线 |
| `finish` | 结束并返回摘要 |

工具范围可以是全部、当前选择、图层、来源类型或明确 ID。涉及具体实体时应先检查真实 ID，
避免模型编造标识。

## 7. 预览与应用

Agent 接收当前点线的深拷贝，在服务端工作副本上执行工具。响应包含修改前后摘要、动作日志
和候选几何。页面先显示预览，只有用户点击应用才替换当前工作副本；原始导入基线仍保留，
可以恢复原图。导出文件使用新文件名，不覆盖导入源文件。

## 8. 安全模型

- 服务只监听 `127.0.0.1`；
- 有 Origin 时必须是与监听端口一致的 loopback HTTP Origin；
- Agent POST 必须使用 `application/json` 和有效 CSRF Token；
- 服务端环境变量密钥只能发送到服务端配置端点；
- 自定义端点必须显式提供请求级密钥；
- 非 localhost HTTP 端点被拒绝，远端必须使用 HTTPS；
- 上游重定向被禁止；
- 上游错误正文不回传浏览器；
- 详细资源限制见[安全限制与故障排查](06-安全限制与故障排查.md)。
