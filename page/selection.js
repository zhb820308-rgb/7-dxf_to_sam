(function (global) {
  "use strict";

  const app = global.DXFStudioApp;

  app.isSelected = function (kind, index) {
    return Boolean(
      (app.state.selected && app.state.selected.kind === kind &&
        app.state.selected.index === index) ||
      app.state.multiSelection.some((item) =>
        item.kind === kind && item.index === index
      )
    );
  };

  app.sameSelection = function (a, b) {
    return Boolean(a && b && a.kind === b.kind && a.index === b.index);
  };

  app.currentSelection = function () {
    if (app.state.multiSelection.length) {
      return app.state.multiSelection.slice();
    }
    return app.state.selected ? [app.state.selected] : [];
  };

  app.applySelection = function (items) {
    const unique = [];
    items.forEach((item) => {
      if (item && !unique.some((existing) => app.sameSelection(existing, item))) {
        unique.push(item);
      }
    });
    if (unique.length <= 1) {
      app.state.selected = unique[0] || null;
      app.state.multiSelection = [];
    } else {
      app.state.selected = null;
      app.state.multiSelection = unique;
    }
  };

  app.toggleSelection = function (hit) {
    const selection = app.currentSelection();
    const index = selection.findIndex((item) => app.sameSelection(item, hit));
    if (index >= 0) selection.splice(index, 1);
    else selection.push(hit);
    app.applySelection(selection);
    return index < 0;
  };

  app.hitTest = function (screen) {
    // The visible handles of the selected line must win over nearby geometry.
    // Otherwise, coincident endpoints can unexpectedly switch selection to
    // another line just before the user starts dragging the handle.
    if (app.state.selected && app.state.selected.kind === "line") {
      const selectedLine = app.state.lines[app.state.selected.index];
      if (selectedLine && app.visible(selectedLine)) {
        const start = app.worldToScreen({
          x: selectedLine.x1,
          y: selectedLine.y1
        });
        const end = app.worldToScreen({
          x: selectedLine.x2,
          y: selectedLine.y2
        });
        if (Math.min(
          Math.hypot(screen.x - start.x, screen.y - start.y),
          Math.hypot(screen.x - end.x, screen.y - end.y)
        ) <= 10) {
          return { kind: "line", index: app.state.selected.index };
        }
      }
    }

    let best = null;
    let distance = 9;
    app.state.points.forEach((point, index) => {
      if (!app.visible(point)) return;
      const p = app.worldToScreen(point);
      const candidate = Math.hypot(screen.x - p.x, screen.y - p.y);
      if (candidate < distance) {
        distance = candidate;
        best = { kind: "point", index };
      }
    });
    app.state.lines.forEach((line, index) => {
      if (!app.visible(line)) return;
      const a = app.worldToScreen({ x: line.x1, y: line.y1 });
      const b = app.worldToScreen({ x: line.x2, y: line.y2 });
      const candidate = app.distanceToSegment(screen, a, b);
      if (candidate < distance) {
        distance = candidate;
        best = { kind: "line", index };
      }
    });
    return best;
  };

  app.lineDragMode = function (line, screen) {
    const start = app.worldToScreen({ x: line.x1, y: line.y1 });
    const end = app.worldToScreen({ x: line.x2, y: line.y2 });
    if (Math.hypot(screen.x - start.x, screen.y - start.y) <= 10) {
      return "start";
    }
    if (Math.hypot(screen.x - end.x, screen.y - end.y) <= 10) return "end";
    return "move";
  };

  app.beginGeometryDrag = function (hit, screen, pointerId) {
    const world = app.screenToWorld(screen);
    const selection = app.currentSelection();
    if (selection.length > 1 &&
      selection.some((item) => app.sameSelection(item, hit))) {
      app.state.dragging = {
        type: "edit-group",
        startWorld: world,
        originals: selection.map((item) => {
          const entity = item.kind === "point"
            ? app.state.points[item.index]
            : app.state.lines[item.index];
          return item.kind === "point"
            ? { ...item, x: entity.x, y: entity.y }
            : {
              ...item,
              x1: entity.x1,
              y1: entity.y1,
              x2: entity.x2,
              y2: entity.y2
            };
        }),
        changed: false,
        saved: false
      };
    } else if (hit.kind === "point") {
      const point = app.state.points[hit.index];
      app.state.dragging = {
        type: "edit-point",
        index: hit.index,
        startWorld: world,
        original: { x: point.x, y: point.y },
        changed: false,
        saved: false
      };
    } else {
      const line = app.state.lines[hit.index];
      app.state.dragging = {
        type: "edit-line",
        index: hit.index,
        mode: app.lineDragMode(line, screen),
        startWorld: world,
        original: {
          x1: line.x1,
          y1: line.y1,
          x2: line.x2,
          y2: line.y2
        },
        changed: false,
        saved: false
      };
    }
    app.canvas.setPointerCapture(pointerId);
  };

  app.dragGeometry = function (world) {
    const drag = app.state.dragging;
    if (!drag || !drag.type.startsWith("edit-")) return;
    const dx = world.x - drag.startWorld.x;
    const dy = world.y - drag.startWorld.y;
    if (!drag.saved && Math.hypot(dx, dy) * app.state.view.scale >= 1) {
      app.snapshot();
      drag.saved = true;
    }
    if (!drag.saved) return;
    drag.changed = true;
    if (drag.type === "edit-group") {
      drag.originals.forEach((original) => {
        if (original.kind === "point") {
          const point = app.state.points[original.index];
          point.x = original.x + dx;
          point.y = original.y + dy;
        } else {
          const line = app.state.lines[original.index];
          line.x1 = original.x1 + dx;
          line.y1 = original.y1 + dy;
          line.x2 = original.x2 + dx;
          line.y2 = original.y2 + dy;
        }
      });
    } else if (drag.type === "edit-point") {
      const point = app.state.points[drag.index];
      point.x = drag.original.x + dx;
      point.y = drag.original.y + dy;
    } else {
      const line = app.state.lines[drag.index];
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
    app.render();
  };

  app.distanceToSegment = function (point, a, b) {
    const dx = b.x - a.x;
    const dy = b.y - a.y;
    if (!dx && !dy) return Math.hypot(point.x - a.x, point.y - a.y);
    const t = Math.max(0, Math.min(
      1,
      ((point.x - a.x) * dx + (point.y - a.y) * dy) /
        (dx * dx + dy * dy)
    ));
    return Math.hypot(
      point.x - (a.x + t * dx),
      point.y - (a.y + t * dy)
    );
  };

  app.normalizedBox = function (a, b) {
    const left = Math.min(a.x, b.x);
    const top = Math.min(a.y, b.y);
    const right = Math.max(a.x, b.x);
    const bottom = Math.max(a.y, b.y);
    return {
      left,
      top,
      right,
      bottom,
      width: right - left,
      height: bottom - top
    };
  };

  app.pointInBox = function (point, box) {
    return point.x >= box.left && point.x <= box.right &&
      point.y >= box.top && point.y <= box.bottom;
  };

  app.lineIntersectsBox = function (a, b, box) {
    if (app.pointInBox(a, box) || app.pointInBox(b, box)) return true;
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
  };

  app.finishBoxSelection = function () {
    if (!app.state.selectionBox) return;
    const box = app.normalizedBox(
      app.state.selectionBox.start,
      app.state.selectionBox.current
    );
    const additive = app.state.selectionBox.additive;
    const selection = [];
    if (box.width >= 3 || box.height >= 3) {
      app.state.points.forEach((point, index) => {
        if (app.visible(point) &&
          app.pointInBox(app.worldToScreen(point), box)) {
          selection.push({ kind: "point", index });
        }
      });
      app.state.lines.forEach((line, index) => {
        if (!app.visible(line)) return;
        const start = app.worldToScreen({ x: line.x1, y: line.y1 });
        const end = app.worldToScreen({ x: line.x2, y: line.y2 });
        if (app.lineIntersectsBox(start, end, box)) {
          selection.push({ kind: "line", index });
        }
      });
    }
    app.state.selectionBox = null;
    app.applySelection(
      additive ? app.currentSelection().concat(selection) : selection
    );
  };

  app.selectTool = function (tool) {
    app.state.tool = tool;
    app.state.drawingStart = null;
    app.$$(".tool[data-tool]").forEach((button) => {
      button.classList.toggle("active", button.dataset.tool === tool);
    });
    app.canvas.style.cursor = tool === "pan"
      ? "grab"
      : tool === "select" ? "default" : "crosshair";
    app.render();
  };

  app.deleteSelected = function () {
    if (!app.state.selected && !app.state.multiSelection.length) {
      return app.toast("请先选择图元");
    }
    app.snapshot();
    if (app.state.multiSelection.length) {
      const pointIndexes = app.state.multiSelection
        .filter((item) => item.kind === "point")
        .map((item) => item.index)
        .sort((a, b) => b - a);
      const lineIndexes = app.state.multiSelection
        .filter((item) => item.kind === "line")
        .map((item) => item.index)
        .sort((a, b) => b - a);
      pointIndexes.forEach((index) => app.state.points.splice(index, 1));
      lineIndexes.forEach((index) => app.state.lines.splice(index, 1));
    } else {
      const list = app.state.selected.kind === "point"
        ? app.state.points
        : app.state.lines;
      list.splice(app.state.selected.index, 1);
    }
    app.state.selected = null;
    app.state.multiSelection = [];
    app.setDirty(true);
    app.refresh();
  };

  app.applyTransform = function () {
    if (!app.state.points.length && !app.state.lines.length) {
      return app.toast("当前没有图元");
    }
    const dx = Number(app.$("#translateX").value);
    const dy = Number(app.$("#translateY").value);
    const angle = Number(app.$("#rotate").value) * Math.PI / 180;
    const scale = Number(app.$("#scale").value);
    if (![dx, dy, angle, scale].every(Number.isFinite) || scale <= 0) {
      return app.toast("请输入有效的变换参数", "error");
    }
    app.snapshot();
    const cos = Math.cos(angle);
    const sin = Math.sin(angle);
    const transform = (x, y) => ({
      x: (x * cos - y * sin) * scale + dx,
      y: (x * sin + y * cos) * scale + dy
    });
    app.state.points.forEach((point) => {
      Object.assign(point, transform(point.x, point.y));
    });
    app.state.lines.forEach((line) => {
      const a = transform(line.x1, line.y1);
      const b = transform(line.x2, line.y2);
      Object.assign(line, { x1: a.x, y1: a.y, x2: b.x, y2: b.y });
    });
    app.setDirty(true);
    app.refresh();
    app.fitView();
    app.toast("整体变换已应用");
  };

  app.selectedEntityIds = function () {
    if (app.state.multiSelection.length) {
      return app.state.multiSelection.map((selection) => {
        const list = selection.kind === "point"
          ? app.state.points
          : app.state.lines;
        return list[selection.index]?.id;
      }).filter(Boolean);
    }
    if (!app.state.selected) return [];
    const list = app.state.selected.kind === "point"
      ? app.state.points
      : app.state.lines;
    const id = list[app.state.selected.index]?.id;
    return id ? [id] : [];
  };
})(window);
