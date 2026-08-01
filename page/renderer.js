(function (global) {
  "use strict";

  const app = global.DXFStudioApp;

  app.render = function () {
    const rect = app.canvas.getBoundingClientRect();
    app.ctx.clearRect(0, 0, rect.width, rect.height);
    if (!app.state.points.length && !app.state.lines.length) return;

    app.ctx.lineCap = "round";
    app.state.lines.forEach((line, index) => {
      if (!app.visible(line)) return;
      const a = app.worldToScreen({ x: line.x1, y: line.y1 });
      const b = app.worldToScreen({ x: line.x2, y: line.y2 });
      const selected = app.isSelected("line", index);
      const layerHighlighted = app.state.selectedLayer !== null &&
        app.affectedByLayer(line, app.state.selectedLayer);
      app.ctx.globalAlpha = app.state.selectedLayer === null ||
        layerHighlighted || selected ? 1 : 0.16;
      app.ctx.beginPath();
      app.ctx.moveTo(a.x, a.y);
      app.ctx.lineTo(b.x, b.y);
      app.ctx.strokeStyle = selected
        ? "#ff6b3d"
        : layerHighlighted
          ? "#75b51b"
          : DXFStudio.colorForLayer(
            line.layer,
            app.state.parsed ? app.state.parsed.layers : {}
          );
      app.ctx.lineWidth = selected ? 3 : layerHighlighted ? 2.6 : 1.35;
      app.ctx.stroke();
      if (selected && app.state.multiSelection.length === 0) {
        app.drawHandle(a);
        app.drawHandle(b);
      }
    });

    app.state.points.forEach((point, index) => {
      if (!app.visible(point)) return;
      const screen = app.worldToScreen(point);
      const selected = app.isSelected("point", index);
      const layerHighlighted = app.state.selectedLayer !== null &&
        app.affectedByLayer(point, app.state.selectedLayer);
      app.ctx.globalAlpha = app.state.selectedLayer === null ||
        layerHighlighted || selected ? 1 : 0.16;
      app.ctx.beginPath();
      app.ctx.arc(
        screen.x,
        screen.y,
        selected ? 5 : layerHighlighted ? 4.5 : 3.2,
        0,
        Math.PI * 2
      );
      app.ctx.fillStyle = selected
        ? "#ff6b3d"
        : layerHighlighted
          ? "#75b51b"
          : DXFStudio.colorForLayer(
            point.layer,
            app.state.parsed ? app.state.parsed.layers : {}
          );
      app.ctx.fill();
      app.ctx.strokeStyle = "#f5f4ef";
      app.ctx.lineWidth = 1.2;
      app.ctx.stroke();
    });
    app.ctx.globalAlpha = 1;

    if (app.state.drawingStart) {
      const a = app.worldToScreen(app.state.drawingStart);
      const b = app.worldToScreen(
        app.state.drawingStart.preview || app.state.drawingStart
      );
      app.ctx.setLineDash([5, 5]);
      app.ctx.beginPath();
      app.ctx.moveTo(a.x, a.y);
      app.ctx.lineTo(b.x, b.y);
      app.ctx.strokeStyle = "#ff6b3d";
      app.ctx.lineWidth = 1.5;
      app.ctx.stroke();
      app.ctx.setLineDash([]);
    }

    if (app.state.selectionBox) {
      const box = app.normalizedBox(
        app.state.selectionBox.start,
        app.state.selectionBox.current
      );
      app.ctx.globalAlpha = 1;
      app.ctx.fillStyle = "rgba(117, 181, 27, .12)";
      app.ctx.strokeStyle = "#75b51b";
      app.ctx.lineWidth = 1;
      app.ctx.setLineDash([5, 4]);
      app.ctx.fillRect(box.left, box.top, box.width, box.height);
      app.ctx.strokeRect(box.left + .5, box.top + .5, box.width, box.height);
      app.ctx.setLineDash([]);
    }
  };

  app.drawHandle = function (point) {
    app.ctx.beginPath();
    app.ctx.arc(point.x, point.y, 4, 0, Math.PI * 2);
    app.ctx.fillStyle = "#fbfaf6";
    app.ctx.fill();
    app.ctx.strokeStyle = "#ff6b3d";
    app.ctx.lineWidth = 2;
    app.ctx.stroke();
  };
})(window);
