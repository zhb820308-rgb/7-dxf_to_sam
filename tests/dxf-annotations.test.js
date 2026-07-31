"use strict";

const assert = require("node:assert/strict");
const fs = require("node:fs");
const path = require("node:path");
const vm = require("node:vm");

const window = {};
const source = fs.readFileSync(path.join(__dirname, "..", "page", "dxf.js"), "utf8");
vm.runInNewContext(source, { window });

const dxf = [
  "0", "SECTION", "2", "BLOCKS",
  "0", "BLOCK", "2", "*D1", "10", "0", "20", "0",
  "0", "LINE", "8", "0", "10", "0", "20", "5", "11", "10", "21", "5",
  "0", "ENDBLK",
  "0", "ENDSEC",
  "0", "SECTION", "2", "ENTITIES",
  "0", "DIMENSION", "5", "D1", "8", "DIMS", "2", "*D1", "70", "0",
  "10", "0", "20", "5", "13", "0", "23", "0", "14", "10", "24", "0",
  "0", "LEADER", "5", "E1", "8", "NOTES", "71", "1",
  "10", "20", "20", "0", "10", "25", "20", "5",
  "0", "LINE", "5", "F1", "8", "ANNO-DIMS", "10", "30", "20", "0", "11", "40", "21", "0",
  "0", "ENDSEC", "0", "EOF", ""
].join("\r\n");

const parsed = window.DXFStudio.parseDxf(dxf);
const result = window.DXFStudio.discretize(parsed, 0.01);

const dimension = result.lines.find((line) => line.sourceType === "DIMENSION");
assert.ok(dimension);
assert.equal(dimension.annotationKind, "dimension");
assert.equal(dimension.annotationConfidence, 1);
assert.equal(dimension.annotationSource, "DXF_DIMENSION");

const leader = result.lines.find((line) => line.sourceType === "LEADER");
assert.ok(leader);
assert.equal(leader.annotationKind, "leader");
assert.equal(leader.annotationConfidence, 1);

const candidate = result.lines.find((line) => line.layer === "ANNO-DIMS");
assert.ok(candidate);
assert.equal(candidate.annotationKind, "dimension_candidate");
assert.ok(candidate.annotationConfidence < 1);

console.log("dxf-annotations.test.js: ok");
