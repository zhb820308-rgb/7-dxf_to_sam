(function (global) {
  "use strict";

  const app = global.DXFStudioApp;

  app.importText = function (text, name, size) {
    app.setStatus("正在解析…");
    try {
      const parsed = DXFStudio.parseDxf(text);
      const discretized = DXFStudio.discretize(
        parsed,
        app.state.tolerance,
        app.state.drawingProfile
      );
      if (!discretized.points.length && !discretized.lines.length) {
        const unsupported = Object.keys(discretized.unsupported).join("、");
        throw new Error(unsupported
          ? `未找到可显示图元（不支持：${unsupported}）`
          : "未找到可显示图元");
      }
      app.state.parsed = parsed;
      app.state.points = discretized.points;
      app.state.lines = discretized.lines;
      app.state.hasAgentChanges = false;
      app.saveOriginalGeometry();
      app.state.fileName = name;
      app.state.fileSize = size;
      app.state.selected = null;
      app.state.multiSelection = [];
      app.state.selectionBox = null;
      app.state.sourceVisibility = {};
      // The editor starts with every imported layer visible, including layers
      // marked off/frozen in the source DXF. Users can hide them from the list.
      app.state.layerVisibility = Object.fromEntries(
        Object.keys(parsed.layers).map((layer) => [layer, true])
      );
      app.state.selectedLayer = null;
      app.state.history = [];
      app.state.future = [];
      app.updateHistoryButtons();
      app.clearAgentResult();
      app.$("#fileName").textContent = name;
      app.$("#sideFileName").textContent = name;
      app.$("#fileMeta").textContent =
        `${app.formatBytes(size)} · ${parsed.entities.length} 个原始图元 · ` +
        `${discretized.points.length + discretized.lines.length} 个输出图元`;
      app.$("#restoreOriginalBtn").disabled = false;
      app.setDirty(false);
      app.refresh();
      requestAnimationFrame(app.fitView);
      app.setStatus("导入完成");
      const unsupportedCount = Object.values(discretized.unsupported)
        .reduce((sum, value) => sum + value, 0);
      app.toast(unsupportedCount
        ? `已导入，跳过 ${unsupportedCount} 个不支持的图元`
        : "DXF 导入成功");
    } catch (error) {
      app.setStatus("导入失败");
      app.toast(error.message || "无法解析该 DXF 文件", "error");
    }
  };

  app.readFile = function (file) {
    if (!file) return;
    if (!/\.dxf$/i.test(file.name)) {
      return app.toast("请选择 .dxf 文件", "error");
    }
    if (file.size > 50 * 1024 * 1024) {
      return app.toast("文件超过 50 MB，无法在浏览器中处理", "error");
    }
    const reader = new FileReader();
    reader.onerror = () => app.toast("文件读取失败", "error");
    reader.onload = () => app.importText(reader.result, file.name, file.size);
    reader.readAsText(file);
  };

  app.formatBytes = function (bytes) {
    if (!bytes) return "0 B";
    const units = ["B", "KB", "MB"];
    const index = Math.min(2, Math.floor(Math.log(bytes) / Math.log(1024)));
    return `${(bytes / 1024 ** index).toFixed(index ? 1 : 0)} ${units[index]}`;
  };

  app.rediscretize = function () {
    if (!app.state.parsed) return app.toast("请先导入 DXF");
    let result;
    try {
      result = DXFStudio.discretize(
        app.state.parsed,
        app.state.tolerance,
        app.state.drawingProfile
      );
    } catch (error) {
      app.setStatus("重新离散化失败");
      return app.toast(
        error.message || "图元数量超过当前图纸模式上限",
        "error"
      );
    }
    app.snapshot();
    app.state.points = result.points;
    app.state.lines = result.lines;
    app.state.hasAgentChanges = false;
    app.saveOriginalGeometry();
    app.state.selected = null;
    app.state.multiSelection = [];
    app.setDirty(true);
    app.refresh();
    app.fitView();
    const typeCount = Object.keys(result.processedByType).length;
    const unsupportedCount = Object.values(result.unsupported)
      .reduce((sum, count) => sum + count, 0);
    app.$("#fileMeta").textContent =
      `${app.formatBytes(app.state.fileSize)} · ${result.sourceCount} 个原始图元 · ` +
      `${result.points.length + result.lines.length} 个输出图元`;
    app.setStatus(`已重新离散化全部 ${result.sourceCount} 个图元`);
    app.toast(unsupportedCount
      ? `已重建 ${result.sourceCount} 个原始图元（${typeCount} 类），` +
        `跳过 ${unsupportedCount} 个不支持图元`
      : `已重新离散化全部 ${result.sourceCount} 个原始图元（${typeCount} 类）`);
  };

  app.restoreOriginalDrawing = function () {
    if (!app.state.originalGeometry) {
      return app.toast("当前没有可恢复的导入原图");
    }
    app.snapshot();
    const original = JSON.parse(app.state.originalGeometry);
    app.state.points = original.points;
    app.state.lines = original.lines;
    app.state.hasAgentChanges = false;
    app.state.selected = null;
    app.state.multiSelection = [];
    app.state.selectionBox = null;
    app.clearAgentResult();
    app.$("#agentStatus").textContent = "待命";
    app.setDirty(false);
    app.refresh();
    app.fitView();
    app.toast("已恢复原图基线，Agent 修改未写入原图");
  };

  app.exportFile = function () {
    if (!app.state.points.length && !app.state.lines.length) {
      return app.toast("没有可导出的图元");
    }
    const content = DXFStudio.exportDxf({
      points: app.state.points,
      lines: app.state.lines,
      layers: app.currentLayers()
    });
    const blob = new Blob([content], { type: "application/dxf;charset=utf-8" });
    const link = document.createElement("a");
    const base = (app.state.fileName || "drawing").replace(/\.dxf$/i, "");
    link.href = URL.createObjectURL(blob);
    link.download =
      `${base}_${app.state.hasAgentChanges ? "ai_revision" : "discretized"}.dxf`;
    document.body.appendChild(link);
    link.click();
    link.remove();
    global.setTimeout(() => URL.revokeObjectURL(link.href), 1000);
    app.setDirty(false);
    app.setStatus("导出完成");
    app.toast("DXF 已导出");
  };

  app.resetDrawing = function () {
    app.state.parsed = null;
    app.state.points = [];
    app.state.lines = [];
    app.state.originalGeometry = null;
    app.state.hasAgentChanges = false;
    app.state.fileName = "";
    app.state.fileSize = 0;
    app.state.selected = null;
    app.state.multiSelection = [];
    app.state.selectionBox = null;
    app.state.history = [];
    app.state.future = [];
    app.updateHistoryButtons();
    app.state.sourceVisibility = {};
    app.state.layerVisibility = {};
    app.state.selectedLayer = null;
    app.clearAgentResult();
    app.$("#fileName").textContent = "未命名图纸";
    app.$("#sideFileName").textContent = "等待导入";
    app.$("#fileMeta").textContent = "—";
    app.$("#restoreOriginalBtn").disabled = true;
    app.setDirty(false);
    app.refresh();
    app.fitView();
    app.setStatus("就绪");
  };
})(window);
