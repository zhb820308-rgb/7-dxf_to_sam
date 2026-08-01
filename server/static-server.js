"use strict";

const fs = require("fs");
const path = require("path");
const { json } = require("./http-utils.js");

const PAGE_ROOT = path.join(__dirname, "..", "page");
const mimeTypes = {
  ".html": "text/html; charset=utf-8",
  ".js": "text/javascript; charset=utf-8",
  ".css": "text/css; charset=utf-8",
  ".md": "text/markdown; charset=utf-8",
  ".json": "application/json; charset=utf-8",
  ".dxf": "application/dxf; charset=utf-8",
  ".svg": "image/svg+xml",
  ".png": "image/png"
};

function serveStatic(req, res) {
  const url = new URL(req.url, "http://localhost");
  const relative = decodeURIComponent(url.pathname === "/" ? "/index.html" : url.pathname);
  const target = path.resolve(PAGE_ROOT, `.${relative}`);
  if (target !== PAGE_ROOT && !target.startsWith(`${PAGE_ROOT}${path.sep}`)) {
    return json(res, 403, { error: "禁止访问" });
  }
  fs.stat(target, (error, stat) => {
    if (error || !stat.isFile()) return json(res, 404, { error: "文件不存在" });
    res.writeHead(200, {
      "Content-Type": mimeTypes[path.extname(target).toLowerCase()] || "application/octet-stream",
      "Cache-Control": "no-cache"
    });
    fs.createReadStream(target).pipe(res);
  });
}

module.exports = { serveStatic };
