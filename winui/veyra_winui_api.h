#pragma once

#include <windows.h>
#include <cstdint>

struct VEYRA_WINUI_TAB {
  std::int32_t id;
  const wchar_t* title;
};

using VeyraWinUiInitializeFn = HRESULT (WINAPI*)(HWND parent);
using VeyraWinUiResizeFn = void (WINAPI*)(int width, int height);
using VeyraWinUiSetTabsFn = void (WINAPI*)(const VEYRA_WINUI_TAB* tabs,
                                           unsigned int count,
                                           int activeId);
using VeyraWinUiSetAddressFn = void (WINAPI*)(const wchar_t* address);
using VeyraWinUiFocusAddressFn = void (WINAPI*)();
using VeyraWinUiShutdownFn = void (WINAPI*)();

#ifdef VEYRA_WINUI_EXPORTS
#define VEYRA_WINUI_API extern "C" __declspec(dllexport)
#else
#define VEYRA_WINUI_API extern "C"
#endif

VEYRA_WINUI_API HRESULT WINAPI VeyraWinUiInitialize(HWND parent);
VEYRA_WINUI_API void WINAPI VeyraWinUiResize(int width, int height);
VEYRA_WINUI_API void WINAPI VeyraWinUiSetTabs(const VEYRA_WINUI_TAB* tabs,
                                               unsigned int count,
                                               int activeId);
VEYRA_WINUI_API void WINAPI VeyraWinUiSetAddress(const wchar_t* address);
VEYRA_WINUI_API void WINAPI VeyraWinUiFocusAddress();
VEYRA_WINUI_API void WINAPI VeyraWinUiShutdown();
