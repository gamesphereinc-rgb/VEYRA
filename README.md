# VEYRA

**Browse beyond.**

VEYRA is an independent Windows browser with its own native interface and visual identity, powered by Chromium through the Chromium Embedded Framework (CEF).

It is not an Electron app and it does not use Microsoft Edge WebView2. VEYRA ships with its own CEF/Chromium runtime.

## First development milestone

- Native Windows application window and custom-drawn VEYRA browser chrome.
- Multiple independent Chromium tabs.
- Address bar with HTTPS navigation and DuckDuckGo search.
- Back, forward, reload/stop and home.
- Original VEYRA new-tab page with web search, quick links, date and clock.
- Persistent Chromium profile/cache in the current Windows user's Local AppData.
- CEF bootstrap architecture for the Windows sandbox.
- Keyboard shortcuts: Ctrl+T, Ctrl+W, Ctrl+L, Ctrl+R, F5, Alt+Left and Alt+Right.

Bookmarks, downloads UI, full history management, extensions, private windows, settings, profile sync and update delivery are not implemented yet.

## Technology

C++20, Win32, Chromium Embedded Framework (CEF), CMake, Visual Studio 2022 and GDI+.

## Build on Windows

Requirements: Windows 10/11 x64, Visual Studio 2022 with **Desktop development with C++**, CMake 3.24+, and a Windows SDK installed through Visual Studio Installer. The pinned CEF download is about 350 MB before extraction.

Open PowerShell in the repository and run:

```powershell
powershell -ExecutionPolicy Bypass -File scripts/setup.ps1
powershell -ExecutionPolicy Bypass -File scripts/build.ps1 -Configuration Release
```

The setup script downloads the official CEF 154 Windows x64 standard distribution, verifies its SHA-1 hash against the CEF build server's checksum, then extracts it under `third_party/cef`. CEF binaries are not committed to this repository.

The launcher is written to `build/out/VEYRA.exe`; the client DLL is `build/out/VEYRA.dll`. Start VEYRA using the EXE, not the DLL.

## Security and project status

VEYRA uses the CEF bootstrap architecture for Chromium's Windows sandbox. Chromium is a security-critical dependency and must be kept current. The browser is early development software; do not treat this first build as production-ready until its sandbox, downloads, permissions, certificate handling and update process have been tested.

The Windows CI workflow is included, but this initial code has not yet been compiled or validated on a developer machine.

## Third-party notices

VEYRA uses Chromium and the Chromium Embedded Framework. Their notices are available through the CEF distribution and the browser's `about:license` page. See the CEF project documentation: https://chromiumembedded.github.io/cef/general_usage.html

## License and project use

VEYRA's original project code is provided under the **PolyForm Strict License 1.0.0**. This is a source-available, noncommercial license, not an open-source license. It permits use for noncommercial purposes, but does not grant permission to distribute the software or make modifications or derivative works. Read the complete terms in [LICENSE](LICENSE).

The repository is public so people can inspect the project. Public visibility does not grant additional copyright permissions. The official VEYRA releases are the supported builds. Contact the project owner for any use that is not clearly permitted by the license.

## Issues and contributions

Bug reports and feature suggestions are welcome through GitHub Issues. The PolyForm Strict license does not grant permission to create modified versions or patches. Any contribution that requires modifying the code needs separate, explicit written permission from the project owner before work begins. Pull requests are not automatically authorised contributions.
