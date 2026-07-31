# AI 图形 Agent 接口

## 架构

浏览器将当前点线模型和本次请求的 AI 配置发送到本地 `server.js`。服务端根据所选协议调用对应 AI API，并运行带有受限图形工具的 Agent。

```text
浏览器画布
  → POST /api/agent/process
  → 多协议适配器（Responses / Chat Completions / Anthropic）
  → 图形 Agent（观察摘要、调用工具、检查结果）
  → 返回处理后的图形副本
  → 前端预览
  → 用户确认后应用
```

Agent 不能执行任意 JavaScript，也不能直接改写浏览器状态，只能调用以下工具：

- `inspect_geometry`：分页检查图形摘要、实体 ID 和坐标。
- `measure_geometry`：测量点数、线数、总线长、包围盒及分图层统计。
- `find_annotation_lines`：查找原生尺寸线、引线和炸开标注候选，返回置信度与精确实体 ID。
- `transform_geometry`：按全部、选择集、图层或实体 ID 平移、旋转、等比缩放。
- `copy_geometry`：复制图元并按坐标偏移，可放到新图层。
- `set_geometry_layer`：修改指定图元所属图层。
- `cleanup_geometry`：网格吸附、删除重复线、删除零长度线。
- `delete_geometry`：按全部、选中内容、图层或实体 ID 删除。
- `add_geometry`：添加点和直线。
- `finish`：结束处理并生成摘要。

所有工具参数使用受约束的 JSON Schema，并在服务端再次校验。Agent 最多执行 8 个工具循环，结果只作用于预览副本。模型不会直接读取或改写 DXF 文本；用户点击“应用到工作副本”后，结果只替换工作副本，导入时保存的原图基线保持不变。用户可随时恢复原图，导出也始终生成新文件，不覆盖原始 DXF。

## 启动

需要 Node.js 18 或更高版本。

```powershell
node server.js
```

如果 PowerShell 当前位于 `page` 目录，同样可以直接运行 `node server.js`；该目录中的启动入口会加载项目根目录的 Agent 服务。

访问：

```text
http://127.0.0.1:8080
```

如果 Windows 拒绝使用或已有程序占用 8080，服务会自动切换到 `http://127.0.0.1:18080`。也可以继续使用 VS Code Live Server 的 `http://127.0.0.1:5500` 页面；前端会自动尝试 8080 和 18080 的 Agent 接口。因此无论使用哪个页面地址，都需要保持 `node server.js` 运行。

页面中的“API 地址”指上游 AI 提供商地址，不是本地 Agent 服务地址。

API 地址、API Key 和模型可以直接在页面填写。也可以用环境变量为 OpenAI Responses 提供默认值：

```text
OPENAI_API_KEY=服务端默认密钥
OPENAI_MODEL=gpt-5.6
OPENAI_BASE_URL=https://api.openai.com/v1
PORT=8080
```

页面会将协议、模型、API 地址和 API Key 保存到当前来源的 `localStorage`，下次打开时自动恢复。API Key 因此会以浏览器可读取的明文形式持久化；只应在可信的本机浏览器中使用，并可随时点击页面中的“清除缓存”删除。服务端仍不会把 Key 写入文件或日志。请勿将真实 Key 写入前端源文件或提交到 Git。

## 支持的 AI 协议

| 页面选项 | 请求格式 | 常见服务 |
| --- | --- | --- |
| OpenAI Responses | `/responses` + function tools | OpenAI 或兼容 Responses API 的服务 |
| OpenAI 兼容 Chat Completions | `/chat/completions` + function tools | DeepSeek、通义千问及其他 OpenAI 兼容服务 |
| Anthropic Messages | `/v1/messages` + `tool_use/tool_result` | Anthropic Claude 或兼容 Messages API 的服务 |

API 地址既可以填写基础地址，也可以填写完整端点。例如：

```text
https://api.openai.com/v1
https://api.deepseek.com
https://api.anthropic.com
https://your-provider.example/v1/chat/completions
```

远程地址必须使用 HTTPS。本机 `localhost`、`127.0.0.1` 和 `::1` 可以使用 HTTP，便于连接本地模型服务。

## 健康检查

```http
GET /api/health
```

响应示例：

```json
{
  "ok": true,
  "model": "gpt-5.6",
  "api_key_configured": true
}
```

## Agent 处理接口

```http
POST /api/agent/process
Content-Type: application/json
```

请求示例：

```json
{
  "instruction": "将选中的线向右移动 100，然后删除重复线",
  "provider": "openai_chat",
  "model": "your-model-id",
  "api_url": "https://your-provider.example/v1",
  "api_key": "本次请求使用的密钥",
  "selected_ids": ["l12", "l13"],
  "geometry": {
    "points": [
      {
        "id": "p1",
        "x": 0,
        "y": 0,
        "layer": "0",
        "sourceType": "POINT"
      }
    ],
    "lines": [
      {
        "id": "l12",
        "x1": 0,
        "y1": 0,
        "x2": 10,
        "y2": 0,
        "layer": "0",
        "sourceType": "LINE"
      }
    ]
  }
}
```

响应包含：

- `summary`：Agent 的处理说明。
- `provider` / `model`：实际使用的协议和模型。
- `actions`：实际执行的工具操作记录。
- `before` / `after`：处理前后的图形统计。
- `geometry`：完整的处理后点线模型。

前端必须在用户确认后才把 `geometry` 应用到工作副本，并应保留撤销能力和不可变的原图基线。

## 限制与安全措施

- 单次请求体最大 16 MB。
- 单个模型最多 100,000 个点和直线。
- Agent 单次工具调用最多添加 500 个图元。
- `inspect_geometry` 单页最多向模型暴露 200 个实体，可用 `next_offset` 继续分页。
- OpenAI 默认先接收图形统计摘要；只有 Agent 主动检查时才发送少量实体坐标。
- API Key 可以来自浏览器恢复的配置、当前输入，也可以回退到服务端 `OPENAI_API_KEY` 环境变量。
- 服务端不会在响应或日志中返回 API Key。
- 远程 API 地址强制使用 HTTPS，并拒绝地址内嵌用户名或密码。
- 返回坐标必须是有限数值且绝对值不超过 `1e15`。
- 删除和变换首先作用于服务端预览副本，前端确认后也只覆盖画布工作副本，不修改原图基线或磁盘文件。
- 当前接口处理二维点和直线，不让 Agent 直接恢复或创建 DXF 圆弧、样条和块定义。

## 可用指令示例

```text
将选中的图元向右移动 500。
```

```text
把 WATER LINE 图层绕自身中心旋转 90 度。
```

```text
将全部端点吸附到 10 单位网格，并删除重复线和零长度线。
```

```text
在坐标 (0,0) 到 (1000,0) 添加一条名为 AI_GUIDE 的基准线。
```

```text
找到 0 图层最上方的那条线，只把它向下移动 10。
```

```text
把选中的结构复制一份到右侧 500，并放到 COPY 图层。
```

```text
统计每个图层的线条数量和总长度，不要修改图形。
```

## 当前聊天能力边界

圆、圆弧、椭圆、样条和普通块可以导入并显示，但进入 Agent 时表现为离散后的直线段。因此聊天 Agent 可以移动、复制、旋转、缩放、清理或删除这些线段，也可以测量离散结果；暂时不能保持其原始 `CIRCLE`、`ARC`、`SPLINE` 或 `INSERT` 参数语义。

原生 DXF `DIMENSION` 和 `LEADER` 会保留确定的标注身份，置信度为 1；其中 `DIMENSION` 优先展开其关联匿名标注块，没有块时可为线性/对齐尺寸重建尺寸线与尺寸界线。已经炸开为普通 `LINE` 的标注不再具有可靠的 DXF 身份，只能在图层名称包含 `DIM`、`DIMENSION`、`ANNO`、`标注` 或 `尺寸` 等标记时作为 `dimension_candidate` 返回，当前置信度为 0.88。Agent 必须在结果中区分“确定标注”和“候选标注”。`MLEADER`、文字、标注数值语义、填充、三维实体和行业规范自动审查仍未完整支持。
