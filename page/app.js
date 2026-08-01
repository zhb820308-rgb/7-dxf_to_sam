(function (global) {
  "use strict";

  const app = global.DXFStudioApp;

  app.sortedLayers = function (layerCounts) {
    return Object.keys(layerCounts).sort((a, b) => a.localeCompare(b, "zh-CN"));
  };

  app.currentLayers = function () {
    return app.sortedLayers(
      DXFStudio.buildLayerCounts(app.state.points, app.state.lines)
    );
  };

  app.updateLists = function () {
    const counts = {};
    app.state.points.forEach((item) => {
      const type = item.sourceType || "POINT";
      counts[type] = (counts[type] || 0) + 1;
    });
    app.state.lines.forEach((item) => {
      const type = item.sourceType || "LINE";
      counts[type] = (counts[type] || 0) + 1;
    });
    const entityList = app.$("#entityList");
    entityList.innerHTML = "";
    Object.entries(counts).sort().forEach(([type, count]) => {
      const row = document.createElement("div");
      row.className = "entity-row";
      const off = app.state.sourceVisibility[type] === false;
      row.innerHTML =
        `<span class="swatch" style="background:${type === "POINT" ? "#ff6b3d" : "#4c7dff"}"></span>` +
        `<span class="label">${app.escapeHtml(type)}</span>` +
        `<span class="num">${count}</span>` +
        `<button class="eye ${off ? "off" : ""}" ` +
        `aria-label="${off ? "显示" : "隐藏"} ${app.escapeHtml(type)}">` +
        `${app.eyeSvg()}</button>`;
      app.$(".eye", row).addEventListener("click", (event) => {
        event.stopPropagation();
        app.state.sourceVisibility[type] = off;
        app.updateLists();
        app.render();
      });
      entityList.appendChild(row);
    });
    if (!Object.keys(counts).length) {
      entityList.innerHTML = '<div class="list-empty">暂无图元</div>';
    }

    const layerCounts = DXFStudio.buildLayerCounts(
      app.state.points,
      app.state.lines
    );
    const layers = app.sortedLayers(layerCounts);
    app.$("#layerCount").textContent = String(layers.length);
    const layerList = app.$("#layerList");
    layerList.innerHTML = "";
    layers.forEach((layer) => {
      // SAM counts every entity controlled by this layer, including explicit
      // child-layer entities inside an INSERT. The index is built in one pass.
      const count = layerCounts[layer] || 0;
      const off = app.state.layerVisibility[layer] === false;
      const row = document.createElement("div");
      row.className =
        `layer-row${app.state.selectedLayer === layer ? " selected" : ""}`;
      row.setAttribute("role", "button");
      row.setAttribute("tabindex", "0");
      row.setAttribute(
        "aria-pressed",
        String(app.state.selectedLayer === layer)
      );
      row.innerHTML =
        `<span class="layer-color" style="background:${DXFStudio.colorForLayer(
          layer,
          app.state.parsed ? app.state.parsed.layers : {}
        )}"></span>` +
        `<span class="label">${app.escapeHtml(layer)}</span>` +
        `<span class="num">${count}</span>` +
        `<button class="eye ${off ? "off" : ""}" ` +
        `aria-label="${off ? "显示" : "隐藏"}图层">${app.eyeSvg()}</button>`;
      const selectLayer = () => {
        app.state.selectedLayer = app.state.selectedLayer === layer
          ? null
          : layer;
        app.state.selected = null;
        app.state.multiSelection = [];
        app.updateLists();
        app.updateInspector();
        app.render();
        app.$("#selectionStatus").textContent =
          app.state.selectedLayer === null
            ? "未选择图层"
            : `已高亮图层 ${app.state.selectedLayer}`;
      };
      row.addEventListener("click", selectLayer);
      row.addEventListener("keydown", (event) => {
        if (event.target !== row) return;
        if (event.key === "Enter" || event.key === " ") {
          event.preventDefault();
          selectLayer();
        }
      });
      app.$(".eye", row).addEventListener("click", (event) => {
        event.stopPropagation();
        app.state.layerVisibility[layer] = off;
        if (off === false && app.state.selectedLayer === layer) {
          app.state.selectedLayer = null;
        }
        app.updateLists();
        app.updateInspector();
        app.render();
      });
      layerList.appendChild(row);
    });
  };

  app.eyeSvg = function () {
    return '<svg viewBox="0 0 24 24"><path d="M2.5 12s3.5-6 9.5-6 9.5 6 9.5 6-3.5 6-9.5 6-9.5-6-9.5-6Z"/><circle cx="12" cy="12" r="2.5"/></svg>';
  };

  app.escapeHtml = function (text) {
    return String(text).replace(/[&<>"']/g, (char) => ({
      "&": "&amp;",
      "<": "&lt;",
      ">": "&gt;",
      '"': "&quot;",
      "'": "&#39;"
    })[char]);
  };

  app.updateStats = function () {
    app.$("#pointStat").textContent = app.state.points.length.toLocaleString();
    app.$("#lineStat").textContent = app.state.lines.length.toLocaleString();
    app.$("#sourceStat").textContent = app.state.parsed
      ? app.state.parsed.entities.length.toLocaleString()
      : "0";
    app.shell.classList.toggle(
      "has-file",
      Boolean(
        app.state.parsed || app.state.points.length || app.state.lines.length
      )
    );
  };

  app.updateInspector = function () {
    const form = app.$("#propertyForm");
    const empty = app.$("#noSelection");
    if (app.state.multiSelection.length) {
      const pointCount = app.state.multiSelection
        .filter((item) => item.kind === "point").length;
      const lineCount = app.state.multiSelection
        .filter((item) => item.kind === "line").length;
      empty.classList.add("hidden");
      form.classList.remove("hidden");
      form.innerHTML = `<div class="property-title"><i></i>多选结果</div>
        <div class="selection-summary">
          <strong>${app.state.multiSelection.length}</strong>
          <span>${pointCount} 个点 · ${lineCount} 条直线</span>
        </div>`;
      app.$("#selectionStatus").textContent =
        `已选择 ${app.state.multiSelection.length} 个图元`;
      return;
    }
    if (!app.state.selected) {
      form.classList.add("hidden");
      empty.classList.remove("hidden");
      app.$("#selectionStatus").textContent = "未选择图元";
      return;
    }
    const item = app.state.selected.kind === "point"
      ? app.state.points[app.state.selected.index]
      : app.state.lines[app.state.selected.index];
    if (!item) {
      app.state.selected = null;
      return app.updateInspector();
    }
    empty.classList.add("hidden");
    form.classList.remove("hidden");
    const fields = app.state.selected.kind === "point"
      ? [["X", "x", item.x], ["Y", "y", item.y]]
      : [
        ["X₁", "x1", item.x1],
        ["Y₁", "y1", item.y1],
        ["X₂", "x2", item.x2],
        ["Y₂", "y2", item.y2]
      ];
    const inputValue = (value) => Number(value.toPrecision(10)).toString();
    form.innerHTML =
      `<div class="property-title"><i></i>` +
      `${app.state.selected.kind === "point" ? "点" : "直线"} · ` +
      `${app.escapeHtml(item.layer || "0")}</div>` +
      `<div class="coord-grid">${fields.map(([label, name, value]) =>
        `<label>${label}<input type="number" step="any" ` +
        `data-property="${name}" value="${inputValue(value)}"></label>`
      ).join("")}</div>`;
    app.$$("[data-property]", form).forEach((input) => {
      input.addEventListener("change", () => {
        const value = Number(input.value);
        if (!Number.isFinite(value)) {
          return app.toast("请输入有效坐标", "error");
        }
        app.snapshot();
        item[input.dataset.property] = value;
        app.setDirty(true);
        app.refresh();
      });
    });
    app.$("#selectionStatus").textContent =
      `已选择${app.state.selected.kind === "point" ? "点" : "直线"} ` +
      `${item.id || ""}`;
  };

  app.refresh = function () {
    app.updateStats();
    app.updateLists();
    app.updateInspector();
    app.render();
  };

  app.$$('input[type="file"]').forEach((input) => {
    input.addEventListener("change", () => {
      app.readFile(input.files[0]);
      input.value = "";
    });
  });
  app.$("#exportBtn").addEventListener("click", app.exportFile);
  app.$("#newBtn").addEventListener("click", app.resetDrawing);
  app.$("#restoreOriginalBtn").addEventListener(
    "click",
    app.restoreOriginalDrawing
  );
  app.$("#fitBtn").addEventListener("click", app.fitView);
  app.$("#gridBtn").addEventListener("click", () => {
    app.shell.classList.toggle("no-grid");
    app.$("#gridBtn").classList.toggle(
      "active",
      !app.shell.classList.contains("no-grid")
    );
  });
  app.$("#zoomInBtn").addEventListener("click", () => app.zoomAt(1.2));
  app.$("#zoomOutBtn").addEventListener("click", () => app.zoomAt(1 / 1.2));
  app.$("#undoBtn").addEventListener("click", app.undo);
  app.$("#redoBtn").addEventListener("click", app.redo);
  app.$("#deleteBtn").addEventListener("click", app.deleteSelected);
  app.$("#rediscretizeBtn").addEventListener("click", app.rediscretize);
  app.$("#applyTransformBtn").addEventListener("click", app.applyTransform);
  app.$("#agentForm").addEventListener("submit", (event) => {
    event.preventDefault();
    app.runAgent();
  });
  app.$("#applyAgentBtn").addEventListener("click", app.applyAgentResult);
  app.$("#cancelAgentBtn").addEventListener("click", () => {
    app.clearAgentResult();
    app.$("#agentStatus").textContent = "已取消";
  });
  app.$("#agentProvider").addEventListener("change", (event) => {
    const defaults = {
      openai_responses: {
        model: "gpt-5.6",
        url: "https://api.openai.com/v1"
      },
      openai_chat: {
        model: "deepseek-v4-pro",
        url: "https://api.deepseek.com"
      },
      anthropic: { model: "", url: "https://api.anthropic.com" }
    }[event.target.value];
    app.$("#agentModel").value = defaults.model;
    app.$("#agentModel").placeholder = event.target.value === "anthropic"
      ? "输入 Anthropic 模型 ID"
      : "输入模型 ID";
    app.$("#agentApiUrl").value = defaults.url;
    app.saveCurrentAgentSettings();
  });
  ["agentModel", "agentApiUrl"].forEach((id) => {
    app.$(`#${id}`).addEventListener("input", app.scheduleAgentSettingsSave);
  });
  app.$("#clearAgentCacheBtn").addEventListener(
    "click",
    app.clearAgentSettings
  );
  app.$("#toggleAllBtn").addEventListener("click", () => {
    app.state.allVisible = !app.state.allVisible;
    app.currentLayers().forEach((layer) => {
      app.state.layerVisibility[layer] = app.state.allVisible;
    });
    if (!app.state.allVisible) app.state.selectedLayer = null;
    app.updateLists();
    app.updateInspector();
    app.render();
  });
  app.$$(".tool[data-tool]").forEach((button) => {
    button.addEventListener("click", () => app.selectTool(button.dataset.tool));
  });

  app.$("#tolerance").addEventListener("change", (event) => {
    const value = Number(event.target.value);
    if (!(value > 0)) {
      event.target.value = app.state.tolerance;
      return app.toast("公差必须大于 0", "error");
    }
    app.state.tolerance = value;
    app.$("#toleranceRange").value = Math.log10(value);
  });
  app.$("#toleranceRange").addEventListener("input", (event) => {
    app.state.tolerance = 10 ** Number(event.target.value);
    app.$("#tolerance").value = Number(
      app.state.tolerance.toPrecision(5)
    );
  });
  app.$$('input[name="drawingProfile"]').forEach((input) => {
    input.addEventListener("change", (event) => {
      app.state.drawingProfile = event.target.value;
      app.state.tolerance = app.currentDrawingProfile().defaultTolerance;
      app.syncToleranceControls();
      app.updateDrawingProfileMeta();
      if (app.state.parsed) {
        app.toast("图纸模式已切换，请点击重新离散化以应用新上限和容差");
      }
    });
  });

  app.canvas.addEventListener("wheel", (event) => {
    event.preventDefault();
    const rect = app.canvas.getBoundingClientRect();
    app.zoomAt(event.deltaY < 0 ? 1.12 : 1 / 1.12, {
      x: event.clientX - rect.left,
      y: event.clientY - rect.top
    });
  }, { passive: false });

  app.canvas.addEventListener("pointerdown", (event) => {
    const rect = app.canvas.getBoundingClientRect();
    const screen = {
      x: event.clientX - rect.left,
      y: event.clientY - rect.top
    };
    if (app.state.tool === "pan" || app.state.spacePressed ||
      event.button === 1) {
      app.state.dragging = {
        type: "pan",
        x: event.clientX,
        y: event.clientY,
        originX: app.state.view.x,
        originY: app.state.view.y
      };
      app.canvas.setPointerCapture(event.pointerId);
      app.canvas.style.cursor = "grabbing";
      return;
    }
    if (app.state.tool === "point") {
      app.snapshot();
      const point = app.screenToWorld(screen);
      app.state.points.push({
        id: `p-user-${Date.now()}`,
        ...point,
        layer: "0",
        sourceType: "POINT",
        visible: true
      });
      app.setDirty(true);
      app.refresh();
      return;
    }
    if (app.state.tool === "line") {
      const point = app.screenToWorld(screen);
      if (!app.state.drawingStart) {
        app.state.drawingStart = { ...point, preview: point };
        app.render();
      } else {
        app.snapshot();
        app.state.lines.push({
          id: `l-user-${Date.now()}`,
          x1: app.state.drawingStart.x,
          y1: app.state.drawingStart.y,
          x2: point.x,
          y2: point.y,
          layer: "0",
          sourceType: "LINE",
          visible: true
        });
        app.state.drawingStart = null;
        app.setDirty(true);
        app.refresh();
      }
      return;
    }
    const hit = app.hitTest(screen);
    let canDrag = Boolean(hit);
    if (event.shiftKey && hit) {
      canDrag = app.toggleSelection(hit);
    } else if (hit) {
      if (!app.isSelected(hit.kind, hit.index) ||
        app.state.multiSelection.length === 0) {
        app.applySelection([hit]);
      }
    } else if (!event.shiftKey) {
      app.applySelection([]);
    }
    app.updateInspector();
    app.render();
    if (hit && canDrag && event.button === 0) {
      app.beginGeometryDrag(hit, screen, event.pointerId);
    } else if (event.button === 0) {
      app.state.selectionBox = {
        start: screen,
        current: screen,
        additive: event.shiftKey
      };
      app.state.dragging = { type: "box" };
      app.canvas.setPointerCapture(event.pointerId);
      app.render();
    }
  });

  app.canvas.addEventListener("pointermove", (event) => {
    const rect = app.canvas.getBoundingClientRect();
    const screen = {
      x: event.clientX - rect.left,
      y: event.clientY - rect.top
    };
    const world = app.screenToWorld(screen);
    app.$("#coordinates").textContent =
      `X ${world.x.toFixed(3)}   Y ${world.y.toFixed(3)}`;
    if (app.state.dragging && app.state.dragging.type === "pan") {
      app.state.view.x = app.state.dragging.originX +
        event.clientX - app.state.dragging.x;
      app.state.view.y = app.state.dragging.originY +
        event.clientY - app.state.dragging.y;
      app.render();
    } else if (app.state.dragging &&
      app.state.dragging.type.startsWith("edit-")) {
      app.dragGeometry(world);
    } else if (app.state.dragging && app.state.dragging.type === "box") {
      app.state.selectionBox.current = screen;
      app.render();
    } else if (app.state.drawingStart) {
      app.state.drawingStart.preview = world;
      app.render();
    } else if (app.state.tool === "select") {
      const hit = app.hitTest(screen);
      if (!hit) app.canvas.style.cursor = "default";
      else if (hit.kind === "point") app.canvas.style.cursor = "move";
      else {
        app.canvas.style.cursor =
          app.lineDragMode(app.state.lines[hit.index], screen) === "move"
            ? "move"
            : "crosshair";
      }
    }
  });

  function endPointer(event) {
    if (!app.state.dragging) return;
    const boxSelection = app.state.dragging.type === "box";
    const changed = app.state.dragging.changed;
    if (boxSelection) {
      const rect = app.canvas.getBoundingClientRect();
      app.state.selectionBox.current = {
        x: event.clientX - rect.left,
        y: event.clientY - rect.top
      };
      app.finishBoxSelection();
    }
    app.state.dragging = null;
    if (app.canvas.hasPointerCapture(event.pointerId)) {
      app.canvas.releasePointerCapture(event.pointerId);
    }
    app.canvas.style.cursor = app.state.tool === "pan"
      ? "grab"
      : app.state.tool === "select" ? "default" : "crosshair";
    if (changed) {
      app.setDirty(true);
      app.refresh();
    } else if (boxSelection) {
      app.refresh();
    }
  }
  app.canvas.addEventListener("pointerup", endPointer);
  app.canvas.addEventListener("pointercancel", endPointer);

  ["dragenter", "dragover"].forEach((type) => {
    app.shell.addEventListener(type, (event) => {
      event.preventDefault();
      app.shell.classList.add("dragover");
    });
  });
  ["dragleave", "drop"].forEach((type) => {
    app.shell.addEventListener(type, (event) => {
      event.preventDefault();
      if (type === "drop") app.readFile(event.dataTransfer.files[0]);
      app.shell.classList.remove("dragover");
    });
  });

  global.addEventListener("keydown", (event) => {
    if (/INPUT|TEXTAREA/.test(document.activeElement.tagName)) return;
    if (event.code === "Space") {
      event.preventDefault();
      app.state.spacePressed = true;
      app.canvas.style.cursor = "grab";
    } else if (event.key === "Delete" || event.key === "Backspace") {
      app.deleteSelected();
    } else if ((event.ctrlKey || event.metaKey) &&
      (event.key.toLowerCase() === "y" ||
        (event.shiftKey && event.key.toLowerCase() === "z"))) {
      event.preventDefault();
      app.redo();
    } else if ((event.ctrlKey || event.metaKey) &&
      event.key.toLowerCase() === "z") {
      event.preventDefault();
      app.undo();
    } else if (event.key.toLowerCase() === "v") app.selectTool("select");
    else if (event.key.toLowerCase() === "h") app.selectTool("pan");
    else if (event.key.toLowerCase() === "p") app.selectTool("point");
    else if (event.key.toLowerCase() === "l") app.selectTool("line");
    else if (event.key.toLowerCase() === "f") app.fitView();
    else if (event.key === "Escape") {
      app.state.drawingStart = null;
      app.state.selected = null;
      app.state.multiSelection = [];
      app.state.selectionBox = null;
      app.updateInspector();
      app.render();
    }
  });
  global.addEventListener("keyup", (event) => {
    if (event.code === "Space") {
      app.state.spacePressed = false;
      app.canvas.style.cursor = app.state.tool === "pan"
        ? "grab"
        : app.state.tool === "select" ? "default" : "crosshair";
    }
  });
  global.addEventListener("resize", app.resizeCanvas);

  if ("ResizeObserver" in global) {
    new ResizeObserver(app.resizeCanvas).observe(app.shell);
  }
  app.loadAgentSettings();
  app.updateDrawingProfileMeta();
  app.resetDrawing();
  requestAnimationFrame(app.resizeCanvas);
})(window);
