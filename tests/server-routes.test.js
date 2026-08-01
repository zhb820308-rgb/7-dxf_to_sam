"use strict";

const assert = require("node:assert/strict");
const http = require("node:http");
const { handleRequest } = require("../server/router.js");

function request(port, path, options = {}) {
  return new Promise((resolve, reject) => {
    const req = http.request({
      host: "127.0.0.1",
      port,
      path,
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
  const server = http.createServer(handleRequest);
  await new Promise((resolve, reject) => {
    server.once("error", reject);
    server.listen(0, "127.0.0.1", resolve);
  });
  const port = server.address().port;
  const origin = `http://127.0.0.1:${port}`;

  try {
    const health = await request(port, "/api/health");
    assert.equal(health.status, 200);
    assert.equal(JSON.parse(health.body).ok, true);

    const session = await request(port, "/api/session", { headers: { origin } });
    assert.equal(session.status, 200);
    assert.equal(session.headers["access-control-allow-origin"], origin);
    assert.match(JSON.parse(session.body).csrf_token, /^[a-f0-9]{64}$/);

    const rejectedOrigin = await request(port, "/api/session", {
      headers: { origin: "https://evil.example" }
    });
    assert.equal(rejectedOrigin.status, 403);

    const missingToken = await request(port, "/api/agent/process", {
      method: "POST",
      headers: { origin, "content-type": "application/json" },
      body: "{}"
    });
    assert.equal(missingToken.status, 403);

    const index = await request(port, "/");
    assert.equal(index.status, 200);
    assert.match(index.headers["content-type"], /^text\/html/);
    assert.match(index.body, /DXF Studio/);

    const unsupported = await request(port, "/api/health", { method: "PUT" });
    assert.equal(unsupported.status, 405);
  } finally {
    await new Promise((resolve, reject) => server.close((error) => error ? reject(error) : resolve()));
  }
}

main().then(
  () => console.log("server-routes.test.js: ok"),
  (error) => {
    console.error(error);
    process.exitCode = 1;
  }
);
