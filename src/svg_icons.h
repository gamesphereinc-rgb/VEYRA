#pragma once
#include <windows.h>
#include <gdiplus.h>

namespace SvgIcons {
bool Draw(Gdiplus::Graphics& graphics, const wchar_t* name,
          const Gdiplus::RectF& destination, BYTE opacity = 255);
void Shutdown();
}
