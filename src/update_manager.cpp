#include "src/update_manager.h"
#include <windows.h>
#include <winhttp.h>
#include <bcrypt.h>
#include <shlobj.h>
#include <filesystem>
#include <fstream>
#include <array>
#include <atomic>
#include <thread>
#include <vector>
#include <algorithm>
#include <stdexcept>
#include <cwctype>
#include "src/messages.h"

namespace VeyraUpdater {
namespace {
std::atomic_bool g_running{false};

std::wstring UpdatesDirectory() {
  PWSTR known = nullptr;
  std::filesystem::path root;
  if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &known))) {
    root = known; CoTaskMemFree(known);
  } else {
    wchar_t temp[MAX_PATH]{};
    GetTempPathW(MAX_PATH, temp);
    root = temp;
  }
  root /= L"VEYRA"; root /= L"Updates";
  std::error_code ec;
  std::filesystem::create_directories(root, ec);
  if (ec) throw std::runtime_error("Cannot create the update folder.");
  return root.wstring();
}
std::wstring Lower(std::wstring value) {
  std::transform(value.begin(), value.end(), value.begin(),
      [](wchar_t c) { return static_cast<wchar_t>(towlower(c)); });
  return value;
}
bool ValidDigest(const std::wstring& value) {
  return value.size() == 64 && std::all_of(value.begin(), value.end(),
      [](wchar_t c) { return (c >= L'0' && c <= L'9') ||
                             (towlower(c) >= L'a' && towlower(c) <= L'f'); });
}

std::wstring Download(const std::wstring& url) {
  URL_COMPONENTS parts{};
  parts.dwStructSize = sizeof(parts);
  parts.dwSchemeLength = parts.dwHostNameLength = static_cast<DWORD>(-1);
  parts.dwUrlPathLength = parts.dwExtraInfoLength = static_cast<DWORD>(-1);
  if (!WinHttpCrackUrl(url.c_str(), static_cast<DWORD>(url.size()), 0, &parts) ||
      parts.nScheme != INTERNET_SCHEME_HTTPS)
    throw std::runtime_error("The update link is not a valid HTTPS URL.");
  const std::wstring host(parts.lpszHostName, parts.dwHostNameLength);
  const std::wstring path(parts.lpszUrlPath, parts.dwUrlPathLength);
  const std::wstring extra = parts.dwExtraInfoLength
      ? std::wstring(parts.lpszExtraInfo, parts.dwExtraInfoLength) : L"";
  if (Lower(host) != L"github.com" ||
      path.find(L"/gamesphereinc-rgb/VEYRA/releases/download/") != 0)
    throw std::runtime_error("This is not an official VEYRA release link.");

  HINTERNET session = WinHttpOpen(L"VEYRA-Updater/1.0",
      WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME,
      WINHTTP_NO_PROXY_BYPASS, 0);
  if (!session) throw std::runtime_error("Could not initialise secure downloads.");
  WinHttpSetTimeouts(session, 15000, 15000, 30000, 30000);
  HINTERNET connection = WinHttpConnect(session, host.c_str(), parts.nPort, 0);
  if (!connection) {
    WinHttpCloseHandle(session);
    throw std::runtime_error("Could not connect to GitHub.");
  }
  HINTERNET request = WinHttpOpenRequest(connection, L"GET",
      (path + extra).c_str(), nullptr, WINHTTP_NO_REFERER,
      WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
  if (!request) {
    WinHttpCloseHandle(connection); WinHttpCloseHandle(session);
    throw std::runtime_error("Could not create the update request.");
  }
  const BOOL sent = WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS,
      0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
  const BOOL received = sent && WinHttpReceiveResponse(request, nullptr);
  DWORD status = 0, statusSize = sizeof(status);
  const BOOL gotStatus = received && WinHttpQueryHeaders(request,
      WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
      WINHTTP_HEADER_NAME_BY_INDEX, &status, &statusSize, WINHTTP_NO_HEADER_INDEX);
  if (!gotStatus || status != 200) {
    WinHttpCloseHandle(request); WinHttpCloseHandle(connection); WinHttpCloseHandle(session);
    throw std::runtime_error("GitHub did not return the update package.");
  }

  const auto destination = std::filesystem::path(UpdatesDirectory()) /
                           L"VEYRA-windows-x64-update.zip";
  std::ofstream output(destination, std::ios::binary | std::ios::trunc);
  if (!output) {
    WinHttpCloseHandle(request); WinHttpCloseHandle(connection); WinHttpCloseHandle(session);
    throw std::runtime_error("Cannot write the update package.");
  }
  std::array<char, 65536> buffer{};
  std::uint64_t total = 0;
  bool ok = true;
  for (;;) {
    DWORD available = 0;
    if (!WinHttpQueryDataAvailable(request, &available)) { ok = false; break; }
    if (!available) break;
    while (available) {
      DWORD count = 0;
      const DWORD wanted = std::min<DWORD>(available, static_cast<DWORD>(buffer.size()));
      if (!WinHttpReadData(request, buffer.data(), wanted, &count) || !count) { ok = false; break; }
      total += count;
      if (total > 700ULL * 1024ULL * 1024ULL) { ok = false; break; }
      output.write(buffer.data(), count);
      if (!output) { ok = false; break; }
      available -= count;
    }
    if (!ok) break;
  }
  output.close();
  WinHttpCloseHandle(request); WinHttpCloseHandle(connection); WinHttpCloseHandle(session);
  if (!ok || total < 1024) {
    std::error_code ec; std::filesystem::remove(destination, ec);
    throw std::runtime_error("The update download was incomplete.");
  }
  return destination.wstring();
}

std::wstring Sha256(const std::wstring& path) {
  BCRYPT_ALG_HANDLE algorithm = nullptr;
  BCRYPT_HASH_HANDLE hash = nullptr;
  DWORD objectLength = 0, hashLength = 0, resultLength = 0;
  if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0)
    throw std::runtime_error("Could not initialise SHA-256 verification.");
  auto closeAlgorithm = [&] { if (algorithm) BCryptCloseAlgorithmProvider(algorithm, 0); };
  if (BCryptGetProperty(algorithm, BCRYPT_OBJECT_LENGTH,
        reinterpret_cast<PUCHAR>(&objectLength), sizeof(objectLength), &resultLength, 0) < 0 ||
      BCryptGetProperty(algorithm, BCRYPT_HASH_LENGTH,
        reinterpret_cast<PUCHAR>(&hashLength), sizeof(hashLength), &resultLength, 0) < 0 ||
      hashLength != 32) {
    closeAlgorithm(); throw std::runtime_error("Could not initialise SHA-256 verification.");
  }
  std::vector<UCHAR> object(objectLength), digest(hashLength);
  if (BCryptCreateHash(algorithm, &hash, object.data(), objectLength, nullptr, 0, 0) < 0) {
    closeAlgorithm(); throw std::runtime_error("Could not create the SHA-256 verifier.");
  }
  std::ifstream input(path, std::ios::binary);
  if (!input) {
    BCryptDestroyHash(hash); closeAlgorithm();
    throw std::runtime_error("Cannot read the downloaded update.");
  }
  std::array<char, 65536> chunk{};
  while (input) {
    input.read(chunk.data(), static_cast<std::streamsize>(chunk.size()));
    const auto count = input.gcount();
    if (count > 0 && BCryptHashData(hash, reinterpret_cast<PUCHAR>(chunk.data()),
                                    static_cast<ULONG>(count), 0) < 0) {
      BCryptDestroyHash(hash); closeAlgorithm();
      throw std::runtime_error("SHA-256 verification failed.");
    }
  }
  if (!input.eof() || BCryptFinishHash(hash, digest.data(), hashLength, 0) < 0) {
    BCryptDestroyHash(hash); closeAlgorithm();
    throw std::runtime_error("SHA-256 verification failed.");
  }
  BCryptDestroyHash(hash); closeAlgorithm();
  constexpr wchar_t hex[] = L"0123456789abcdef";
  std::wstring result; result.reserve(64);
  for (UCHAR byte : digest) { result.push_back(hex[byte >> 4]); result.push_back(hex[byte & 15]); }
  return result;
}
}

void StartDownload(HWND window, const std::wstring& url, const std::wstring& expectedSha256) {
  if (!window || g_running.exchange(true)) {
    if (window) MessageBeep(MB_ICONWARNING);
    return;
  }
  if (!ValidDigest(expectedSha256)) {
    g_running = false;
    MessageBoxW(window, L"This release has no valid SHA-256 checksum. The installer was not downloaded.",
                L"VEYRA updater", MB_OK | MB_ICONWARNING);
    return;
  }
  std::thread([window, url, expectedSha256] {
    try {
      const auto path = Download(url);
      if (Lower(Sha256(path)) != Lower(expectedSha256)) {
        std::error_code ec; std::filesystem::remove(path, ec);
        throw std::runtime_error("The update failed its SHA-256 check and was deleted.");
      }
      PostMessageW(window, WM_VEYRA_UPDATE_READY, 0,
                   reinterpret_cast<LPARAM>(new std::wstring(path)));
    } catch (const std::exception& error) {
      const int length = MultiByteToWideChar(CP_UTF8, 0, error.what(), -1, nullptr, 0);
      std::wstring message(static_cast<std::size_t>(std::max(1, length)), L'\0');
      MultiByteToWideChar(CP_UTF8, 0, error.what(), -1, message.data(), length);
      PostMessageW(window, WM_VEYRA_UPDATE_FAILED, 0,
                   reinterpret_cast<LPARAM>(new std::wstring(message)));
    }
    g_running = false;
  }).detach();
}

bool LaunchInstaller(const std::wstring& archivePath, DWORD processId, std::wstring& error) {
  wchar_t module[MAX_PATH]{};
  const DWORD length = GetModuleFileNameW(nullptr, module, MAX_PATH);
  if (!length || length >= MAX_PATH) { error = L"Could not locate the VEYRA installation folder."; return false; }
  const auto install = std::filesystem::path(module).parent_path();
  const auto sourceScript = install / L"scripts" / L"apply-update.ps1";
  const auto updates = std::filesystem::path(UpdatesDirectory());
  const auto script = updates / L"apply-update.ps1";
  std::error_code ec;
  std::filesystem::copy_file(sourceScript, script,
      std::filesystem::copy_options::overwrite_existing, ec);
  if (ec) { error = L"Could not prepare the updater script. " + ec.message(); return false; }

  wchar_t system[MAX_PATH]{};
  GetSystemDirectoryW(system, MAX_PATH);
  const auto powershell = std::filesystem::path(system) / L"WindowsPowerShell" / L"v1.0" / L"powershell.exe";
  std::wstring command = L"\"" + powershell.wstring() +
      L"\" -NoProfile -NonInteractive -ExecutionPolicy Bypass -WindowStyle Hidden"
      L" -File \"" + script.wstring() + L"\""
      L" -ArchivePath \"" + archivePath + L"\""
      L" -ProcessId " + std::to_wstring(processId) +
      L" -InstallDirectory \"" + install.wstring() + L"\"";
  std::vector<wchar_t> mutableCommand(command.begin(), command.end());
  mutableCommand.push_back(L'\0');
  STARTUPINFOW startup{sizeof(startup)};
  PROCESS_INFORMATION process{};
  if (!CreateProcessW(powershell.c_str(), mutableCommand.data(), nullptr, nullptr,
      FALSE, CREATE_NO_WINDOW, nullptr, install.c_str(), &startup, &process)) {
    error = L"Could not start the updater. Install VEYRA in a folder you can write to.";
    return false;
  }
  CloseHandle(process.hThread); CloseHandle(process.hProcess);
  return true;
}
}
