#pragma once
#include <windows.h>

constexpr UINT WM_VEYRA_NEW_TAB = WM_APP + 10;
constexpr UINT WM_VEYRA_CLOSE_TAB = WM_APP + 11;
constexpr UINT WM_VEYRA_FOCUS_ADDRESS = WM_APP + 12;
constexpr UINT WM_VEYRA_BACK = WM_APP + 13;
constexpr UINT WM_VEYRA_FORWARD = WM_APP + 14;
constexpr UINT WM_VEYRA_RELOAD = WM_APP + 15;
constexpr UINT WM_VEYRA_HOME = WM_APP + 16;
constexpr UINT WM_VEYRA_CLOSE_READY = WM_APP + 17;
constexpr UINT WM_VEYRA_NAVIGATE = WM_APP + 18;

constexpr int ID_ADDRESS_EDIT = 1001;
constexpr int ID_MENU_NEW_TAB = 2001;
constexpr int ID_MENU_RELOAD = 2002;
constexpr int ID_MENU_CLOSE_TAB = 2003;
constexpr int ID_MENU_ABOUT = 2004;
constexpr int ID_MENU_EXIT = 2005;

constexpr UINT WM_VEYRA_BOOKMARK = WM_APP + 19;

constexpr UINT WM_VEYRA_UPDATE_READY = WM_APP + 20;
constexpr UINT WM_VEYRA_UPDATE_FAILED = WM_APP + 21;

constexpr UINT WM_VEYRA_MENU_COMMAND = WM_APP + 22;
