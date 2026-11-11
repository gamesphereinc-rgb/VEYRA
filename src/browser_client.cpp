#include "src/browser_client.h"
#include "src/app_state.h"
#include "src/messages.h"

namespace { CefRefPtr<BrowserClient> g_client; }

CefRefPtr<BrowserClient> GetBrowserClient() {
  if (!g_client) g_client = new BrowserClient();
  return g_client;
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
