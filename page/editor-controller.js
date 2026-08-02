(function (global) {
  "use strict";

  const app = global.DXFStudioApp;

  app.bindEditorEvents = function () {
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
  };
})(window);
