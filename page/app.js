(function (global) {
  "use strict";

  const app = global.DXFStudioApp;

  app.bindEditorEvents();
  app.loadAgentSettings();
  app.updateDrawingProfileMeta();
  app.resetDrawing();
  global.requestAnimationFrame(app.resizeCanvas);
})(window);
