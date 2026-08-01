(function (global) {
  "use strict";

  const app = global.DXFStudioApp;
  const MIN_VIEW_SCALE = 1e-12;
  const MAX_VIEW_SCALE = 1e12;

  app.worldToScreen = function (point) {
    return {
      x: app.state.view.x + point.x * app.state.view.scale,
      y: app.state.view.y - point.y * app.state.view.scale
    };
  };

  app.screenToWorld = function (point) {
    return {
      x: (point.x - app.state.view.x) / app.state.view.scale,
      y: (app.state.view.y - point.y) / app.state.view.scale
    };
  };

  app.visible = function (item) {
    return app.state.sourceVisibility[item.sourceType] !== false &&
      DXFStudio.isLayerVisible(item, app.state.layerVisibility);
  };

  app.affectedByLayer = function (item, layer) {
    return DXFStudio.affectsLayer(item, layer);
  };

  app.resizeCanvas = function () {
    const rect = app.canvas.getBoundingClientRect();
    const dpr = global.devicePixelRatio || 1;
    const width = Math.max(1, Math.round(rect.width * dpr));
    const height = Math.max(1, Math.round(rect.height * dpr));
    if (app.canvas.width !== width || app.canvas.height !== height) {
      app.canvas.width = width;
      app.canvas.height = height;
    }
    app.ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
    app.render();
  };

  app.bounds = function () {
    const box = {
      minX: Infinity,
      maxX: -Infinity,
      minY: Infinity,
      maxY: -Infinity
    };
    const include = (x, y) => {
      box.minX = Math.min(box.minX, x);
      box.maxX = Math.max(box.maxX, x);
      box.minY = Math.min(box.minY, y);
      box.maxY = Math.max(box.maxY, y);
    };
    app.state.points.filter(app.visible).forEach((point) => include(point.x, point.y));
    app.state.lines.filter(app.visible).forEach((line) => {
      include(line.x1, line.y1);
      include(line.x2, line.y2);
    });
    return Number.isFinite(box.minX) ? box : null;
  };

  app.fitView = function () {
    const box = app.bounds();
    const rect = app.canvas.getBoundingClientRect();
    if (!box || !rect.width || !rect.height) {
      app.state.view = { scale: 1, x: rect.width / 2, y: rect.height / 2 };
      app.updateZoom();
      return app.render();
    }
    const width = Math.max(box.maxX - box.minX, 1e-6);
    const height = Math.max(box.maxY - box.minY, 1e-6);
    app.state.view.scale = Math.max(
      MIN_VIEW_SCALE,
      Math.min(
        MAX_VIEW_SCALE,
        Math.min((rect.width - 100) / width, (rect.height - 100) / height)
      )
    );
    app.state.view.x = rect.width / 2 -
      (box.minX + box.maxX) / 2 * app.state.view.scale;
    app.state.view.y = rect.height / 2 +
      (box.minY + box.maxY) / 2 * app.state.view.scale;
    app.updateZoom();
    app.render();
  };

  app.updateZoom = function () {
    app.$("#zoomLabel").textContent = app.state.view.scale >= 10
      ? `${Math.round(app.state.view.scale)}×`
      : `${Math.round(app.state.view.scale * 100)}%`;
  };

  app.zoomAt = function (factor, screenPoint) {
    const rect = app.canvas.getBoundingClientRect();
    const anchor = screenPoint || { x: rect.width / 2, y: rect.height / 2 };
    const world = app.screenToWorld(anchor);
    app.state.view.scale = Math.max(
      MIN_VIEW_SCALE,
      Math.min(MAX_VIEW_SCALE, app.state.view.scale * factor)
    );
    app.state.view.x = anchor.x - world.x * app.state.view.scale;
    app.state.view.y = anchor.y + world.y * app.state.view.scale;
    app.updateZoom();
    app.render();
  };
})(window);
