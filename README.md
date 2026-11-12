# VEYRA

**Browse beyond.**

**Current development version: 0.4.1**

VEYRA is an independent Windows browser with its own native interface and visual identity, powered by Chromium through the Chromium Embedded Framework (CEF).

VEYRA is not an Electron app and does not use Microsoft Edge WebView2. Its Windows download includes the VEYRA application and the CEF/Chromium runtime it needs.

> **Early development software**
>
> VEYRA is an unfinished test build. Core browser features are still being developed, and security and compatibility testing are incomplete. Do not use it for sensitive browsing, passwords, payments, or other important activity.

## Download for Windows

**[Download VEYRA test builds](https://github.com/gamesphereinc-rgb/VEYRA/actions/workflows/windows-build.yml)**

The Windows build produces two packages:

- **VEYRA-Setup-x64** — a normal setup wizard. Download the artifact, extract its ZIP, and run the included `VEYRA-Setup-0.4.1-x64.exe`. Choose the install location and whether to create a desktop shortcut. VEYRA is installed for the current Windows user, without requiring administrator access.
- **VEYRA-windows-x64** — a portable ZIP for people who prefer not to install the browser. Extract the complete folder and run `VEYRA.exe` from inside it. The package also includes the original `bootstrap.exe`, which can be run with `--module=VEYRA` for a direct test of CEF's unmodified launcher.

The installer creates a Start Menu shortcut and registers VEYRA with Windows so it can be removed from **Settings → Apps → Installed apps**. It does not delete your browser profile during uninstall; profile data remains in your Local AppData folder.

To download a test installer, open the newest successful **Windows build** run on the `main` branch, scroll down to **Artifacts**, download **VEYRA-Setup-x64**, extract it, and run the setup EXE. Both packages include the full Chromium runtime. Keep the portable files together if you use that option.

Test-build artifacts expire after 14 days. These development builds are not code-signed, so Windows may show a Windows security warning. Keeping the CEF bootstrap unmodified is a packaging experiment, not a guarantee that Smart App Control will allow the application. Only download builds from this official repository.

## What's included so far

- Native Windows application window and custom VEYRA browser interface, with aligned icons and a VEYRA icon loaded from a sidecar file for the window and shortcuts. The CEF bootstrap executable is kept byte-for-byte unchanged.
- A Windows setup wizard with per-user installation, Start Menu shortcuts, optional desktop shortcut, and an uninstaller.
- Multiple Chromium tabs.
- Address bar with HTTPS navigation and DuckDuckGo search.
- Back, forward, reload, stop, and home controls.
- Redesigned VEYRA new-tab page with web search, editable local shortcuts, date, and clock.
- In-page Settings with scroll-aware section navigation, themes, search engine choice, and built-in or custom new-tab backgrounds.
- Compact new-tab layout on shorter desktop windows, keeping search and shortcuts in view.
- Custom SVG toolbar icons, a custom browser popup menu, and a double-buffered native chrome paint path.
- Release checking and a verified updater flow for published GitHub releases; stable releases are published from version tags.
- Persistent bookmarks and browsing history stored in the local VEYRA profile.
- Downloads saved to the current Windows user's Downloads folder, with duplicate filenames kept separate.
- Persistent browser profile and cache in the current Windows user's Local AppData.
- Chromium's CEF bootstrap architecture for the Windows sandbox.
- Pop-up requests open in VEYRA tabs.

Keyboard shortcuts: **Ctrl+T** new tab, **Ctrl+W** close tab, **Ctrl+L** focus address bar, **Ctrl+R** reload, **Ctrl+,** open Settings from any tab, **Escape** close Settings, **F5** reload, **Alt+Left** back, and **Alt+Right** forward.

## Still being developed

A dedicated downloads manager, extensions, private windows, profile sync, and a polished first-run experience are not implemented yet. The updater requires a published stable release with a checksum and still needs end-to-end testing. Bookmarks and history are early implementations and still need more testing. Some browser behaviours and security controls still need testing.

## Technology

VEYRA is built with C++20, Win32, Chromium Embedded Framework (CEF), and GDI+.

## Third-party notices

VEYRA uses Chromium and the Chromium Embedded Framework. Their notices are available through the CEF distribution and the browser's `about:license` page. See the [CEF project documentation](https://chromiumembedded.github.io/cef/general_usage.html).

## License and project use

VEYRA's original project code is provided under the **PolyForm Strict License 1.0.0**. This is a source-available, noncommercial license, not an open-source license. It does not grant permission to distribute the software or make modified versions. Read the complete terms in [LICENSE](LICENSE).

The repository is public so people can inspect the project. Public visibility does not grant additional copyright permissions. Official VEYRA builds are provided by the project owner.

## Security

Please read the [security policy](SECURITY.md) before reporting a suspected vulnerability. Do not publish security details in public issues. Private vulnerability reporting should be used when enabled.

## Bugs and suggestions

Found a non-security bug or have an idea? Open an [issue](https://github.com/gamesphereinc-rgb/VEYRA/issues). The licence does not automatically authorise code modifications or pull-request contributions; please contact the project owner for permission before proposing code changes.
