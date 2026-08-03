"use strict";

const assert = require("node:assert/strict");
const { executeAgentTool, validateGeometry } = require("../../server.js");

const geometry = validateGeometry({
  points: [{ id: "p1", x: 1, y: 2, layer: "P" }],
  lines: [
    {
      id: "l1", x1: 0, y1: 0, x2: 3, y2: 4, layer: "A",
      annotationKind: "dimension", annotationConfidence: 1, annotationSource: "DXF_DIMENSION"
    },
    { id: "l2", x1: 10, y1: 0, x2: 10, y2: 2, layer: "B" }
  ]
});
const context = {
  geometry,
  selectedIds: new Set(["p1", "l2"]),
  actionLog: [],
  finished: false,
  agentSummary: ""
};

const page = executeAgentTool("inspect_geometry", {
  scope: "all", layer: null, ids: [], offset: 1, limit: 1
}, context);
assert.equal(page.returned, 1);
assert.equal(page.lines[0].id, "l1");
assert.equal(page.next_offset, 2);

const measurement = executeAgentTool("measure_geometry", {
  scope: "ids", layer: null, ids: ["l1"]
}, context);
assert.equal(measurement.line_count, 1);
assert.equal(measurement.total_line_length, 5);

const annotations = executeAgentTool("find_annotation_lines", {
  kinds: ["dimension"], layer: null, min_confidence: 1, offset: 0, limit: 20
}, context);
assert.equal(annotations.total, 1);
assert.equal(annotations.lines[0].id, "l1");

executeAgentTool("transform_geometry", {
  scope: "ids", layer: null, ids: ["l1"],
  translate_x: 2, translate_y: -1, rotate_degrees: 0, scale: 1,
  center_x: null, center_y: null
}, context);
assert.deepEqual(
  [geometry.lines[0].x1, geometry.lines[0].y1, geometry.lines[0].x2, geometry.lines[0].y2],
  [2, -1, 5, 3]
);

const copy = executeAgentTool("copy_geometry", {
  scope: "selected", layer: null, ids: [],
  translate_x: 100, translate_y: 10, target_layer: "COPY"
}, context);
assert.equal(copy.copied_count, 2);
assert.equal(geometry.points.at(-1).layer, "COPY");
assert.equal(geometry.lines.at(-1).layer, "COPY");

const copiedIds = [
  geometry.points.at(-1).id,
  geometry.lines.at(-1).id
];
const relayer = executeAgentTool("set_geometry_layer", {
  scope: "ids", layer: null, ids: copiedIds, target_layer: "REVIEW"
}, context);
assert.equal(relayer.changed_count, 2);
assert.equal(geometry.points.at(-1).layer, "REVIEW");
assert.equal(geometry.lines.at(-1).layer, "REVIEW");

const insertGeometry = validateGeometry({
  points: [],
  lines: [{
    id: "nested-fixed", x1: 0, y1: 0, x2: 1, y2: 0,
    layer: "FIXED", insertLayers: ["IGNORE"]
  }]
});
const insertContext = {
  geometry: insertGeometry,
  selectedIds: new Set(),
  actionLog: [],
  finished: false,
  agentSummary: ""
};
assert.deepEqual(insertGeometry.lines[0].insertLayers, ["IGNORE"]);
const controlledPage = executeAgentTool("inspect_geometry", {
  scope: "layer", layer: "IGNORE", ids: [], offset: 0, limit: 10
}, insertContext);
assert.equal(controlledPage.returned, 1);
assert.equal(controlledPage.lines[0].id, "nested-fixed");

const added = executeAgentTool("add_geometry", {
  points: [{ x: 2, y: 3, layer: "NEW" }],
  lines: [{ x1: 2, y1: 3, x2: 4, y2: 5, layer: "NEW" }]
}, insertContext);
assert.equal(added.added_count, 2);
assert.deepEqual(insertGeometry.points.at(-1).insertLayers, []);
assert.deepEqual(insertGeometry.lines.at(-1).insertLayers, []);

console.log("agent-tools.test.js: ok");
