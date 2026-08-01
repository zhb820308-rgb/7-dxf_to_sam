"use strict";

const { json, readJson } = require("./http-utils.js");
const {
  API_CSRF_TOKEN,
  isTrustedBrowserOrigin,
  allowLocalCors,
  validateAgentRequest
} = require("./security.js");
const { MODEL, runGeometryAgent } = require("./provider-controller.js");
const { serveStatic } = require("./static-server.js");

async function handleRequest(req, res) {
  try {
    allowLocalCors(req, res);
    if (req.method === "OPTIONS" && req.url.startsWith("/api/")) {
      if (!isTrustedBrowserOrigin(req)) {
        return json(res, 403, { error: "不允许来自非本地页面的请求" });
      }
      res.writeHead(204);
      return res.end();
    }
    if (req.method === "GET" && req.url === "/api/session") {
      if (!isTrustedBrowserOrigin(req)) {
        return json(res, 403, { error: "不允许来自非本地页面的请求" });
      }
      return json(res, 200, { csrf_token: API_CSRF_TOKEN });
    }
    if (req.method === "GET" && req.url === "/api/health") {
      return json(res, 200, {
        ok: true,
        model: MODEL,
        api_key_configured: Boolean(process.env.OPENAI_API_KEY)
      });
    }
    if (req.method === "POST" && req.url === "/api/agent/process") {
      validateAgentRequest(req);
      const body = await readJson(req);
      return json(res, 200, await runGeometryAgent(body));
    }
    if (req.method === "GET" || req.method === "HEAD") return serveStatic(req, res);
    return json(res, 405, { error: "不支持的请求方法" });
  } catch (error) {
    json(res, error.status || 500, { error: error.message || "服务器内部错误" });
  }
}

module.exports = { handleRequest };
