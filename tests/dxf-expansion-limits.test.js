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

const largeResult = window.DXFStudio.discretize(outputHeavy, 0.05, "large");
assert.equal(largeResult.lines.length, 100002);
assert.equal(largeResult.maxOutputEntities, 500000);

const largeOutputHeavy = parsedInsert(83334, 1);
for (let index = 0; index < 6; index += 1) {
  largeOutputHeavy.blocks.CELL.entities.push({
    type: "LINE", layer: "0", visible: true,
    start: { x: 0, y: index + 1 }, end: { x: 1, y: index + 1 }
  });
}
assert.throws(
  () => window.DXFStudio.discretize(largeOutputHeavy, 0.05, "large"),
  (error) => error.code === "DXF_EXPANSION_LIMIT" && /output exceeds 500000/i.test(error.message)
);

const unlimitedResult = window.DXFStudio.discretize(
  largeOutputHeavy, 0.05, "unlimited"
);
assert.equal(unlimitedResult.lines.length, 583338);
assert.equal(unlimitedResult.maxOutputEntities, Number.POSITIVE_INFINITY);
assert.throws(
  () => window.DXFStudio.discretize(
    parsedInsert(100001, 1), 0.05, "unlimited"
  ),
  (error) => error.code === "DXF_EXPANSION_LIMIT" && /instances per INSERT/i.test(error.message)
);

console.log("dxf-expansion-limits.test.js: ok");
