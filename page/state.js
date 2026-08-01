(function (global) {
  "use strict";

  const app = global.DXFStudioApp = global.DXFStudioApp || {};

  app.$ = (selector, root = document) => root.querySelector(selector);
  app.$$ = (selector, root = document) => Array.from(root.querySelectorAll(selector));
  app.canvas = app.$("#canvas");
  app.ctx = app.canvas.getContext("2d");
  app.shell = app.$("#dropZone");

  app.state = {
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

  app.setStatus = function (message) {
    app.$("#statusText").textContent = message;
  };

  app.currentDrawingProfile = function () {
    return DXFStudio.EXPANSION_PROFILES[app.state.drawingProfile]
      || DXFStudio.EXPANSION_PROFILES.small;
  };

  app.syncToleranceControls = function () {
    app.$("#tolerance").value = Number(app.state.tolerance.toPrecision(5));
    app.$("#toleranceRange").value = Math.max(
      -4,
      Math.min(2, Math.log10(app.state.tolerance))
    );
  };

  app.updateDrawingProfileMeta = function () {
    const profile = app.currentDrawingProfile();
    const outputLimit = Number.isFinite(profile.maxOutputEntities)
      ? `最多 ${profile.maxOutputEntities.toLocaleString()} 个输出图元`
      : "不限制最终输出图元数（仍保留 INSERT 安全限制）";
    app.$("#profileHint").textContent =
      `${profile.label}：${outputLimit}；默认容差 ${profile.defaultTolerance}`;
  };

  app.toast = function (message, type = "") {
    const item = document.createElement("div");
    item.className = `toast ${type}`.trim();
    item.textContent = message;
    app.$("#toasts").appendChild(item);
    global.setTimeout(() => item.remove(), 3200);
  };

  app.setDirty = function (value) {
    if (value && app.state.pendingAgentResult) {
      app.clearAgentResult();
      app.$("#agentStatus").textContent = "待命";
    }
    app.state.dirty = value;
    app.$("#savedDot").classList.toggle("dirty", value);
    app.$("#savedDot").title = value ? "有尚未导出的更改" : "所有更改已保存";
  };

  app.serializeDrawing = function () {
    return JSON.stringify({
      points: app.state.points,
      lines: app.state.lines,
      hasAgentChanges: app.state.hasAgentChanges
    });
  };

  app.saveOriginalGeometry = function () {
    app.state.originalGeometry = JSON.stringify({
      points: app.state.points,
      lines: app.state.lines
    });
  };

  app.snapshot = function () {
    app.state.history.push(app.serializeDrawing());
    if (app.state.history.length > 50) app.state.history.shift();
    app.state.future = [];
    app.updateHistoryButtons();
  };

  app.restoreSnapshot = function (serialized) {
    const data = JSON.parse(serialized);
    app.state.points = data.points;
    app.state.lines = data.lines;
    app.state.hasAgentChanges = Boolean(data.hasAgentChanges);
    app.state.selected = null;
    app.state.multiSelection = [];
    app.state.selectionBox = null;
  };

  app.undo = function () {
    const previous = app.state.history.pop();
    if (!previous) return app.toast("没有可撤销的操作");
    app.state.future.push(app.serializeDrawing());
    if (app.state.future.length > 50) app.state.future.shift();
    app.restoreSnapshot(previous);
    app.setDirty(true);
    app.refresh();
    app.updateHistoryButtons();
    app.toast("已撤销");
  };

  app.redo = function () {
    const next = app.state.future.pop();
    if (!next) return app.toast("没有可重做的操作");
    app.state.history.push(app.serializeDrawing());
    if (app.state.history.length > 50) app.state.history.shift();
    app.restoreSnapshot(next);
    app.setDirty(true);
    app.refresh();
    app.updateHistoryButtons();
    app.toast("已重做");
  };

  app.updateHistoryButtons = function () {
    const undoButton = app.$("#undoBtn");
    const redoButton = app.$("#redoBtn");
    if (undoButton) undoButton.disabled = app.state.history.length === 0;
    if (redoButton) redoButton.disabled = app.state.future.length === 0;
  };
})(window);
