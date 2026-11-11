#pragma once
#include <windows.h>
RECT GetBrowserContentRect(HWND window);
HWND CreateVeyraWindow(HINSTANCE instance, int showCommand);
void LayoutAddressEdit(HWND window);
