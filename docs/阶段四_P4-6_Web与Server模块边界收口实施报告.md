# 阶段四 P4-6：Web 与 Server 模块边界收口实施报告

## 1. 实施结论

P4-6 已完成页面启动入口和 Server 路由边界整理，并通过自动化回归。当前状态为：

> `app.js` 已收敛为模块组装与启动入口；页面 UI 展示和编辑事件分别由独立控制器负责；Server 请求处理器支持依赖注入且默认生产行为保持不变；新增无真实上游请求的路由边界测试。等待用户验收，尚未提交或推送本批次 Git。

## 2. 页面模块整理

### 2.1 启动入口收口

`page/app.js` 从 545 行缩减为 11 行，仅负责：

```text
绑定编辑器事件
加载 Agent 设置
初始化图纸 Profile
重置图纸状态
安排首次 Canvas 尺寸刷新
```

入口不再保存列表渲染、属性面板、鼠标、键盘、拖放和表单业务实现。

### 2.2 UI 控制器

新增：

```text
page/ui-controller.js
```

负责：

- 图元类型列表；
- 图层列表及显隐操作；
- 图层高亮；
- 点、线和来源统计；
- 单选及多选属性面板；
- HTML 转义和页面整体刷新。

### 2.3 编辑交互控制器

新增：

```text
page/editor-controller.js
```

负责绑定浏览器输入：

- 文件选择和拖放；
- 导出、新建、恢复、撤销和重做；
- 缩放、平移、点线绘制和框选；
- 图元拖动和键盘快捷键；
- Profile、公差和 Agent 表单事件；
- Canvas 及窗口尺寸事件。

原事件处理逻辑按原顺序迁移，没有改变快捷键、工具行为、坐标计算或状态更新规则。

### 2.4 脚本加载顺序

页面继续使用浏览器原生脚本，不引入打包器或前端框架。加载顺序明确为：

```text
数据与状态
→ Viewport / Renderer / Selection
→ Import / Agent Controller
→ UI Controller
→ Editor Controller
→ app.js 启动入口
```

## 3. Server 模块整理

`server/router.js` 新增：

```js
createRequestHandler(options)
```

可注入以下路由依赖：

```text
agentRunner
staticHandler
model
csrfToken
```

生产环境仍导出并使用原来的 `handleRequest`，其默认依赖仍为：

- `runGeometryAgent`；
- `serveStatic`；
- 当前生产模型；
- 随机生成的生产 CSRF Token。

因此现有 `server.js`、API 路径、成功响应、错误响应、Origin 校验、CSRF 校验和静态文件行为保持兼容。

## 4. 测试增强

新增：

```text
tests/server-module-boundaries.test.js
```

该测试使用本地临时端口和注入的假 Agent，验证：

- `/api/session` 使用注入的 CSRF Token；
- `/api/health` 使用注入的模型名称；
- `/api/agent/process` 将 JSON 请求交给指定 Agent；
- Agent 结构化错误状态和消息能够返回；
- GET 静态请求交给静态资源模块；
- 测试过程不会调用真实 OpenAI、Anthropic 或兼容接口。

现有 `web-editor-modules.test.js` 的脚本清单同步加入两个新页面控制器，继续覆盖最小图导入、编辑、选择、撤销、重做、整体变换、失败恢复和启动事件注册。

## 5. 涉及文件

### 5.1 新增

```text
page/ui-controller.js
page/editor-controller.js
tests/server-module-boundaries.test.js
docs/阶段四_P4-6_Web与Server模块边界收口实施报告.md
```

### 5.2 修改

```text
page/app.js
page/index.html
server/router.js
package.json
tests/web-editor-modules.test.js（仅新增两个模块的加载项）
```

工作区中原有的 API Key 文档、安全边界、请求体限制、Agent 工具原子性及相应测试修改不属于 P4-6。最终提交时只暂存本批次文件和 `web-editor-modules.test.js` 中的模块加载清单，不会混入那些既有修改。

## 6. 自动化验证结果

| 检查 | 结果 |
|---|---|
| `npm.cmd run check` | 通过，包含两个新增页面模块 |
| `npm.cmd test` | 9/9 测试文件通过 |
| Server 模块边界专项 | 通过，无真实上游请求 |
| Web 编辑器模块专项 | 通过 |
| Release 构建 | 通过，正式插件产物未受影响 |
| 全量 CTest | 14/14 通过 |
| `git diff --check` | 通过，仅有既有行尾转换提示 |

## 7. 用户验收步骤

### 7.1 自动化验收

在仓库根目录执行：

```powershell
npm.cmd run check
npm.cmd test
node tests/server-module-boundaries.test.js
node tests/web-editor-modules.test.js
```

预期：

```text
server-module-boundaries.test.js: ok
web-editor-modules.test.js: ok
```

并且完整 `npm.cmd test` 中 9 个测试文件全部显示 `ok`。

### 7.2 浏览器启动验收

启动本地服务：

```powershell
node server.js
```

浏览器打开：

```text
http://127.0.0.1:8080
```

按 `F12` 打开开发者工具，刷新页面。预期：

- [ ] 页面正常显示，没有 JavaScript 初始化错误；
- [ ] `ui-controller.js`、`editor-controller.js` 和 `app.js` 均返回 200；
- [ ] 控制台没有 `bindEditorEvents is not a function` 等加载顺序错误。

### 7.3 页面功能验收

导入：

```text
example\block_test_minimal.dxf
```

预期：

- [ ] 显示 3 条直线；
- [ ] 图元列表、图层列表和统计正常刷新；
- [ ] 单击直线可显示属性面板；
- [ ] 拖动、框选、Delete、Ctrl+Z、Ctrl+Y 正常；
- [ ] 平移、缩放、适应窗口和整体变换正常；
- [ ] 重新离散化和导出仍可使用。

### 7.4 Server 路由验收

保持服务运行，在另一个 PowerShell 窗口执行：

```powershell
Invoke-RestMethod http://127.0.0.1:8080/api/health
Invoke-RestMethod http://127.0.0.1:8080/api/session
```

预期：

- [ ] health 返回 `ok = true` 和当前模型；
- [ ] session 返回非空 `csrf_token`；
- [ ] 页面静态文件仍由原静态资源模块提供；
- [ ] 未配置 API Key 时只影响真实 Agent 请求，不影响普通编辑功能。

## 8. 验收判定

满足以下条件即可回复“通过”：

- [ ] Node 语法检查和 9/9 测试通过；
- [ ] 页面刷新无脚本加载错误；
- [ ] 最小图仍为 3 条直线；
- [ ] 列表、属性面板、选择、编辑和快捷键行为正常；
- [ ] health 与 session 路由返回正常；
- [ ] 没有发现 API、安全边界或静态资源回归。

收到本报告后的“通过”后，再提交并推送 P4-6。
