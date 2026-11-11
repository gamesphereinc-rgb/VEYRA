#include "src/app_state.h"
#include <algorithm>
#include <cwctype>
#include <filesystem>
#include <string>
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
}

AppState& AppState::Get() { static AppState state; return state; }
void AppState::SetWindow(HWND hwnd) { window_ = hwnd; }
void AppState::SetAddressEdit(HWND edit) { address_edit_ = edit; }
void AppState::Start() { if (window_ && tabs_.empty()) CreateTab(HomeUrl()); }

std::wstring AppState::HomeUrl() {
  wchar_t module[MAX_PATH]{};
  GetModuleFileNameW(nullptr, module, MAX_PATH);
  auto path = std::filesystem::path(module).parent_path() / L"ui" / L"newtab.html";
  return L"file:///" + Encode(path.wstring(), true);
}

void AppState::CreateTab(const std::wstring& requested) {
  if (!window_ || closing_) return;
  RECT bounds = GetBrowserContentRect(window_);
  CefWindowInfo info;
  info.SetAsChild(window_, bounds);
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
void AppState::FocusAddress() {
  if (address_edit_) { SetFocus(address_edit_); SendMessageW(address_edit_, EM_SETSEL, 0, -1); }
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
    InvalidateRect(window_, nullptr, FALSE);
  }
}
void AppState::OnAddressChanged(int id, const std::wstring& url) {
  if (auto* t = FindTab(id)) {
    t->url = url;
    if (id == active_id_ && address_edit_ && GetFocus() != address_edit_) {
      const auto shown = url.find(L"newtab.html") != std::wstring::npos ? L"" : url;
      SetWindowTextW(address_edit_, shown);
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
