"use strict";

const assert = require("node:assert/strict");
const fs = require("node:fs");
const path = require("node:path");
const vm = require("node:vm");

const window = {};
const source = fs.readFileSync(path.join(__dirname, "..", "page", "dxf.js"), "utf8");
vm.runInNewContext(source, { window });

function parsedInsert(rows, columns) {
  return {
    layers: { "0": { name: "0", colorIndex: 7, visible: true } },
    blocks: {
      CELL: {
        name: "CELL",
        base: { x: 0, y: 0 },
        entities: [{
          type: "LINE", layer: "0", visible: true,
          start: { x: 0, y: 0 }, end: { x: 1, y: 0 }
        }]
      }
    },
    entities: [{
      type: "INSERT", name: "CELL", layer: "0", visible: true,
      sourceId: 1, point: { x: 0, y: 0 }, scaleX: 1, scaleY: 1,
      rotation: 0, rows, columns, rowSpacing: 2, columnSpacing: 2
    }]
  };
}

const normal = window.DXFStudio.discretize(parsedInsert(2, 3), 0.01);
assert.equal(normal.lines.length, 6);

assert.throws(
  () => window.DXFStudio.discretize(parsedInsert(100001, 1), 0.01),
  (error) => error.code === "DXF_EXPANSION_LIMIT" && /INSERT.*limit|INSERT.*限制/i.test(error.message)
);
assert.throws(
  () => window.DXFStudio.discretize(parsedInsert(1.5, 2), 0.01),
  /INSERT.*integer|INSERT.*整数/i
);

const outputHeavy = parsedInsert(50001, 1);
outputHeavy.blocks.CELL.entities.push({
  type: "LINE", layer: "0", visible: true,
  start: { x: 0, y: 1 }, end: { x: 1, y: 1 }
});
assert.throws(
  () => window.DXFStudio.discretize(outputHeavy, 0.01),
  (error) => error.code === "DXF_EXPANSION_LIMIT" && /output exceeds/i.test(error.message)
);

console.log("dxf-expansion-limits.test.js: ok");
