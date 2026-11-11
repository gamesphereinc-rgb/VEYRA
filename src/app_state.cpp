#include "src/app_state.h"
#include <algorithm>
#include <cwctype>
#include <filesystem>
#include <string>
#include <iterator>
#include <shlobj.h>
#include "include/cef_browser.h"
#include "src/browser_client.h"
#include "src/messages.h"
#include "src/window.h"

namespace {
constexpr int kChromeHeight = 116;

std::string Utf8(const std::wstring& text) {
  if (text.empty()) return {};
  int n = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), (int)text.size(), nullptr, 0, nullptr, nullptr);
  std::string out(n, '\0');
  WideCharToMultiByte(CP_UTF8, 0, text.c_str(), (int)text.size(), out.data(), n, nullptr, nullptr);
  return out;
}
std::wstring Encode(const std::wstring& text, bool keepPathChars) {
  const auto bytes = Utf8(text);
  constexpr char hex[] = "0123456789ABCDEF";
  std::string encoded;
  for (unsigned char c : bytes) {
    const bool safe = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
      (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~' ||
      (keepPathChars && (c == '/' || c == ':'));
    if (safe) encoded.push_back((char)c);
    else { encoded.push_back('%'); encoded.push_back(hex[c >> 4]); encoded.push_back(hex[c & 15]); }
  }
  int n = MultiByteToWideChar(CP_UTF8, 0, encoded.c_str(), (int)encoded.size(), nullptr, 0);
  std::wstring out(n, L'\0');
  MultiByteToWideChar(CP_UTF8, 0, encoded.c_str(), (int)encoded.size(), out.data(), n);
  return out;
}
bool StartsWith(const std::wstring& s, const std::wstring& prefix) {
  if (s.size() < prefix.size()) return false;
  return std::equal(prefix.begin(), prefix.end(), s.begin(),
    [](wchar_t a, wchar_t b) { return towlower(a) == towlower(b); });
}
bool LooksLikeHost(const std::wstring& s) {
  return s.find(L'.') != std::wstring::npos || StartsWith(s, L"localhost") ||
         StartsWith(s, L"127.0.0.1") || StartsWith(s, L"[::1]");
}

std::wstring BrowserDataFile() {
  PWSTR known = nullptr;
  std::filesystem::path root;
  if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &known))) {
    root = known;
    CoTaskMemFree(known);
  } else {
    wchar_t temp[MAX_PATH]{};
    GetTempPathW(MAX_PATH, temp);
    root = temp;
  }
  root /= L"VEYRA";
  root /= L"Profile";
  std::error_code ec;
  std::filesystem::create_directories(root, ec);
  return (root / L"browser-data.ini").wstring();
}

std::wstring ReadIni(const wchar_t* section, const wchar_t* key,
                     const std::wstring& path) {
  wchar_t value[8192]{};
  GetPrivateProfileStringW(section, key, L"", value,
                           static_cast<DWORD>(std::size(value)), path.c_str());
  return value;
}
}

AppState& AppState::Get() { static AppState state; return state; }
void AppState::SetWindow(HWND hwnd) { window_ = hwnd; }
void AppState::SetAddressEdit(HWND edit) { address_edit_ = edit; }
void AppState::Start() {
  LoadBrowserData();
  if (window_ && tabs_.empty()) CreateTab(HomeUrl());
}

std::wstring AppState::HomeUrl() {
  wchar_t module[MAX_PATH]{};
  GetModuleFileNameW(nullptr, module, MAX_PATH);
  auto path = std::filesystem::path(module).parent_path() / L"ui" / L"newtab.html";
  // Convert Windows separators before URL-encoding. Encoding the native
  // path directly turns backslashes into %5C and breaks Chromium file URLs.
  return L"file:///" + Encode(path.generic_wstring(), true);
}

void AppState::CreateTab(const std::wstring& requested) {
  if (!window_ || closing_) return;
  RECT bounds = GetBrowserContentRect(window_);
  CefWindowInfo info;
  info.SetAsChild(window_, CefRect(bounds.left, bounds.top,
                                   bounds.right - bounds.left,
                                   bounds.bottom - bounds.top));
  CefBrowserSettings settings;
  settings.default_font_size = 16;
  settings.default_fixed_font_size = 13;
  settings.javascript = STATE_ENABLED;
  settings.webgl = STATE_ENABLED;
  const std::wstring url = requested.empty() ? HomeUrl() : requested;
  if (!CefBrowserHost::CreateBrowser(info, GetBrowserClient(), CefString(url),
                                     settings, nullptr, nullptr)) {
    MessageBoxW(window_, L"VEYRA couldn't create a browser tab.", L"VEYRA", MB_OK | MB_ICONERROR);
  }
}

void AppState::ActivateTab(int id) {
  if (!FindTab(id)) return;
  active_id_ = id;
  LayoutTabs();
  RefreshChrome();
  FocusBrowser();
}
void AppState::CloseActiveTab() { if (active_id_ != -1) CloseTab(active_id_); }
void AppState::CloseTab(int id) {
  BrowserTab* tab = FindTab(id);
  if (!tab || !tab->browser) return;
  if (id == active_id_ && tabs_.size() > 1) {
    auto it = std::find_if(tabs_.begin(), tabs_.end(), [id](const BrowserTab& t) { return t.id == id; });
    active_id_ = (std::next(it) != tabs_.end() ? std::next(it) : std::prev(it))->id;
  }
  tab->browser->GetHost()->CloseBrowser(true);
  LayoutTabs();
  RefreshChrome();
}
void AppState::NavigateActive(const std::wstring& address) {
  auto* tab = FindTab(active_id_);
  if (tab && tab->browser) {
    const auto url = NormalizeAddress(address);
    if (!url.empty()) tab->browser->GetMainFrame()->LoadURL(CefString(url));
  }
}
void AppState::GoBack() { if (auto* t = FindTab(active_id_); t && t->browser && t->can_go_back) t->browser->GoBack(); }
void AppState::GoForward() { if (auto* t = FindTab(active_id_); t && t->browser && t->can_go_forward) t->browser->GoForward(); }
void AppState::Reload() { if (auto* t = FindTab(active_id_); t && t->browser) t->browser->Reload(); }
void AppState::Stop() { if (auto* t = FindTab(active_id_); t && t->browser) t->browser->StopLoad(); }
void AppState::GoHome() { NavigateActive(HomeUrl()); }

bool AppState::AddBookmark() {
  auto* tab = FindTab(active_id_);
  if (!tab || tab->url.empty() ||
      tab->url.find(L"newtab.html") != std::wstring::npos) return false;
  auto it = std::find_if(bookmarks_.begin(), bookmarks_.end(),
    [&](const SavedPage& page) { return page.url == tab->url; });
  if (it == bookmarks_.end()) {
    bookmarks_.insert(bookmarks_.begin(), SavedPage{tab->title, tab->url});
  } else {
    it->title = tab->title;
    std::rotate(bookmarks_.begin(), it, std::next(it));
  }
  if (bookmarks_.size() > 100) bookmarks_.resize(100);
  SaveBrowserData();
  return true;
}

bool AppState::IsBookmarked(const std::wstring& url) const {
  return std::any_of(bookmarks_.begin(), bookmarks_.end(),
    [&](const SavedPage& page) { return page.url == url; });
}

void AppState::OpenBookmark(std::size_t index) {
  if (index < bookmarks_.size()) NavigateActive(bookmarks_[index].url);
}

void AppState::OpenHistory(std::size_t index) {
  if (index < history_.size()) NavigateActive(history_[index].url);
}

void AppState::ClearHistory() {
  history_.clear();
  SaveBrowserData();
}

void AppState::LoadBrowserData() {
  const auto path = BrowserDataFile();
  bookmarks_.clear();
  history_.clear();

  const int bookmarkCount = std::clamp(
    static_cast<int>(GetPrivateProfileIntW(L"Meta", L"BookmarkCount", 0, path.c_str())), 0, 100);
  for (int i = 0; i < bookmarkCount; ++i) {
    const auto section = L"Bookmark" + std::to_wstring(i);
    auto title = ReadIni(section.c_str(), L"Title", path);
    auto url = ReadIni(section.c_str(), L"Url", path);
    if (!url.empty()) bookmarks_.push_back({title.empty() ? L"Bookmark" : title, url});
  }

  const int historyCount = std::clamp(
    static_cast<int>(GetPrivateProfileIntW(L"Meta", L"HistoryCount", 0, path.c_str())), 0, 200);
  for (int i = 0; i < historyCount; ++i) {
    const auto section = L"History" + std::to_wstring(i);
    auto title = ReadIni(section.c_str(), L"Title", path);
    auto url = ReadIni(section.c_str(), L"Url", path);
    if (!url.empty()) history_.push_back({title.empty() ? url : title, url});
  }
}

void AppState::SaveBrowserData() const {
  const auto path = BrowserDataFile();
  for (std::size_t i = 0; i < bookmarks_.size(); ++i) {
    const auto section = L"Bookmark" + std::to_wstring(i);
    WritePrivateProfileStringW(section.c_str(), L"Title", bookmarks_[i].title.c_str(), path.c_str());
    WritePrivateProfileStringW(section.c_str(), L"Url", bookmarks_[i].url.c_str(), path.c_str());
  }
  for (std::size_t i = 0; i < history_.size(); ++i) {
    const auto section = L"History" + std::to_wstring(i);
    WritePrivateProfileStringW(section.c_str(), L"Title", history_[i].title.c_str(), path.c_str());
    WritePrivateProfileStringW(section.c_str(), L"Url", history_[i].url.c_str(), path.c_str());
  }
  const auto bookmarkCount = std::to_wstring(bookmarks_.size());
  const auto historyCount = std::to_wstring(history_.size());
  WritePrivateProfileStringW(L"Meta", L"BookmarkCount", bookmarkCount.c_str(), path.c_str());
  WritePrivateProfileStringW(L"Meta", L"HistoryCount", historyCount.c_str(), path.c_str());
}

void AppState::RecordHistory(int tabId, const std::wstring& url) {
  if (url.empty() || url.find(L"newtab.html") != std::wstring::npos ||
      StartsWith(url, L"file://") || StartsWith(url, L"about:")) return;

  auto* tab = FindTab(tabId);
  std::wstring title = tab ? tab->title : url;
  auto it = std::find_if(history_.begin(), history_.end(),
    [&](const SavedPage& page) { return page.url == url; });
  if (it != history_.end()) {
    if ((title.empty() || title == L"New tab") && !it->title.empty())
      title = it->title;
    history_.erase(it);
  }
  history_.insert(history_.begin(), SavedPage{title.empty() ? url : title, url});
  if (history_.size() > 200) history_.resize(200);
  SaveBrowserData();
}

void AppState::FocusAddress() {
  if (address_edit_) { SetFocus(address_edit_); SendMessageW(address_edit_, EM_SETSEL, 0, -1); }
}
void AppState::FocusBrowser() {
  if (auto* tab = FindTab(active_id_); tab && tab->browser) {
    tab->browser->GetHost()->SetFocus(true);
  }
}
void AppState::BeginShutdown() {
  if (closing_) return;
  closing_ = true;
  if (tabs_.empty()) { PostMessageW(window_, WM_VEYRA_CLOSE_READY, 0, 0); return; }
  const auto snapshot = tabs_;
  for (const auto& tab : snapshot) if (tab.browser) tab.browser->GetHost()->CloseBrowser(true);
}
void AppState::ResizeTabs() { LayoutTabs(); }

void AppState::OnBrowserCreated(CefRefPtr<CefBrowser> browser) {
  if (closing_) { browser->GetHost()->CloseBrowser(true); return; }
  BrowserTab tab;
  tab.id = browser->GetIdentifier();
  tab.browser = browser;
  tab.url = browser->GetMainFrame()->GetURL().ToWString();
  tabs_.push_back(tab);
  active_id_ = tab.id;
  LayoutTabs();
  RefreshChrome();
  FocusBrowser();
}
void AppState::OnBrowserClosed(CefRefPtr<CefBrowser> browser) {
  const int id = browser->GetIdentifier();
  tabs_.erase(std::remove_if(tabs_.begin(), tabs_.end(),
    [id](const BrowserTab& t) { return t.id == id; }), tabs_.end());
  if (active_id_ == id) active_id_ = tabs_.empty() ? -1 : tabs_.back().id;
  if (tabs_.empty()) {
    if (closing_) PostMessageW(window_, WM_VEYRA_CLOSE_READY, 0, 0);
    else CreateTab(HomeUrl());
  } else { LayoutTabs(); RefreshChrome(); }
}
void AppState::OnLoadingStateChanged(int id, bool loading, bool back, bool forward) {
  if (auto* t = FindTab(id)) {
    t->loading = loading; t->can_go_back = back; t->can_go_forward = forward;
    RefreshChrome();
  }
}
void AppState::OnTitleChanged(int id, const std::wstring& title) {
  if (auto* t = FindTab(id)) {
    t->title = title.empty() ? L"New tab" : title;
    if (id == active_id_) SetWindowTextW(window_, (t->title + L" — VEYRA").c_str());
    auto history = std::find_if(history_.begin(), history_.end(),
      [&](const SavedPage& page) { return page.url == t->url; });
    if (history != history_.end()) {
      history->title = t->title;
      SaveBrowserData();
    }
    InvalidateRect(window_, nullptr, FALSE);
  }
}
void AppState::OnAddressChanged(int id, const std::wstring& url) {
  if (auto* t = FindTab(id)) {
    t->url = url;
    RecordHistory(id, url);
    if (id == active_id_ && address_edit_ && GetFocus() != address_edit_) {
      const auto shown = url.find(L"newtab.html") != std::wstring::npos ? L"" : url;
      SetWindowTextW(address_edit_, shown.c_str());
    }
    InvalidateRect(window_, nullptr, FALSE);
  }
}
const BrowserTab* AppState::ActiveTab() const {
  auto it = std::find_if(tabs_.begin(), tabs_.end(), [this](const BrowserTab& t) { return t.id == active_id_; });
  return it == tabs_.end() ? nullptr : &*it;
}
BrowserTab* AppState::FindTab(int id) {
  auto it = std::find_if(tabs_.begin(), tabs_.end(), [id](const BrowserTab& t) { return t.id == id; });
  return it == tabs_.end() ? nullptr : &*it;
}
void AppState::RefreshChrome() {
  if (!window_) return;
  if (const auto* t = ActiveTab()) {
    if (address_edit_ && GetFocus() != address_edit_) {
      const auto shown = t->url.find(L"newtab.html") != std::wstring::npos ? L"" : t->url;
      SetWindowTextW(address_edit_, shown.c_str());
    }
    SetWindowTextW(window_, (t->title + L" — VEYRA").c_str());
  }
  InvalidateRect(window_, nullptr, FALSE);
}
void AppState::LayoutTabs() {
  if (!window_) return;
  const RECT r = GetBrowserContentRect(window_);
  for (const auto& t : tabs_) {
    if (!t.browser) continue;
    HWND child = t.browser->GetHost()->GetWindowHandle();
    if (!child) continue;
    const bool active = t.id == active_id_;
    SetWindowPos(child, HWND_TOP, r.left, r.top, r.right-r.left, r.bottom-r.top,
                 SWP_NOACTIVATE | (active ? SWP_SHOWWINDOW : SWP_HIDEWINDOW));
  }
}
std::wstring AppState::NormalizeAddress(const std::wstring& raw) {
  auto first = raw.find_first_not_of(L" \t\r\n");
  auto last = raw.find_last_not_of(L" \t\r\n");
  if (first == std::wstring::npos) return {};
  std::wstring s = raw.substr(first, last-first+1);
  if (StartsWith(s,L"https://") || StartsWith(s,L"http://") ||
      StartsWith(s,L"file://") || StartsWith(s,L"about:") || StartsWith(s,L"veyra://")) return s;
  if (LooksLikeHost(s)) return L"https://" + s;
  return L"https://duckduckgo.com/?q=" + Encode(s, false);
}
