# G6 Agent 密钥与上游请求安全修复说明

## 1. 状态

- 实施日期：2026-08-01
- 基于提交：`849017e`
- 涉及问题：R-07、R-08
- 状态：代码与自动化验证已完成

## 2. R-07：API Key 生命周期

新增 `page/agent-settings.js`，对允许持久化的设置使用严格白名单：

```text
provider
model
apiUrl
```

`apiKey` 不进入设置对象和 localStorage，只保留在当前页面的密码输入框中，并在
页面初始化时强制清空。加载旧版设置时会按白名单重新写入，因此历史 `apiKey`
字段和其他未知字段都会被删除。UI 已明确提示刷新后需要重新输入 Key。

服务端环境变量仍可作为首选凭据：

```text
OPENAI_API_KEY
ANTHROPIC_API_KEY
```

既有的“服务端 Key 只能发送到服务端配置端点”规则保持不变。

## 3. R-08：上游请求边界

独立模块 `provider-request.js` 中的 `providerRequest()` 使用同一个
`AbortController` 覆盖连接、等待响应和读取
响应体的全过程：

| 边界 | 默认值 | 失败映射 |
| --- | ---: | ---: |
| 总超时 | 30 秒 | HTTP 504 |
| 解压后响应体 | 4 MiB | HTTP 502 |
| 上游非 2xx | 不读取/不透传正文 | HTTP 502 |
| 无效 JSON | 拒绝 | HTTP 502 |

读取响应时先检查可信度有限的 `Content-Length`，随后仍对流的实际字节数累计。
一旦超过上限立即取消 reader 并 abort 请求，不会继续把响应缓存到内存。网络错误、
超时和上游正文都使用本地通用错误信息，不包含 API Key 或上游响应内容。

## 4. 回归测试

新增 `tests/agent-boundaries.test.js`，覆盖：

1. 旧 localStorage 中的 Key 被迁移清除。
2. 新设置对象即使包含 Key 也不会持久化。
3. 永不返回响应头的 fetch 被主动 abort，并映射为 504。
4. 已返回响应头但永久停顿的响应流被取消，并映射为 504。
5. 流式响应超过上限后停止读取并映射为 502。
6. 上游非成功响应映射为 502，错误正文不泄露。
7. 网络异常信息和最终错误中不出现 API Key。

验证命令：

```powershell
npm.cmd run check
npm.cmd test
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```
