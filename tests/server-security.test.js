"use strict";

const assert = require("node:assert/strict");
const {
  isTrustedBrowserOrigin,
  resolveProviderConfig,
  validateAgentRequest
} = require("../server.js");

function fakeRequest(headers = {}, localPort = 8080) {
  return { headers, socket: { localPort } };
}

assert.equal(
  isTrustedBrowserOrigin(fakeRequest({ origin: "http://127.0.0.1:8080" })),
  true
);
assert.equal(
  isTrustedBrowserOrigin(fakeRequest({ origin: "http://localhost:8080" })),
  true
);
assert.equal(
  isTrustedBrowserOrigin(fakeRequest({ origin: "https://evil.example" })),
  false
);
assert.equal(
  isTrustedBrowserOrigin(fakeRequest({ origin: "http://127.0.0.1:18080" })),
  false
);

assert.throws(
  () => validateAgentRequest(fakeRequest({
    origin: "https://evil.example",
    "content-type": "application/json",
    "x-dxf-csrf-token": "known-token"
  }), "known-token"),
  (error) => error.status === 403
);
assert.throws(
  () => validateAgentRequest(fakeRequest({
    origin: "http://127.0.0.1:8080",
    "content-type": "text/plain",
    "x-dxf-csrf-token": "known-token"
  }), "known-token"),
  (error) => error.status === 415
);
assert.throws(
  () => validateAgentRequest(fakeRequest({
    origin: "http://127.0.0.1:8080",
    "content-type": "application/json",
    "x-dxf-csrf-token": "wrong-token"
  }), "known-token"),
  (error) => error.status === 403
);
assert.doesNotThrow(() => validateAgentRequest(fakeRequest({
  origin: "http://127.0.0.1:8080",
  "content-type": "application/json; charset=utf-8",
  "x-dxf-csrf-token": "known-token"
}), "known-token"));

const savedOpenAiKey = process.env.OPENAI_API_KEY;
const savedAnthropicKey = process.env.ANTHROPIC_API_KEY;
const savedOpenAiBase = process.env.OPENAI_BASE_URL;
const savedAnthropicBase = process.env.ANTHROPIC_BASE_URL;
try {
  process.env.OPENAI_API_KEY = "server-only-secret";
  delete process.env.ANTHROPIC_API_KEY;
  delete process.env.OPENAI_BASE_URL;
  delete process.env.ANTHROPIC_BASE_URL;

  const serverCredential = resolveProviderConfig({
    provider: "openai_responses",
    model: "test-model",
    api_url: "https://attacker.example/collect",
    api_key: ""
  });
  assert.equal(serverCredential.credentialSource, "server");
  assert.equal(serverCredential.endpoint, "https://api.openai.com/v1/responses");

  const userCredential = resolveProviderConfig({
    provider: "openai_chat",
    model: "test-model",
    api_url: "https://gateway.example/v1",
    api_key: "user-supplied-secret"
  });
  assert.equal(userCredential.credentialSource, "request");
  assert.equal(userCredential.endpoint, "https://gateway.example/v1/chat/completions");

  assert.throws(
    () => resolveProviderConfig({
      provider: "anthropic",
      model: "test-model",
      api_url: "https://api.anthropic.com",
      api_key: ""
    }),
    (error) => error.status === 503
  );
} finally {
  if (savedOpenAiKey === undefined) delete process.env.OPENAI_API_KEY;
  else process.env.OPENAI_API_KEY = savedOpenAiKey;
  if (savedAnthropicKey === undefined) delete process.env.ANTHROPIC_API_KEY;
  else process.env.ANTHROPIC_API_KEY = savedAnthropicKey;
  if (savedOpenAiBase === undefined) delete process.env.OPENAI_BASE_URL;
  else process.env.OPENAI_BASE_URL = savedOpenAiBase;
  if (savedAnthropicBase === undefined) delete process.env.ANTHROPIC_BASE_URL;
  else process.env.ANTHROPIC_BASE_URL = savedAnthropicBase;
}

console.log("server-security.test.js: ok");
