const fs = require("fs");
const path = require("path");

const gridSize = 10;
const spacing = 10.0;
const jitter = 2.0e-7;
const outputPath = path.resolve(
  __dirname,
  "..",
  "tests",
  "fixtures",
  "dxf",
  "function",
  "merge_points_stress.dxf",
);

const pairs = [];

function pair(code, value) {
  pairs.push(String(code), String(value));
}

function point(layer, x, y, z = 0.0) {
  pair(0, "POINT");
  pair(8, layer);
  pair(10, x);
  pair(20, y);
  pair(30, z);
}

function line(layer, x1, y1, x2, y2, z = 0.0) {
  pair(0, "LINE");
  pair(8, layer);
  pair(10, x1);
  pair(20, y1);
  pair(30, z);
  pair(11, x2);
  pair(21, y2);
  pair(31, z);
}

pair(0, "SECTION");
pair(2, "HEADER");
pair(9, "$ACADVER");
pair(1, "AC1021");
pair(0, "ENDSEC");
pair(0, "SECTION");
pair(2, "TABLES");
pair(0, "ENDSEC");
pair(0, "SECTION");
pair(2, "BLOCKS");
pair(0, "ENDSEC");
pair(0, "SECTION");
pair(2, "ENTITIES");
pair(999, "100 merge locations; near points are within 1e-6 tolerance");

let pointCount = 0;
let lineCount = 0;

for (let row = 0; row < gridSize; row += 1) {
  for (let column = 0; column < gridSize; column += 1) {
    const x = column * spacing;
    const y = row * spacing;
    point("MERGE_POINTS", x, y);
    point(
      "MERGE_POINTS_NEAR",
      x + (column % 2 === 0 ? jitter : -jitter),
      y + (row % 2 === 0 ? -jitter : jitter),
    );
    pointCount += 2;
  }
}

for (let row = 0; row < gridSize; row += 1) {
  for (let column = 0; column < gridSize - 1; column += 1) {
    const x1 = column * spacing;
    const x2 = (column + 1) * spacing;
    const y = row * spacing;
    const endpointJitter = row % 2 === 0 ? jitter : -jitter;
    line("GRID_HORIZONTAL", x1 + endpointJitter, y, x2, y - endpointJitter);
    lineCount += 1;

    if ((row * (gridSize - 1) + column) % 10 === 0) {
      line("DUPLICATE_REVERSED", x2, y, x1, y);
      lineCount += 1;
    }
  }
}

for (let column = 0; column < gridSize; column += 1) {
  for (let row = 0; row < gridSize - 1; row += 1) {
    const y1 = row * spacing;
    const y2 = (row + 1) * spacing;
    const x = column * spacing;
    const endpointJitter = column % 2 === 0 ? -jitter : jitter;
    line("GRID_VERTICAL", x, y1 + endpointJitter, x - endpointJitter, y2);
    lineCount += 1;

    if ((column * (gridSize - 1) + row) % 10 === 0) {
      line("DUPLICATE_REVERSED", x, y2, x, y1);
      lineCount += 1;
    }
  }
}

for (let index = 0; index < gridSize - 1; index += 1) {
  const start = index * spacing;
  const end = (index + 1) * spacing;
  line("DIAGONAL", start, start, end + jitter, end - jitter);
  line("DIAGONAL", end, start, start - jitter, end + jitter);
  lineCount += 2;
}

pair(0, "ENDSEC");
pair(0, "EOF");

fs.writeFileSync(outputPath, `${pairs.join("\r\n")}\r\n`, "ascii");

console.log(`Generated: ${outputPath}`);
console.log(`POINT entities: ${pointCount}`);
console.log(`LINE entities: ${lineCount}`);
console.log(`Expected merged grid nodes at tolerance 1e-6: ${gridSize * gridSize}`);
