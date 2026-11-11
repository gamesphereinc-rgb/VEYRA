(() => {
  "use strict";

  const clock = document.getElementById("clock");
  const date = document.getElementById("date");
  const greeting = document.getElementById("greeting");
  const input = document.querySelector(".search input");
  const grid = document.getElementById("shortcuts");
  const addButton = document.getElementById("add-shortcut");
  const storageKey = "veyra.custom-shortcuts.v1";
  const preferencesKey = "veyra.preferences.v1";
  const settingsView = document.getElementById("settings-view");
  const settingsButton = document.getElementById("open-settings");
  const closeSettingsButton = document.getElementById("close-settings");
  const themeChoice = document.getElementById("theme-choice");
  const searchEngine = document.getElementById("search-engine");
  const wallpaperUpload = document.getElementById("wallpaper-upload");
  const wallpaperGrid = document.getElementById("wallpaper-grid");
  const resetWallpaper = document.getElementById("reset-wallpaper");
  const updateButton = document.getElementById("check-updates");
  const updateStatus = document.getElementById("update-status");
  const searchEngines = {
    duckduckgo:"https://duckduckgo.com/?q=", google:"https://www.google.com/search?q=",
    bing:"https://www.bing.com/search?q=", brave:"https://search.brave.com/search?q="
  };
  function readPreferences(){
    try{const value=JSON.parse(localStorage.getItem(preferencesKey)||"{}");return value&&typeof value==="object"?value:{};}
    catch(_){return {};}
  }
  function savePreferences(patch){
    const value={...readPreferences(),...patch};
    try{localStorage.setItem(preferencesKey,JSON.stringify(value));}catch(_){}
    applyPreferences();return value;
  }
  function applyPreferences(){
    const prefs=readPreferences();
    let theme=prefs.theme||"dark";
    if(theme==="system")theme=window.matchMedia("(prefers-color-scheme: light)").matches?"light":"dark";
    document.body.dataset.theme=theme;
    const wallpaper=prefs.wallpaper||"default";
    document.body.dataset.wallpaper=wallpaper;
    document.body.style.backgroundImage="";
    if(wallpaper==="custom"&&typeof prefs.customWallpaper==="string"&&prefs.customWallpaper.startsWith("data:image/")){
      document.body.style.backgroundImage="linear-gradient(180deg,rgba(7,18,24,.32),rgba(7,18,24,.72)),url("+JSON.stringify(prefs.customWallpaper)+")";
    }
    if(themeChoice)themeChoice.value=prefs.theme||"dark";
    if(searchEngine)searchEngine.value=prefs.searchEngine||"duckduckgo";
    document.querySelectorAll(".wallpaper-option").forEach(button=>button.classList.toggle("active",button.dataset.wallpaper===wallpaper));
  }
  function showSettings(show){
    document.querySelector(".welcome").classList.toggle("hidden",show);
    settingsView.hidden=!show;
    document.querySelector(".footer").classList.toggle("hidden",show);
    settingsButton.setAttribute("aria-expanded",String(show));
    history.replaceState(null,"",show?"#settings":location.pathname);
    if(show){applyPreferences();document.querySelector(".page").scrollTo({top:0,behavior:"smooth"});}
  }
  settingsButton.addEventListener("click",()=>showSettings(true));
  closeSettingsButton.addEventListener("click",()=>showSettings(false));
  themeChoice.addEventListener("change",()=>savePreferences({theme:themeChoice.value}));
  searchEngine.addEventListener("change",()=>savePreferences({searchEngine:searchEngine.value}));
  wallpaperGrid.addEventListener("click",event=>{
    const button=event.target.closest("[data-wallpaper]");
    if(button)savePreferences({wallpaper:button.dataset.wallpaper});
  });
  resetWallpaper.addEventListener("click",()=>savePreferences({wallpaper:"default",customWallpaper:""}));
  wallpaperUpload.addEventListener("change",()=>{
    const file=wallpaperUpload.files&&wallpaperUpload.files[0];if(!file)return;
    if(!/^image\/(png|jpeg|webp)$/i.test(file.type)){window.alert("Choose a PNG, JPG or WebP image.");wallpaperUpload.value="";return;}
    const reader=new FileReader(),image=new Image();
    reader.onload=()=>{image.onload=()=>{
      const scale=Math.min(1,1920/image.width,1080/image.height),canvas=document.createElement("canvas");
      canvas.width=Math.max(1,Math.round(image.width*scale));canvas.height=Math.max(1,Math.round(image.height*scale));
      canvas.getContext("2d").drawImage(image,0,0,canvas.width,canvas.height);
      try{savePreferences({wallpaper:"custom",customWallpaper:canvas.toDataURL("image/jpeg",.82)});}
      catch(_){window.alert("VEYRA couldn't save that image. Try a smaller image.");}
    };image.onerror=()=>window.alert("VEYRA couldn't read that image.");image.src=reader.result;};
    reader.onerror=()=>window.alert("VEYRA couldn't read that file.");reader.readAsDataURL(file);
  });
  document.querySelectorAll("[data-settings-nav]").forEach(link=>link.addEventListener("click",()=>{
    document.querySelectorAll("[data-settings-nav]").forEach(item=>item.classList.remove("active"));link.classList.add("active");
  }));
  let availableUpdate = null;
  updateButton.addEventListener("click",async()=>{
    if(availableUpdate){
      updateButton.disabled=true;
      updateStatus.textContent="Starting secure download and checksum verification…";
      const target="veyra://install-update?url="+encodeURIComponent(availableUpdate.url)+"&sha256="+encodeURIComponent(availableUpdate.digest);
      window.location.href=target;
      return;
    }
    updateButton.disabled=true;updateButton.textContent="Checking…";
    updateStatus.textContent="Contacting the official VEYRA release feed…";
    try{
      const response=await fetch("https://api.github.com/repos/gamesphereinc-rgb/VEYRA/releases/latest",{headers:{Accept:"application/vnd.github+json"}});
      if(response.status===404){
        updateStatus.textContent="No stable release has been published yet. Test builds are available from GitHub Actions.";
        updateButton.textContent="Check again";
      }else if(!response.ok)throw new Error("Release check failed.");
      else{
        const release=await response.json();
        const tag=String(release.tag_name||"").replace(/^v/i,"");
        const parts=tag.split(".").map(value=>Number.parseInt(value,10)||0);
        const current=[0,2,0];
        let newer=false;
        for(let i=0;i<3;i++){if((parts[i]||0)>current[i]){newer=true;break;}if((parts[i]||0)<current[i])break;}
        if(!newer){
          updateStatus.textContent="You're running VEYRA 0.2.0. No newer stable version is available.";
          updateButton.textContent="Check again";
        }else{
          const asset=Array.isArray(release.assets)?release.assets.find(item=>item.name==="VEYRA-windows-x64.zip"):null;
          const digest=asset&&typeof asset.digest==="string"?asset.digest.replace(/^sha256:/i,""):"";
          if(asset&&asset.browser_download_url&&/^[a-f0-9]{64}$/i.test(digest)){
            availableUpdate={url:asset.browser_download_url,digest};
            updateStatus.textContent="VEYRA "+release.tag_name+" is available. The package will be verified before installation.";
            updateButton.textContent="Download & install";
          }else{
            updateStatus.textContent="VEYRA "+release.tag_name+" is available, but this release has no verifiable package. Open the release page to review it.";
            updateButton.textContent="View release";
            updateButton.onclick=()=>window.open(release.html_url,"_blank","noopener");
          }
        }
      }
    }catch(_){
      updateStatus.textContent="Couldn't check for updates. Check your connection or visit the official releases page.";
      updateButton.textContent="Check again";
    }finally{updateButton.disabled=false;}
  });

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

  document.querySelector(".search").addEventListener("submit",event=>{
    event.preventDefault();const query=input.value.trim();if(!query)return;
    const prefs=readPreferences(),base=searchEngines[prefs.searchEngine||"duckduckgo"]||searchEngines.duckduckgo;
    window.location.href=base+encodeURIComponent(query);
  });

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

  applyPreferences();
  if(location.hash==="#settings")showSettings(true);
  window.matchMedia("(prefers-color-scheme: light)").addEventListener("change",()=>{
    if(readPreferences().theme==="system")applyPreferences();
  });
  updateTime();
  window.setInterval(updateTime,15000);
  renderShortcuts();
})();
