# VEYRA architecture

VEYRA is a native Win32 browser host. CEF supplies the Chromium engine and its multi-process runtime. This is not Electron and does not use Edge WebView2.

- `src/main_win.cpp`: CEF bootstrap entry point, initialization and user profile path.
- `src/app_state.*`: tab lifecycle, navigation, address normalization and page layout.
- `src/browser_client.*`: CEF callbacks, tab metadata and keyboard shortcuts.
- `src/window.*`: native window, GDI+ browser chrome and address control.
- `ui/*`: local new-tab experience.
- `scripts/*`: CEF download verification, configuration and build steps.

Each tab owns a CEF browser window positioned below the native browser chrome. The tabs use the default CEF request context and share a profile stored in the current user's Local AppData.

This first milestone is Windows x64 only. Bookmarks, downloads, history, permissions UI, extensions, private windows, updater, crash reporting and import/export are future work.
