"use strict";

const http = require("http");
const fs = require("fs");
const path = require("path");
const crypto = require("crypto");

const ROOT = __dirname;
const PAGE_ROOT = path.join(ROOT, "page");
const PORT = Number(process.env.PORT) || 8080;
const MODEL = process.env.OPENAI_MODEL || "gpt-5.6";
const DEFAULT_OPENAI_API_BASE = "https://api.openai.com/v1";
const DEFAULT_ANTHROPIC_API_BASE = "https://api.anthropic.com";
const MAX_BODY_BYTES = 16 * 1024 * 1024;
const MAX_ENTITIES = 100000;
const MAX_AGENT_STEPS = 8;
const API_CSRF_TOKEN = crypto.randomBytes(32).toString("hex");

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

function json(res, status, data) {
  const body = JSON.stringify(data);
  res.writeHead(status, {
    "Content-Type": "application/json; charset=utf-8",
    "Content-Length": Buffer.byteLength(body),
    "Cache-Control": "no-store"
  });
  res.end(body);
}

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

function readJson(req) {
  return new Promise((resolve, reject) => {
    const chunks = [];
    let size = 0;
    req.on("data", (chunk) => {
      size += chunk.length;
      if (size > MAX_BODY_BYTES) {
        reject(Object.assign(new Error("请求数据超过 16 MB"), { status: 413 }));
        req.destroy();
        return;
      }
      chunks.push(chunk);
    });
    req.on("end", () => {
      try {
        resolve(JSON.parse(Buffer.concat(chunks).toString("utf8") || "{}"));
      } catch {
        reject(Object.assign(new Error("请求 JSON 格式无效"), { status: 400 }));
      }
    });
    req.on("error", reject);
  });
}

function finiteNumber(value, field) {
  const result = Number(value);
  if (!Number.isFinite(result) || Math.abs(result) > 1e15) {
    throw Object.assign(new Error(`${field} 必须是有效有限数值`), { status: 400 });
  }
  return result;
}

function shortText(value, fallback, max = 120) {
  const text = String(value == null ? fallback : value).trim();
  return text.slice(0, max) || fallback;
}

function annotationMetadata(item) {
  const confidence = item.annotationConfidence == null ? null : Number(item.annotationConfidence);
  return {
    sourceId: item.sourceId == null ? null : shortText(item.sourceId, "", 100),
    annotationKind: item.annotationKind == null ? null : shortText(item.annotationKind, "", 40),
    annotationConfidence: Number.isFinite(confidence) ? Math.max(0, Math.min(1, confidence)) : null,
    annotationSource: item.annotationSource == null ? null : shortText(item.annotationSource, "", 80),
    annotationText: item.annotationText == null ? null : shortText(item.annotationText, "", 300)
  };
}

function insertLayerMetadata(item) {
  if (!Array.isArray(item.insertLayers)) return { insertLayers: [] };
  return {
    insertLayers: item.insertLayers.slice(0, 64).map((layer) => shortText(layer, "0"))
  };
}

function affectsLayer(item, layer) {
  return item.layer === layer || item.insertLayers.includes(layer);
}

function validateGeometry(input) {
  if (!input || !Array.isArray(input.points) || !Array.isArray(input.lines)) {
    throw Object.assign(new Error("geometry 必须包含 points 和 lines 数组"), { status: 400 });
  }
  if (input.points.length + input.lines.length > MAX_ENTITIES) {
    throw Object.assign(new Error(`图元数量不能超过 ${MAX_ENTITIES}`), { status: 413 });
  }
  const ids = new Set();
  const uniqueId = (raw, prefix, index) => {
    let id = shortText(raw, `${prefix}-${index}`, 100);
    while (ids.has(id)) id = `${prefix}-${index}-${crypto.randomBytes(3).toString("hex")}`;
    ids.add(id);
    return id;
  };
  const points = input.points.map((point, index) => ({
    id: uniqueId(point.id, "p", index),
    x: finiteNumber(point.x, "point.x"),
    y: finiteNumber(point.y, "point.y"),
    layer: shortText(point.layer, "0"),
    ...insertLayerMetadata(point),
    sourceType: shortText(point.sourceType, "POINT", 40),
    ...annotationMetadata(point),
    visible: true
  }));
  const lines = input.lines.map((line, index) => ({
    id: uniqueId(line.id, "l", index),
    x1: finiteNumber(line.x1, "line.x1"),
    y1: finiteNumber(line.y1, "line.y1"),
    x2: finiteNumber(line.x2, "line.x2"),
    y2: finiteNumber(line.y2, "line.y2"),
    layer: shortText(line.layer, "0"),
    ...insertLayerMetadata(line),
    sourceType: shortText(line.sourceType, "LINE", 40),
    ...annotationMetadata(line),
    visible: true
  }));
  return { points, lines };
}

function geometrySummary(geometry, selectedIds = new Set()) {
  const layers = {};
  const annotations = {};
  let minX = Infinity, minY = Infinity, maxX = -Infinity, maxY = -Infinity;
  const include = (x, y) => {
    minX = Math.min(minX, x); minY = Math.min(minY, y);
    maxX = Math.max(maxX, x); maxY = Math.max(maxY, y);
  };
  geometry.points.forEach((point) => {
    new Set([point.layer, ...point.insertLayers]).forEach((layer) => {
      layers[layer] = (layers[layer] || 0) + 1;
    });
    if (point.annotationKind) annotations[point.annotationKind] = (annotations[point.annotationKind] || 0) + 1;
    include(point.x, point.y);
  });
  geometry.lines.forEach((line) => {
    new Set([line.layer, ...line.insertLayers]).forEach((layer) => {
      layers[layer] = (layers[layer] || 0) + 1;
    });
    if (line.annotationKind) annotations[line.annotationKind] = (annotations[line.annotationKind] || 0) + 1;
    include(line.x1, line.y1); include(line.x2, line.y2);
  });
  const entityIds = new Set([
    ...geometry.points.map((item) => item.id),
    ...geometry.lines.map((item) => item.id)
  ]);
  return {
    point_count: geometry.points.length,
    line_count: geometry.lines.length,
    selected_count: [...selectedIds].filter((id) => entityIds.has(id)).length,
    layers,
    annotations,
    bounds: Number.isFinite(minX) ? { min_x: minX, min_y: minY, max_x: maxX, max_y: maxY } : null
  };
}

function matchingEntities(geometry, selectedIds, scope, layer, ids = []) {
  const requestedIds = new Set(Array.isArray(ids) ? ids.map(String) : []);
  const match = (item) =>
    scope === "all" ||
    (scope === "selected" && selectedIds.has(item.id)) ||
    (scope === "layer" && affectsLayer(item, layer)) ||
    (scope === "ids" && requestedIds.has(item.id));
  return {
    points: geometry.points.filter(match),
    lines: geometry.lines.filter(match),
    match
  };
}

function transformEntityPoint(x, y, args, center) {
  const radians = args.rotate_degrees * Math.PI / 180;
  const cos = Math.cos(radians);
  const sin = Math.sin(radians);
  const localX = (x - center.x) * args.scale;
  const localY = (y - center.y) * args.scale;
  return {
    x: center.x + localX * cos - localY * sin + args.translate_x,
    y: center.y + localX * sin + localY * cos + args.translate_y
  };
}

function executeAgentTool(name, args, context) {
  const { geometry, selectedIds, actionLog } = context;
  if (name === "inspect_geometry") {
    const matched = matchingEntities(geometry, selectedIds, args.scope, args.layer, args.ids);
    const limit = Math.max(1, Math.min(200, args.limit));
    const offset = Math.max(0, Math.floor(args.offset));
    const entities = [
      ...matched.points.map((entity) => ({ kind: "point", entity })),
      ...matched.lines.map((entity) => ({ kind: "line", entity }))
    ];
    const page = entities.slice(offset, offset + limit);
    const points = page.filter((item) => item.kind === "point").map((item) => item.entity);
    const lines = page.filter((item) => item.kind === "line").map((item) => item.entity);
    const nextOffset = offset + page.length;
    return {
      summary: geometrySummary({ points: matched.points, lines: matched.lines }, selectedIds),
      points,
      lines,
      offset,
      returned: page.length,
      truncated: nextOffset < entities.length,
      next_offset: nextOffset < entities.length ? nextOffset : null
    };
  }

  if (name === "measure_geometry") {
    const matched = matchingEntities(geometry, selectedIds, args.scope, args.layer, args.ids);
    const byLayer = {};
    matched.points.forEach((point) => {
      byLayer[point.layer] ||= { point_count: 0, line_count: 0, total_length: 0 };
      byLayer[point.layer].point_count += 1;
    });
    let totalLength = 0;
    matched.lines.forEach((line) => {
      const length = Math.hypot(line.x2 - line.x1, line.y2 - line.y1);
      totalLength += length;
      byLayer[line.layer] ||= { point_count: 0, line_count: 0, total_length: 0 };
      byLayer[line.layer].line_count += 1;
      byLayer[line.layer].total_length += length;
    });
    return {
      ...geometrySummary({ points: matched.points, lines: matched.lines }, selectedIds),
      total_line_length: totalLength,
      by_layer: byLayer
    };
  }

  if (name === "find_annotation_lines") {
    const requestedKinds = new Set(args.kinds);
    const minimum = Math.max(0, Math.min(1, Number(args.min_confidence)));
    const matched = geometry.lines.filter((line) =>
      line.annotationKind &&
      (!requestedKinds.size || requestedKinds.has(line.annotationKind)) &&
      (args.layer == null || line.layer === args.layer) &&
      (line.annotationConfidence ?? 0) >= minimum);
    const offset = Math.max(0, Math.floor(args.offset));
    const limit = Math.max(1, Math.min(200, args.limit));
    const lines = matched.slice(offset, offset + limit);
    const byKind = {};
    matched.forEach((line) => {
      byKind[line.annotationKind] = (byKind[line.annotationKind] || 0) + 1;
    });
    const nextOffset = offset + lines.length;
    return {
      total: matched.length,
      by_kind: byKind,
      lines,
      returned: lines.length,
      next_offset: nextOffset < matched.length ? nextOffset : null,
      note: "dimension/leader 为 DXF 原生确定标注；dimension_candidate 仅为炸开标注候选。"
    };
  }

  if (name === "transform_geometry") {
    if (!(args.scale > 0 && args.scale <= 1000000)) throw new Error("scale 必须大于 0 且不超过 1000000");
    args = {
      ...args,
      translate_x: finiteNumber(args.translate_x, "translate_x"),
      translate_y: finiteNumber(args.translate_y, "translate_y"),
      rotate_degrees: finiteNumber(args.rotate_degrees, "rotate_degrees"),
      scale: finiteNumber(args.scale, "scale"),
      center_x: args.center_x == null ? null : finiteNumber(args.center_x, "center_x"),
      center_y: args.center_y == null ? null : finiteNumber(args.center_y, "center_y")
    };
    const matched = matchingEntities(geometry, selectedIds, args.scope, args.layer, args.ids);
    const summary = geometrySummary({ points: matched.points, lines: matched.lines }, selectedIds);
    const center = {
      x: args.center_x == null ? ((summary.bounds?.min_x || 0) + (summary.bounds?.max_x || 0)) / 2 : args.center_x,
      y: args.center_y == null ? ((summary.bounds?.min_y || 0) + (summary.bounds?.max_y || 0)) / 2 : args.center_y
    };
    matched.points.forEach((point) => Object.assign(point, transformEntityPoint(point.x, point.y, args, center)));
    matched.lines.forEach((line) => {
      const start = transformEntityPoint(line.x1, line.y1, args, center);
      const end = transformEntityPoint(line.x2, line.y2, args, center);
      Object.assign(line, { x1: start.x, y1: start.y, x2: end.x, y2: end.y });
    });
    const count = matched.points.length + matched.lines.length;
    actionLog.push(`变换 ${count} 个图元`);
    return { ok: true, changed_count: count, summary: geometrySummary(geometry, selectedIds) };
  }

  if (name === "cleanup_geometry") {
    const matched = matchingEntities(geometry, selectedIds, args.scope, args.layer, args.ids);
    let snapped = 0;
    if (args.snap_grid != null) {
      if (!(args.snap_grid > 0)) throw new Error("snap_grid 必须大于 0");
      const snap = (value) => Math.round(value / args.snap_grid) * args.snap_grid;
      matched.points.forEach((point) => { point.x = snap(point.x); point.y = snap(point.y); snapped += 1; });
      matched.lines.forEach((line) => {
        line.x1 = snap(line.x1); line.y1 = snap(line.y1);
        line.x2 = snap(line.x2); line.y2 = snap(line.y2); snapped += 1;
      });
    }
    let removed = 0;
    if (args.remove_zero_length) {
      const before = geometry.lines.length;
      geometry.lines = geometry.lines.filter((line) =>
        !matched.match(line) || Math.hypot(line.x2 - line.x1, line.y2 - line.y1) > 1e-9);
      removed += before - geometry.lines.length;
    }
    if (args.remove_duplicates) {
      const seen = new Set();
      geometry.lines = geometry.lines.filter((line) => {
        if (!matched.match(line)) return true;
        const a = `${line.x1.toPrecision(12)},${line.y1.toPrecision(12)}`;
        const b = `${line.x2.toPrecision(12)},${line.y2.toPrecision(12)}`;
        const key = a < b ? `${line.layer}|${a}|${b}` : `${line.layer}|${b}|${a}`;
        if (seen.has(key)) { removed += 1; return false; }
        seen.add(key); return true;
      });
    }
    actionLog.push(`清理图元：吸附 ${snapped}，删除 ${removed}`);
    return { ok: true, snapped_count: snapped, removed_count: removed, summary: geometrySummary(geometry, selectedIds) };
  }

  if (name === "delete_geometry") {
    const matched = matchingEntities(geometry, selectedIds, args.scope, args.layer, args.ids);
    const before = geometry.points.length + geometry.lines.length;
    geometry.points = geometry.points.filter((item) => !matched.match(item));
    geometry.lines = geometry.lines.filter((item) => !matched.match(item));
    const removed = before - geometry.points.length - geometry.lines.length;
    actionLog.push(`删除 ${removed} 个图元`);
    return { ok: true, removed_count: removed, summary: geometrySummary(geometry, selectedIds) };
  }

  if (name === "copy_geometry") {
    const matched = matchingEntities(geometry, selectedIds, args.scope, args.layer, args.ids);
    const count = matched.points.length + matched.lines.length;
    if (count > 10000) throw new Error("单次最多复制 10000 个图元");
    if (geometry.points.length + geometry.lines.length + count > MAX_ENTITIES) {
      throw new Error(`图元总数不能超过 ${MAX_ENTITIES}`);
    }
    const dx = finiteNumber(args.translate_x, "translate_x");
    const dy = finiteNumber(args.translate_y, "translate_y");
    const targetLayer = args.target_layer == null ? null : shortText(args.target_layer, "0");
    matched.points.forEach((point) => geometry.points.push({
      ...point,
      id: `ai-p-${crypto.randomBytes(6).toString("hex")}`,
      x: point.x + dx,
      y: point.y + dy,
      layer: targetLayer || point.layer,
      insertLayers: [],
      sourceType: "AI_COPY"
    }));
    matched.lines.forEach((line) => geometry.lines.push({
      ...line,
      id: `ai-l-${crypto.randomBytes(6).toString("hex")}`,
      x1: line.x1 + dx,
      y1: line.y1 + dy,
      x2: line.x2 + dx,
      y2: line.y2 + dy,
      layer: targetLayer || line.layer,
      insertLayers: [],
      sourceType: "AI_COPY"
    }));
    actionLog.push(`复制 ${count} 个图元并平移 (${dx}, ${dy})`);
    return { ok: true, copied_count: count, summary: geometrySummary(geometry, selectedIds) };
  }

  if (name === "set_geometry_layer") {
    const matched = matchingEntities(geometry, selectedIds, args.scope, args.layer, args.ids);
    const targetLayer = shortText(args.target_layer, "0");
    matched.points.forEach((point) => { point.layer = targetLayer; });
    matched.lines.forEach((line) => { line.layer = targetLayer; });
    const count = matched.points.length + matched.lines.length;
    actionLog.push(`将 ${count} 个图元移动到图层 ${targetLayer}`);
    return { ok: true, changed_count: count, target_layer: targetLayer, summary: geometrySummary(geometry, selectedIds) };
  }

  if (name === "add_geometry") {
    if (args.points.length + args.lines.length > 500) throw new Error("单次最多添加 500 个图元");
    if (geometry.points.length + geometry.lines.length + args.points.length + args.lines.length > MAX_ENTITIES) {
      throw new Error(`图元总数不能超过 ${MAX_ENTITIES}`);
    }
    for (const point of args.points) {
      geometry.points.push({
        id: `ai-p-${crypto.randomBytes(6).toString("hex")}`,
        x: finiteNumber(point.x, "point.x"), y: finiteNumber(point.y, "point.y"),
        layer: shortText(point.layer, "AI"), insertLayers: [],
        sourceType: "AI_POINT", visible: true
      });
    }
    for (const line of args.lines) {
      geometry.lines.push({
        id: `ai-l-${crypto.randomBytes(6).toString("hex")}`,
        x1: finiteNumber(line.x1, "line.x1"), y1: finiteNumber(line.y1, "line.y1"),
        x2: finiteNumber(line.x2, "line.x2"), y2: finiteNumber(line.y2, "line.y2"),
        layer: shortText(line.layer, "AI"), insertLayers: [],
        sourceType: "AI_LINE", visible: true
      });
    }
    const added = args.points.length + args.lines.length;
    actionLog.push(`添加 ${added} 个图元`);
    return { ok: true, added_count: added, summary: geometrySummary(geometry, selectedIds) };
  }

  if (name === "finish") {
    context.finished = true;
    context.agentSummary = shortText(args.summary, "AI Agent 已完成处理", 500);
    return { ok: true, final_summary: geometrySummary(geometry, selectedIds) };
  }

  throw new Error(`未知工具：${name}`);
}

const scopeProperties = {
  scope: { type: "string", enum: ["all", "selected", "layer", "ids"], description: "操作范围。selected 是用户选择，layer 是指定图层，ids 是精确实体列表。" },
  layer: { type: ["string", "null"], description: "scope=layer 时的图层名，否则为 null。" },
  ids: {
    type: "array",
    maxItems: 200,
    items: { type: "string" },
    description: "scope=ids 时使用 inspect_geometry 获得的实体 ID；其他范围传空数组。"
  }
};
const scopeRequired = ["scope", "layer", "ids"];

const tools = [
  {
    type: "function", name: "inspect_geometry", strict: true,
    description: "查看指定范围内的图形摘要和少量实体坐标。需要具体坐标证据时使用。",
    parameters: {
      type: "object", additionalProperties: false,
      properties: {
        ...scopeProperties,
        offset: { type: "integer", minimum: 0, description: "分页起点，首次传 0，之后使用上次返回的 next_offset。" },
        limit: { type: "integer", minimum: 1, maximum: 200 }
      },
      required: [...scopeRequired, "offset", "limit"]
    }
  },
  {
    type: "function", name: "measure_geometry", strict: true,
    description: "确定性测量指定范围的点数、线数、总线长、包围盒和各图层统计，不修改图形。",
    parameters: {
      type: "object", additionalProperties: false,
      properties: scopeProperties,
      required: scopeRequired
    }
  },
  {
    type: "function", name: "find_annotation_lines", strict: true,
    description: "确定性查找 DXF 标注线。原生 DIMENSION/LEADER 置信度为 1；炸开后仅凭标注图层识别的线为 dimension_candidate。",
    parameters: {
      type: "object", additionalProperties: false,
      properties: {
        kinds: {
          type: "array",
          items: { type: "string", enum: ["dimension", "leader", "dimension_candidate"] },
          description: "需要的标注类型；空数组表示全部。"
        },
        layer: { type: ["string", "null"], description: "可选图层过滤，否则为 null。" },
        min_confidence: { type: "number", minimum: 0, maximum: 1 },
        offset: { type: "integer", minimum: 0 },
        limit: { type: "integer", minimum: 1, maximum: 200 }
      },
      required: ["kinds", "layer", "min_confidence", "offset", "limit"]
    }
  },
  {
    type: "function", name: "transform_geometry", strict: true,
    description: "平移、旋转或等比缩放指定范围内的图元。多个变换可在一次调用中组合。",
    parameters: {
      type: "object", additionalProperties: false,
      properties: {
        ...scopeProperties,
        translate_x: { type: "number" }, translate_y: { type: "number" },
        rotate_degrees: { type: "number" }, scale: { type: "number", exclusiveMinimum: 0 },
        center_x: { type: ["number", "null"] }, center_y: { type: ["number", "null"] }
      },
      required: [...scopeRequired, "translate_x", "translate_y", "rotate_degrees", "scale", "center_x", "center_y"]
    }
  },
  {
    type: "function", name: "cleanup_geometry", strict: true,
    description: "对指定范围执行网格吸附、删除重复直线和删除零长度直线。",
    parameters: {
      type: "object", additionalProperties: false,
      properties: {
        ...scopeProperties,
        snap_grid: { type: ["number", "null"], description: "网格步长；不吸附时为 null。" },
        remove_duplicates: { type: "boolean" },
        remove_zero_length: { type: "boolean" }
      },
      required: [...scopeRequired, "snap_grid", "remove_duplicates", "remove_zero_length"]
    }
  },
  {
    type: "function", name: "delete_geometry", strict: true,
    description: "删除指定范围的图元。结果只作用于预览副本，用户确认后才应用。",
    parameters: {
      type: "object", additionalProperties: false,
      properties: scopeProperties,
      required: scopeRequired
    }
  },
  {
    type: "function", name: "copy_geometry", strict: true,
    description: "复制指定范围图元并按 X/Y 偏移放置，可选择复制到新图层。",
    parameters: {
      type: "object", additionalProperties: false,
      properties: {
        ...scopeProperties,
        translate_x: { type: "number" },
        translate_y: { type: "number" },
        target_layer: { type: ["string", "null"] }
      },
      required: [...scopeRequired, "translate_x", "translate_y", "target_layer"]
    }
  },
  {
    type: "function", name: "set_geometry_layer", strict: true,
    description: "将指定范围的图元改到目标图层。",
    parameters: {
      type: "object", additionalProperties: false,
      properties: { ...scopeProperties, target_layer: { type: "string" } },
      required: [...scopeRequired, "target_layer"]
    }
  },
  {
    type: "function", name: "add_geometry", strict: true,
    description: "添加点或直线，单次最多 500 个。适合补线、添加基准线或生成简单规则几何。",
    parameters: {
      type: "object", additionalProperties: false,
      properties: {
        points: {
          type: "array", maxItems: 500,
          items: {
            type: "object", additionalProperties: false,
            properties: { x: { type: "number" }, y: { type: "number" }, layer: { type: "string" } },
            required: ["x", "y", "layer"]
          }
        },
        lines: {
          type: "array", maxItems: 500,
          items: {
            type: "object", additionalProperties: false,
            properties: {
              x1: { type: "number" }, y1: { type: "number" },
              x2: { type: "number" }, y2: { type: "number" }, layer: { type: "string" }
            },
            required: ["x1", "y1", "x2", "y2", "layer"]
          }
        }
      },
      required: ["points", "lines"]
    }
  },
  {
    type: "function", name: "finish", strict: true,
    description: "确认图形处理已完成并给出简短中文摘要。完成前必须调用。",
    parameters: {
      type: "object", additionalProperties: false,
      properties: { summary: { type: "string" } },
      required: ["summary"]
    }
  }
];

function resolveProviderConfig(body) {
  const provider = ["openai_responses", "openai_chat", "anthropic"].includes(body.provider)
    ? body.provider : "openai_responses";
  const model = shortText(body.model, MODEL, 160);
  const requestApiKey = shortText(body.api_key, "", 1000);
  const serverApiKey = provider === "anthropic"
    ? shortText(process.env.ANTHROPIC_API_KEY, "", 1000)
    : shortText(process.env.OPENAI_API_KEY, "", 1000);
  const credentialSource = requestApiKey ? "request" : "server";
  const apiKey = requestApiKey || serverApiKey;
  const defaultUrl = provider === "anthropic"
    ? (process.env.ANTHROPIC_BASE_URL || DEFAULT_ANTHROPIC_API_BASE)
    : (process.env.OPENAI_BASE_URL || DEFAULT_OPENAI_API_BASE);
  // A server-side secret is only ever sent to the server-configured endpoint.
  // User-selected endpoints must carry an explicitly user-supplied key.
  const rawUrl = credentialSource === "server"
    ? defaultUrl
    : shortText(body.api_url, defaultUrl, 2000);
  if (!apiKey) {
    const variable = provider === "anthropic" ? "ANTHROPIC_API_KEY" : "OPENAI_API_KEY";
    throw Object.assign(new Error(`请输入 API Key，或在服务端配置 ${variable}`), { status: 503 });
  }
  if (!model) throw Object.assign(new Error("model 不能为空"), { status: 400 });
  let url;
  try {
    url = new URL(rawUrl);
  } catch {
    throw Object.assign(new Error("API 地址格式无效"), { status: 400 });
  }
  const localHttp = url.protocol === "http:" && ["localhost", "127.0.0.1", "::1"].includes(url.hostname);
  if (url.protocol !== "https:" && !localHttp) {
    throw Object.assign(new Error("API 地址必须使用 HTTPS；本机 localhost 可使用 HTTP"), { status: 400 });
  }
  if (url.username || url.password) throw Object.assign(new Error("API 地址不能包含用户名或密码"), { status: 400 });
  const cleanPath = url.pathname.replace(/\/+$/, "");
  if (provider === "openai_responses" && !cleanPath.endsWith("/responses")) {
    url.pathname = `${cleanPath}/responses`;
  } else if (provider === "openai_chat" && !cleanPath.endsWith("/chat/completions")) {
    url.pathname = `${cleanPath}/chat/completions`;
  } else if (provider === "anthropic" && !cleanPath.endsWith("/messages")) {
    url.pathname = cleanPath === "" ? "/v1/messages" : `${cleanPath}/messages`;
  }
  return { provider, model, apiKey, endpoint: url.toString(), credentialSource };
}

async function providerRequest(config, payload) {
  const headers = { "Content-Type": "application/json" };
  if (config.provider === "anthropic") {
    headers["x-api-key"] = config.apiKey;
    headers["anthropic-version"] = "2023-06-01";
  } else {
    headers.Authorization = `Bearer ${config.apiKey}`;
  }
  const response = await fetch(config.endpoint, {
    method: "POST",
    headers,
    body: JSON.stringify(payload),
    redirect: "error"
  });
  const data = await response.json().catch(() => ({}));
  if (!response.ok) {
    const message = data.error?.message || data.error?.error?.message ||
      `AI API 请求失败（HTTP ${response.status}）`;
    throw Object.assign(new Error(message), { status: response.status >= 500 ? 502 : 400 });
  }
  return data;
}

const agentInstructions = [
  "你是二维 DXF 点线图形处理 Agent。",
  "根据用户目标观察图形并调用工具修改工作副本；不要只给建议。",
  "优先使用摘要或 measure_geometry 完成全局分析；需要具体坐标或实体时再分页 inspect_geometry。",
  "定位到具体图元后，优先使用 scope=ids 和 inspect 返回的真实 ID 精确操作，不要扩大操作范围。",
  "用户提到标注线、尺寸线、尺寸界线或引线时，必须先调用 find_annotation_lines；dimension 和 leader 是原生确定标注，dimension_candidate 只能作为候选并在摘要中说明不确定性。",
  "不要编造实体 ID。删除和大范围变换必须符合用户明确指令。",
  "用户只要求分析或测量时，不要修改图形。",
  "完成后调用 finish。若请求超出工具能力，在 finish 摘要中说明未完成部分。",
  "用最少的有效工具循环完成任务。"
].join("\n");

function runCalls(calls, context, adapter) {
  const outputs = [];
  for (const call of calls) {
    let result;
    try {
      result = executeAgentTool(call.name, typeof call.arguments === "string"
        ? JSON.parse(call.arguments || "{}") : call.arguments || {}, context);
    } catch (error) {
      result = { ok: false, error: error.message };
    }
    outputs.push(adapter(call, result));
  }
  return outputs;
}

async function runResponsesProtocol(config, initialInput, context) {
  let input = initialInput;
  let previousResponseId;
  for (let step = 0; step < MAX_AGENT_STEPS; step += 1) {
    const response = await providerRequest(config, {
      model: config.model,
      reasoning: { effort: "medium" },
      store: true,
      parallel_tool_calls: false,
      instructions: agentInstructions,
      tools,
      tool_choice: "auto",
      input,
      previous_response_id: previousResponseId
    });
    previousResponseId = response.id;
    const calls = (response.output || []).filter((item) => item.type === "function_call");
    if (!calls.length) {
      const text = (response.output || []).flatMap((item) => item.content || [])
        .filter((item) => item.type === "output_text").map((item) => item.text).join("\n");
      context.agentSummary = text || "Agent 未生成可执行操作";
      break;
    }
    const outputs = runCalls(calls, context, (call, result) => ({
      type: "function_call_output", call_id: call.call_id, output: JSON.stringify(result)
    }));
    if (context.finished) break;
    input = outputs;
  }
}

function chatTools() {
  return tools.map((tool) => ({
    type: "function",
    function: {
      name: tool.name,
      description: tool.description,
      parameters: tool.parameters
    }
  }));
}

async function runChatProtocol(config, initialInput, context) {
  const messages = [
    { role: "system", content: agentInstructions },
    { role: "user", content: initialInput }
  ];
  for (let step = 0; step < MAX_AGENT_STEPS; step += 1) {
    const response = await providerRequest(config, {
      model: config.model,
      messages,
      tools: chatTools(),
      tool_choice: "auto",
      parallel_tool_calls: false,
      max_tokens: 2048
    });
    const message = response.choices?.[0]?.message;
    if (!message) throw Object.assign(new Error("Chat Completions 响应缺少 message"), { status: 502 });
    const calls = (message.tool_calls || []).map((call) => ({
      id: call.id, name: call.function?.name, arguments: call.function?.arguments
    }));
    if (!calls.length) {
      context.agentSummary = message.content || "Agent 未生成可执行操作";
      break;
    }
    messages.push({
      role: "assistant",
      content: message.content || null,
      tool_calls: message.tool_calls
    });
    const outputs = runCalls(calls, context, (call, result) => ({
      role: "tool", tool_call_id: call.id, content: JSON.stringify(result)
    }));
    if (context.finished) break;
    messages.push(...outputs);
  }
}

function anthropicTools() {
  return tools.map((tool) => ({
    name: tool.name,
    description: tool.description,
    input_schema: tool.parameters
  }));
}

async function runAnthropicProtocol(config, initialInput, context) {
  const messages = [{ role: "user", content: initialInput }];
  for (let step = 0; step < MAX_AGENT_STEPS; step += 1) {
    const response = await providerRequest(config, {
      model: config.model,
      system: agentInstructions,
      messages,
      tools: anthropicTools(),
      max_tokens: 2048
    });
    const content = Array.isArray(response.content) ? response.content : [];
    const calls = content.filter((item) => item.type === "tool_use").map((item) => ({
      id: item.id, name: item.name, arguments: item.input
    }));
    if (!calls.length) {
      context.agentSummary = content.filter((item) => item.type === "text").map((item) => item.text).join("\n") ||
        "Agent 未生成可执行操作";
      break;
    }
    messages.push({ role: "assistant", content });
    const outputs = runCalls(calls, context, (call, result) => ({
      type: "tool_result", tool_use_id: call.id, content: JSON.stringify(result)
    }));
    if (context.finished) break;
    messages.push({ role: "user", content: outputs });
  }
}

async function runGeometryAgent(body) {
  const instruction = shortText(body.instruction, "", 2000);
  if (!instruction) throw Object.assign(new Error("instruction 不能为空"), { status: 400 });
  const config = resolveProviderConfig(body);
  const geometry = validateGeometry(body.geometry);
  const selectedIds = new Set(Array.isArray(body.selected_ids) ? body.selected_ids.map(String) : []);
  const context = { geometry, selectedIds, actionLog: [], finished: false, agentSummary: "" };
  const initialSummary = geometrySummary(geometry, selectedIds);
  const initialInput = JSON.stringify({
    user_instruction: instruction,
    geometry: initialSummary,
    note: "你操作的是预览副本。selected_count 为 0 时不要使用 selected 范围。"
  });

  if (config.provider === "openai_responses") await runResponsesProtocol(config, initialInput, context);
  else if (config.provider === "openai_chat") await runChatProtocol(config, initialInput, context);
  else await runAnthropicProtocol(config, initialInput, context);

  if (!context.finished && !context.agentSummary) {
    context.agentSummary = `Agent 达到 ${MAX_AGENT_STEPS} 步限制`;
  }
  return {
    ok: true,
    provider: config.provider,
    model: config.model,
    summary: context.agentSummary,
    actions: context.actionLog,
    before: initialSummary,
    after: geometrySummary(context.geometry, selectedIds),
    geometry: context.geometry
  };
}

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

const server = http.createServer(async (req, res) => {
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
      return json(res, 200, { ok: true, model: MODEL, api_key_configured: Boolean(process.env.OPENAI_API_KEY) });
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
});

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
  validateAgentRequest,
  server,
  startServer
};
