#include <windows.h>
#include <shlobj.h>
#include <filesystem>
#include <string>
#include "include/cef_app.h"
#include "include/cef_version_info.h"
#include "src/app_state.h"
#include "src/window.h"

namespace {
class VeyraApp final : public CefApp {
 private:
  IMPLEMENT_REFCOUNTING(VeyraApp);
};

std::wstring ProfilePath() {
  PWSTR known = nullptr;
  std::wstring path;
  if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &known))) {
    path = known; CoTaskMemFree(known);
  } else {
    wchar_t temp[MAX_PATH]{}; GetTempPathW(MAX_PATH,temp); path=temp;
  }
  path += L"\\VEYRA\\Profile";
  std::filesystem::create_directories(path);
  return path;
}

int RunMain(HINSTANCE instance, int showCommand, void* sandboxInfo) {
  CefMainArgs args(instance);
  CefRefPtr<CefApp> app(new VeyraApp());
  const int childExit = CefExecuteProcess(args,app,sandboxInfo);
  if(childExit>=0)return childExit;

  CefEnableHighDPISupport();
  CefSettings settings;
  settings.no_sandbox = sandboxInfo == nullptr;
  settings.multi_threaded_message_loop = false;
  settings.windowless_rendering_enabled = false;
  settings.persist_session_cookies = true;
  settings.log_severity = LOGSEVERITY_WARNING;
  CefString(&settings.cache_path) = ProfilePath();

  if(!CefInitialize(args,settings,app,sandboxInfo)){
    MessageBoxW(nullptr,L"VEYRA couldn't initialise Chromium. Check the CEF runtime files.",
      L"VEYRA startup error",MB_OK|MB_ICONERROR);
    return 1;
  }
  HWND hwnd=CreateVeyraWindow(instance,showCommand);
  if(!hwnd){CefShutdown();MessageBoxW(nullptr,L"VEYRA couldn't create its window.",
    L"VEYRA startup error",MB_OK|MB_ICONERROR);return 1;}
  AppState::Get().Start();
  CefRunMessageLoop();
  CefShutdown();
  return 0;
}
}

#if defined(CEF_USE_BOOTSTRAP)
CEF_BOOTSTRAP_EXPORT int RunWinMain(HINSTANCE instance, LPWSTR, int showCommand,
                                    void* sandboxInfo, cef_version_info_t*) {
  return RunMain(instance,showCommand,sandboxInfo);
}
#else
int APIENTRY wWinMain(HINSTANCE instance,HINSTANCE,LPWSTR,int showCommand) {
  return RunMain(instance,showCommand,nullptr);
}
#endif
