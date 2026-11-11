#pragma once
#include <windows.h>
#include <string>
namespace VeyraUpdater {
void StartDownload(HWND window, const std::wstring& downloadUrl,
                   const std::wstring& expectedSha256);
bool LaunchInstaller(const std::wstring& archivePath, DWORD processId,
                     std::wstring& error);
}
