# VEYRA

**Browse beyond.**

VEYRA is an independent Windows browser with its own native interface and visual identity, powered by Chromium through the Chromium Embedded Framework (CEF).

It is not an Electron app and it does not use Microsoft Edge WebView2. VEYRA ships with its own CEF/Chromium runtime.

## Download VEYRA for Windows

VEYRA is currently in early development. Windows x64 test builds are published as GitHub Actions artifacts when the Windows build completes successfully.

**[Open the Windows builds](https://github.com/gamesphereinc-rgb/VEYRA/actions/workflows/windows-build.yml)**

To download a test build:

1. Open the newest run marked **Success** (green tick) for the Windows build workflow.
2. Scroll to **Artifacts** at the bottom of that run.
3. Download **VEYRA-windows-x64**.
4. Extract the downloaded ZIP to a folder.
5. Open **VEYRA.exe** from the extracted folder.

The ZIP contains the launcher, VEYRA application files, Chromium runtime files, and required browser resources. Keep the extracted files together: **VEYRA.exe will not work correctly if you copy it out on its own.** You do not need to install Visual Studio, CMake, or download CEF separately to run a packaged build.

GitHub Actions artifacts are temporary downloads and currently expire after 14 days. A missing Artifacts section means that run did not produce a downloadable package. Only use a successful build from this official repository.

These are unsigned development builds. Windows may show a SmartScreen or unknown-publisher warning. VEYRA is not production-ready; only test it if you understand that it is unfinished software.

## Current features

- Native Windows application window and custom-drawn VEYRA browser interface.
- Multiple independent Chromium tabs.
- Address bar with HTTPS navigation and DuckDuckGo search.
- Back, forward, reload/stop and home.
- Original VEYRA new-tab page with web search, quick links, date and clock.
- Persistent Chromium profile and cache in the current Windows user's Local AppData.
- CEF bootstrap architecture for the Windows sandbox.
- Keyboard shortcuts: Ctrl+T, Ctrl+W, Ctrl+L, Ctrl+R, F5, Alt+Left and Alt+Right.
- Pop-up requests are opened as VEYRA tabs.

## Still in development

Bookmarks, downloads UI, full history management, extensions, private windows, settings, profile sync, browser updates, and a polished first-run experience are not implemented yet. Some browser behaviours and security controls still need testing.

## Technology

C++20, Win32, Chromium Embedded Framework (CEF), CMake, Visual Studio 2022 and GDI+.

## Build from source (developers)

Running a packaged build does not require these tools. To compile VEYRA yourself, use Windows 10/11 x64 with Visual Studio 2022 and the **Desktop development with C++** workload, CMake 3.24+, and a Windows SDK.

Open PowerShell in the repository and run:

```powershell
powershell -ExecutionPolicy Bypass -File scripts/setup.ps1
powershell -ExecutionPolicy Bypass -File scripts/build.ps1 -Configuration Release
```

The setup script downloads the pinned official CEF 154 Windows x64 standard distribution, verifies its SHA-1 hash against the CEF build server's checksum, and extracts it under `third_party/cef`. CEF binaries are not committed to this repository.

The launcher is written to `build/out/VEYRA.exe`; the client DLL is `build/out/VEYRA.dll`. Start VEYRA using the EXE, not the DLL.

## Security and project status

VEYRA uses the CEF bootstrap architecture for Chromium's Windows sandbox. Chromium is a security-critical dependency and must be kept current. The sandbox, downloads, permissions, certificate handling, and update process require further validation before VEYRA can be considered production-ready.

## Third-party notices

VEYRA uses Chromium and the Chromium Embedded Framework. Their notices are available through the CEF distribution and the browser's `about:license` page. See the [CEF project documentation](https://chromiumembedded.github.io/cef/general_usage.html).

## License and project use

VEYRA's original project code is provided under the **PolyForm Strict License 1.0.0**. This is a source-available, noncommercial license, not an open-source license. It permits use for noncommercial purposes, but does not grant permission to distribute the software or make modifications or derivative works. Read the complete terms in [LICENSE](LICENSE).

The repository is public so people can inspect the project. Public visibility does not grant additional copyright permissions. Official VEYRA builds are provided by the project owner. Contact the project owner for any use that is not clearly permitted by the license.

## Issues and suggestions

Bug reports and feature suggestions are welcome through [GitHub Issues](https://github.com/gamesphereinc-rgb/VEYRA/issues). The PolyForm Strict license does not grant permission to create modified versions or patches. Any contribution that requires modifying the code needs separate, explicit written permission from the project owner before work begins. Pull requests are not automatically authorised contributions.
