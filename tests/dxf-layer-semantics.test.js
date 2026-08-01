"use strict";

const assert = require("node:assert/strict");
const fs = require("node:fs");
const path = require("node:path");
const vm = require("node:vm");

const window = {};
const source = fs.readFileSync(path.join(__dirname, "..", "page", "dxf.js"), "utf8");
vm.runInNewContext(source, { window });

const fixture = fs.readFileSync(
  path.join(__dirname, "..", "test", "data", "block_layer_inheritance.dxf"),
  "utf8"
);
const geometry = window.DXFStudio.discretize(window.DXFStudio.parseDxf(fixture), 0.01);
const entities = geometry.points.concat(geometry.lines);

assert.equal(entities.length, 9);

function affectedCount(layer) {
  return entities.filter((item) => window.DXFStudio.affectsLayer(item, layer)).length;
}

function visibleCount(hiddenLayer) {
  const visibility = { [hiddenLayer]: false };
  return entities.filter((item) => window.DXFStudio.isLayerVisible(item, visibility)).length;
}

// These are SAM-style affected counts, not exclusive entity ownership counts.
assert.equal(affectedCount("KEEP"), 1);
assert.equal(affectedCount("IGNORE"), 3);
assert.equal(affectedCount("PARENT"), 5);
assert.equal(affectedCount("FIXED"), 3);

assert.equal(visibleCount("KEEP"), 8);
assert.equal(visibleCount("IGNORE"), 6);
assert.equal(visibleCount("PARENT"), 4);
assert.equal(visibleCount("FIXED"), 6);
assert.equal(visibleCount("0"), 9);

const fixedInsideIgnoredInsert = geometry.lines.find((line) => line.x1 === 20 && line.y1 === 1);
assert.ok(fixedInsideIgnoredInsert);
assert.equal(fixedInsideIgnoredInsert.layer, "FIXED");
assert.deepEqual(Array.from(fixedInsideIgnoredInsert.insertLayers), ["IGNORE"]);

console.log("dxf-layer-semantics.test.js: ok");
