#include "src/browser_client.h"
#include "src/app_state.h"
#include "src/messages.h"
#include "src/update_manager.h"
#include <filesystem>
#include <shlobj.h>

namespace {
CefRefPtr<BrowserClient> g_client;

std::filesystem::path DownloadsDirectory() {
  PWSTR known = nullptr;
  if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Downloads, 0, nullptr, &known))) {
    std::filesystem::path result(known);
    CoTaskMemFree(known);
    return result;
  }
  wchar_t temp[MAX_PATH]{};
  GetTempPathW(MAX_PATH, temp);
  return std::filesystem::path(temp);
}
}

CefRefPtr<BrowserClient> GetBrowserClient() {
  if (!g_client) g_client = new BrowserClient();
  return g_client;
}

namespace {
int HexValue(wchar_t c) {
  if(c>=L'0'&&c<=L'9')return c-L'0';
  if(c>=L'a'&&c<=L'f')return c-L'a'+10;
  if(c>=L'A'&&c<=L'F')return c-L'A'+10;
  return -1;
}
std::wstring QueryParameter(const std::wstring& url,const std::wstring& key) {
  const auto query = url.find(L'?');
  if (query == std::wstring::npos) return {};

  std::size_t cursor = query + 1;
  while (cursor < url.size()) {
    const auto end = url.find_first_of(L"&#", cursor);
    const auto tokenEnd = end == std::wstring::npos ? url.size() : end;
    const auto equals = url.find(L'=', cursor);
    if (equals != std::wstring::npos && equals < tokenEnd &&
        url.compare(cursor, equals - cursor, key) == 0) {
      const auto encoded = url.substr(equals + 1, tokenEnd - equals - 1);
      std::wstring decoded;
      for (std::size_t i = 0; i < encoded.size(); ++i) {
        if (encoded[i] == L'%' && i + 2 < encoded.size()) {
          const int high = HexValue(encoded[i + 1]);
          const int low = HexValue(encoded[i + 2]);
          if (high >= 0 && low >= 0) {
            decoded.push_back(static_cast<wchar_t>((high << 4) | low));
            i += 2;
            continue;
          }
        }
        decoded.push_back(encoded[i] == L'+' ? L' ' : encoded[i]);
      }
      return decoded;
    }
    if (end == std::wstring::npos) break;
    cursor = end + 1;
  }
  return {};
}
}

bool BrowserClient::OnBeforeBrowse(CefRefPtr<CefBrowser> browser,
    CefRefPtr<CefFrame> frame, CefRefPtr<CefRequest> request, bool, bool) {
  if(!request||!browser||!frame||!frame->IsMain())return false;
  const auto url=request->GetURL().ToWString();
  if(url.rfind(L"veyra://install-update",0)!=0)return false;
  const auto current = browser->GetMainFrame()->GetURL().ToWString();
  const auto home = AppState::Get().HomeUrl();
  const bool trustedHome = current == home ||
      (current.size() > home.size() &&
       current.compare(0, home.size(), home) == 0 &&
       current[home.size()] == L'#');
  if (!trustedHome) {
    MessageBeep(MB_ICONWARNING);
    return true;
  }
  const auto download=QueryParameter(url,L"url");
  const auto digest=QueryParameter(url,L"sha256");
  VeyraUpdater::StartDownload(AppState::Get().MainWindow(),download,digest);
  return true;
}

bool BrowserClient::OnBeforePopup(CefRefPtr<CefBrowser>,
                                  CefRefPtr<CefFrame>,
                                  int,
                                  const CefString& target_url,
                                  const CefString&,
                                  CefLifeSpanHandler::WindowOpenDisposition,
                                  bool,
                                  const CefPopupFeatures&,
                                  CefWindowInfo&,
                                  CefRefPtr<CefClient>&,
                                  CefBrowserSettings&,
                                  CefRefPtr<CefDictionaryValue>&,
                                  bool*) {
  // Route target=_blank, window.open and other popup requests into a normal
  // VEYRA tab instead of creating an unmanaged Chromium window.
  if (!target_url.empty()) {
    AppState::Get().CreateTab(target_url.ToWString());
  }
  return true;
}

void BrowserClient::OnAfterCreated(CefRefPtr<CefBrowser> browser) {
  AppState::Get().OnBrowserCreated(browser);
}
void BrowserClient::OnBeforeClose(CefRefPtr<CefBrowser> browser) {
  AppState::Get().OnBrowserClosed(browser);
}
void BrowserClient::OnLoadingStateChange(CefRefPtr<CefBrowser> browser, bool loading,
                                         bool canGoBack, bool canGoForward) {
  AppState::Get().OnLoadingStateChanged(browser->GetIdentifier(), loading,
                                        canGoBack, canGoForward);
}
void BrowserClient::OnTitleChange(CefRefPtr<CefBrowser> browser, const CefString& title) {
  AppState::Get().OnTitleChanged(browser->GetIdentifier(), title.ToWString());
}
void BrowserClient::OnAddressChange(CefRefPtr<CefBrowser> browser,
                                    CefRefPtr<CefFrame> frame, const CefString& url) {
  if (frame && frame->IsMain())
    AppState::Get().OnAddressChanged(browser->GetIdentifier(), url.ToWString());
}
bool BrowserClient::OnPreKeyEvent(CefRefPtr<CefBrowser>, const CefKeyEvent& event,
                                  CefEventHandle, bool* is_keyboard_shortcut) {
  if (event.type != KEYEVENT_RAWKEYDOWN) return false;
  const bool ctrl = (event.modifiers & EVENTFLAG_CONTROL_DOWN) != 0;
  const bool alt = (event.modifiers & EVENTFLAG_ALT_DOWN) != 0;
  const int key = event.windows_key_code;
  HWND hwnd = AppState::Get().MainWindow();
  if (!hwnd) return false;
  UINT message = 0;
  if (ctrl) {
    switch (key) {
      case 'T': message = WM_VEYRA_NEW_TAB; break;
      case 'W': message = WM_VEYRA_CLOSE_TAB; break;
      case 'L': message = WM_VEYRA_FOCUS_ADDRESS; break;
      case 'R': message = WM_VEYRA_RELOAD; break;
      case 'D': message = WM_VEYRA_BOOKMARK; break;
      default: return false;
    }
  } else if (key == VK_F5) {
    message = WM_VEYRA_RELOAD;
  } else if (alt && key == VK_LEFT) {
    message = WM_VEYRA_BACK;
  } else if (alt && key == VK_RIGHT) {
    message = WM_VEYRA_FORWARD;
  } else {
    return false;
  }
  PostMessageW(hwnd, message, 0, 0);
  if (is_keyboard_shortcut) *is_keyboard_shortcut = true;
  return true;
}


bool BrowserClient::OnBeforeDownload(
    CefRefPtr<CefBrowser>,
    CefRefPtr<CefDownloadItem>,
    const CefString& suggested_name,
    CefRefPtr<CefBeforeDownloadCallback> callback) {
  if (!callback) return false;

  const auto directory = DownloadsDirectory();
  std::error_code ec;
  std::filesystem::create_directories(directory, ec);
  if (ec) return false;

  auto filename = std::filesystem::path(suggested_name.ToWString()).filename();
  if (filename.empty() || filename == std::filesystem::path(L".") ||
      filename == std::filesystem::path(L"..")) filename = L"download";

  auto destination = directory / filename;
  const auto stem = filename.stem().wstring();
  const auto extension = filename.extension().wstring();
  for (unsigned int suffix = 1; std::filesystem::exists(destination, ec) && suffix < 10000; ++suffix) {
    destination = directory /
      (stem + L" (" + std::to_wstring(suffix) + L")" + extension);
  }
  if (ec) return false;

  callback->Continue(CefString(destination.wstring()), false);
  return true;
}
