"use strict";

const assert = require("node:assert/strict");
const http = require("node:http");
const { createRequestHandler } = require("../../server/router.js");

function request(port, route, options = {}) {
  return new Promise((resolve, reject) => {
    const req = http.request({
      host: "127.0.0.1",
      port,
      path: route,
      method: options.method || "GET",
      headers: options.headers || {}
    }, (res) => {
      const chunks = [];
      res.on("data", (chunk) => chunks.push(chunk));
      res.on("end", () => resolve({
        status: res.statusCode,
        headers: res.headers,
        body: Buffer.concat(chunks).toString("utf8")
      }));
    });
    req.on("error", reject);
    if (options.body) req.write(options.body);
    req.end();
  });
}

async function main() {
  const csrfToken = "injected-test-token";
  const receivedBodies = [];
  const handler = createRequestHandler({
    model: "injected-test-model",
    csrfToken,
    agentRunner: async (body) => {
      receivedBodies.push(body);
      if (body.fail) {
        throw Object.assign(new Error("injected failure"), { status: 422 });
      }
      return { ok: true, source: "injected-agent" };
    },
    staticHandler: (req, res) => {
      res.writeHead(209, { "Content-Type": "text/plain" });
      res.end(`static:${req.url}`);
    }
  });
  const server = http.createServer(handler);
  await new Promise((resolve) => server.listen(0, "127.0.0.1", resolve));
  const port = server.address().port;
  const origin = `http://127.0.0.1:${port}`;

  try {
    const session = await request(port, "/api/session", {
      headers: { origin }
    });
    assert.equal(session.status, 200);
    assert.deepEqual(JSON.parse(session.body), { csrf_token: csrfToken });

    const health = await request(port, "/api/health");
    assert.equal(health.status, 200);
    assert.equal(JSON.parse(health.body).model, "injected-test-model");

    const headers = {
      origin,
      "content-type": "application/json",
      "x-dxf-csrf-token": csrfToken
    };
    const processed = await request(port, "/api/agent/process", {
      method: "POST",
      headers,
      body: JSON.stringify({ instruction: "test" })
    });
    assert.equal(processed.status, 200);
    assert.deepEqual(JSON.parse(processed.body), {
      ok: true,
      source: "injected-agent"
    });
    assert.deepEqual(receivedBodies, [{ instruction: "test" }]);

    const failed = await request(port, "/api/agent/process", {
      method: "POST",
      headers,
      body: JSON.stringify({ fail: true })
    });
    assert.equal(failed.status, 422);
    assert.deepEqual(JSON.parse(failed.body), { error: "injected failure" });

    const staticResponse = await request(port, "/module-owned-resource");
    assert.equal(staticResponse.status, 209);
    assert.equal(staticResponse.body, "static:/module-owned-resource");
  } finally {
    await new Promise((resolve) => server.close(resolve));
  }
}

main().then(
  () => console.log("server-module-boundaries.test.js: ok"),
  (error) => {
    console.error(error);
    process.exitCode = 1;
  }
);
