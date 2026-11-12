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
#include <cmath>

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

  constexpr int kSourceSize = 192;
  const float scale = std::min((kSourceSize - 24.0f) / image->width,
                               (kSourceSize - 24.0f) / image->height);
  const float offsetX = (kSourceSize - image->width * scale) * 0.5f;
  const float offsetY = (kSourceSize - image->height * scale) * 0.5f;
  std::vector<unsigned char> rgba(
      static_cast<std::size_t>(kSourceSize * kSourceSize * 4), 0);
  nsvgRasterize(g_rasterizer, image, offsetX, offsetY, scale,
                rgba.data(), kSourceSize, kSourceSize, kSourceSize * 4);
  nsvgDelete(image);

  auto sourceBitmap = std::make_unique<Gdiplus::Bitmap>(
      kSourceSize, kSourceSize, PixelFormat32bppARGB);
  if (sourceBitmap->GetLastStatus() != Gdiplus::Ok) return nullptr;

  Gdiplus::Rect sourceBounds(0, 0, kSourceSize, kSourceSize);
  Gdiplus::BitmapData locked{};
  if (sourceBitmap->LockBits(&sourceBounds, Gdiplus::ImageLockModeWrite,
                             PixelFormat32bppARGB, &locked) != Gdiplus::Ok) {
    return nullptr;
  }
  int minX = kSourceSize, minY = kSourceSize, maxX = -1, maxY = -1;
  for (int y = 0; y < kSourceSize; ++y) {
    auto* destination = static_cast<unsigned char*>(locked.Scan0) +
                        static_cast<std::ptrdiff_t>(y) * locked.Stride;
    for (int x = 0; x < kSourceSize; ++x) {
      const auto sourceIndex =
          static_cast<std::size_t>((y * kSourceSize + x) * 4);
      const auto destinationIndex = static_cast<std::size_t>(x * 4);
      destination[destinationIndex + 0] = rgba[sourceIndex + 2];
      destination[destinationIndex + 1] = rgba[sourceIndex + 1];
      destination[destinationIndex + 2] = rgba[sourceIndex + 0];
      destination[destinationIndex + 3] = rgba[sourceIndex + 3];
      if (rgba[sourceIndex + 3] > 8) {
        minX = std::min(minX, x);
        minY = std::min(minY, y);
        maxX = std::max(maxX, x);
        maxY = std::max(maxY, y);
      }
    }
  }
  sourceBitmap->UnlockBits(&locked);
  if (maxX < minX || maxY < minY) return nullptr;

  // Centre the visible alpha bounds, not the SVG viewBox. This compensates for
  // icons whose paths have different internal whitespace.
  const int visibleWidth = maxX - minX + 1;
  const int visibleHeight = maxY - minY + 1;
  const float outputScale = std::min((kRasterSize - 16.0f) / visibleWidth,
                                     (kRasterSize - 16.0f) / visibleHeight);
  const float drawWidth = visibleWidth * outputScale;
  const float drawHeight = visibleHeight * outputScale;
  const Gdiplus::RectF destination(
      (kRasterSize - drawWidth) * 0.5f,
      (kRasterSize - drawHeight) * 0.5f,
      drawWidth, drawHeight);

  auto result = std::make_unique<Gdiplus::Bitmap>(
      kRasterSize, kRasterSize, PixelFormat32bppARGB);
  if (result->GetLastStatus() != Gdiplus::Ok) return nullptr;
  Gdiplus::Graphics graphics(result.get());
  graphics.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
  graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);
  if (graphics.DrawImage(sourceBitmap.get(), destination,
                         static_cast<Gdiplus::REAL>(minX),
                         static_cast<Gdiplus::REAL>(minY),
                         static_cast<Gdiplus::REAL>(visibleWidth),
                         static_cast<Gdiplus::REAL>(visibleHeight),
                         Gdiplus::UnitPixel) != Gdiplus::Ok) {
    return nullptr;
  }
  return result;
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
  const auto state = graphics.Save();
  graphics.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
  graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);
  const auto status = graphics.DrawImage(found->second.get(), destination,
      0.0f, 0.0f, static_cast<float>(kRasterSize),
      static_cast<float>(kRasterSize), Gdiplus::UnitPixel, &attributes);
  graphics.Restore(state);
  return status == Gdiplus::Ok;
}
}

void Shutdown() {
  g_icons.clear();
  if (g_rasterizer) {
    nsvgDeleteRasterizer(g_rasterizer);
    g_rasterizer = nullptr;
  }
}
}
