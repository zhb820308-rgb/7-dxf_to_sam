(function () {
  "use strict";

  const $ = (selector, root = document) => root.querySelector(selector);
  const $$ = (selector, root = document) => Array.from(root.querySelectorAll(selector));
  const canvas = $("#canvas");
  const ctx = canvas.getContext("2d");
  const shell = $("#dropZone");
  const MIN_VIEW_SCALE = 1e-12;
  const MAX_VIEW_SCALE = 1e12;
  const AGENT_SETTINGS_KEY = "dxf-studio.agent-settings.v1";
  let agentSettingsTimer = null;

  const state = {
    parsed: null,
    points: [],
    lines: [],
    originalGeometry: null,
    hasAgentChanges: false,
    fileName: "",
    fileSize: 0,
    drawingProfile: "small",
    tolerance: 0.01,
    selected: null,
    multiSelection: [],
    selectionBox: null,
    sourceVisibility: {},
    layerVisibility: {},
    selectedLayer: null,
    tool: "select",
    drawingStart: null,
    view: { scale: 1, x: 0, y: 0 },
    dragging: null,
    history: [],
    future: [],
    spacePressed: false,
    allVisible: true,
    dirty: false,
    pendingAgentResult: null
  };

  function setStatus(message) {
    $("#statusText").textContent = message;
  }

  function currentDrawingProfile() {
    return DXFStudio.EXPANSION_PROFILES[state.drawingProfile]
      || DXFStudio.EXPANSION_PROFILES.small;
  }

  function syncToleranceControls() {
    $("#tolerance").value = Number(state.tolerance.toPrecision(5));
    $("#toleranceRange").value = Math.max(-4, Math.min(2, Math.log10(state.tolerance)));
  }

  function updateDrawingProfileMeta() {
    const profile = currentDrawingProfile();
    const outputLimit = Number.isFinite(profile.maxOutputEntities)
      ? `最多 ${profile.maxOutputEntities.toLocaleString()} 个输出图元`
      : "不限制最终输出图元数（仍保留 INSERT 安全限制）";
    $("#profileHint").textContent =
      `${profile.label}：${outputLimit}；默认容差 ${profile.defaultTolerance}`;
  }

  function toast(message, type = "") {
    const item = document.createElement("div");
    item.className = `toast ${type}`.trim();
    item.textContent = message;
    $("#toasts").appendChild(item);
    window.setTimeout(() => item.remove(), 3200);
  }

  function setDirty(value) {
    if (value && state.pendingAgentResult) {
      clearAgentResult();
      $("#agentStatus").textContent = "待命";
    }
    state.dirty = value;
    $("#savedDot").classList.toggle("dirty", value);
    $("#savedDot").title = value ? "有尚未导出的更改" : "所有更改已保存";
  }

  function serializeDrawing() {
    return JSON.stringify({
      points: state.points,
      lines: state.lines,
      hasAgentChanges: state.hasAgentChanges
    });
  }

  function saveOriginalGeometry() {
    state.originalGeometry = JSON.stringify({ points: state.points, lines: state.lines });
  }

  function snapshot() {
    state.history.push(serializeDrawing());
    if (state.history.length > 50) state.history.shift();
    state.future = [];
    updateHistoryButtons();
  }

  function restoreSnapshot(serialized) {
    const data = JSON.parse(serialized);
    state.points = data.points;
    state.lines = data.lines;
    state.hasAgentChanges = Boolean(data.hasAgentChanges);
    state.selected = null;
    state.multiSelection = [];
    state.selectionBox = null;
  }

  function undo() {
    const previous = state.history.pop();
    if (!previous) return toast("没有可撤销的操作");
    state.future.push(serializeDrawing());
    if (state.future.length > 50) state.future.shift();
    restoreSnapshot(previous);
    setDirty(true);
    refresh();
    updateHistoryButtons();
    toast("已撤销");
  }

  function redo() {
    const next = state.future.pop();
    if (!next) return toast("没有可重做的操作");
    state.history.push(serializeDrawing());
    if (state.history.length > 50) state.history.shift();
    restoreSnapshot(next);
    setDirty(true);
    refresh();
    updateHistoryButtons();
    toast("已重做");
  }

  function updateHistoryButtons() {
    const undoButton = $("#undoBtn");
    const redoButton = $("#redoBtn");
    if (undoButton) undoButton.disabled = state.history.length === 0;
    if (redoButton) redoButton.disabled = state.future.length === 0;
  }

  function worldToScreen(point) {
    return {
      x: state.view.x + point.x * state.view.scale,
      y: state.view.y - point.y * state.view.scale
    };
  }

  function screenToWorld(point) {
    return {
      x: (point.x - state.view.x) / state.view.scale,
      y: (state.view.y - point.y) / state.view.scale
    };
  }

  function visible(item) {
    return state.sourceVisibility[item.sourceType] !== false &&
      DXFStudio.isLayerVisible(item, state.layerVisibility);
  }

  function affectedByLayer(item, layer) {
    return DXFStudio.affectsLayer(item, layer);
  }

  function isSelected(kind, index) {
    return Boolean(
      (state.selected && state.selected.kind === kind && state.selected.index === index) ||
      state.multiSelection.some((item) => item.kind === kind && item.index === index)
    );
  }

  function sameSelection(a, b) {
    return Boolean(a && b && a.kind === b.kind && a.index === b.index);
  }

  function currentSelection() {
    if (state.multiSelection.length) return state.multiSelection.slice();
    return state.selected ? [state.selected] : [];
  }

  function applySelection(items) {
    const unique = [];
    items.forEach((item) => {
      if (item && !unique.some((existing) => sameSelection(existing, item))) unique.push(item);
    });
    if (unique.length <= 1) {
      state.selected = unique[0] || null;
      state.multiSelection = [];
    } else {
      state.selected = null;
      state.multiSelection = unique;
    }
  }

  function toggleSelection(hit) {
    const selection = currentSelection();
    const index = selection.findIndex((item) => sameSelection(item, hit));
    if (index >= 0) selection.splice(index, 1);
    else selection.push(hit);
    applySelection(selection);
    return index < 0;
  }

  function resizeCanvas() {
    const rect = canvas.getBoundingClientRect();
    const dpr = window.devicePixelRatio || 1;
    const width = Math.max(1, Math.round(rect.width * dpr));
    const height = Math.max(1, Math.round(rect.height * dpr));
    if (canvas.width !== width || canvas.height !== height) {
      canvas.width = width;
      canvas.height = height;
    }
    ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
    render();
  }

  function render() {
    const rect = canvas.getBoundingClientRect();
    ctx.clearRect(0, 0, rect.width, rect.height);
    if (!state.points.length && !state.lines.length) return;

    ctx.lineCap = "round";
    state.lines.forEach((line, index) => {
      if (!visible(line)) return;
      const a = worldToScreen({ x: line.x1, y: line.y1 });
      const b = worldToScreen({ x: line.x2, y: line.y2 });
      const selected = isSelected("line", index);
      const layerHighlighted = state.selectedLayer !== null && affectedByLayer(line, state.selectedLayer);
      ctx.globalAlpha = state.selectedLayer === null || layerHighlighted || selected ? 1 : 0.16;
      ctx.beginPath();
      ctx.moveTo(a.x, a.y);
      ctx.lineTo(b.x, b.y);
      ctx.strokeStyle = selected
        ? "#ff6b3d"
        : layerHighlighted
          ? "#75b51b"
          : DXFStudio.colorForLayer(line.layer, state.parsed ? state.parsed.layers : {});
      ctx.lineWidth = selected ? 3 : layerHighlighted ? 2.6 : 1.35;
      ctx.stroke();
      if (selected && state.multiSelection.length === 0) {
        drawHandle(a);
        drawHandle(b);
      }
    });

    state.points.forEach((point, index) => {
      if (!visible(point)) return;
      const screen = worldToScreen(point);
      const selected = isSelected("point", index);
      const layerHighlighted = state.selectedLayer !== null && affectedByLayer(point, state.selectedLayer);
      ctx.globalAlpha = state.selectedLayer === null || layerHighlighted || selected ? 1 : 0.16;
      ctx.beginPath();
      ctx.arc(screen.x, screen.y, selected ? 5 : layerHighlighted ? 4.5 : 3.2, 0, Math.PI * 2);
      ctx.fillStyle = selected
        ? "#ff6b3d"
        : layerHighlighted
          ? "#75b51b"
          : DXFStudio.colorForLayer(point.layer, state.parsed ? state.parsed.layers : {});
      ctx.fill();
      ctx.strokeStyle = "#f5f4ef";
      ctx.lineWidth = 1.2;
      ctx.stroke();
    });
    ctx.globalAlpha = 1;

    if (state.drawingStart) {
      const a = worldToScreen(state.drawingStart);
      const b = worldToScreen(state.drawingStart.preview || state.drawingStart);
      ctx.setLineDash([5, 5]);
      ctx.beginPath();
      ctx.moveTo(a.x, a.y);
      ctx.lineTo(b.x, b.y);
      ctx.strokeStyle = "#ff6b3d";
      ctx.lineWidth = 1.5;
      ctx.stroke();
      ctx.setLineDash([]);
    }

    if (state.selectionBox) {
      const box = normalizedBox(state.selectionBox.start, state.selectionBox.current);
      ctx.globalAlpha = 1;
      ctx.fillStyle = "rgba(117, 181, 27, .12)";
      ctx.strokeStyle = "#75b51b";
      ctx.lineWidth = 1;
      ctx.setLineDash([5, 4]);
      ctx.fillRect(box.left, box.top, box.width, box.height);
      ctx.strokeRect(box.left + .5, box.top + .5, box.width, box.height);
      ctx.setLineDash([]);
    }
  }

  function drawHandle(point) {
    ctx.beginPath();
    ctx.arc(point.x, point.y, 4, 0, Math.PI * 2);
    ctx.fillStyle = "#fbfaf6";
    ctx.fill();
    ctx.strokeStyle = "#ff6b3d";
    ctx.lineWidth = 2;
    ctx.stroke();
  }

  function bounds() {
    const box = { minX: Infinity, maxX: -Infinity, minY: Infinity, maxY: -Infinity };
    const include = (x, y) => {
      box.minX = Math.min(box.minX, x);
      box.maxX = Math.max(box.maxX, x);
      box.minY = Math.min(box.minY, y);
      box.maxY = Math.max(box.maxY, y);
    };
    state.points.filter(visible).forEach((point) => include(point.x, point.y));
    state.lines.filter(visible).forEach((line) => {
      include(line.x1, line.y1);
      include(line.x2, line.y2);
    });
    return Number.isFinite(box.minX) ? box : null;
  }

  function fitView() {
    const box = bounds();
    const rect = canvas.getBoundingClientRect();
    if (!box || !rect.width || !rect.height) {
      state.view = { scale: 1, x: rect.width / 2, y: rect.height / 2 };
      updateZoom();
      return render();
    }
    const width = Math.max(box.maxX - box.minX, 1e-6);
    const height = Math.max(box.maxY - box.minY, 1e-6);
    state.view.scale = Math.max(
      MIN_VIEW_SCALE,
      Math.min(MAX_VIEW_SCALE, Math.min((rect.width - 100) / width, (rect.height - 100) / height))
    );
    state.view.x = rect.width / 2 - (box.minX + box.maxX) / 2 * state.view.scale;
    state.view.y = rect.height / 2 + (box.minY + box.maxY) / 2 * state.view.scale;
    updateZoom();
    render();
  }

  function updateZoom() {
    $("#zoomLabel").textContent = state.view.scale >= 10
      ? `${Math.round(state.view.scale)}×`
      : `${Math.round(state.view.scale * 100)}%`;
  }

  function zoomAt(factor, screenPoint) {
    const rect = canvas.getBoundingClientRect();
    const anchor = screenPoint || { x: rect.width / 2, y: rect.height / 2 };
    const world = screenToWorld(anchor);
    state.view.scale = Math.max(MIN_VIEW_SCALE, Math.min(MAX_VIEW_SCALE, state.view.scale * factor));
    state.view.x = anchor.x - world.x * state.view.scale;
    state.view.y = anchor.y + world.y * state.view.scale;
    updateZoom();
    render();
  }

  function sortedLayers(layerCounts) {
    return Object.keys(layerCounts).sort((a, b) => a.localeCompare(b, "zh-CN"));
  }

  function currentLayers() {
    return sortedLayers(DXFStudio.buildLayerCounts(state.points, state.lines));
  }

  function updateLists() {
    const counts = {};
    state.points.forEach((item) => { counts[item.sourceType || "POINT"] = (counts[item.sourceType || "POINT"] || 0) + 1; });
    state.lines.forEach((item) => { counts[item.sourceType || "LINE"] = (counts[item.sourceType || "LINE"] || 0) + 1; });
    const entityList = $("#entityList");
    entityList.innerHTML = "";
    Object.entries(counts).sort().forEach(([type, count]) => {
      const row = document.createElement("div");
      row.className = "entity-row";
      const off = state.sourceVisibility[type] === false;
      row.innerHTML = `<span class="swatch" style="background:${type === "POINT" ? "#ff6b3d" : "#4c7dff"}"></span>
        <span class="label">${escapeHtml(type)}</span><span class="num">${count}</span>
        <button class="eye ${off ? "off" : ""}" aria-label="${off ? "显示" : "隐藏"} ${escapeHtml(type)}">${eyeSvg()}</button>`;
      $(".eye", row).addEventListener("click", (event) => {
        event.stopPropagation();
        state.sourceVisibility[type] = off;
        updateLists();
        render();
      });
      entityList.appendChild(row);
    });
    if (!Object.keys(counts).length) entityList.innerHTML = '<div class="list-empty">暂无图元</div>';

    const layerCounts = DXFStudio.buildLayerCounts(state.points, state.lines);
    const layers = sortedLayers(layerCounts);
    $("#layerCount").textContent = String(layers.length);
    const layerList = $("#layerList");
    layerList.innerHTML = "";
    layers.forEach((layer) => {
      // SAM counts every entity controlled by this layer, including explicit
      // child-layer entities inside an INSERT. The index is built in one pass.
      const count = layerCounts[layer] || 0;
      const off = state.layerVisibility[layer] === false;
      const row = document.createElement("div");
      row.className = `layer-row${state.selectedLayer === layer ? " selected" : ""}`;
      row.setAttribute("role", "button");
      row.setAttribute("tabindex", "0");
      row.setAttribute("aria-pressed", String(state.selectedLayer === layer));
      row.innerHTML = `<span class="layer-color" style="background:${DXFStudio.colorForLayer(layer, state.parsed ? state.parsed.layers : {})}"></span>
        <span class="label">${escapeHtml(layer)}</span><span class="num">${count}</span>
        <button class="eye ${off ? "off" : ""}" aria-label="${off ? "显示" : "隐藏"}图层">${eyeSvg()}</button>`;
      const selectLayer = () => {
        state.selectedLayer = state.selectedLayer === layer ? null : layer;
        state.selected = null;
        state.multiSelection = [];
        updateLists();
        updateInspector();
        render();
        $("#selectionStatus").textContent = state.selectedLayer === null
          ? "未选择图层"
          : `已高亮图层 ${state.selectedLayer}`;
      };
      row.addEventListener("click", selectLayer);
      row.addEventListener("keydown", (event) => {
        if (event.target !== row) return;
        if (event.key === "Enter" || event.key === " ") {
          event.preventDefault();
          selectLayer();
        }
      });
      $(".eye", row).addEventListener("click", (event) => {
        event.stopPropagation();
        state.layerVisibility[layer] = off;
        if (off === false && state.selectedLayer === layer) state.selectedLayer = null;
        updateLists();
        updateInspector();
        render();
      });
      layerList.appendChild(row);
    });
  }

  function eyeSvg() {
    return '<svg viewBox="0 0 24 24"><path d="M2.5 12s3.5-6 9.5-6 9.5 6 9.5 6-3.5 6-9.5 6-9.5-6-9.5-6Z"/><circle cx="12" cy="12" r="2.5"/></svg>';
  }

  function escapeHtml(text) {
    return String(text).replace(/[&<>"']/g, (char) => ({
      "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;", "'": "&#39;"
    })[char]);
  }

  function updateStats() {
    $("#pointStat").textContent = state.points.length.toLocaleString();
    $("#lineStat").textContent = state.lines.length.toLocaleString();
    $("#sourceStat").textContent = state.parsed ? state.parsed.entities.length.toLocaleString() : "0";
    shell.classList.toggle("has-file", Boolean(state.parsed || state.points.length || state.lines.length));
  }

  function updateInspector() {
    const form = $("#propertyForm");
    const empty = $("#noSelection");
    if (state.multiSelection.length) {
      const pointCount = state.multiSelection.filter((item) => item.kind === "point").length;
      const lineCount = state.multiSelection.filter((item) => item.kind === "line").length;
      empty.classList.add("hidden");
      form.classList.remove("hidden");
      form.innerHTML = `<div class="property-title"><i></i>多选结果</div>
        <div class="selection-summary">
          <strong>${state.multiSelection.length}</strong>
          <span>${pointCount} 个点 · ${lineCount} 条直线</span>
        </div>`;
      $("#selectionStatus").textContent = `已选择 ${state.multiSelection.length} 个图元`;
      return;
    }
    if (!state.selected) {
      form.classList.add("hidden");
      empty.classList.remove("hidden");
      $("#selectionStatus").textContent = "未选择图元";
      return;
    }
    const item = state.selected.kind === "point"
      ? state.points[state.selected.index]
      : state.lines[state.selected.index];
    if (!item) {
      state.selected = null;
      return updateInspector();
    }
    empty.classList.add("hidden");
    form.classList.remove("hidden");
    const fields = state.selected.kind === "point"
      ? [["X", "x", item.x], ["Y", "y", item.y]]
      : [["X₁", "x1", item.x1], ["Y₁", "y1", item.y1], ["X₂", "x2", item.x2], ["Y₂", "y2", item.y2]];
    const inputValue = (value) => Number(value.toPrecision(10)).toString();
    form.innerHTML = `<div class="property-title"><i></i>${state.selected.kind === "point" ? "点" : "直线"} · ${escapeHtml(item.layer || "0")}</div>
      <div class="coord-grid">${fields.map(([label, name, value]) =>
        `<label>${label}<input type="number" step="any" data-property="${name}" value="${inputValue(value)}"></label>`).join("")}</div>`;
    $$("[data-property]", form).forEach((input) => input.addEventListener("change", () => {
      const value = Number(input.value);
      if (!Number.isFinite(value)) return toast("请输入有效坐标", "error");
      snapshot();
      item[input.dataset.property] = value;
      setDirty(true);
      refresh();
    }));
    $("#selectionStatus").textContent = `已选择${state.selected.kind === "point" ? "点" : "直线"} ${item.id || ""}`;
  }

  function refresh() {
    updateStats();
    updateLists();
    updateInspector();
    render();
  }

  function importText(text, name, size) {
    setStatus("正在解析…");
    try {
      const parsed = DXFStudio.parseDxf(text);
      const discretized = DXFStudio.discretize(parsed, state.tolerance, state.drawingProfile);
      if (!discretized.points.length && !discretized.lines.length) {
        const unsupported = Object.keys(discretized.unsupported).join("、");
        throw new Error(unsupported ? `未找到可显示图元（不支持：${unsupported}）` : "未找到可显示图元");
      }
      state.parsed = parsed;
      state.points = discretized.points;
      state.lines = discretized.lines;
      state.hasAgentChanges = false;
      saveOriginalGeometry();
      state.fileName = name;
      state.fileSize = size;
      state.selected = null;
      state.multiSelection = [];
      state.selectionBox = null;
      state.sourceVisibility = {};
      // The editor starts with every imported layer visible, including layers
      // marked off/frozen in the source DXF. Users can hide them from the list.
      state.layerVisibility = Object.fromEntries(
        Object.keys(parsed.layers).map((layer) => [layer, true])
      );
      state.selectedLayer = null;
      state.history = [];
      state.future = [];
      updateHistoryButtons();
      clearAgentResult();
      $("#fileName").textContent = name;
      $("#sideFileName").textContent = name;
      $("#fileMeta").textContent = `${formatBytes(size)} · ${parsed.entities.length} 个原始图元 · ${discretized.points.length + discretized.lines.length} 个输出图元`;
      $("#restoreOriginalBtn").disabled = false;
      setDirty(false);
      refresh();
      requestAnimationFrame(fitView);
      setStatus("导入完成");
      const unsupportedCount = Object.values(discretized.unsupported).reduce((sum, value) => sum + value, 0);
      toast(unsupportedCount ? `已导入，跳过 ${unsupportedCount} 个不支持的图元` : "DXF 导入成功");
    } catch (error) {
      setStatus("导入失败");
      toast(error.message || "无法解析该 DXF 文件", "error");
    }
  }

  function readFile(file) {
    if (!file) return;
    if (!/\.dxf$/i.test(file.name)) return toast("请选择 .dxf 文件", "error");
    if (file.size > 50 * 1024 * 1024) return toast("文件超过 50 MB，无法在浏览器中处理", "error");
    const reader = new FileReader();
    reader.onerror = () => toast("文件读取失败", "error");
    reader.onload = () => importText(reader.result, file.name, file.size);
    reader.readAsText(file);
  }

  function formatBytes(bytes) {
    if (!bytes) return "0 B";
    const units = ["B", "KB", "MB"];
    const index = Math.min(2, Math.floor(Math.log(bytes) / Math.log(1024)));
    return `${(bytes / 1024 ** index).toFixed(index ? 1 : 0)} ${units[index]}`;
  }

  function hitTest(screen) {
    // The visible handles of the selected line must win over nearby geometry.
    // Otherwise, coincident endpoints can unexpectedly switch selection to
    // another line just before the user starts dragging the handle.
    if (state.selected && state.selected.kind === "line") {
      const selectedLine = state.lines[state.selected.index];
      if (selectedLine && visible(selectedLine)) {
        const start = worldToScreen({ x: selectedLine.x1, y: selectedLine.y1 });
        const end = worldToScreen({ x: selectedLine.x2, y: selectedLine.y2 });
        if (Math.min(
          Math.hypot(screen.x - start.x, screen.y - start.y),
          Math.hypot(screen.x - end.x, screen.y - end.y)
        ) <= 10) {
          return { kind: "line", index: state.selected.index };
        }
      }
    }

    let best = null;
    let distance = 9;
    state.points.forEach((point, index) => {
      if (!visible(point)) return;
      const p = worldToScreen(point);
      const d = Math.hypot(screen.x - p.x, screen.y - p.y);
      if (d < distance) { distance = d; best = { kind: "point", index }; }
    });
    state.lines.forEach((line, index) => {
      if (!visible(line)) return;
      const a = worldToScreen({ x: line.x1, y: line.y1 });
      const b = worldToScreen({ x: line.x2, y: line.y2 });
      const d = distanceToSegment(screen, a, b);
      if (d < distance) { distance = d; best = { kind: "line", index }; }
    });
    return best;
  }

  function lineDragMode(line, screen) {
    const start = worldToScreen({ x: line.x1, y: line.y1 });
    const end = worldToScreen({ x: line.x2, y: line.y2 });
    if (Math.hypot(screen.x - start.x, screen.y - start.y) <= 10) return "start";
    if (Math.hypot(screen.x - end.x, screen.y - end.y) <= 10) return "end";
    return "move";
  }

  function beginGeometryDrag(hit, screen, pointerId) {
    const world = screenToWorld(screen);
    const selection = currentSelection();
    if (selection.length > 1 && selection.some((item) => sameSelection(item, hit))) {
      state.dragging = {
        type: "edit-group",
        startWorld: world,
        originals: selection.map((item) => {
          const entity = item.kind === "point" ? state.points[item.index] : state.lines[item.index];
          return item.kind === "point"
            ? { ...item, x: entity.x, y: entity.y }
            : { ...item, x1: entity.x1, y1: entity.y1, x2: entity.x2, y2: entity.y2 };
        }),
        changed: false,
        saved: false
      };
    } else if (hit.kind === "point") {
      const point = state.points[hit.index];
      state.dragging = {
        type: "edit-point", index: hit.index, startWorld: world,
        original: { x: point.x, y: point.y }, changed: false, saved: false
      };
    } else {
      const line = state.lines[hit.index];
      state.dragging = {
        type: "edit-line", index: hit.index, mode: lineDragMode(line, screen),
        startWorld: world,
        original: { x1: line.x1, y1: line.y1, x2: line.x2, y2: line.y2 },
        changed: false, saved: false
      };
    }
    canvas.setPointerCapture(pointerId);
  }

  function dragGeometry(world) {
    const drag = state.dragging;
    if (!drag || !drag.type.startsWith("edit-")) return;
    const dx = world.x - drag.startWorld.x;
    const dy = world.y - drag.startWorld.y;
    if (!drag.saved && Math.hypot(dx, dy) * state.view.scale >= 1) {
      snapshot();
      drag.saved = true;
    }
    if (!drag.saved) return;
    drag.changed = true;
    if (drag.type === "edit-group") {
      drag.originals.forEach((original) => {
        if (original.kind === "point") {
          const point = state.points[original.index];
          point.x = original.x + dx;
          point.y = original.y + dy;
        } else {
          const line = state.lines[original.index];
          line.x1 = original.x1 + dx;
          line.y1 = original.y1 + dy;
          line.x2 = original.x2 + dx;
          line.y2 = original.y2 + dy;
        }
      });
    } else if (drag.type === "edit-point") {
      const point = state.points[drag.index];
      point.x = drag.original.x + dx;
      point.y = drag.original.y + dy;
    } else {
      const line = state.lines[drag.index];
      if (drag.mode === "start") {
        line.x1 = world.x;
        line.y1 = world.y;
      } else if (drag.mode === "end") {
        line.x2 = world.x;
        line.y2 = world.y;
      } else {
        line.x1 = drag.original.x1 + dx;
        line.y1 = drag.original.y1 + dy;
        line.x2 = drag.original.x2 + dx;
        line.y2 = drag.original.y2 + dy;
      }
    }
    render();
  }

  function distanceToSegment(point, a, b) {
    const dx = b.x - a.x;
    const dy = b.y - a.y;
    if (!dx && !dy) return Math.hypot(point.x - a.x, point.y - a.y);
    const t = Math.max(0, Math.min(1, ((point.x - a.x) * dx + (point.y - a.y) * dy) / (dx * dx + dy * dy)));
    return Math.hypot(point.x - (a.x + t * dx), point.y - (a.y + t * dy));
  }

  function normalizedBox(a, b) {
    const left = Math.min(a.x, b.x);
    const top = Math.min(a.y, b.y);
    const right = Math.max(a.x, b.x);
    const bottom = Math.max(a.y, b.y);
    return { left, top, right, bottom, width: right - left, height: bottom - top };
  }

  function pointInBox(point, box) {
    return point.x >= box.left && point.x <= box.right &&
      point.y >= box.top && point.y <= box.bottom;
  }

  function lineIntersectsBox(a, b, box) {
    if (pointInBox(a, box) || pointInBox(b, box)) return true;
    let t0 = 0;
    let t1 = 1;
    const dx = b.x - a.x;
    const dy = b.y - a.y;
    for (const [p, q] of [
      [-dx, a.x - box.left],
      [dx, box.right - a.x],
      [-dy, a.y - box.top],
      [dy, box.bottom - a.y]
    ]) {
      if (p === 0) {
        if (q < 0) return false;
        continue;
      }
      const ratio = q / p;
      if (p < 0) {
        if (ratio > t1) return false;
        t0 = Math.max(t0, ratio);
      } else {
        if (ratio < t0) return false;
        t1 = Math.min(t1, ratio);
      }
    }
    return true;
  }

  function finishBoxSelection() {
    if (!state.selectionBox) return;
    const box = normalizedBox(state.selectionBox.start, state.selectionBox.current);
    const additive = state.selectionBox.additive;
    const selection = [];
    if (box.width >= 3 || box.height >= 3) {
      state.points.forEach((point, index) => {
        if (visible(point) && pointInBox(worldToScreen(point), box)) {
          selection.push({ kind: "point", index });
        }
      });
      state.lines.forEach((line, index) => {
        if (!visible(line)) return;
        const start = worldToScreen({ x: line.x1, y: line.y1 });
        const end = worldToScreen({ x: line.x2, y: line.y2 });
        if (lineIntersectsBox(start, end, box)) selection.push({ kind: "line", index });
      });
    }
    state.selectionBox = null;
    applySelection(additive ? currentSelection().concat(selection) : selection);
  }

  function selectTool(tool) {
    state.tool = tool;
    state.drawingStart = null;
    $$(".tool[data-tool]").forEach((button) => button.classList.toggle("active", button.dataset.tool === tool));
    canvas.style.cursor = tool === "pan" ? "grab" : tool === "select" ? "default" : "crosshair";
    render();
  }

  function deleteSelected() {
    if (!state.selected && !state.multiSelection.length) return toast("请先选择图元");
    snapshot();
    if (state.multiSelection.length) {
      const pointIndexes = state.multiSelection
        .filter((item) => item.kind === "point").map((item) => item.index).sort((a, b) => b - a);
      const lineIndexes = state.multiSelection
        .filter((item) => item.kind === "line").map((item) => item.index).sort((a, b) => b - a);
      pointIndexes.forEach((index) => state.points.splice(index, 1));
      lineIndexes.forEach((index) => state.lines.splice(index, 1));
    } else {
      const list = state.selected.kind === "point" ? state.points : state.lines;
      list.splice(state.selected.index, 1);
    }
    state.selected = null;
    state.multiSelection = [];
    setDirty(true);
    refresh();
  }

  function rediscretize() {
    if (!state.parsed) return toast("请先导入 DXF");
    let result;
    try {
      result = DXFStudio.discretize(state.parsed, state.tolerance, state.drawingProfile);
    } catch (error) {
      setStatus("重新离散化失败");
      return toast(error.message || "图元数量超过当前图纸模式上限", "error");
    }
    snapshot();
    state.points = result.points;
    state.lines = result.lines;
    state.hasAgentChanges = false;
    saveOriginalGeometry();
    state.selected = null;
    state.multiSelection = [];
    setDirty(true);
    refresh();
    fitView();
    const typeCount = Object.keys(result.processedByType).length;
    const unsupportedCount = Object.values(result.unsupported).reduce((sum, count) => sum + count, 0);
    $("#fileMeta").textContent = `${formatBytes(state.fileSize)} · ${result.sourceCount} 个原始图元 · ${result.points.length + result.lines.length} 个输出图元`;
    setStatus(`已重新离散化全部 ${result.sourceCount} 个图元`);
    toast(
      unsupportedCount
        ? `已重建 ${result.sourceCount} 个原始图元（${typeCount} 类），跳过 ${unsupportedCount} 个不支持图元`
        : `已重新离散化全部 ${result.sourceCount} 个原始图元（${typeCount} 类）`
    );
  }

  function applyTransform() {
    if (!state.points.length && !state.lines.length) return toast("当前没有图元");
    const dx = Number($("#translateX").value);
    const dy = Number($("#translateY").value);
    const angle = Number($("#rotate").value) * Math.PI / 180;
    const scale = Number($("#scale").value);
    if (![dx, dy, angle, scale].every(Number.isFinite) || scale <= 0) return toast("请输入有效的变换参数", "error");
    snapshot();
    const cos = Math.cos(angle);
    const sin = Math.sin(angle);
    const transform = (x, y) => ({ x: (x * cos - y * sin) * scale + dx, y: (x * sin + y * cos) * scale + dy });
    state.points.forEach((point) => Object.assign(point, transform(point.x, point.y)));
    state.lines.forEach((line) => {
      const a = transform(line.x1, line.y1);
      const b = transform(line.x2, line.y2);
      Object.assign(line, { x1: a.x, y1: a.y, x2: b.x, y2: b.y });
    });
    setDirty(true);
    refresh();
    fitView();
    toast("整体变换已应用");
  }

  function selectedEntityIds() {
    if (state.multiSelection.length) {
      return state.multiSelection.map((selection) => {
        const list = selection.kind === "point" ? state.points : state.lines;
        return list[selection.index]?.id;
      }).filter(Boolean);
    }
    if (!state.selected) return [];
    const list = state.selected.kind === "point" ? state.points : state.lines;
    const id = list[state.selected.index]?.id;
    return id ? [id] : [];
  }

  function clearAgentResult() {
    state.pendingAgentResult = null;
    $("#agentResult").classList.add("hidden");
    $("#agentSummary").textContent = "";
    $("#agentChanges").textContent = "";
  }

  function saveAgentSettings(settings) {
    try {
      DxfAgentSettings.saveSettings(localStorage, AGENT_SETTINGS_KEY, settings);
    } catch {
      toast("浏览器禁止本地存储，Agent 配置未缓存", "error");
    }
  }

  function saveCurrentAgentSettings() {
    saveAgentSettings({
      provider: $("#agentProvider").value,
      model: $("#agentModel").value.trim(),
      apiUrl: $("#agentApiUrl").value.trim()
    });
  }

  function scheduleAgentSettingsSave() {
    window.clearTimeout(agentSettingsTimer);
    agentSettingsTimer = window.setTimeout(saveCurrentAgentSettings, 250);
  }

  function loadAgentSettings() {
    // Password managers and back/forward caches must not resurrect a prior key.
    $("#agentApiKey").value = "";
    try {
      const settings = DxfAgentSettings.loadSettings(localStorage, AGENT_SETTINGS_KEY);
      if (!settings || typeof settings !== "object") return;
      if (["openai_responses", "openai_chat", "anthropic"].includes(settings.provider)) {
        $("#agentProvider").value = settings.provider;
      }
      if (typeof settings.model === "string") $("#agentModel").value = settings.model;
      if (typeof settings.apiUrl === "string") $("#agentApiUrl").value = settings.apiUrl;
      $("#agentModel").placeholder = settings.provider === "anthropic"
        ? "输入 Anthropic 模型 ID"
        : "输入模型 ID";
    } catch {
      DxfAgentSettings.clearSettings(localStorage, AGENT_SETTINGS_KEY);
    }
  }

  function clearAgentSettings() {
    window.clearTimeout(agentSettingsTimer);
    agentSettingsTimer = null;
    try {
      DxfAgentSettings.clearSettings(localStorage, AGENT_SETTINGS_KEY);
    } catch {
      // The visible fields can still be cleared when storage is unavailable.
    }
    $("#agentProvider").value = "openai_responses";
    $("#agentModel").value = "gpt-5.6";
    $("#agentModel").placeholder = "输入模型 ID";
    $("#agentApiUrl").value = "https://api.openai.com/v1";
    $("#agentApiKey").value = "";
    toast("已清除浏览器缓存的 Agent 配置");
  }

  const agentCsrfTokens = new Map();

  async function csrfTokenFor(agentUrl) {
    const sessionUrl = new URL("/api/session", agentUrl).toString();
    if (agentCsrfTokens.has(sessionUrl)) return agentCsrfTokens.get(sessionUrl);
    const response = await fetch(sessionUrl, { cache: "no-store" });
    const data = await response.json().catch(() => ({}));
    if (!response.ok || typeof data.csrf_token !== "string" || !data.csrf_token) {
      throw new Error(data.error || `无法建立 Agent 安全会话（HTTP ${response.status}）`);
    }
    agentCsrfTokens.set(sessionUrl, data.csrf_token);
    return data.csrf_token;
  }

  async function requestAgentApi(payload) {
    const urls = [];
    if (location.protocol !== "file:") urls.push("/api/agent/process");
    [
      "http://127.0.0.1:8080/api/agent/process",
      "http://127.0.0.1:18080/api/agent/process"
    ].forEach((url) => {
      if (!urls.includes(url)) urls.push(url);
    });
    let lastError = null;
    for (const url of urls) {
      try {
        const absoluteUrl = new URL(url, location.href).toString();
        const csrfToken = await csrfTokenFor(absoluteUrl);
        const options = {
          method: "POST",
          headers: {
            "Content-Type": "application/json",
            "X-DXF-CSRF-Token": csrfToken
          },
          body: JSON.stringify(payload)
        };
        const response = await fetch(absoluteUrl, options);
        if (response.status !== 404 && response.status !== 405) return response;
        lastError = new Error(`${url} 返回 ${response.status}`);
      } catch (error) {
        lastError = error;
      }
    }
    throw new Error(`无法连接本地 Agent 服务（已尝试 8080 和 18080 端口）：${lastError?.message || "连接失败"}`);
  }

  async function runAgent() {
    const instruction = $("#agentPrompt").value.trim();
    const provider = $("#agentProvider").value;
    const model = $("#agentModel").value.trim();
    const apiUrl = $("#agentApiUrl").value.trim();
    const apiKeyInput = $("#agentApiKey");
    const apiKey = apiKeyInput.value.trim();
    if (!instruction) return toast("请输入 AI 图形处理指令", "error");
    if (!model) return toast("请输入模型名称", "error");
    if (!apiUrl) return toast("请输入 API 地址", "error");
    saveAgentSettings({ provider, model, apiUrl });
    if (!state.points.length && !state.lines.length) return toast("当前没有可处理的图元", "error");
    const button = $("#runAgentBtn");
    const status = $("#agentStatus");
    clearAgentResult();
    button.disabled = true;
    button.textContent = "Agent 处理中…";
    status.textContent = "运行中";
    status.classList.add("running");
    setStatus("AI Agent 正在处理图形副本…");
    try {
      const response = await requestAgentApi({
        instruction,
        provider,
        model,
        api_url: apiUrl,
        api_key: apiKey,
        selected_ids: selectedEntityIds(),
        geometry: { points: state.points, lines: state.lines }
      });
      const data = await response.json().catch(() => ({}));
      if (!response.ok) throw new Error(data.error || `Agent 请求失败（HTTP ${response.status}）`);
      if (!data.geometry || !Array.isArray(data.geometry.points) || !Array.isArray(data.geometry.lines)) {
        throw new Error("Agent 返回的图形结果无效");
      }
      state.pendingAgentResult = data;
      $("#agentSummary").textContent = data.summary || "Agent 已完成处理";
      const beforeCount = (data.before?.point_count || 0) + (data.before?.line_count || 0);
      const afterCount = (data.after?.point_count || 0) + (data.after?.line_count || 0);
      const actions = Array.isArray(data.actions) && data.actions.length ? data.actions.join("；") : "未修改图元";
      $("#agentChanges").textContent = `${actions}。图元数量 ${beforeCount} → ${afterCount}`;
      $("#agentResult").classList.remove("hidden");
      status.textContent = "等待确认";
      setStatus("AI Agent 已生成预览结果");
      toast("Agent 处理完成，请确认是否应用");
    } catch (error) {
      status.textContent = "失败";
      setStatus("AI Agent 处理失败");
      const hint = location.protocol === "file:" || location.port === "5500"
        ? "；请先在项目根目录运行 node server.js" : "";
      toast(`${error.message}${hint}`, "error");
    } finally {
      button.disabled = false;
      button.textContent = "运行 Agent";
      status.classList.remove("running");
    }
  }

  function applyAgentResult() {
    const result = state.pendingAgentResult;
    if (!result) return toast("没有可应用的 Agent 结果");
    snapshot();
    state.points = JSON.parse(JSON.stringify(result.geometry.points));
    state.lines = JSON.parse(JSON.stringify(result.geometry.lines));
    state.hasAgentChanges = true;
    state.selected = null;
    state.multiSelection = [];
    state.selectionBox = null;
    currentLayers().forEach((layer) => { state.layerVisibility[layer] = true; });
    setDirty(true);
    clearAgentResult();
    $("#agentStatus").textContent = "已应用";
    refresh();
    fitView();
    toast("AI Agent 结果已应用到工作副本，原图未修改");
  }

  function restoreOriginalDrawing() {
    if (!state.originalGeometry) return toast("当前没有可恢复的导入原图");
    snapshot();
    const original = JSON.parse(state.originalGeometry);
    state.points = original.points;
    state.lines = original.lines;
    state.hasAgentChanges = false;
    state.selected = null;
    state.multiSelection = [];
    state.selectionBox = null;
    clearAgentResult();
    $("#agentStatus").textContent = "待命";
    setDirty(false);
    refresh();
    fitView();
    toast("已恢复原图基线，Agent 修改未写入原图");
  }

  function exportFile() {
    if (!state.points.length && !state.lines.length) return toast("没有可导出的图元");
    const content = DXFStudio.exportDxf({
      points: state.points,
      lines: state.lines,
      layers: currentLayers()
    });
    const blob = new Blob([content], { type: "application/dxf;charset=utf-8" });
    const link = document.createElement("a");
    const base = (state.fileName || "drawing").replace(/\.dxf$/i, "");
    link.href = URL.createObjectURL(blob);
    link.download = `${base}_${state.hasAgentChanges ? "ai_revision" : "discretized"}.dxf`;
    document.body.appendChild(link);
    link.click();
    link.remove();
    window.setTimeout(() => URL.revokeObjectURL(link.href), 1000);
    setDirty(false);
    setStatus("导出完成");
    toast("DXF 已导出");
  }

  function resetDrawing() {
    state.parsed = null;
    state.points = [];
    state.lines = [];
    state.originalGeometry = null;
    state.hasAgentChanges = false;
    state.fileName = "";
    state.fileSize = 0;
    state.selected = null;
    state.multiSelection = [];
    state.selectionBox = null;
    state.history = [];
    state.future = [];
    updateHistoryButtons();
    state.sourceVisibility = {};
    state.layerVisibility = {};
    state.selectedLayer = null;
    clearAgentResult();
    $("#fileName").textContent = "未命名图纸";
    $("#sideFileName").textContent = "等待导入";
    $("#fileMeta").textContent = "—";
    $("#restoreOriginalBtn").disabled = true;
    setDirty(false);
    refresh();
    fitView();
    setStatus("就绪");
  }

  $$('input[type="file"]').forEach((input) => input.addEventListener("change", () => {
    readFile(input.files[0]);
    input.value = "";
  }));
  $("#exportBtn").addEventListener("click", exportFile);
  $("#newBtn").addEventListener("click", resetDrawing);
  $("#restoreOriginalBtn").addEventListener("click", restoreOriginalDrawing);
  $("#fitBtn").addEventListener("click", fitView);
  $("#gridBtn").addEventListener("click", () => {
    shell.classList.toggle("no-grid");
    $("#gridBtn").classList.toggle("active", !shell.classList.contains("no-grid"));
  });
  $("#zoomInBtn").addEventListener("click", () => zoomAt(1.2));
  $("#zoomOutBtn").addEventListener("click", () => zoomAt(1 / 1.2));
  $("#undoBtn").addEventListener("click", undo);
  $("#redoBtn").addEventListener("click", redo);
  $("#deleteBtn").addEventListener("click", deleteSelected);
  $("#rediscretizeBtn").addEventListener("click", rediscretize);
  $("#applyTransformBtn").addEventListener("click", applyTransform);
  $("#agentForm").addEventListener("submit", (event) => {
    event.preventDefault();
    runAgent();
  });
  $("#applyAgentBtn").addEventListener("click", applyAgentResult);
  $("#cancelAgentBtn").addEventListener("click", () => {
    clearAgentResult();
    $("#agentStatus").textContent = "已取消";
  });
  $("#agentProvider").addEventListener("change", (event) => {
    const defaults = {
      openai_responses: { model: "gpt-5.6", url: "https://api.openai.com/v1" },
      openai_chat: { model: "deepseek-v4-pro", url: "https://api.deepseek.com" },
      anthropic: { model: "", url: "https://api.anthropic.com" }
    }[event.target.value];
    $("#agentModel").value = defaults.model;
    $("#agentModel").placeholder = event.target.value === "anthropic" ? "输入 Anthropic 模型 ID" : "输入模型 ID";
    $("#agentApiUrl").value = defaults.url;
    saveCurrentAgentSettings();
  });
  ["agentModel", "agentApiUrl"].forEach((id) => {
    $(`#${id}`).addEventListener("input", scheduleAgentSettingsSave);
  });
  $("#clearAgentCacheBtn").addEventListener("click", clearAgentSettings);
  $("#toggleAllBtn").addEventListener("click", () => {
    state.allVisible = !state.allVisible;
    currentLayers().forEach((layer) => { state.layerVisibility[layer] = state.allVisible; });
    if (!state.allVisible) state.selectedLayer = null;
    updateLists();
    updateInspector();
    render();
  });
  $$(".tool[data-tool]").forEach((button) => button.addEventListener("click", () => selectTool(button.dataset.tool)));

  $("#tolerance").addEventListener("change", (event) => {
    const value = Number(event.target.value);
    if (!(value > 0)) {
      event.target.value = state.tolerance;
      return toast("公差必须大于 0", "error");
    }
    state.tolerance = value;
    $("#toleranceRange").value = Math.log10(value);
  });
  $("#toleranceRange").addEventListener("input", (event) => {
    state.tolerance = 10 ** Number(event.target.value);
    $("#tolerance").value = Number(state.tolerance.toPrecision(5));
  });
  $$('input[name="drawingProfile"]').forEach((input) => input.addEventListener("change", (event) => {
    state.drawingProfile = event.target.value;
    state.tolerance = currentDrawingProfile().defaultTolerance;
    syncToleranceControls();
    updateDrawingProfileMeta();
    if (state.parsed) {
      toast("图纸模式已切换，请点击重新离散化以应用新上限和容差");
    }
  }));

  canvas.addEventListener("wheel", (event) => {
    event.preventDefault();
    const rect = canvas.getBoundingClientRect();
    zoomAt(event.deltaY < 0 ? 1.12 : 1 / 1.12, { x: event.clientX - rect.left, y: event.clientY - rect.top });
  }, { passive: false });

  canvas.addEventListener("pointerdown", (event) => {
    const rect = canvas.getBoundingClientRect();
    const screen = { x: event.clientX - rect.left, y: event.clientY - rect.top };
    if (state.tool === "pan" || state.spacePressed || event.button === 1) {
      state.dragging = { type: "pan", x: event.clientX, y: event.clientY, originX: state.view.x, originY: state.view.y };
      canvas.setPointerCapture(event.pointerId);
      canvas.style.cursor = "grabbing";
      return;
    }
    if (state.tool === "point") {
      snapshot();
      const point = screenToWorld(screen);
      state.points.push({ id: `p-user-${Date.now()}`, ...point, layer: "0", sourceType: "POINT", visible: true });
      setDirty(true);
      refresh();
      return;
    }
    if (state.tool === "line") {
      const point = screenToWorld(screen);
      if (!state.drawingStart) {
        state.drawingStart = { ...point, preview: point };
        render();
      } else {
        snapshot();
        state.lines.push({
          id: `l-user-${Date.now()}`, x1: state.drawingStart.x, y1: state.drawingStart.y,
          x2: point.x, y2: point.y, layer: "0", sourceType: "LINE", visible: true
        });
        state.drawingStart = null;
        setDirty(true);
        refresh();
      }
      return;
    }
    const hit = hitTest(screen);
    let canDrag = Boolean(hit);
    if (event.shiftKey && hit) {
      canDrag = toggleSelection(hit);
    } else if (hit) {
      if (!isSelected(hit.kind, hit.index) || state.multiSelection.length === 0) {
        applySelection([hit]);
      }
    } else if (!event.shiftKey) {
      applySelection([]);
    }
    updateInspector();
    render();
    if (hit && canDrag && event.button === 0) {
      beginGeometryDrag(hit, screen, event.pointerId);
    } else if (event.button === 0) {
      state.selectionBox = { start: screen, current: screen, additive: event.shiftKey };
      state.dragging = { type: "box" };
      canvas.setPointerCapture(event.pointerId);
      render();
    }
  });

  canvas.addEventListener("pointermove", (event) => {
    const rect = canvas.getBoundingClientRect();
    const screen = { x: event.clientX - rect.left, y: event.clientY - rect.top };
    const world = screenToWorld(screen);
    $("#coordinates").textContent = `X ${world.x.toFixed(3)}   Y ${world.y.toFixed(3)}`;
    if (state.dragging && state.dragging.type === "pan") {
      state.view.x = state.dragging.originX + event.clientX - state.dragging.x;
      state.view.y = state.dragging.originY + event.clientY - state.dragging.y;
      render();
    } else if (state.dragging && state.dragging.type.startsWith("edit-")) {
      dragGeometry(world);
    } else if (state.dragging && state.dragging.type === "box") {
      state.selectionBox.current = screen;
      render();
    } else if (state.drawingStart) {
      state.drawingStart.preview = world;
      render();
    } else if (state.tool === "select") {
      const hit = hitTest(screen);
      if (!hit) canvas.style.cursor = "default";
      else if (hit.kind === "point") canvas.style.cursor = "move";
      else canvas.style.cursor = lineDragMode(state.lines[hit.index], screen) === "move" ? "move" : "crosshair";
    }
  });

  function endPointer(event) {
    if (!state.dragging) return;
    const boxSelection = state.dragging.type === "box";
    const changed = state.dragging.changed;
    if (boxSelection) {
      const rect = canvas.getBoundingClientRect();
      state.selectionBox.current = {
        x: event.clientX - rect.left,
        y: event.clientY - rect.top
      };
      finishBoxSelection();
    }
    state.dragging = null;
    if (canvas.hasPointerCapture(event.pointerId)) canvas.releasePointerCapture(event.pointerId);
    canvas.style.cursor = state.tool === "pan" ? "grab" : state.tool === "select" ? "default" : "crosshair";
    if (changed) {
      setDirty(true);
      refresh();
    } else if (boxSelection) {
      refresh();
    }
  }
  canvas.addEventListener("pointerup", endPointer);
  canvas.addEventListener("pointercancel", endPointer);

  ["dragenter", "dragover"].forEach((type) => shell.addEventListener(type, (event) => {
    event.preventDefault();
    shell.classList.add("dragover");
  }));
  ["dragleave", "drop"].forEach((type) => shell.addEventListener(type, (event) => {
    event.preventDefault();
    if (type === "drop") readFile(event.dataTransfer.files[0]);
    shell.classList.remove("dragover");
  }));

  window.addEventListener("keydown", (event) => {
    if (/INPUT|TEXTAREA/.test(document.activeElement.tagName)) return;
    if (event.code === "Space") {
      event.preventDefault();
      state.spacePressed = true;
      canvas.style.cursor = "grab";
    } else if (event.key === "Delete" || event.key === "Backspace") deleteSelected();
    else if ((event.ctrlKey || event.metaKey) &&
      (event.key.toLowerCase() === "y" || (event.shiftKey && event.key.toLowerCase() === "z"))) {
      event.preventDefault();
      redo();
    } else if ((event.ctrlKey || event.metaKey) && event.key.toLowerCase() === "z") {
      event.preventDefault();
      undo();
    } else if (event.key.toLowerCase() === "v") selectTool("select");
    else if (event.key.toLowerCase() === "h") selectTool("pan");
    else if (event.key.toLowerCase() === "p") selectTool("point");
    else if (event.key.toLowerCase() === "l") selectTool("line");
    else if (event.key.toLowerCase() === "f") fitView();
    else if (event.key === "Escape") {
      state.drawingStart = null;
      state.selected = null;
      state.multiSelection = [];
      state.selectionBox = null;
      updateInspector();
      render();
    }
  });
  window.addEventListener("keyup", (event) => {
    if (event.code === "Space") {
      state.spacePressed = false;
      canvas.style.cursor = state.tool === "pan" ? "grab" : state.tool === "select" ? "default" : "crosshair";
    }
  });
  window.addEventListener("resize", resizeCanvas);

  if ("ResizeObserver" in window) new ResizeObserver(resizeCanvas).observe(shell);
  loadAgentSettings();
  updateDrawingProfileMeta();
  resetDrawing();
  requestAnimationFrame(resizeCanvas);
})();
