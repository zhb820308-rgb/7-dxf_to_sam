"use strict";

const crypto = require("crypto");

const API_CSRF_TOKEN = crypto.randomBytes(32).toString("hex");

function isLoopbackHostname(hostname) {
  return ["localhost", "127.0.0.1", "::1"].includes(String(hostname).toLowerCase());
}

function isTrustedBrowserOrigin(req) {
  const origin = req.headers.origin;
  if (!origin) return true;
  try {
    const url = new URL(origin);
    const originPort = Number(url.port || (url.protocol === "http:" ? 80 : 443));
    return url.protocol === "http:" && isLoopbackHostname(url.hostname) &&
      originPort === Number(req.socket.localPort);
  } catch {
    return false;
  }
}

function allowLocalCors(req, res) {
  const origin = req.headers.origin;
  if (origin && isTrustedBrowserOrigin(req)) {
    res.setHeader("Access-Control-Allow-Origin", origin);
    res.setHeader("Vary", "Origin");
    res.setHeader("Access-Control-Allow-Headers", "Content-Type, X-DXF-CSRF-Token");
    res.setHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
  }
}

function tokensEqual(actual, expected) {
  const actualBuffer = Buffer.from(String(actual || ""));
  const expectedBuffer = Buffer.from(String(expected || ""));
  return actualBuffer.length === expectedBuffer.length &&
    crypto.timingSafeEqual(actualBuffer, expectedBuffer);
}

function validateAgentRequest(req, expectedToken = API_CSRF_TOKEN) {
  if (!isTrustedBrowserOrigin(req)) {
    throw Object.assign(new Error("不允许来自非本地页面的 Agent 请求"), { status: 403 });
  }
  const contentType = String(req.headers["content-type"] || "").toLowerCase();
  if (!contentType.startsWith("application/json")) {
    throw Object.assign(new Error("Agent 请求必须使用 application/json"), { status: 415 });
  }
  if (!tokensEqual(req.headers["x-dxf-csrf-token"], expectedToken)) {
    throw Object.assign(new Error("Agent 请求缺少有效的 CSRF Token"), { status: 403 });
  }
}

module.exports = {
  API_CSRF_TOKEN,
  isTrustedBrowserOrigin,
  allowLocalCors,
  validateAgentRequest
};
