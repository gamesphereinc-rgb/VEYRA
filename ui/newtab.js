(() => {
  "use strict";

  const clock = document.getElementById("clock");
  const date = document.getElementById("date");
  const greeting = document.getElementById("greeting");
  const input = document.querySelector(".search input");
  const grid = document.getElementById("shortcuts");
  const addButton = document.getElementById("add-shortcut");
  const storageKey = "veyra.custom-shortcuts.v1";

  const defaults = [
    { name: "DuckDuckGo", url: "https://duckduckgo.com/", detail: "Search privately", icon: "D", style: "icon-search" },
    { name: "GitHub", url: "https://github.com/", detail: "Build something", icon: "GH", style: "icon-github" },
    { name: "YouTube", url: "https://youtube.com/", detail: "Watch and discover", icon: "▶", style: "icon-video" },
    { name: "Wikipedia", url: "https://wikipedia.org/", detail: "Follow curiosity", icon: "W", style: "icon-wiki" },
    { name: "Reddit", url: "https://reddit.com/", detail: "Find your people", icon: "r/", style: "icon-reddit" },
    { name: "Stack Overflow", url: "https://stackoverflow.com/", detail: "Learn and solve", icon: "S", style: "icon-stack" }
  ];

  function updateTime() {
    const now = new Date();
    clock.textContent = new Intl.DateTimeFormat(undefined, {
      hour: "2-digit", minute: "2-digit", hour12: false
    }).format(now);
    date.textContent = new Intl.DateTimeFormat(undefined, {
      weekday: "long", day: "numeric", month: "long", year: "numeric"
    }).format(now);
    const hour = now.getHours();
    const greetingText = hour < 12 ? "Good morning" : hour < 18 ? "Good afternoon" : "Good evening";
    greeting.firstChild.textContent = greetingText;
  }

  function readCustomShortcuts() {
    try {
      const parsed = JSON.parse(localStorage.getItem(storageKey) || "[]");
      if (!Array.isArray(parsed)) return [];
      return parsed.filter(item =>
        item && typeof item.name === "string" && typeof item.url === "string" &&
        /^https?:\/\//i.test(item.url)
      ).slice(0, 6);
    } catch (_) {
      return [];
    }
  }

  function saveCustomShortcuts(items) {
    try { localStorage.setItem(storageKey, JSON.stringify(items)); } catch (_) { /* Storage may be unavailable. */ }
  }

  function makeShortcut(item, custom, index) {
    const wrap = document.createElement("div");
    wrap.className = "shortcut-wrap";

    const link = document.createElement("a");
    link.className = "shortcut";
    link.href = item.url;

    const icon = document.createElement("span");
    icon.className = "shortcut-icon " + (item.style || "icon-search");
    icon.textContent = item.icon || item.name.slice(0, 2).toUpperCase();

    const copy = document.createElement("span");
    copy.className = "shortcut-copy";
    const title = document.createElement("strong");
    title.textContent = item.name;
    const detail = document.createElement("small");
    detail.textContent = item.detail || item.url.replace(/^https?:\/\//i, "");
    copy.append(title, detail);

    const arrow = document.createElement("span");
    arrow.className = "shortcut-arrow";
    arrow.textContent = "↗";
    link.append(icon, copy, arrow);
    wrap.append(link);

    if (custom) {
      const remove = document.createElement("button");
      remove.className = "remove-shortcut";
      remove.type = "button";
      remove.textContent = "×";
      remove.setAttribute("aria-label", "Remove " + item.name);
      remove.addEventListener("click", () => {
        const items = readCustomShortcuts();
        items.splice(index, 1);
        saveCustomShortcuts(items);
        renderShortcuts();
      });
      wrap.append(remove);
    }
    return wrap;
  }

  function renderShortcuts() {
    const custom = readCustomShortcuts();
    grid.replaceChildren();
    defaults.forEach(item => grid.append(makeShortcut(item, false, -1)));
    custom.forEach((item, index) => grid.append(makeShortcut(item, true, index)));
  }

  addButton.addEventListener("click", () => {
    const current = readCustomShortcuts();
    if (current.length >= 6) {
      window.alert("You can save up to six custom shortcuts.");
      return;
    }

    const name = window.prompt("Name this shortcut:");
    if (!name || !name.trim()) return;
    let address = window.prompt("Website address (https://...):");
    if (!address || !address.trim()) return;
    address = address.trim();
    if (!/^https?:\/\//i.test(address)) address = "https://" + address;

    try {
      const parsed = new URL(address);
      if (parsed.protocol !== "https:" && parsed.protocol !== "http:") throw new Error("Invalid protocol");
      current.push({
        name: name.trim().slice(0, 28),
        url: parsed.href,
        detail: parsed.hostname,
        icon: name.trim().slice(0, 2).toUpperCase(),
        style: "icon-search"
      });
      saveCustomShortcuts(current);
      renderShortcuts();
    } catch (_) {
      window.alert("Please enter a valid website address.");
    }
  });

  window.addEventListener("keydown", event => {
    if (event.key === "/" && document.activeElement !== input &&
        !event.ctrlKey && !event.altKey && !event.metaKey) {
      event.preventDefault();
      input.focus();
    }
  });

  updateTime();
  window.setInterval(updateTime, 15000);
  renderShortcuts();
})();
