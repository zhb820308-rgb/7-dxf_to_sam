(function (global) {
  "use strict";

  const TAU = Math.PI * 2;
  const DEG = Math.PI / 180;
  const EXPANSION_LIMITS = Object.freeze({
    maxDepth: 8,
    maxArrayInstancesPerInsert: 100000,
    maxExpandedBlockInstances: 100000,
    maxOutputEntities: 100000
  });
  const ACI = [
    "#ffffff", "#ff3b30", "#ffd60a", "#34c759", "#00d7ff",
    "#0a84ff", "#bf5af2", "#8e8e93", "#c7c7cc", "#f2f2f7"
  ];

  function number(value, fallback = 0) {
    if (value === "" || value === null || value === undefined) return fallback;
    const parsed = Number(value);
    return Number.isFinite(parsed) ? parsed : fallback;
  }

  function pairsFromText(text) {
    const lines = String(text).replace(/^\uFEFF/, "").split(/\r?\n/);
    const pairs = [];
    for (let i = 0; i + 1 < lines.length; i += 2) {
      const code = Number.parseInt(lines[i].trim(), 10);
      if (Number.isFinite(code)) pairs.push({ code, value: lines[i + 1].trim() });
    }
    return pairs;
  }

  function values(pairs, code) {
    return pairs.filter((pair) => pair.code === code).map((pair) => pair.value);
  }

  function first(pairs, code, fallback = "") {
    const pair = pairs.find((item) => item.code === code);
    return pair ? pair.value : fallback;
  }

  function entityFromPairs(type, pairs) {
    const entity = {
      type,
      layer: first(pairs, 8, "0"),
      colorIndex: number(first(pairs, 62, 0)),
      handle: first(pairs, 5, ""),
      visible: number(first(pairs, 60, 0)) !== 1
    };

    if (type === "POINT") {
      entity.point = { x: number(first(pairs, 10)), y: number(first(pairs, 20)) };
    } else if (type === "LINE") {
      entity.start = { x: number(first(pairs, 10)), y: number(first(pairs, 20)) };
      entity.end = { x: number(first(pairs, 11)), y: number(first(pairs, 21)) };
    } else if (type === "CIRCLE" || type === "ARC") {
      entity.center = { x: number(first(pairs, 10)), y: number(first(pairs, 20)) };
      entity.radius = Math.abs(number(first(pairs, 40)));
      entity.startAngle = type === "ARC" ? number(first(pairs, 50)) * DEG : 0;
      entity.endAngle = type === "ARC" ? number(first(pairs, 51)) * DEG : TAU;
    } else if (type === "ELLIPSE") {
      entity.center = { x: number(first(pairs, 10)), y: number(first(pairs, 20)) };
      entity.major = { x: number(first(pairs, 11)), y: number(first(pairs, 21)) };
      entity.ratio = Math.abs(number(first(pairs, 40), 1));
      entity.startParam = number(first(pairs, 41), 0);
      entity.endParam = number(first(pairs, 42), TAU);
    } else if (type === "LWPOLYLINE") {
      const xs = values(pairs, 10);
      const ys = values(pairs, 20);
      const bulges = [];
      let vertexIndex = -1;
      pairs.forEach((pair) => {
        if (pair.code === 10) {
          vertexIndex += 1;
          bulges[vertexIndex] = 0;
        } else if (pair.code === 42 && vertexIndex >= 0) {
          bulges[vertexIndex] = number(pair.value);
        }
      });
      entity.vertices = xs.map((x, i) => ({
        x: number(x),
        y: number(ys[i]),
        bulge: bulges[i] || 0
      }));
      entity.closed = (number(first(pairs, 70)) & 1) !== 0;
    } else if (type === "SPLINE") {
      const xs = values(pairs, 10);
      const ys = values(pairs, 20);
      const fitXs = values(pairs, 11);
      const fitYs = values(pairs, 21);
      entity.degree = Math.max(1, number(first(pairs, 71), 3));
      entity.knots = values(pairs, 40).map(Number);
      entity.weights = values(pairs, 41).map(Number);
      entity.controlPoints = xs.map((x, i) => ({ x: number(x), y: number(ys[i]) }));
      entity.fitPoints = fitXs.map((x, i) => ({ x: number(x), y: number(fitYs[i]) }));
      entity.closed = (number(first(pairs, 70)) & 1) !== 0;
    } else if (type === "INSERT") {
      entity.name = first(pairs, 2, "");
      entity.point = { x: number(first(pairs, 10)), y: number(first(pairs, 20)) };
      entity.scaleX = number(first(pairs, 41), 1);
      entity.scaleY = number(first(pairs, 42), 1);
      entity.rotation = number(first(pairs, 50), 0) * DEG;
      entity.columns = Math.max(1, number(first(pairs, 70), 1));
      entity.rows = Math.max(1, number(first(pairs, 71), 1));
      entity.columnSpacing = number(first(pairs, 44), 0);
      entity.rowSpacing = number(first(pairs, 45), 0);
    } else if (type === "TEXT" || type === "MTEXT") {
      entity.point = { x: number(first(pairs, 10)), y: number(first(pairs, 20)) };
      entity.text = values(pairs, type === "MTEXT" ? 3 : 1).concat(values(pairs, 1)).join("");
    } else if (type === "DIMENSION") {
      entity.name = first(pairs, 2, "");
      entity.dimensionType = number(first(pairs, 70)) & 15;
      entity.definitionPoint = { x: number(first(pairs, 10)), y: number(first(pairs, 20)) };
      entity.textPoint = { x: number(first(pairs, 11)), y: number(first(pairs, 21)) };
      entity.extension1 = { x: number(first(pairs, 13)), y: number(first(pairs, 23)) };
      entity.extension2 = { x: number(first(pairs, 14)), y: number(first(pairs, 24)) };
      entity.rotation = number(first(pairs, 50)) * DEG;
      entity.text = first(pairs, 1, "");
    } else if (type === "LEADER") {
      const xs = values(pairs, 10);
      const ys = values(pairs, 20);
      entity.vertices = xs.map((x, index) => ({ x: number(x), y: number(ys[index]) }));
      entity.hasArrowhead = number(first(pairs, 71), 1) !== 0;
    }
    return entity;
  }

  function readEntity(pairs, index) {
    const type = pairs[index].value.toUpperCase();
    const data = [];
    let cursor = index + 1;

    if (type === "POLYLINE") {
      while (cursor < pairs.length && pairs[cursor].code !== 0) data.push(pairs[cursor++]);
      const entity = entityFromPairs(type, data);
      entity.vertices = [];
      while (cursor < pairs.length && pairs[cursor].code === 0 && pairs[cursor].value.toUpperCase() === "VERTEX") {
        const vertexPairs = [];
        cursor += 1;
        while (cursor < pairs.length && pairs[cursor].code !== 0) vertexPairs.push(pairs[cursor++]);
        entity.vertices.push({
          x: number(first(vertexPairs, 10)),
          y: number(first(vertexPairs, 20)),
          bulge: number(first(vertexPairs, 42))
        });
      }
      if (cursor < pairs.length && pairs[cursor].code === 0 && pairs[cursor].value.toUpperCase() === "SEQEND") {
        cursor += 1;
        while (cursor < pairs.length && pairs[cursor].code !== 0) cursor += 1;
      }
      entity.closed = (number(first(data, 70)) & 1) !== 0;
      return { entity, next: cursor };
    }

    while (cursor < pairs.length && pairs[cursor].code !== 0) data.push(pairs[cursor++]);
    return { entity: entityFromPairs(type, data), next: cursor };
  }

  function parseDxf(text) {
    const pairs = pairsFromText(text);
    if (!pairs.length) throw new Error("文件不是有效的 ASCII DXF");

    const entities = [];
    const blocks = {};
    const layers = { "0": { name: "0", colorIndex: 7, visible: true } };
    let section = "";
    let currentBlock = null;

    for (let i = 0; i < pairs.length;) {
      const pair = pairs[i];
      if (pair.code === 0 && pair.value.toUpperCase() === "SECTION") {
        section = (pairs[i + 1] && pairs[i + 1].code === 2) ? pairs[i + 1].value.toUpperCase() : "";
        i += 2;
        continue;
      }
      if (pair.code === 0 && pair.value.toUpperCase() === "ENDSEC") {
        section = "";
        currentBlock = null;
        i += 1;
        continue;
      }

      if (section === "TABLES" && pair.code === 0 && pair.value.toUpperCase() === "LAYER") {
        const result = readEntity(pairs, i);
        const raw = [];
        for (let j = i + 1; j < result.next; j += 1) raw.push(pairs[j]);
        const name = first(raw, 2, "0");
        const colorIndex = number(first(raw, 62, 7), 7);
        layers[name] = { name, colorIndex: Math.abs(colorIndex), visible: colorIndex >= 0 };
        i = result.next;
        continue;
      }

      if (section === "BLOCKS" && pair.code === 0 && pair.value.toUpperCase() === "BLOCK") {
        const result = readEntity(pairs, i);
        const raw = [];
        for (let j = i + 1; j < result.next; j += 1) raw.push(pairs[j]);
        const name = first(raw, 2, first(raw, 3, ""));
        currentBlock = { name, base: { x: number(first(raw, 10)), y: number(first(raw, 20)) }, entities: [] };
        if (name) blocks[name] = currentBlock;
        i = result.next;
        continue;
      }
      if (section === "BLOCKS" && pair.code === 0 && pair.value.toUpperCase() === "ENDBLK") {
        currentBlock = null;
        i += 1;
        continue;
      }

      const isEntityArea = section === "ENTITIES" || (section === "BLOCKS" && currentBlock);
      if (isEntityArea && pair.code === 0) {
        const type = pair.value.toUpperCase();
        if (!["ENDSEC", "ENDBLK", "SEQEND", "VERTEX"].includes(type)) {
          const result = readEntity(pairs, i);
          if (currentBlock) currentBlock.entities.push(result.entity);
          else entities.push(result.entity);
          i = result.next;
          continue;
        }
      }
      i += 1;
    }

    if (!entities.length && !Object.keys(blocks).length) {
      throw new Error("DXF 中没有可读取的图元");
    }
    entities.forEach((entity, index) => { entity.sourceId = index + 1; });
    return { entities, blocks, layers };
  }

  function arcStep(radius, tolerance) {
    if (!(radius > 0)) return Math.PI / 4;
    const ratio = Math.max(-1, Math.min(1, 1 - Math.max(tolerance, 1e-9) / radius));
    const byTolerance = 2 * Math.acos(ratio);
    return Math.max(DEG, Math.min(Math.PI / 4, byTolerance || DEG));
  }

  function pointOnArc(center, radius, angle) {
    return { x: center.x + radius * Math.cos(angle), y: center.y + radius * Math.sin(angle) };
  }

  function normalizeSweep(start, end, closed) {
    let sweep = end - start;
    while (sweep < 0) sweep += TAU;
    if (closed && Math.abs(sweep) < 1e-10) sweep = TAU;
    return sweep;
  }

  function sampleArc(center, radius, start, end, tolerance, closed) {
    const sweep = normalizeSweep(start, end, closed);
    const count = Math.max(1, Math.ceil(sweep / arcStep(radius, tolerance)));
    return Array.from({ length: count + 1 }, (_, i) => pointOnArc(center, radius, start + sweep * i / count));
  }

  function bulgePoints(a, b, bulge, tolerance) {
    if (Math.abs(bulge) < 1e-12) return [a, b];
    const dx = b.x - a.x;
    const dy = b.y - a.y;
    const chord = Math.hypot(dx, dy);
    if (chord < 1e-12) return [a, b];
    const theta = 4 * Math.atan(bulge);
    const radius = Math.abs(chord / (2 * Math.sin(theta / 2)));
    const mid = { x: (a.x + b.x) / 2, y: (a.y + b.y) / 2 };
    const offset = chord / (2 * Math.tan(theta / 2));
    const center = { x: mid.x - dy / chord * offset, y: mid.y + dx / chord * offset };
    const start = Math.atan2(a.y - center.y, a.x - center.x);
    const count = Math.max(1, Math.ceil(Math.abs(theta) / arcStep(radius, tolerance)));
    return Array.from({ length: count + 1 }, (_, i) =>
      pointOnArc(center, radius, start + theta * i / count));
  }

  function splinePoint(entity, u) {
    const points = entity.controlPoints;
    const degree = Math.min(entity.degree, points.length - 1);
    let knots = entity.knots;
    if (knots.length < points.length + degree + 1) {
      knots = [];
      const count = points.length + degree + 1;
      for (let i = 0; i < count; i += 1) {
        knots.push(i <= degree ? 0 : (i >= points.length ? 1 : (i - degree) / (points.length - degree)));
      }
    }
    const weights = entity.weights.length === points.length ? entity.weights : points.map(() => 1);
    let span = degree;
    for (let i = degree; i < points.length; i += 1) {
      if (u >= knots[i] && u <= knots[i + 1]) { span = i; break; }
    }
    if (u >= knots[points.length]) span = points.length - 1;
    const work = [];
    for (let j = 0; j <= degree; j += 1) {
      const index = span - degree + j;
      const w = weights[index] || 1;
      work[j] = { x: points[index].x * w, y: points[index].y * w, w };
    }
    for (let level = 1; level <= degree; level += 1) {
      for (let j = degree; j >= level; j -= 1) {
        const index = span - degree + j;
        const denominator = knots[index + degree - level + 1] - knots[index];
        const alpha = denominator ? (u - knots[index]) / denominator : 0;
        work[j] = {
          x: (1 - alpha) * work[j - 1].x + alpha * work[j].x,
          y: (1 - alpha) * work[j - 1].y + alpha * work[j].y,
          w: (1 - alpha) * work[j - 1].w + alpha * work[j].w
        };
      }
    }
    const result = work[degree];
    return { x: result.x / result.w, y: result.y / result.w };
  }

  function transformPoint(point, transform) {
    if (!transform) return { x: point.x, y: point.y };
    return {
      x: transform.a * point.x + transform.c * point.y + transform.e,
      y: transform.b * point.x + transform.d * point.y + transform.f
    };
  }

  function composeTransforms(parent, local) {
    if (!parent) return local;
    return {
      a: parent.a * local.a + parent.c * local.b,
      b: parent.b * local.a + parent.d * local.b,
      c: parent.a * local.c + parent.c * local.d,
      d: parent.b * local.c + parent.d * local.d,
      e: parent.a * local.e + parent.c * local.f + parent.e,
      f: parent.b * local.e + parent.d * local.f + parent.f
    };
  }

  function insertTransform(source, block, column, row, parent) {
    const cos = Math.cos(source.rotation);
    const sin = Math.sin(source.rotation);
    const a = cos * source.scaleX;
    const b = sin * source.scaleX;
    const c = -sin * source.scaleY;
    const d = cos * source.scaleY;
    const localX = column * source.columnSpacing - block.base.x;
    const localY = row * source.rowSpacing - block.base.y;
    return composeTransforms(parent, {
      a, b, c, d,
      e: source.point.x + a * localX + c * localY,
      f: source.point.y + b * localX + d * localY
    });
  }

  function discretize(parsed, tolerance) {
    const result = {
      points: [],
      lines: [],
      unsupported: {},
      sourceCount: parsed.entities.length,
      processedByType: {}
    };
    let nextId = 1;
    let expandedBlockInstances = 0;
    const blocksByName = Object.fromEntries(
      Object.entries(parsed.blocks).map(([name, block]) => [name.toUpperCase(), block])
    );

    function expansionLimit(message) {
      const error = new Error(`DXF INSERT expansion limit: ${message}`);
      error.code = "DXF_EXPANSION_LIMIT";
      return error;
    }

    function consumeOutput(count) {
      const current = result.points.length + result.lines.length;
      if (!Number.isSafeInteger(count) || count < 0 ||
          count > EXPANSION_LIMITS.maxOutputEntities - current) {
        throw expansionLimit(`output exceeds ${EXPANSION_LIMITS.maxOutputEntities} entities`);
      }
    }

    function consumeInsertArray(source) {
      if (!Number.isSafeInteger(source.rows) || !Number.isSafeInteger(source.columns) ||
          source.rows < 1 || source.columns < 1) {
        throw expansionLimit("rows and columns must be positive safe integers");
      }
      if (source.rows > Math.floor(EXPANSION_LIMITS.maxArrayInstancesPerInsert / source.columns)) {
        throw expansionLimit(
          `${source.rows} rows x ${source.columns} columns exceeds ` +
          `${EXPANSION_LIMITS.maxArrayInstancesPerInsert} instances per INSERT`
        );
      }
      const count = source.rows * source.columns;
      if (count > EXPANSION_LIMITS.maxExpandedBlockInstances - expandedBlockInstances) {
        throw expansionLimit(
          `total block instances exceed ${EXPANSION_LIMITS.maxExpandedBlockInstances}`
        );
      }
      expandedBlockInstances += count;
    }

    function addPoint(point, source, transform) {
      consumeOutput(1);
      const p = transformPoint(point, transform);
      result.points.push({
        id: `p${nextId++}`, x: p.x, y: p.y, layer: source.layer || "0",
        insertLayers: (source.insertLayers || []).slice(),
        sourceType: source.originType || source.type, sourceId: source.sourceId,
        annotationKind: source.annotationKind || null,
        annotationConfidence: source.annotationConfidence ?? null,
        annotationSource: source.annotationSource || null,
        annotationText: source.annotationText || null,
        visible: source.visible !== false
      });
    }

    function addPath(points, source, transform) {
      consumeOutput(Math.max(0, points.length - 1));
      for (let i = 0; i + 1 < points.length; i += 1) {
        const a = transformPoint(points[i], transform);
        const b = transformPoint(points[i + 1], transform);
        result.lines.push({
          id: `l${nextId++}`, x1: a.x, y1: a.y, x2: b.x, y2: b.y,
          layer: source.layer || "0", sourceType: source.originType || source.type,
          insertLayers: (source.insertLayers || []).slice(),
          sourceId: source.sourceId,
          annotationKind: source.annotationKind || null,
          annotationConfidence: source.annotationConfidence ?? null,
          annotationSource: source.annotationSource || null,
          annotationText: source.annotationText || null,
          visible: source.visible !== false
        });
      }
    }

    function visit(source, transform, depth) {
      if (depth > EXPANSION_LIMITS.maxDepth) {
        throw expansionLimit(`nesting depth exceeds ${EXPANSION_LIMITS.maxDepth}`);
      }
      if (source.type === "POINT") addPoint(source.point, source, transform);
      else if (source.type === "LINE") addPath([source.start, source.end], source, transform);
      else if (source.type === "CIRCLE" || source.type === "ARC") {
        addPath(sampleArc(source.center, source.radius, source.startAngle, source.endAngle, tolerance, source.type === "CIRCLE"), source, transform);
      } else if (source.type === "ELLIPSE") {
        const majorLength = Math.hypot(source.major.x, source.major.y);
        const sweep = normalizeSweep(source.startParam, source.endParam, true);
        const count = Math.max(4, Math.ceil(sweep / arcStep(majorLength, tolerance)));
        const ux = source.major.x;
        const uy = source.major.y;
        const points = Array.from({ length: count + 1 }, (_, i) => {
          const angle = source.startParam + sweep * i / count;
          return {
            x: source.center.x + ux * Math.cos(angle) - uy * source.ratio * Math.sin(angle),
            y: source.center.y + uy * Math.cos(angle) + ux * source.ratio * Math.sin(angle)
          };
        });
        addPath(points, source, transform);
      } else if (source.type === "LWPOLYLINE" || source.type === "POLYLINE") {
        const vertices = source.vertices || [];
        const edgeCount = source.closed ? vertices.length : Math.max(0, vertices.length - 1);
        for (let i = 0; i < edgeCount; i += 1) {
          const a = vertices[i];
          const b = vertices[(i + 1) % vertices.length];
          addPath(bulgePoints(a, b, a.bulge || 0, tolerance), source, transform);
        }
      } else if (source.type === "SPLINE" && source.controlPoints.length > 1) {
        const degree = Math.min(source.degree, source.controlPoints.length - 1);
        const knots = source.knots;
        const start = knots.length ? knots[degree] : 0;
        const end = knots.length ? knots[source.controlPoints.length] : 1;
        const controlLength = source.controlPoints.slice(1).reduce((sum, point, i) =>
          sum + Math.hypot(point.x - source.controlPoints[i].x, point.y - source.controlPoints[i].y), 0);
        const count = Math.max(8, Math.min(4096, Math.ceil(controlLength / Math.sqrt(Math.max(tolerance, 1e-8)))));
        const points = Array.from({ length: count + 1 }, (_, i) => splinePoint(source, start + (end - start) * i / count));
        addPath(points, source, transform);
      } else if (source.type === "SPLINE" && source.fitPoints.length > 1) {
        const points = source.fitPoints.slice();
        if (source.closed) points.push(source.fitPoints[0]);
        addPath(points, source, transform);
      } else if (source.type === "LEADER" && source.vertices.length > 1) {
        addPath(source.vertices, Object.assign({}, source, {
          originType: "LEADER",
          annotationKind: "leader",
          annotationConfidence: 1,
          annotationSource: "DXF_LEADER"
        }), transform);
      } else if (source.type === "DIMENSION" && blocksByName[source.name.toUpperCase()]) {
        const block = blocksByName[source.name.toUpperCase()];
        block.entities.forEach((entity) => {
          const inherited = Object.assign({}, entity, {
            layer: entity.layer === "0" ? source.layer : entity.layer,
            sourceId: source.sourceId,
            originType: "DIMENSION",
            annotationKind: "dimension",
            annotationConfidence: 1,
            annotationSource: "DXF_DIMENSION",
            annotationText: source.text || null,
            visible: entity.visible !== false && source.visible !== false
          });
          visit(inherited, transform, depth + 1);
        });
      } else if (source.type === "DIMENSION" && (source.dimensionType === 0 || source.dimensionType === 1)) {
        const direction = source.dimensionType === 1
          ? {
              x: source.extension2.x - source.extension1.x,
              y: source.extension2.y - source.extension1.y
            }
          : { x: Math.cos(source.rotation), y: Math.sin(source.rotation) };
        const length = Math.hypot(direction.x, direction.y) || 1;
        const normal = { x: -direction.y / length, y: direction.x / length };
        const project = (point) => {
          const distance = (source.definitionPoint.x - point.x) * normal.x +
            (source.definitionPoint.y - point.y) * normal.y;
          return { x: point.x + normal.x * distance, y: point.y + normal.y * distance };
        };
        const q1 = project(source.extension1);
        const q2 = project(source.extension2);
        const annotated = Object.assign({}, source, {
          originType: "DIMENSION",
          annotationKind: "dimension",
          annotationConfidence: 1,
          annotationSource: "DXF_DIMENSION_GEOMETRY",
          annotationText: source.text || null
        });
        addPath([source.extension1, q1], annotated, transform);
        addPath([q1, q2], annotated, transform);
        addPath([q2, source.extension2], annotated, transform);
      } else if (source.type === "INSERT" && blocksByName[source.name.toUpperCase()]) {
        const block = blocksByName[source.name.toUpperCase()];
        consumeInsertArray(source);
        const insertLayers = (source.insertLayers || []).concat(source.layer || "0");
        for (let row = 0; row < source.rows; row += 1) {
          for (let column = 0; column < source.columns; column += 1) {
            const local = insertTransform(source, block, column, row, transform);
            block.entities.forEach((entity) => {
              const inherited = Object.assign({}, entity, {
                layer: entity.layer === "0" ? source.layer : entity.layer,
                insertLayers,
                sourceId: source.sourceId,
                visible: entity.visible !== false && source.visible !== false
              });
              visit(inherited, local, depth + 1);
            });
          }
        }
      } else {
        result.unsupported[source.type] = (result.unsupported[source.type] || 0) + 1;
      }
    }

    // Always rebuild every top-level source entity. Visibility belongs to the
    // renderer and must never filter the discretization pass.
    parsed.entities.forEach((entity) => {
      result.processedByType[entity.type] = (result.processedByType[entity.type] || 0) + 1;
      visit(entity, null, 0);
    });

    // Exploded dimensions lose their DIMENSION identity. A conventional
    // annotation-layer name is useful evidence, but remains a candidate rather
    // than a certain classification.
    const annotationLayerPattern = /(^|[_\-\s])(DIMS?|DIMENSIONS?|ANNO|ANNOTATION|尺寸|标注)([_\-\s]|$)/i;
    result.lines.forEach((line) => {
      if (!line.annotationKind && annotationLayerPattern.test(line.layer || "")) {
        line.annotationKind = "dimension_candidate";
        line.annotationConfidence = 0.88;
        line.annotationSource = "LAYER_NAME";
      }
    });
    return result;
  }

  // SAM treats an INSERT layer as a control layer for its entire block
  // reference. An entity still owns one effective layer, but disabling any
  // ancestor INSERT layer suppresses it as well.
  function affectsLayer(item, layer) {
    return (item.layer || "0") === layer || (item.insertLayers || []).includes(layer);
  }

  function isLayerVisible(item, layerVisibility) {
    if (layerVisibility[item.layer || "0"] === false) return false;
    return !(item.insertLayers || []).some((layer) => layerVisibility[layer] === false);
  }

  function colorForLayer(layer, layers) {
    const item = layers[layer];
    const index = item ? item.colorIndex : 7;
    return ACI[index % ACI.length] || "#8e8e93";
  }

  function fmt(value) {
    if (!Number.isFinite(value)) return "0";
    return Number(value.toFixed(9)).toString();
  }

  function exportDxf(model) {
    const lines = [
      "0", "SECTION", "2", "HEADER", "9", "$ACADVER", "1", "AC1015", "0", "ENDSEC",
      "0", "SECTION", "2", "TABLES", "0", "TABLE", "2", "LAYER", "70", String(model.layers.length)
    ];
    model.layers.forEach((layer, index) => {
      lines.push("0", "LAYER", "2", layer, "70", "0", "62", String((index % 7) + 1), "6", "CONTINUOUS");
    });
    lines.push("0", "ENDTAB", "0", "ENDSEC", "0", "SECTION", "2", "ENTITIES");
    model.points.forEach((point) => {
      lines.push("0", "POINT", "8", point.layer || "0", "10", fmt(point.x), "20", fmt(point.y), "30", "0");
    });
    model.lines.forEach((line) => {
      lines.push("0", "LINE", "8", line.layer || "0", "10", fmt(line.x1), "20", fmt(line.y1), "30", "0",
        "11", fmt(line.x2), "21", fmt(line.y2), "31", "0");
    });
    lines.push("0", "ENDSEC", "0", "EOF", "");
    return lines.join("\r\n");
  }

  global.DXFStudio = {
    parseDxf, discretize, exportDxf, colorForLayer,
    affectsLayer, isLayerVisible, EXPANSION_LIMITS
  };
})(window);
