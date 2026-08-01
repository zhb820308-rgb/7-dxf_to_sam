(function exposeAgentSettings(root, factory) {
  "use strict";
  const api = factory();
  if (typeof module === "object" && module.exports) module.exports = api;
  if (root) root.DxfAgentSettings = api;
}(typeof globalThis === "object" ? globalThis : this, function createAgentSettings() {
  "use strict";

  const PROVIDERS = new Set(["openai_responses", "openai_chat", "anthropic"]);

  function sanitizeSettings(settings) {
    if (!settings || typeof settings !== "object" || Array.isArray(settings)) return null;
    const sanitized = {};
    if (PROVIDERS.has(settings.provider)) sanitized.provider = settings.provider;
    if (typeof settings.model === "string") sanitized.model = settings.model;
    if (typeof settings.apiUrl === "string") sanitized.apiUrl = settings.apiUrl;
    return sanitized;
  }

  function saveSettings(storage, key, settings) {
    const sanitized = sanitizeSettings(settings) || {};
    storage.setItem(key, JSON.stringify(sanitized));
    return sanitized;
  }

  function loadSettings(storage, key) {
    const raw = storage.getItem(key);
    if (!raw) return null;
    try {
      const parsed = JSON.parse(raw);
      const sanitized = sanitizeSettings(parsed);
      if (!sanitized) throw new Error("invalid settings");
      // Rewriting a strict whitelist removes API keys left by older versions.
      storage.setItem(key, JSON.stringify(sanitized));
      return sanitized;
    } catch {
      storage.removeItem(key);
      return null;
    }
  }

  function clearSettings(storage, key) {
    storage.removeItem(key);
  }

  return { clearSettings, loadSettings, sanitizeSettings, saveSettings };
}));
