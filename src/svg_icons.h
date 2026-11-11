#pragma once
#include <windows.h>

namespace Gdiplus {
class Graphics;
class RectF;
}

namespace SvgIcons {
bool Draw(Gdiplus::Graphics& graphics, const wchar_t* name,
          const Gdiplus::RectF& destination, BYTE opacity = 255);
void Shutdown();
}
