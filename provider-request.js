"use strict";

const MAX_UPSTREAM_RESPONSE_BYTES = 4 * 1024 * 1024;
const UPSTREAM_TIMEOUT_MS = 30 * 1000;

class ProviderRequestError extends Error {
  constructor(message, status) {
    super(message);
    this.status = status;
  }
}

async function cancelResponseBody(response) {
  try {
    if (response.body) await response.body.cancel();
  } catch {
    // The connection may already have been aborted or closed by the peer.
  }
}

function readNextChunk(reader, signal) {
  if (signal.aborted) {
    return Promise.reject(Object.assign(new Error("aborted"), { name: "AbortError" }));
  }
  return new Promise((resolve, reject) => {
    const onAbort = () => reject(Object.assign(new Error("aborted"), { name: "AbortError" }));
    signal.addEventListener("abort", onAbort, { once: true });
    reader.read().then(
      (value) => {
        signal.removeEventListener("abort", onAbort);
        resolve(value);
      },
      (error) => {
        signal.removeEventListener("abort", onAbort);
        reject(error);
      }
    );
  });
}

async function readBoundedResponse(response, controller, maxResponseBytes) {
  const declaredLength = Number(response.headers.get("content-length"));
  if (Number.isFinite(declaredLength) && declaredLength > maxResponseBytes) {
    await cancelResponseBody(response);
    controller.abort();
    throw new ProviderRequestError("AI API 响应体超过大小上限", 502);
  }

  if (!response.body) return Buffer.alloc(0);
  const reader = response.body.getReader();
  const chunks = [];
  let totalBytes = 0;
  try {
    while (true) {
      const { done, value } = await readNextChunk(reader, controller.signal);
      if (done) break;
      totalBytes += value.byteLength;
      if (totalBytes > maxResponseBytes) {
        await reader.cancel().catch(() => {});
        controller.abort();
        throw new ProviderRequestError("AI API 响应体超过大小上限", 502);
      }
      chunks.push(Buffer.from(value));
    }
  } catch (error) {
    await reader.cancel().catch(() => {});
    throw error;
  }
  return Buffer.concat(chunks, totalBytes);
}

async function providerRequest(config, payload, options = {}) {
  const fetchImpl = options.fetchImpl || fetch;
  const timeoutMs = Number.isFinite(options.timeoutMs) && options.timeoutMs > 0
    ? options.timeoutMs : UPSTREAM_TIMEOUT_MS;
  const maxResponseBytes = Number.isFinite(options.maxResponseBytes) && options.maxResponseBytes > 0
    ? options.maxResponseBytes : MAX_UPSTREAM_RESPONSE_BYTES;
  const headers = { "Content-Type": "application/json" };
  if (config.provider === "anthropic") {
    headers["x-api-key"] = config.apiKey;
    headers["anthropic-version"] = "2023-06-01";
  } else {
    headers.Authorization = `Bearer ${config.apiKey}`;
  }
  const controller = new AbortController();
  let timedOut = false;
  const timer = setTimeout(() => {
    timedOut = true;
    controller.abort();
  }, timeoutMs);

  try {
    const response = await fetchImpl(config.endpoint, {
      method: "POST",
      headers,
      body: JSON.stringify(payload),
      redirect: "error",
      signal: controller.signal
    });
    if (!response.ok) {
      await cancelResponseBody(response);
      throw new ProviderRequestError(`AI API 请求失败（HTTP ${response.status}）`, 502);
    }
    const rawBody = await readBoundedResponse(response, controller, maxResponseBytes);
    try {
      return JSON.parse(rawBody.toString("utf8"));
    } catch {
      throw new ProviderRequestError("AI API 返回的 JSON 无效", 502);
    }
  } catch (error) {
    if (error instanceof ProviderRequestError) throw error;
    if (timedOut || error.name === "AbortError") {
      throw new ProviderRequestError("AI API 请求超时", 504);
    }
    throw new ProviderRequestError("无法连接 AI API", 502);
  } finally {
    clearTimeout(timer);
  }
}

module.exports = {
  MAX_UPSTREAM_RESPONSE_BYTES,
  UPSTREAM_TIMEOUT_MS,
  providerRequest
};
