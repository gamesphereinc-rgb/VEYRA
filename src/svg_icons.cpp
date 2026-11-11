#include <windows.h>
#include <objbase.h>
#include <wingdi.h>
#include <gdiplus.h>
#include "src/svg_icons.h"

#define NANOSVG_IMPLEMENTATION
#include "third_party/nanosvg/nanosvg.h"
#define NANOSVGRAST_IMPLEMENTATION
#include "third_party/nanosvg/nanosvgrast.h"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <algorithm>
#include <cstddef>

namespace SvgIcons {
namespace {
constexpr int kRasterSize = 96;
NSVGrasterizer* g_rasterizer = nullptr;
std::unordered_map<std::wstring, std::unique_ptr<Gdiplus::Bitmap>> g_icons;

std::unique_ptr<Gdiplus::Bitmap> LoadIcon(const std::wstring& name) {
  if (!g_rasterizer) g_rasterizer = nsvgCreateRasterizer();
  if (!g_rasterizer) return nullptr;

  wchar_t executable[MAX_PATH]{};
  const DWORD length = GetModuleFileNameW(nullptr, executable, MAX_PATH);
  if (!length || length >= MAX_PATH) return nullptr;

  const auto filePath = std::filesystem::path(executable).parent_path() /
                        L"ui" / L"icons" / (name + L".svg");
  std::ifstream file(filePath, std::ios::binary);
  if (!file) return nullptr;

  std::string source((std::istreambuf_iterator<char>(file)),
                     std::istreambuf_iterator<char>());
  if (source.empty()) return nullptr;
  source.push_back('\0');

  NSVGimage* image = nsvgParse(source.data(), "px", 96.0f);
  if (!image) return nullptr;
  if (image->width <= 0.0f || image->height <= 0.0f) {
    nsvgDelete(image);
    return nullptr;
  }

  const float scale = std::min((kRasterSize - 8.0f) / image->width,
                               (kRasterSize - 8.0f) / image->height);
  const float offsetX = (kRasterSize - image->width * scale) * 0.5f;
  const float offsetY = (kRasterSize - image->height * scale) * 0.5f;
  std::vector<unsigned char> rgba(
      static_cast<std::size_t>(kRasterSize * kRasterSize * 4), 0);
  nsvgRasterize(g_rasterizer, image, offsetX, offsetY, scale,
               rgba.data(), kRasterSize, kRasterSize, kRasterSize * 4);
  nsvgDelete(image);

  auto bitmap = std::make_unique<Gdiplus::Bitmap>(
      kRasterSize, kRasterSize, PixelFormat32bppARGB);
  if (bitmap->GetLastStatus() != Gdiplus::Ok) return nullptr;

  Gdiplus::Rect bounds(0, 0, kRasterSize, kRasterSize);
  Gdiplus::BitmapData locked{};
  if (bitmap->LockBits(&bounds, Gdiplus::ImageLockModeWrite,
                       PixelFormat32bppARGB, &locked) != Gdiplus::Ok) {
    return nullptr;
  }
  for (int y = 0; y < kRasterSize; ++y) {
    auto* destination = static_cast<unsigned char*>(locked.Scan0) +
                        static_cast<std::ptrdiff_t>(y) * locked.Stride;
    for (int x = 0; x < kRasterSize; ++x) {
      const auto src = static_cast<std::size_t>((y * kRasterSize + x) * 4);
      const auto dst = static_cast<std::size_t>(x * 4);
      destination[dst + 0] = rgba[src + 2];
      destination[dst + 1] = rgba[src + 1];
      destination[dst + 2] = rgba[src + 0];
      destination[dst + 3] = rgba[src + 3];
    }
  }
  bitmap->UnlockBits(&locked);
  return bitmap;
}
}

bool Draw(Gdiplus::Graphics& graphics, const wchar_t* name,
          const Gdiplus::RectF& destination, BYTE opacity) {
  if (!name || !*name || opacity == 0) return false;
  const std::wstring key(name);
  auto found = g_icons.find(key);
  if (found == g_icons.end()) {
    auto icon = LoadIcon(key);
    if (!icon) return false;
    found = g_icons.emplace(key, std::move(icon)).first;
  }

  Gdiplus::ColorMatrix matrix{};
  matrix.m[0][0] = 1.0f;
  matrix.m[1][1] = 1.0f;
  matrix.m[2][2] = 1.0f;
  matrix.m[3][3] = static_cast<float>(opacity) / 255.0f;
  matrix.m[4][4] = 1.0f;
  Gdiplus::ImageAttributes attributes;
  attributes.SetColorMatrix(&matrix, Gdiplus::ColorMatrixFlagsDefault,
                            Gdiplus::ColorAdjustTypeBitmap);
  return graphics.DrawImage(found->second.get(), destination, 0.0f, 0.0f,
      static_cast<float>(kRasterSize), static_cast<float>(kRasterSize),
      Gdiplus::UnitPixel, &attributes) == Gdiplus::Ok;
}

void Shutdown() {
  g_icons.clear();
  if (g_rasterizer) {
    nsvgDeleteRasterizer(g_rasterizer);
    g_rasterizer = nullptr;
  }
}
}
