"use strict";

const assert = require("node:assert/strict");
const {
  loadSettings,
  saveSettings
} = require("../../page/agent-settings.js");
const { providerRequest } = require("../../provider-request.js");

function memoryStorage() {
  const values = new Map();
  return {
    getItem(key) { return values.has(key) ? values.get(key) : null; },
    setItem(key, value) { values.set(key, String(value)); },
    removeItem(key) { values.delete(key); }
  };
}

async function expectStatus(promise, status, forbiddenText = "") {
  await assert.rejects(promise, (error) => {
    assert.equal(error.status, status);
    if (forbiddenText) assert.equal(error.message.includes(forbiddenText), false);
    return true;
  });
}

async function main() {
  const storage = memoryStorage();
  const key = "dxf-agent-settings";
  storage.setItem(key, JSON.stringify({
    provider: "openai_chat",
    model: "test-model",
    apiUrl: "https://gateway.example/v1",
    apiKey: "legacy-secret"
  }));

  const loaded = loadSettings(storage, key);
  assert.deepEqual(loaded, {
    provider: "openai_chat",
    model: "test-model",
    apiUrl: "https://gateway.example/v1"
  });
  assert.equal(storage.getItem(key).includes("legacy-secret"), false);
  assert.equal(Object.hasOwn(JSON.parse(storage.getItem(key)), "apiKey"), false);

  saveSettings(storage, key, {
    provider: "anthropic",
    model: "claude-test",
    apiUrl: "https://api.anthropic.com",
    apiKey: "new-secret"
  });
  assert.equal(storage.getItem(key).includes("new-secret"), false);
  assert.equal(Object.hasOwn(JSON.parse(storage.getItem(key)), "apiKey"), false);

  const config = {
    provider: "openai_chat",
    endpoint: "https://gateway.example/v1/chat/completions",
    apiKey: "top-secret"
  };

  const neverResponds = (_url, options) => new Promise((_resolve, reject) => {
    options.signal.addEventListener("abort", () => {
      reject(Object.assign(new Error("aborted top-secret"), { name: "AbortError" }));
    }, { once: true });
  });
  await expectStatus(
    providerRequest(config, {}, { fetchImpl: neverResponds, timeoutMs: 20 }),
    504,
    "top-secret"
  );

  let stalledStreamCanceled = false;
  const stalledStream = new ReadableStream({
    cancel() { stalledStreamCanceled = true; }
  });
  await expectStatus(
    providerRequest(config, {}, {
      fetchImpl: async () => new Response(stalledStream, { status: 200 }),
      timeoutMs: 20
    }),
    504,
    "top-secret"
  );
  assert.equal(stalledStreamCanceled, true);

  const validResponse = await providerRequest(config, {}, {
    fetchImpl: async () => new Response('{"ok":true}', { status: 200 }),
    maxResponseBytes: 16,
    timeoutMs: 1000
  });
  assert.deepEqual(validResponse, { ok: true });

  let streamCanceled = false;
  const oversizedStream = new ReadableStream({
    start(controller) {
      controller.enqueue(new Uint8Array(12));
      controller.enqueue(new Uint8Array(12));
    },
    cancel() { streamCanceled = true; }
  });
  await expectStatus(
    providerRequest(config, {}, {
      fetchImpl: async () => new Response(oversizedStream, { status: 200 }),
      maxResponseBytes: 16,
      timeoutMs: 1000
    }),
    502,
    "top-secret"
  );
  assert.equal(streamCanceled, true);

  await expectStatus(
    providerRequest(config, {}, {
      fetchImpl: async () => new Response("upstream response secret", { status: 401 }),
      timeoutMs: 1000
    }),
    502,
    "upstream response secret"
  );

  await expectStatus(
    providerRequest(config, {}, {
      fetchImpl: async () => {
        throw Object.assign(new Error("network top-secret"), { status: 418 });
      },
      timeoutMs: 1000
    }),
    502,
    "top-secret"
  );

  console.log("agent-boundaries.test.js: ok");
}

main().catch((error) => {
  console.error(error);
  process.exitCode = 1;
});
