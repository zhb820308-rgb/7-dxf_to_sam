"use strict";

const assert = require("node:assert/strict");
const crypto = require("node:crypto");
const fs = require("node:fs");
const path = require("node:path");

global.window = global;
require("../page/dxf.js");

const DXF_FIXTURE_ROOT = path.join(__dirname, "..", "tests", "fixtures", "dxf");
const BASELINES = [
  {
    file: "block/minimal_block_insert.dxf",
    inputHash: "963B8A90BE8014B2AD8F5AF69C5AF4C0872AD83AF612D54EFE9081249446656F",
    points: 0,
    lines: 3,
    bounds: [400, 300, 1100, 1000],
    geometryHash: "E9E68AA652D2A17D2365B559FEA2F6B2027784686C3689471E72D3B53EEDAB19"
  },
  {
    file: "function/spline_profile_array_stress.dxf",
    inputHash: "A8D43AA0F4926CD7E7CD04ED8088E3D8EF0F774B21B93CF79B04C5E245428922",
    points: 0,
    lines: 49512,
    bounds: [
      8405.880227183145,
      4370.679497087856,
      86281.65881876461,
      35596.27999163745
    ],
    geometryHash: "143835F9F0EB5EC2770626D8669C81CFBCD36D2EFAF76CA2F21FD5928F62B9AD"
  },
  {
    file: "block/ship_block_expansion_stress.dxf",
    inputHash: "8CA7FB11D9B8A958E8D3BEDFB8E579737014EA0D94C347AADC1A3289E4FD8BC3",
    points: 92,
    lines: 47611,
    bounds: [
      178224.7172270408,
      12138.49298654927,
      353424.7172270408,
      198886.39635584483
    ],
    geometryHash: "8962F01D707AA1EF8E27A6D7BE06D53FAF49C6FE3550054AF9713FA3157973F9"
  }
];

function sha256(value) {
  return crypto.createHash("sha256").update(value).digest("hex").toUpperCase();
}

function geometryFingerprint(geometry) {
  return sha256(JSON.stringify({
    points: geometry.points.map((point) => [
      point.x,
      point.y,
      point.layer,
      point.sourceType
    ]),
    lines: geometry.lines.map((line) => [
      line.x1,
      line.y1,
      line.x2,
      line.y2,
      line.layer,
      line.sourceType
    ]),
    unsupported: geometry.unsupported
  }));
}

function geometryBounds(geometry) {
  const bounds = [Infinity, Infinity, -Infinity, -Infinity];
  const include = (x, y) => {
    bounds[0] = Math.min(bounds[0], x);
    bounds[1] = Math.min(bounds[1], y);
    bounds[2] = Math.max(bounds[2], x);
    bounds[3] = Math.max(bounds[3], y);
  };
  geometry.points.forEach((point) => include(point.x, point.y));
  geometry.lines.forEach((line) => {
    include(line.x1, line.y1);
    include(line.x2, line.y2);
  });
  return bounds;
}

function assertBounds(actual, expected, file) {
  actual.forEach((value, index) => {
    assert.ok(
      Math.abs(value - expected[index]) <= 1e-6,
      `${file} bounds[${index}] expected ${expected[index]}, got ${value}`
    );
  });
}

for (const baseline of BASELINES) {
  const filePath = path.join(DXF_FIXTURE_ROOT, baseline.file);
  const source = fs.readFileSync(filePath);
  assert.equal(sha256(source), baseline.inputHash, `${baseline.file} input hash`);

  const parsed = global.DXFStudio.parseDxf(source.toString("utf8"));
  const geometry = global.DXFStudio.discretize(parsed, 0.01, "unlimited");
  assert.equal(geometry.points.length, baseline.points, `${baseline.file} points`);
  assert.equal(geometry.lines.length, baseline.lines, `${baseline.file} lines`);
  assertBounds(geometryBounds(geometry), baseline.bounds, baseline.file);
  assert.equal(
    geometryFingerprint(geometry),
    baseline.geometryHash,
    `${baseline.file} geometry fingerprint`
  );

  console.log(
    `${baseline.file}: ${geometry.points.length} points / ${geometry.lines.length} lines ` +
    `${baseline.geometryHash}`
  );
}

console.log("verify-geometry-baseline.js: ok");
