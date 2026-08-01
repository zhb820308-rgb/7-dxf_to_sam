"use strict";

const http = require("http");
const { providerRequest } = require("./provider-request.js");
const {
  executeAgentTool,
  validateGeometry,
  geometrySummary
} = require("./server/geometry-tools.js");
const {
  MODEL,
  runGeometryAgent,
  resolveProviderConfig
} = require("./server/provider-controller.js");
const {
  isTrustedBrowserOrigin,
  validateAgentRequest
} = require("./server/security.js");
const { handleRequest } = require("./server/router.js");

const PORT = Number(process.env.PORT) || 8080;
const server = http.createServer(handleRequest);

function startServer(port = PORT) {
  if (server.listening) return server;
  const requestedPort = Number(port);
  const hasExplicitPort = Object.prototype.hasOwnProperty.call(process.env, "PORT");

  const listen = (candidatePort) => {
    const handleListening = () => {
      server.removeListener("error", handleError);
      console.log(`DXF Studio: http://127.0.0.1:${candidatePort}`);
      console.log(`AI model: ${MODEL}; API key: ${process.env.OPENAI_API_KEY ? "configured" : "missing"}`);
    };
    const handleError = (error) => {
      server.removeListener("listening", handleListening);
      if (!hasExplicitPort && candidatePort === 8080 && (error.code === "EACCES" || error.code === "EADDRINUSE")) {
        console.warn(`端口 8080 无法使用（${error.code}），自动切换到 18080。`);
        listen(18080);
        return;
      }
      console.error(`服务启动失败：无法监听 127.0.0.1:${candidatePort}（${error.code || error.message}）`);
      process.exitCode = 1;
    };
    server.once("error", handleError);
    server.once("listening", handleListening);
    server.listen(candidatePort, "127.0.0.1");
  };

  listen(requestedPort);
  return server;
}

if (require.main === module) {
  startServer();
}

module.exports = {
  runGeometryAgent,
  executeAgentTool,
  validateGeometry,
  geometrySummary,
  isTrustedBrowserOrigin,
  resolveProviderConfig,
  providerRequest,
  validateAgentRequest,
  server,
  startServer
};
