#pragma once
#include <windows.h>
#include <string>
#include <vector>
#include "include/cef_browser.h"

struct SavedPage {
  std::wstring title;
  std::wstring url;
};

struct BrowserTab {
  int id = -1;
  CefRefPtr<CefBrowser> browser;
  std::wstring title = L"New tab";
  std::wstring url;
  bool loading = false;
  bool can_go_back = false;
  bool can_go_forward = false;
};

class AppState final {
 public:
  static AppState& Get();
  void SetWindow(HWND hwnd);
  void SetAddressEdit(HWND edit);
  void Start();
  void CreateTab(const std::wstring& url = L"");
  void ActivateTab(int id);
  void CloseActiveTab();
  void CloseTab(int id);
  void NavigateActive(const std::wstring& address);
  void GoBack();
  void GoForward();
  void Reload();
  void Stop();
  void GoHome();
  bool AddBookmark();
  bool IsBookmarked(const std::wstring& url) const;
  void OpenBookmark(std::size_t index);
  void OpenHistory(std::size_t index);
  void ClearHistory();
  const std::vector<SavedPage>& Bookmarks() const { return bookmarks_; }
  const std::vector<SavedPage>& History() const { return history_; }
  void FocusAddress();
  void FocusBrowser();
  void NotifyTabUpdateFailed(int id);
  void NotifyHistoryCleared(int id);
  void BeginShutdown();
  void ResizeTabs();
  void OnBrowserCreated(CefRefPtr<CefBrowser> browser);
  void OnBrowserClosed(CefRefPtr<CefBrowser> browser);
  void OnLoadingStateChanged(int id, bool loading, bool back, bool forward);
  void OnTitleChanged(int id, const std::wstring& title);
  void OnAddressChanged(int id, const std::wstring& url);
  const std::vector<BrowserTab>& Tabs() const { return tabs_; }
  const BrowserTab* ActiveTab() const;
  HWND MainWindow() const { return window_; }
  static std::wstring HomeUrl();

 private:
  AppState() = default;
  BrowserTab* FindTab(int id);
  void RefreshChrome();
  void LayoutTabs();
  void LoadBrowserData();
  void SaveBrowserData() const;
  void RecordHistory(int tabId, const std::wstring& url);
  static std::wstring NormalizeAddress(const std::wstring& address);
  HWND window_ = nullptr;
  HWND address_edit_ = nullptr;
  std::vector<BrowserTab> tabs_;
  std::vector<SavedPage> bookmarks_;
  std::vector<SavedPage> history_;
  int active_id_ = -1;
  bool closing_ = false;
};
