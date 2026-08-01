"use strict";

const assert = require("assert");
const fs = require("fs");
const path = require("path");
const vm = require("vm");

class FakeClassList {
  constructor() {
    this.names = new Set();
  }

  add(...names) {
    names.forEach((name) => this.names.add(name));
  }

  remove(...names) {
    names.forEach((name) => this.names.delete(name));
  }

  contains(name) {
    return this.names.has(name);
  }

  toggle(name, force) {
    const enabled = force === undefined ? !this.contains(name) : Boolean(force);
    if (enabled) this.add(name);
    else this.remove(name);
    return enabled;
  }
}

class FakeElement {
  constructor(id = "") {
    this.id = id;
    this.value = "";
    this.textContent = "";
    this.innerHTML = "";
    this.disabled = false;
    this.files = [];
    this.dataset = {};
    this.style = {};
    this.className = "";
    this.classList = new FakeClassList();
    this.children = [];
    this.listeners = new Map();
    this.attributes = new Map();
    this.eye = null;
  }

  addEventListener(type, listener) {
    if (!this.listeners.has(type)) this.listeners.set(type, []);
    this.listeners.get(type).push(listener);
  }

  appendChild(child) {
    this.children.push(child);
    return child;
  }

  remove() {}
  click() {}
  setPointerCapture() {}
  releasePointerCapture() {}
  hasPointerCapture() { return false; }

  setAttribute(name, value) {
    this.attributes.set(name, String(value));
  }

  querySelector(selector) {
    if (selector === ".eye") {
      if (!this.eye) this.eye = new FakeElement("eye");
      return this.eye;
    }
    return null;
  }

  querySelectorAll() {
    return [];
  }

  getBoundingClientRect() {
    return { left: 0, top: 0, width: 800, height: 600 };
  }
}

function createCanvasContext() {
  return new Proxy({}, {
    get(target, property) {
      if (!(property in target)) target[property] = () => {};
      return target[property];
    },
    set(target, property, value) {
      target[property] = value;
      return true;
    }
  });
}

const html = fs.readFileSync(path.join(__dirname, "../page/index.html"), "utf8");
const ids = Array.from(html.matchAll(/id="([^"]+)"/g), (match) => match[1]);
const elements = Object.fromEntries(ids.map((id) => [id, new FakeElement(id)]));
elements.canvas.getContext = () => createCanvasContext();
elements.translateX.value = "0";
elements.translateY.value = "0";
elements.rotate.value = "0";
elements.scale.value = "1";
elements.agentProvider.value = "openai_responses";
elements.agentModel.value = "gpt-5.6";
elements.agentApiUrl.value = "https://api.openai.com/v1";

const fileInput = elements.fileInput;
const tools = ["select", "pan", "point", "line"].map((tool) => {
  const element = new FakeElement(`tool-${tool}`);
  element.dataset.tool = tool;
  return element;
});
const profiles = ["small", "large", "unlimited"].map((profile) => {
  const element = new FakeElement(`profile-${profile}`);
  element.value = profile;
  return element;
});

const document = {
  activeElement: { tagName: "BODY" },
  body: new FakeElement("body"),
  querySelector(selector) {
    return selector.startsWith("#") ? elements[selector.slice(1)] || null : null;
  },
  querySelectorAll(selector) {
    if (selector === 'input[type="file"]') return [fileInput];
    if (selector === ".tool[data-tool]") return tools;
    if (selector === 'input[name="drawingProfile"]') return profiles;
    return [];
  },
  createElement() {
    return new FakeElement();
  }
};

const storage = new Map();
const localStorage = {
  getItem(key) { return storage.has(key) ? storage.get(key) : null; },
  setItem(key, value) { storage.set(key, String(value)); },
  removeItem(key) { storage.delete(key); }
};

const windowListeners = new Map();
const sandbox = {
  console,
  document,
  localStorage,
  location: { protocol: "http:", port: "8080", href: "http://127.0.0.1:8080/" },
  URL,
  Blob,
  TextDecoder,
  setTimeout: () => 1,
  clearTimeout: () => {},
  requestAnimationFrame: (callback) => callback(),
  ResizeObserver: class { observe() {} },
  addEventListener(type, listener) { windowListeners.set(type, listener); },
  devicePixelRatio: 1
};
sandbox.window = sandbox;
vm.createContext(sandbox);

const scripts = [
  "dxf.js",
  "agent-settings.js",
  "state.js",
  "viewport.js",
  "renderer.js",
  "selection.js",
  "import-controller.js",
  "agent-controller.js",
  "app.js"
];
scripts.forEach((file) => {
  const source = fs.readFileSync(path.join(__dirname, "../page", file), "utf8");
  vm.runInContext(source, sandbox, { filename: file });
});

const app = sandbox.DXFStudioApp;
assert(app, "editor namespace should be initialized");
[
  "render", "fitView", "applySelection", "importText", "runAgent", "refresh"
].forEach((name) => assert.strictEqual(typeof app[name], "function", name));

const fixturePath = path.join(__dirname, "../example/block_test_minimal.dxf");
app.importText(fs.readFileSync(fixturePath, "utf8"), "block_test_minimal.dxf", 1024);
assert.strictEqual(app.state.points.length, 0);
assert.strictEqual(app.state.lines.length, 3);
assert.strictEqual(elements.lineStat.textContent, "3");

app.applySelection([{ kind: "line", index: 0 }]);
app.deleteSelected();
assert.strictEqual(app.state.lines.length, 2);
app.undo();
assert.strictEqual(app.state.lines.length, 3);
app.redo();
assert.strictEqual(app.state.lines.length, 2);
app.undo();

const originalX = app.state.lines[0].x1;
elements.translateX.value = "10";
app.applyTransform();
assert.strictEqual(app.state.lines[0].x1, originalX + 10);
app.restoreOriginalDrawing();
assert.strictEqual(app.state.lines[0].x1, originalX);
assert(windowListeners.has("keydown"));

console.log("web-editor-modules.test.js: ok");
