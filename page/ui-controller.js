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
    app.$$('[data-property]', form).forEach((input) => {
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
})(window);
