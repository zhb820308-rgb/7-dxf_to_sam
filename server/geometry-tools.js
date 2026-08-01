"use strict";

const crypto = require("crypto");
const MAX_ENTITIES = 100000;

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

module.exports = {
  shortText,
  validateGeometry,
  geometrySummary,
  executeAgentTool,
  tools
};
