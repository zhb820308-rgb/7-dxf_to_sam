(function (global) {
  "use strict";

  const app = global.DXFStudioApp;
  const AGENT_SETTINGS_KEY = "dxf-studio.agent-settings.v1";
  const agentCsrfTokens = new Map();
  let agentSettingsTimer = null;

  app.clearAgentResult = function () {
    app.state.pendingAgentResult = null;
    app.$("#agentResult").classList.add("hidden");
    app.$("#agentSummary").textContent = "";
    app.$("#agentChanges").textContent = "";
  };

  app.saveAgentSettings = function (settings) {
    try {
      DxfAgentSettings.saveSettings(
        localStorage,
        AGENT_SETTINGS_KEY,
        settings
      );
    } catch {
      app.toast("浏览器禁止本地存储，Agent 配置未缓存", "error");
    }
  };

  app.saveCurrentAgentSettings = function () {
    app.saveAgentSettings({
      provider: app.$("#agentProvider").value,
      model: app.$("#agentModel").value.trim(),
      apiUrl: app.$("#agentApiUrl").value.trim()
    });
  };

  app.scheduleAgentSettingsSave = function () {
    global.clearTimeout(agentSettingsTimer);
    agentSettingsTimer = global.setTimeout(app.saveCurrentAgentSettings, 250);
  };

  app.loadAgentSettings = function () {
    // Password managers and back/forward caches must not resurrect a prior key.
    app.$("#agentApiKey").value = "";
    try {
      const settings = DxfAgentSettings.loadSettings(
        localStorage,
        AGENT_SETTINGS_KEY
      );
      if (!settings || typeof settings !== "object") return;
      if (["openai_responses", "openai_chat", "anthropic"]
        .includes(settings.provider)) {
        app.$("#agentProvider").value = settings.provider;
      }
      if (typeof settings.model === "string") {
        app.$("#agentModel").value = settings.model;
      }
      if (typeof settings.apiUrl === "string") {
        app.$("#agentApiUrl").value = settings.apiUrl;
      }
      app.$("#agentModel").placeholder = settings.provider === "anthropic"
        ? "输入 Anthropic 模型 ID"
        : "输入模型 ID";
    } catch {
      DxfAgentSettings.clearSettings(localStorage, AGENT_SETTINGS_KEY);
    }
  };

  app.clearAgentSettings = function () {
    global.clearTimeout(agentSettingsTimer);
    agentSettingsTimer = null;
    try {
      DxfAgentSettings.clearSettings(localStorage, AGENT_SETTINGS_KEY);
    } catch {
      // The visible fields can still be cleared when storage is unavailable.
    }
    app.$("#agentProvider").value = "openai_responses";
    app.$("#agentModel").value = "gpt-5.6";
    app.$("#agentModel").placeholder = "输入模型 ID";
    app.$("#agentApiUrl").value = "https://api.openai.com/v1";
    app.$("#agentApiKey").value = "";
    app.toast("已清除浏览器缓存的 Agent 配置");
  };

  app.csrfTokenFor = async function (agentUrl) {
    const sessionUrl = new URL("/api/session", agentUrl).toString();
    if (agentCsrfTokens.has(sessionUrl)) return agentCsrfTokens.get(sessionUrl);
    const response = await fetch(sessionUrl, { cache: "no-store" });
    const data = await response.json().catch(() => ({}));
    if (!response.ok || typeof data.csrf_token !== "string" ||
      !data.csrf_token) {
      throw new Error(
        data.error || `无法建立 Agent 安全会话（HTTP ${response.status}）`
      );
    }
    agentCsrfTokens.set(sessionUrl, data.csrf_token);
    return data.csrf_token;
  };

  app.requestAgentApi = async function (payload) {
    const urls = [];
    if (location.protocol !== "file:") urls.push("/api/agent/process");
    [
      "http://127.0.0.1:8080/api/agent/process",
      "http://127.0.0.1:18080/api/agent/process"
    ].forEach((url) => {
      if (!urls.includes(url)) urls.push(url);
    });
    let lastError = null;
    for (const url of urls) {
      try {
        const absoluteUrl = new URL(url, location.href).toString();
        const csrfToken = await app.csrfTokenFor(absoluteUrl);
        const options = {
          method: "POST",
          headers: {
            "Content-Type": "application/json",
            "X-DXF-CSRF-Token": csrfToken
          },
          body: JSON.stringify(payload)
        };
        const response = await fetch(absoluteUrl, options);
        if (response.status !== 404 && response.status !== 405) return response;
        lastError = new Error(`${url} 返回 ${response.status}`);
      } catch (error) {
        lastError = error;
      }
    }
    throw new Error(
      "无法连接本地 Agent 服务（已尝试 8080 和 18080 端口）：" +
      (lastError?.message || "连接失败")
    );
  };

  app.runAgent = async function () {
    const instruction = app.$("#agentPrompt").value.trim();
    const provider = app.$("#agentProvider").value;
    const model = app.$("#agentModel").value.trim();
    const apiUrl = app.$("#agentApiUrl").value.trim();
    const apiKeyInput = app.$("#agentApiKey");
    const apiKey = apiKeyInput.value.trim();
    if (!instruction) return app.toast("请输入 AI 图形处理指令", "error");
    if (!model) return app.toast("请输入模型名称", "error");
    if (!apiUrl) return app.toast("请输入 API 地址", "error");
    app.saveAgentSettings({ provider, model, apiUrl });
    if (!app.state.points.length && !app.state.lines.length) {
      return app.toast("当前没有可处理的图元", "error");
    }
    const button = app.$("#runAgentBtn");
    const status = app.$("#agentStatus");
    app.clearAgentResult();
    button.disabled = true;
    button.textContent = "Agent 处理中…";
    status.textContent = "运行中";
    status.classList.add("running");
    app.setStatus("AI Agent 正在处理图形副本…");
    try {
      const response = await app.requestAgentApi({
        instruction,
        provider,
        model,
        api_url: apiUrl,
        api_key: apiKey,
        selected_ids: app.selectedEntityIds(),
        geometry: { points: app.state.points, lines: app.state.lines }
      });
      const data = await response.json().catch(() => ({}));
      if (!response.ok) {
        throw new Error(data.error || `Agent 请求失败（HTTP ${response.status}）`);
      }
      if (!data.geometry || !Array.isArray(data.geometry.points) ||
        !Array.isArray(data.geometry.lines)) {
        throw new Error("Agent 返回的图形结果无效");
      }
      app.state.pendingAgentResult = data;
      app.$("#agentSummary").textContent = data.summary || "Agent 已完成处理";
      const beforeCount = (data.before?.point_count || 0) +
        (data.before?.line_count || 0);
      const afterCount = (data.after?.point_count || 0) +
        (data.after?.line_count || 0);
      const actions = Array.isArray(data.actions) && data.actions.length
        ? data.actions.join("；")
        : "未修改图元";
      app.$("#agentChanges").textContent =
        `${actions}。图元数量 ${beforeCount} → ${afterCount}`;
      app.$("#agentResult").classList.remove("hidden");
      status.textContent = "等待确认";
      app.setStatus("AI Agent 已生成预览结果");
      app.toast("Agent 处理完成，请确认是否应用");
    } catch (error) {
      status.textContent = "失败";
      app.setStatus("AI Agent 处理失败");
      const hint = location.protocol === "file:" || location.port === "5500"
        ? "；请先在项目根目录运行 node server.js"
        : "";
      app.toast(`${error.message}${hint}`, "error");
    } finally {
      button.disabled = false;
      button.textContent = "运行 Agent";
      status.classList.remove("running");
    }
  };

  app.applyAgentResult = function () {
    const result = app.state.pendingAgentResult;
    if (!result) return app.toast("没有可应用的 Agent 结果");
    app.snapshot();
    app.state.points = JSON.parse(JSON.stringify(result.geometry.points));
    app.state.lines = JSON.parse(JSON.stringify(result.geometry.lines));
    app.state.hasAgentChanges = true;
    app.state.selected = null;
    app.state.multiSelection = [];
    app.state.selectionBox = null;
    app.currentLayers().forEach((layer) => {
      app.state.layerVisibility[layer] = true;
    });
    app.setDirty(true);
    app.clearAgentResult();
    app.$("#agentStatus").textContent = "已应用";
    app.refresh();
    app.fitView();
    app.toast("AI Agent 结果已应用到工作副本，原图未修改");
  };
})(window);
