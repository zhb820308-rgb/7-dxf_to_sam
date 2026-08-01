"use strict";

const { providerRequest } = require("../provider-request.js");
const {
  shortText,
  validateGeometry,
  geometrySummary,
  executeAgentTool,
  tools
} = require("./geometry-tools.js");

const MODEL = process.env.OPENAI_MODEL || "gpt-5.6";
const DEFAULT_OPENAI_API_BASE = "https://api.openai.com/v1";
const DEFAULT_ANTHROPIC_API_BASE = "https://api.anthropic.com";
const MAX_AGENT_STEPS = 8;

function resolveProviderConfig(body) {
  const provider = ["openai_responses", "openai_chat", "anthropic"].includes(body.provider)
    ? body.provider : "openai_responses";
  const model = shortText(body.model, MODEL, 160);
  const requestApiKey = shortText(body.api_key, "", 1000);
  const serverApiKey = provider === "anthropic"
    ? shortText(process.env.ANTHROPIC_API_KEY, "", 1000)
    : shortText(process.env.OPENAI_API_KEY, "", 1000);
  const credentialSource = requestApiKey ? "request" : "server";
  const apiKey = requestApiKey || serverApiKey;
  const defaultUrl = provider === "anthropic"
    ? (process.env.ANTHROPIC_BASE_URL || DEFAULT_ANTHROPIC_API_BASE)
    : (process.env.OPENAI_BASE_URL || DEFAULT_OPENAI_API_BASE);
  // A server-side secret is only ever sent to the server-configured endpoint.
  // User-selected endpoints must carry an explicitly user-supplied key.
  const rawUrl = credentialSource === "server"
    ? defaultUrl
    : shortText(body.api_url, defaultUrl, 2000);
  if (!apiKey) {
    const variable = provider === "anthropic" ? "ANTHROPIC_API_KEY" : "OPENAI_API_KEY";
    throw Object.assign(new Error(`请输入 API Key，或在服务端配置 ${variable}`), { status: 503 });
  }
  if (!model) throw Object.assign(new Error("model 不能为空"), { status: 400 });
  let url;
  try {
    url = new URL(rawUrl);
  } catch {
    throw Object.assign(new Error("API 地址格式无效"), { status: 400 });
  }
  const localHttp = url.protocol === "http:" && ["localhost", "127.0.0.1", "::1"].includes(url.hostname);
  if (url.protocol !== "https:" && !localHttp) {
    throw Object.assign(new Error("API 地址必须使用 HTTPS；本机 localhost 可使用 HTTP"), { status: 400 });
  }
  if (url.username || url.password) throw Object.assign(new Error("API 地址不能包含用户名或密码"), { status: 400 });
  const cleanPath = url.pathname.replace(/\/+$/, "");
  if (provider === "openai_responses" && !cleanPath.endsWith("/responses")) {
    url.pathname = `${cleanPath}/responses`;
  } else if (provider === "openai_chat" && !cleanPath.endsWith("/chat/completions")) {
    url.pathname = `${cleanPath}/chat/completions`;
  } else if (provider === "anthropic" && !cleanPath.endsWith("/messages")) {
    url.pathname = cleanPath === "" ? "/v1/messages" : `${cleanPath}/messages`;
  }
  return { provider, model, apiKey, endpoint: url.toString(), credentialSource };
}

const agentInstructions = [
  "你是二维 DXF 点线图形处理 Agent。",
  "根据用户目标观察图形并调用工具修改工作副本；不要只给建议。",
  "优先使用摘要或 measure_geometry 完成全局分析；需要具体坐标或实体时再分页 inspect_geometry。",
  "定位到具体图元后，优先使用 scope=ids 和 inspect 返回的真实 ID 精确操作，不要扩大操作范围。",
  "用户提到标注线、尺寸线、尺寸界线或引线时，必须先调用 find_annotation_lines；dimension 和 leader 是原生确定标注，dimension_candidate 只能作为候选并在摘要中说明不确定性。",
  "不要编造实体 ID。删除和大范围变换必须符合用户明确指令。",
  "用户只要求分析或测量时，不要修改图形。",
  "完成后调用 finish。若请求超出工具能力，在 finish 摘要中说明未完成部分。",
  "用最少的有效工具循环完成任务。"
].join("\n");

function runCalls(calls, context, adapter) {
  const outputs = [];
  for (const call of calls) {
    let result;
    try {
      result = executeAgentTool(call.name, typeof call.arguments === "string"
        ? JSON.parse(call.arguments || "{}") : call.arguments || {}, context);
    } catch (error) {
      result = { ok: false, error: error.message };
    }
    outputs.push(adapter(call, result));
  }
  return outputs;
}

async function runResponsesProtocol(config, initialInput, context) {
  let input = initialInput;
  let previousResponseId;
  for (let step = 0; step < MAX_AGENT_STEPS; step += 1) {
    const response = await providerRequest(config, {
      model: config.model,
      reasoning: { effort: "medium" },
      store: true,
      parallel_tool_calls: false,
      instructions: agentInstructions,
      tools,
      tool_choice: "auto",
      input,
      previous_response_id: previousResponseId
    });
    previousResponseId = response.id;
    const calls = (response.output || []).filter((item) => item.type === "function_call");
    if (!calls.length) {
      const text = (response.output || []).flatMap((item) => item.content || [])
        .filter((item) => item.type === "output_text").map((item) => item.text).join("\n");
      context.agentSummary = text || "Agent 未生成可执行操作";
      break;
    }
    const outputs = runCalls(calls, context, (call, result) => ({
      type: "function_call_output", call_id: call.call_id, output: JSON.stringify(result)
    }));
    if (context.finished) break;
    input = outputs;
  }
}

function chatTools() {
  return tools.map((tool) => ({
    type: "function",
    function: {
      name: tool.name,
      description: tool.description,
      parameters: tool.parameters
    }
  }));
}

async function runChatProtocol(config, initialInput, context) {
  const messages = [
    { role: "system", content: agentInstructions },
    { role: "user", content: initialInput }
  ];
  for (let step = 0; step < MAX_AGENT_STEPS; step += 1) {
    const response = await providerRequest(config, {
      model: config.model,
      messages,
      tools: chatTools(),
      tool_choice: "auto",
      parallel_tool_calls: false,
      max_tokens: 2048
    });
    const message = response.choices?.[0]?.message;
    if (!message) throw Object.assign(new Error("Chat Completions 响应缺少 message"), { status: 502 });
    const calls = (message.tool_calls || []).map((call) => ({
      id: call.id, name: call.function?.name, arguments: call.function?.arguments
    }));
    if (!calls.length) {
      context.agentSummary = message.content || "Agent 未生成可执行操作";
      break;
    }
    messages.push({
      role: "assistant",
      content: message.content || null,
      tool_calls: message.tool_calls
    });
    const outputs = runCalls(calls, context, (call, result) => ({
      role: "tool", tool_call_id: call.id, content: JSON.stringify(result)
    }));
    if (context.finished) break;
    messages.push(...outputs);
  }
}

function anthropicTools() {
  return tools.map((tool) => ({
    name: tool.name,
    description: tool.description,
    input_schema: tool.parameters
  }));
}

async function runAnthropicProtocol(config, initialInput, context) {
  const messages = [{ role: "user", content: initialInput }];
  for (let step = 0; step < MAX_AGENT_STEPS; step += 1) {
    const response = await providerRequest(config, {
      model: config.model,
      system: agentInstructions,
      messages,
      tools: anthropicTools(),
      max_tokens: 2048
    });
    const content = Array.isArray(response.content) ? response.content : [];
    const calls = content.filter((item) => item.type === "tool_use").map((item) => ({
      id: item.id, name: item.name, arguments: item.input
    }));
    if (!calls.length) {
      context.agentSummary = content.filter((item) => item.type === "text").map((item) => item.text).join("\n") ||
        "Agent 未生成可执行操作";
      break;
    }
    messages.push({ role: "assistant", content });
    const outputs = runCalls(calls, context, (call, result) => ({
      type: "tool_result", tool_use_id: call.id, content: JSON.stringify(result)
    }));
    if (context.finished) break;
    messages.push({ role: "user", content: outputs });
  }
}

async function runGeometryAgent(body) {
  const instruction = shortText(body.instruction, "", 2000);
  if (!instruction) throw Object.assign(new Error("instruction 不能为空"), { status: 400 });
  const config = resolveProviderConfig(body);
  const geometry = validateGeometry(body.geometry);
  const selectedIds = new Set(Array.isArray(body.selected_ids) ? body.selected_ids.map(String) : []);
  const context = { geometry, selectedIds, actionLog: [], finished: false, agentSummary: "" };
  const initialSummary = geometrySummary(geometry, selectedIds);
  const initialInput = JSON.stringify({
    user_instruction: instruction,
    geometry: initialSummary,
    note: "你操作的是预览副本。selected_count 为 0 时不要使用 selected 范围。"
  });

  if (config.provider === "openai_responses") await runResponsesProtocol(config, initialInput, context);
  else if (config.provider === "openai_chat") await runChatProtocol(config, initialInput, context);
  else await runAnthropicProtocol(config, initialInput, context);

  if (!context.finished && !context.agentSummary) {
    context.agentSummary = `Agent 达到 ${MAX_AGENT_STEPS} 步限制`;
  }
  return {
    ok: true,
    provider: config.provider,
    model: config.model,
    summary: context.agentSummary,
    actions: context.actionLog,
    before: initialSummary,
    after: geometrySummary(context.geometry, selectedIds),
    geometry: context.geometry
  };
}

module.exports = {
  MODEL,
  resolveProviderConfig,
  runGeometryAgent
};
