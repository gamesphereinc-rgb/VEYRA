#include "veyra_winui_api.h"
#include "../src/messages.h"

#include <appmodel.h>
#include <windows.h>
#include <algorithm>
#include <functional>
#include <string>

#include <winrt/base.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Graphics.h>
#include <winrt/Windows.System.h>
#include <winrt/Windows.UI.h>
#include <winrt/Microsoft.UI.Interop.h>
#include <winrt/Microsoft.UI.Dispatching.h>
#include <winrt/Microsoft.UI.Xaml.h>
#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.Controls.Primitives.h>
#include <winrt/Microsoft.UI.Xaml.Hosting.h>
#include <winrt/Microsoft.UI.Xaml.Input.h>
#include <winrt/Microsoft.UI.Xaml.Media.h>

namespace {
using namespace winrt;
using namespace Windows::Foundation;
using namespace Windows::Graphics;
using namespace Windows::System;
using namespace Windows::UI;
using namespace Microsoft::UI;
using namespace Microsoft::UI::Dispatching;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Controls;
using namespace Microsoft::UI::Xaml::Hosting;
using namespace Microsoft::UI::Xaml::Input;
using namespace Microsoft::UI::Xaml::Media;

constexpr UINT32 kWindowsAppSdkMajorMinor = 0x00020006;

using BootstrapInitializeFn = HRESULT (WINAPI*)(UINT32, PCWSTR, PACKAGE_VERSION);
using BootstrapShutdownFn = void (WINAPI*)();

HWND g_parent = nullptr;
HMODULE g_bootstrapModule = nullptr;
BootstrapShutdownFn g_bootstrapShutdown = nullptr;
bool g_bootstrapInitialized = false;
bool g_comInitialized = false;
bool g_ownsDispatcher = false;
DispatcherQueueController g_dispatcherController{ nullptr };
DesktopWindowXamlSource g_source{ nullptr };
Grid g_root{ nullptr };
TabView g_tabs{ nullptr };
TextBox g_address{ nullptr };
bool g_updatingTabs = false;

HRESULT InitializeWindowsAppRuntime() {
  g_bootstrapModule = LoadLibraryW(L"Microsoft.WindowsAppRuntime.Bootstrap.dll");
  if (!g_bootstrapModule) {
    return HRESULT_FROM_WIN32(GetLastError());
  }

  auto initialize = reinterpret_cast<BootstrapInitializeFn>(
      GetProcAddress(g_bootstrapModule, "MddBootstrapInitialize"));
  g_bootstrapShutdown = reinterpret_cast<BootstrapShutdownFn>(
      GetProcAddress(g_bootstrapModule, "MddBootstrapShutdown"));

  if (!initialize || !g_bootstrapShutdown) {
    return HRESULT_FROM_WIN32(ERROR_PROC_NOT_FOUND);
  }

  PACKAGE_VERSION minimumVersion{};
  const HRESULT hr = initialize(kWindowsAppSdkMajorMinor, L"", minimumVersion);
  if (SUCCEEDED(hr)) {
    g_bootstrapInitialized = true;
  }
  return hr;
}

void ReleaseWindowsAppRuntime() noexcept {
  if (g_bootstrapInitialized && g_bootstrapShutdown) {
    g_bootstrapShutdown();
    g_bootstrapInitialized = false;
  }
  g_bootstrapShutdown = nullptr;
  if (g_bootstrapModule) {
    FreeLibrary(g_bootstrapModule);
    g_bootstrapModule = nullptr;
  }
}

void PostCommand(UINT message, WPARAM wParam = 0, LPARAM lParam = 0) {
  if (g_parent && IsWindow(g_parent)) {
    PostMessageW(g_parent, message, wParam, lParam);
  }
}

Button MakeIconButton(const wchar_t* glyph, std::function<void()> action) {
  Button button;
  FontIcon icon;
  icon.Glyph(glyph);
  icon.FontFamily(FontFamily(L"Segoe Fluent Icons"));
  icon.FontSize(16.0);
  button.Content(icon);
  button.Width(36.0);
  button.Height(36.0);
  button.Padding(Thickness{ 0.0, 0.0, 0.0, 0.0 });
  button.Click([action = std::move(action)](
      IInspectable const&, RoutedEventArgs const&) {
    action();
  });
  return button;
}

void AddMenuItem(MenuFlyout const& menu, const wchar_t* label, UINT command) {
  MenuFlyoutItem item;
  item.Text(label);
  item.Click([command](IInspectable const&, RoutedEventArgs const&) {
    PostCommand(WM_VEYRA_MENU_COMMAND, command);
  });
  menu.Items().Append(item);
}

void BuildChrome() {
  g_root = Grid{};
  g_root.RequestedTheme(ElementTheme::Dark);
  g_root.Background(SolidColorBrush(Color{ 224, 16, 25, 38 }));

  RowDefinition tabsRow;
  tabsRow.Height(GridLength{ 44.0, GridUnitType::Pixel });
  g_root.RowDefinitions().Append(tabsRow);

  RowDefinition toolbarRow;
  toolbarRow.Height(GridLength{ 64.0, GridUnitType::Pixel });
  g_root.RowDefinitions().Append(toolbarRow);

  Grid tabHeader;
  ColumnDefinition brandColumn;
  brandColumn.Width(GridLength{ 106.0, GridUnitType::Pixel });
  tabHeader.ColumnDefinitions().Append(brandColumn);

  ColumnDefinition tabsColumn;
  tabsColumn.Width(GridLength{ 1.0, GridUnitType::Star });
  tabHeader.ColumnDefinitions().Append(tabsColumn);

  StackPanel brand;
  brand.Orientation(Orientation::Vertical);
  brand.VerticalAlignment(VerticalAlignment::Center);
  brand.HorizontalAlignment(HorizontalAlignment::Center);
  brand.Spacing(0.0);

  TextBlock brandName;
  brandName.Text(L"VEYRA");
  brandName.FontSize(14.0);
  brandName.Foreground(SolidColorBrush(Color{ 255, 239, 247, 255 }));

  TextBlock brandTagline;
  brandTagline.Text(L"BROWSE BEYOND");
  brandTagline.FontSize(8.0);
  brandTagline.CharacterSpacing(900);
  brandTagline.Foreground(SolidColorBrush(Color{ 255, 132, 157, 180 }));

  brand.Children().Append(brandName);
  brand.Children().Append(brandTagline);
  brand.PointerPressed([](UIElement const&, PointerRoutedEventArgs const&) {
    if (g_parent) {
      ReleaseCapture();
      SendMessageW(g_parent, WM_NCLBUTTONDOWN, HTCAPTION, 0);
    }
  });
  Grid::SetColumn(brand, 0);
  tabHeader.Children().Append(brand);

  g_tabs = TabView{};
  g_tabs.IsAddTabButtonVisible(true);
  g_tabs.TabWidthMode(TabViewWidthMode::SizeToContent);
  g_tabs.AddTabButtonClick([](TabView const&, IInspectable const&) {
    PostCommand(WM_VEYRA_NEW_TAB);
  });
  g_tabs.SelectionChanged([](IInspectable const&, SelectionChangedEventArgs const&) {
    if (g_updatingTabs || !g_tabs.SelectedItem()) return;
    auto item = g_tabs.SelectedItem().try_as<TabViewItem>();
    if (!item || !item.Tag()) return;
    const int id = unbox_value<int>(item.Tag());
    PostCommand(WM_VEYRA_ACTIVATE_TAB, static_cast<WPARAM>(id));
  });
  g_tabs.TabCloseRequested([](
      TabView const&, TabViewTabCloseRequestedEventArgs const& args) {
    auto item = args.Item().try_as<TabViewItem>();
    if (!item || !item.Tag()) return;
    const int id = unbox_value<int>(item.Tag());
    PostCommand(WM_VEYRA_CLOSE_TAB, static_cast<WPARAM>(id));
  });
  Grid::SetColumn(g_tabs, 1);
  tabHeader.Children().Append(g_tabs);

  Grid::SetRow(tabHeader, 0);
  g_root.Children().Append(tabHeader);

  Grid toolbar;
  toolbar.VerticalAlignment(VerticalAlignment::Center);

  const double widths[] = { 42.0, 42.0, 42.0, 42.0, 1.0, 42.0, 42.0 };
  for (double width : widths) {
    ColumnDefinition column;
    column.Width(width == 1.0
        ? GridLength{ 1.0, GridUnitType::Star }
        : GridLength{ width, GridUnitType::Pixel });
    toolbar.ColumnDefinitions().Append(column);
  }

  auto back = MakeIconButton(L"\uE72B", [] {
    PostCommand(WM_VEYRA_BACK);
  });
  auto forward = MakeIconButton(L"\uE72A", [] {
    PostCommand(WM_VEYRA_FORWARD);
  });
  auto reload = MakeIconButton(L"\uE72C", [] {
    PostCommand(WM_VEYRA_RELOAD);
  });
  auto home = MakeIconButton(L"\uE80F", [] {
    PostCommand(WM_VEYRA_HOME);
  });

  Grid::SetColumn(back, 0);
  Grid::SetColumn(forward, 1);
  Grid::SetColumn(reload, 2);
  Grid::SetColumn(home, 3);
  toolbar.Children().Append(back);
  toolbar.Children().Append(forward);
  toolbar.Children().Append(reload);
  toolbar.Children().Append(home);

  g_address = TextBox{};
  g_address.PlaceholderText(L"Search or enter an address");
  g_address.FontSize(14.0);
  g_address.VerticalContentAlignment(VerticalAlignment::Center);
  g_address.MinWidth(180.0);
  g_address.KeyDown([](IInspectable const&, KeyRoutedEventArgs const& args) {
    if (args.Key() != VirtualKey::Enter) return;
    const std::wstring text = g_address.Text().c_str();
    if (g_parent) {
      SendMessageW(g_parent, WM_VEYRA_NAVIGATE_TEXT, 0,
                   reinterpret_cast<LPARAM>(text.c_str()));
    }
    args.Handled(true);
  });
  Grid::SetColumn(g_address, 4);
  toolbar.Children().Append(g_address);

  auto bookmark = MakeIconButton(L"\uE8A5", [] {
    PostCommand(WM_VEYRA_BOOKMARK);
  });
  Grid::SetColumn(bookmark, 5);
  toolbar.Children().Append(bookmark);

  auto menuButton = MakeIconButton(L"\uE712", [] {});
  MenuFlyout menu;
  AddMenuItem(menu, L"New tab", ID_MENU_NEW_TAB);
  AddMenuItem(menu, L"Bookmark this page", ID_MENU_BOOKMARK_THIS);
  AddMenuItem(menu, L"Downloads", 2007);
  AddMenuItem(menu, L"Settings", 2009);
  menu.Items().Append(MenuFlyoutSeparator{});
  AddMenuItem(menu, L"About VEYRA", ID_MENU_ABOUT);
  AddMenuItem(menu, L"Exit VEYRA", ID_MENU_EXIT);
  menuButton.Flyout(menu);
  Grid::SetColumn(menuButton, 6);
  toolbar.Children().Append(menuButton);

  Grid::SetRow(toolbar, 1);
  g_root.Children().Append(toolbar);
}

void CloseIsland() noexcept {
  try {
    if (g_source) {
      g_source.Close();
      g_source = nullptr;
    }
  } catch (...) {
  }

  g_root = nullptr;
  g_tabs = nullptr;
  g_address = nullptr;
  g_parent = nullptr;

  if (g_ownsDispatcher && g_dispatcherController) {
    try {
      g_dispatcherController.ShutdownQueue();
    } catch (...) {
    }
  }
  g_dispatcherController = nullptr;
  g_ownsDispatcher = false;

  ReleaseWindowsAppRuntime();

  if (g_comInitialized) {
    winrt::uninit_apartment();
    g_comInitialized = false;
  }
}
}  // namespace

HRESULT WINAPI VeyraWinUiInitialize(HWND parent) {
  if (!parent || !IsWindow(parent)) return E_INVALIDARG;
  if (g_source) return S_FALSE;

  HRESULT hr = InitializeWindowsAppRuntime();
  if (FAILED(hr)) {
    CloseIsland();
    return hr;
  }

  try {
    winrt::init_apartment(apartment_type::single_threaded);
    g_comInitialized = true;

    g_dispatcherController = DispatcherQueueController::CreateOnCurrentThread();
    g_ownsDispatcher = true;

    g_parent = parent;
    g_source = DesktopWindowXamlSource{};
    const auto windowId = Microsoft::UI::GetWindowIdFromWindow(parent);
    g_source.Initialize(windowId);

    BuildChrome();
    g_source.Content(g_root);

    try {
      g_source.SystemBackdrop(MicaBackdrop{});
    } catch (...) {
      // The solid Fluent dark surface remains as a safe fallback.
    }

    RECT client{};
    GetClientRect(parent, &client);
    g_source.SiteBridge().MoveAndResize(RectInt32{
        0, 0, std::max(0, static_cast<int>(client.right) - 138), 108 });
    g_source.SiteBridge().Show();
    return S_OK;
  } catch (winrt::hresult_error const& error) {
    hr = error.code();
  } catch (...) {
    hr = E_FAIL;
  }

  CloseIsland();
  return hr;
}

void WINAPI VeyraWinUiResize(int width, int height) {
  if (!g_source) return;
  try {
    g_source.SiteBridge().MoveAndResize(RectInt32{
        0, 0, std::max(0, width), std::max(0, height) });
  } catch (...) {
  }
}

void WINAPI VeyraWinUiSetTabs(const VEYRA_WINUI_TAB* tabs,
                              unsigned int count,
                              int activeId) {
  if (!g_tabs) return;
  try {
    g_updatingTabs = true;
    g_tabs.TabItems().Clear();

    TabViewItem selected{ nullptr };
    for (unsigned int i = 0; tabs && i < count; ++i) {
      TabViewItem item;
      item.Header(tabs[i].title && *tabs[i].title
          ? hstring(tabs[i].title)
          : hstring(L"New tab"));
      item.Tag(box_value(tabs[i].id));
      item.IsClosable(count > 1);
      g_tabs.TabItems().Append(item);
      if (tabs[i].id == activeId) selected = item;
    }

    if (selected) g_tabs.SelectedItem(selected);
    else if (g_tabs.TabItems().Size() > 0) g_tabs.SelectedIndex(0);
  } catch (...) {
  }
  g_updatingTabs = false;
}

void WINAPI VeyraWinUiSetAddress(const wchar_t* address) {
  if (!g_address) return;
  try {
    if (g_address.FocusState() == FocusState::Unfocused) {
      g_address.Text(address ? address : L"");
    }
  } catch (...) {
  }
}

void WINAPI VeyraWinUiFocusAddress() {
  if (!g_address) return;
  try {
    g_address.Focus(FocusState::Programmatic);
    g_address.SelectAll();
  } catch (...) {
  }
}

void WINAPI VeyraWinUiShutdown() {
  CloseIsland();
}
