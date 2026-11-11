#include "src/window.h"
#include <windows.h>
#include <windowsx.h>
#ifdef GetFirstChild
#undef GetFirstChild
#endif
#ifdef GetNextSibling
#undef GetNextSibling
#endif
#include <objbase.h>
#include <commctrl.h>
#include <dwmapi.h>
#include <gdiplus.h>
#include <shellapi.h>
#include <shlobj.h>
#include <algorithm>
#include <string>
#include <vector>
#include <memory>
#include "src/app_state.h"
#include "src/messages.h"
#include "src/svg_icons.h"

using namespace Gdiplus;
namespace {
constexpr wchar_t kClassName[] = L"VEYRA.NativeBrowserWindow";
constexpr int kChromeHeight = 108;
enum class Action {
  Back, Forward, Reload, Home, NewTab, ActivateTab, CloseTab, Bookmark, Menu,
  MinimizeWindow, ToggleMaximizeWindow, CloseWindow
};
constexpr UINT ID_MENU_BOOKMARK_THIS = 2006;
constexpr UINT ID_MENU_OPEN_DOWNLOADS = 2007;
constexpr UINT ID_MENU_CLEAR_HISTORY = 2008;
constexpr UINT ID_MENU_BOOKMARK_BASE = 3000;
constexpr UINT ID_MENU_HISTORY_BASE = 3200;
struct Hit { RECT rect; Action action; int tabId; };
std::vector<Hit> g_hits;
HWND g_address = nullptr;
HBRUSH g_editBrush = nullptr;
HFONT g_editFont = nullptr;
ULONG_PTR g_gdiplusToken = 0;
int g_hoveredWindowControl = -1;

RECT MakeRect(int x,int y,int w,int h){return RECT{x,y,x+w,y+h};}
bool Contains(const RECT& r, POINT p){return p.x>=r.left&&p.x<r.right&&p.y>=r.top&&p.y<r.bottom;}
void HitAdd(int x,int y,int w,int h,Action a,int id=-1){g_hits.push_back({MakeRect(x,y,w,h),a,id});}
void Round(Graphics& g, RectF r, float radius, Color color) {
  GraphicsPath p;
  p.AddArc(r.X,r.Y,radius*2,radius*2,180,90);
  p.AddArc(r.X+r.Width-radius*2,r.Y,radius*2,radius*2,270,90);
  p.AddArc(r.X+r.Width-radius*2,r.Y+r.Height-radius*2,radius*2,radius*2,0,90);
  p.AddArc(r.X,r.Y+r.Height-radius*2,radius*2,radius*2,90,90);
  p.CloseFigure(); SolidBrush b(color); g.FillPath(&b,&p);
}
void Label(Graphics& g,const std::wstring& s,float x,float y,float w,float h,float size,Color c,bool bold=false) {
  Font f(L"Segoe UI",size,bold?FontStyleBold:FontStyleRegular,UnitPixel);
  SolidBrush b(c); StringFormat fmt; fmt.SetLineAlignment(StringAlignmentCenter);
  fmt.SetTrimming(StringTrimmingEllipsisCharacter);
  g.DrawString(s.c_str(),(INT)s.size(),&f,RectF(x,y,w,h),&fmt,&b);
}
void VMark(Graphics& g,float x,float y) {
  PointF pts[]{PointF(x+2,y+2),PointF(x+15,y+26),PointF(x+28,y+2)};
  Pen halo(Color(42,100,232,212),8); halo.SetStartCap(LineCapRound); halo.SetEndCap(LineCapRound); g.DrawLines(&halo,pts,3);
  LinearGradientBrush brush(PointF(x,y),PointF(x+28,y+28),Color(255,114,236,203),Color(255,124,157,255));
  Pen mark(&brush,4); mark.SetStartCap(LineCapRound); mark.SetEndCap(LineCapRound); g.DrawLines(&mark,pts,3);
}
void Button(Graphics& g,int x,int y,const wchar_t* icon,bool enabled) {
  Round(g,RectF((REAL)x,(REAL)y,36.0f,36.0f),11.0f,
        enabled?Color(255,24,38,55):Color(100,20,31,45));
  SvgIcons::Draw(g,icon,RectF((REAL)x+9.0f,(REAL)y+9.0f,18.0f,18.0f),
                 enabled?255:105);
}
void WindowControl(Graphics& g, int x, int control, bool maximized) {
  const bool hovered = g_hoveredWindowControl == control;
  if (hovered) {
    const Color bg = control == 2 ? Color(255, 190, 55, 66) : Color(255, 35, 51, 70);
    SolidBrush fill(bg);
    g.FillRectangle(&fill, x, 0, 46, 44);
  }

  const wchar_t* icon = control == 0 ? L"window-minimize" :
      (control == 1 ? (maximized ? L"window-restore" : L"window-maximize") :
                      L"window-close");
  SvgIcons::Draw(g,icon,RectF((REAL)x+14.0f,11.0f,18.0f,18.0f),
                 hovered?255:220);

  if (control == 0) {
    HitAdd(x, 0, 46, 44, Action::MinimizeWindow);
  } else if (control == 1) {
    HitAdd(x, 0, 46, 44, Action::ToggleMaximizeWindow);
  } else {
    HitAdd(x, 0, 46, 44, Action::CloseWindow);
  }
}

void Paint(HWND hwnd,HDC hdc) {
  RECT cr{}; GetClientRect(hwnd,&cr); const int w=cr.right;
  Graphics g(hdc); g.SetSmoothingMode(SmoothingModeAntiAlias); g.SetTextRenderingHint(TextRenderingHintClearTypeGridFit);
  LinearGradientBrush bg(Point(0,0),Point(w,kChromeHeight),Color(255,9,15,27),Color(255,14,24,40));
  g.FillRectangle(&bg,0,0,w,kChromeHeight);
  SolidBrush line(Color(255,32,48,67)); g.FillRectangle(&line,0,kChromeHeight-1,w,1);

  VMark(g,18,8);
  Label(g,L"VEYRA",52,5,82,23,14,Color(255,239,247,255),true);
  Label(g,L"BROWSE BEYOND",53,25,92,14,7,Color(255,111,140,165),true);

  g_hits.clear();
  const auto& tabs=AppState::Get().Tabs();
  const int available=std::max(150,w-520);
  const int count=std::max(1,std::min(7,available/146));
  const int start=std::max(0,(int)tabs.size()-count);
  const int visible=(int)tabs.size()-start;
  const int tw=visible?std::min(210,std::max(132,available/visible)):176;
  int x=154;
  for(int i=start;i<(int)tabs.size();++i){
    const auto& t=tabs[(size_t)i];
    const auto* active=AppState::Get().ActiveTab();
    const bool selected=active&&active->id==t.id;
    RectF r((REAL)x,7,(REAL)(tw-5),34);
    if(selected) Round(g,r,10,Color(255,25,39,57));
    else Round(g,r,10,Color(68,35,51,70));
    if(t.loading){Pen p(Color(255,111,225,207),2);g.DrawArc(&p,(REAL)x+12.0f,17.0f,13.0f,13.0f,35.0f,245.0f);}
    else {SolidBrush dot(selected?Color(255,112,231,207):Color(255,91,119,146));g.FillEllipse(&dot,(REAL)x+16.0f,21.0f,6.0f,6.0f);}
    Label(g,t.title.empty()?L"New tab":t.title,(float)x+34,8,(float)tw-70,32,10,
          selected?Color(255,232,241,250):Color(255,154,174,193));
    SvgIcons::Draw(g,L"close",RectF((REAL)x+(REAL)tw-27.0f,15.0f,14.0f,14.0f),190);
    HitAdd(x,6,tw-5,36,Action::ActivateTab,t.id);
    HitAdd(x+tw-30,7,25,34,Action::CloseTab,t.id);
    x+=tw;
  }
  const int plus=std::min(w-46,x+4);
  Round(g,RectF((REAL)plus,10,28,28),9,Color(70,86,130,153));
  SvgIcons::Draw(g,L"plus",RectF((REAL)plus+6.0f,16.0f,16.0f,16.0f),235);
  HitAdd(plus,8,30,32,Action::NewTab);

  const auto* tab=AppState::Get().ActiveTab();
  Button(g,17,58,L"arrow-left",tab&&tab->can_go_back);
  Button(g,58,58,L"arrow-right",tab&&tab->can_go_forward);
  Button(g,99,58,tab&&tab->loading?L"stop":L"refresh",true);
  Button(g,140,58,L"home",true);
  HitAdd(17,57,34,36,Action::Back); HitAdd(58,57,34,36,Action::Forward);
  HitAdd(99,57,34,36,Action::Reload); HitAdd(140,57,34,36,Action::Home);

  const int left=184, right=std::max(left+180,w-112);
  Round(g,RectF((REAL)left,56,(REAL)(right-left),40),13,Color(255,20,31,47));
  Pen border(Color(255,42,61,81),1);
  GraphicsPath p; p.AddArc((REAL)left,56.0f,26.0f,26.0f,180.0f,90.0f);p.AddArc((REAL)right-26.0f,56.0f,26.0f,26.0f,270.0f,90.0f);
  p.AddArc((REAL)right-26.0f,70.0f,26.0f,26.0f,0.0f,90.0f);p.AddArc((REAL)left,70.0f,26.0f,26.0f,90.0f,90.0f);p.CloseFigure();g.DrawPath(&border,&p);
  SvgIcons::Draw(g,tab&&tab->url.rfind(L"https://",0)==0?L"lock":L"globe",
                 RectF(196.0f,64.0f,16.0f,16.0f),220);
  const bool saved = tab && AppState::Get().IsBookmarked(tab->url);
  Button(g,w-104,58,saved?L"bookmark-filled":L"bookmark",true);
  HitAdd(w-104,57,38,38,Action::Bookmark);
  Round(g,RectF((REAL)w-58.0f,58.0f,38.0f,36.0f),11.0f,Color(255,23,36,52));
  SvgIcons::Draw(g,L"more",RectF((REAL)w-49.0f,67.0f,20.0f,20.0f),225);
  HitAdd(w-58,57,38,38,Action::Menu);

  // VEYRA's own frameless window controls. No stock Windows caption buttons.
  WindowControl(g, w - 138, 0, IsZoomed(hwnd) != FALSE);
  WindowControl(g, w - 92, 1, IsZoomed(hwnd) != FALSE);
  WindowControl(g, w - 46, 2, IsZoomed(hwnd) != FALSE);
}
LRESULT CALLBACK AddressProc(HWND edit,UINT msg,WPARAM wp,LPARAM lp,UINT_PTR id,DWORD_PTR) {
  if(msg==WM_KEYDOWN&&wp==VK_RETURN){SendMessageW(GetParent(edit),WM_VEYRA_NAVIGATE,0,0);return 0;}
  if(msg==WM_KEYDOWN&&wp==VK_ESCAPE){
    if(auto* t=AppState::Get().ActiveTab()){
      auto url=t->url.find(L"newtab.html")!=std::wstring::npos?L"":t->url;
      SetWindowTextW(edit,url.c_str());
    }
    SetFocus(GetParent(edit));return 0;
  }
  if(msg==WM_NCDESTROY) RemoveWindowSubclass(edit,AddressProc,id);
  return DefSubclassProc(edit,msg,wp,lp);
}
void AppendSavedPages(HMENU menu, const std::vector<SavedPage>& pages,
                      UINT firstId, std::size_t limit) {
  if (pages.empty()) {
    AppendMenuW(menu, MF_STRING | MF_GRAYED, 0, L"Nothing here yet");
    return;
  }
  const auto count = std::min(pages.size(), limit);
  for (std::size_t i = 0; i < count; ++i) {
    std::wstring label = pages[i].title.empty() ? pages[i].url : pages[i].title;
    if (label.size() > 72) label.resize(69), label += L"...";
    AppendMenuW(menu, MF_STRING, firstId + static_cast<UINT>(i), label.c_str());
  }
}

void OpenDownloadsFolder(HWND hwnd) {
  PWSTR known = nullptr;
  if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Downloads, 0, nullptr, &known))) {
    ShellExecuteW(hwnd, L"open", known, nullptr, nullptr, SW_SHOWNORMAL);
    CoTaskMemFree(known);
  } else {
    ShellExecuteW(hwnd, L"open", L"shell:Downloads", nullptr, nullptr, SW_SHOWNORMAL);
  }
}

void ShowMenu(HWND hwnd) {
  HMENU menu = CreatePopupMenu();
  if (!menu) return;

  AppendMenuW(menu, MF_STRING, ID_MENU_BOOKMARK_THIS, L"Bookmark this page\tCtrl+D");

  HMENU bookmarks = CreatePopupMenu();
  AppendSavedPages(bookmarks, AppState::Get().Bookmarks(), ID_MENU_BOOKMARK_BASE, 100);
  AppendMenuW(menu, MF_POPUP, (UINT_PTR)bookmarks, L"Bookmarks");

  HMENU history = CreatePopupMenu();
  AppendSavedPages(history, AppState::Get().History(), ID_MENU_HISTORY_BASE, 200);
  AppendMenuW(history, MF_SEPARATOR, 0, nullptr);
  AppendMenuW(history, MF_STRING, ID_MENU_CLEAR_HISTORY, L"Clear browsing history");
  AppendMenuW(menu, MF_POPUP, (UINT_PTR)history, L"History");

  AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
  AppendMenuW(menu, MF_STRING, ID_MENU_OPEN_DOWNLOADS, L"Open downloads folder");
  AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
  AppendMenuW(menu, MF_STRING, ID_MENU_NEW_TAB, L"New tab\tCtrl+T");
  AppendMenuW(menu, MF_STRING, ID_MENU_RELOAD, L"Reload page\tCtrl+R");
  AppendMenuW(menu, MF_STRING, ID_MENU_CLOSE_TAB, L"Close tab\tCtrl+W");
  AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
  AppendMenuW(menu, MF_STRING, ID_MENU_ABOUT, L"About VEYRA");
  AppendMenuW(menu, MF_STRING, ID_MENU_EXIT, L"Exit");

  POINT p{};
  GetCursorPos(&p);
  SetForegroundWindow(hwnd);
  TrackPopupMenu(menu, TPM_RIGHTBUTTON | TPM_LEFTALIGN, p.x, p.y, 0, hwnd, nullptr);
  DestroyMenu(menu);
}
void Act(HWND hwnd,const Hit& hit) {
  auto& a=AppState::Get();
  switch(hit.action){
    case Action::Back:a.GoBack();break; case Action::Forward:a.GoForward();break;
    case Action::Reload: if(auto* t=const_cast<BrowserTab*>(a.ActiveTab());t&&t->loading)a.Stop();else a.Reload();break;
    case Action::Home:a.GoHome();break; case Action::NewTab:a.CreateTab();break;
    case Action::ActivateTab:a.ActivateTab(hit.tabId);break; case Action::CloseTab:a.CloseTab(hit.tabId);break;
    case Action::Bookmark:if(!a.AddBookmark()) MessageBeep(MB_ICONWARNING);break;
    case Action::MinimizeWindow:ShowWindow(hwnd, SW_MINIMIZE);break;
    case Action::ToggleMaximizeWindow:
      ShowWindow(hwnd, IsZoomed(hwnd) ? SW_RESTORE : SW_MAXIMIZE);
      break;
    case Action::CloseWindow:SendMessageW(hwnd, WM_CLOSE, 0, 0);break;
    case Action::Menu:ShowMenu(hwnd);break;
  }
}
LRESULT CALLBACK Proc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp) {
  switch(msg){
    case WM_NCCALCSIZE:
      // Use the full client area for VEYRA's custom title bar and resize frame.
      if (wp) return 0;
      break;
    case WM_NCHITTEST: {
      POINT screen{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
      RECT frame{};
      GetWindowRect(hwnd, &frame);
      if (!IsZoomed(hwnd)) {
        constexpr int border = 8;
        const bool left = screen.x < frame.left + border;
        const bool right = screen.x >= frame.right - border;
        const bool top = screen.y < frame.top + border;
        const bool bottom = screen.y >= frame.bottom - border;
        if (top && left) return HTTOPLEFT;
        if (top && right) return HTTOPRIGHT;
        if (bottom && left) return HTBOTTOMLEFT;
        if (bottom && right) return HTBOTTOMRIGHT;
        if (left) return HTLEFT;
        if (right) return HTRIGHT;
        if (top) return HTTOP;
        if (bottom) return HTBOTTOM;
      }

      POINT client = screen;
      ScreenToClient(hwnd, &client);
      for (auto it = g_hits.rbegin(); it != g_hits.rend(); ++it) {
        if (Contains(it->rect, client)) return HTCLIENT;
      }
      if (client.y >= 0 && client.y < 44) return HTCAPTION;
      return HTCLIENT;
    }
    case WM_CREATE:{
      g_editBrush=CreateSolidBrush(RGB(20,31,47));
      g_editFont=CreateFontW(-15,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH|FF_DONTCARE,L"Segoe UI");
      g_address=CreateWindowExW(0,L"EDIT",L"",WS_CHILD|WS_VISIBLE|ES_AUTOHSCROLL|ES_LEFT,
        218,64,420,25,hwnd,(HMENU)(INT_PTR)ID_ADDRESS_EDIT,GetModuleHandleW(nullptr),nullptr);
      if(g_address){
        SendMessageW(g_address,WM_SETFONT,(WPARAM)g_editFont,TRUE);
        SendMessageW(g_address,EM_SETCUEBANNER,TRUE,(LPARAM)L"Search or enter an address");
        SetWindowSubclass(g_address,AddressProc,1,0);AppState::Get().SetAddressEdit(g_address);
      }
      LayoutAddressEdit(hwnd);return 0;
    }
    case WM_SIZE:LayoutAddressEdit(hwnd);AppState::Get().ResizeTabs();InvalidateRect(hwnd,nullptr,FALSE);return 0;
    case WM_MOUSEMOVE: {
      TRACKMOUSEEVENT tracking{sizeof(tracking), TME_LEAVE, hwnd, 0};
      TrackMouseEvent(&tracking);
      POINT point{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
      int hovered = -1;
      for (auto it = g_hits.rbegin(); it != g_hits.rend(); ++it) {
        if (!Contains(it->rect, point)) continue;
        if (it->action == Action::MinimizeWindow) hovered = 0;
        else if (it->action == Action::ToggleMaximizeWindow) hovered = 1;
        else if (it->action == Action::CloseWindow) hovered = 2;
        break;
      }
      if (hovered != g_hoveredWindowControl) {
        g_hoveredWindowControl = hovered;
        RECT top{0, 0, 0, 44};
        GetClientRect(hwnd, &top);
        top.bottom = 44;
        InvalidateRect(hwnd, &top, FALSE);
      }
      return 0;
    }
    case WM_MOUSELEAVE:
      if (g_hoveredWindowControl != -1) {
        g_hoveredWindowControl = -1;
        RECT top{};
        GetClientRect(hwnd, &top);
        top.bottom = 44;
        InvalidateRect(hwnd, &top, FALSE);
      }
      return 0;
    case WM_PAINT:{
      PAINTSTRUCT ps{};
      HDC target=BeginPaint(hwnd,&ps);
      RECT client{};
      GetClientRect(hwnd,&client);
      const int width=client.right;
      const int height=std::min<LONG>(client.bottom,kChromeHeight);
      if(width>0&&height>0){
        HDC buffer=CreateCompatibleDC(target);
        HBITMAP surface=CreateCompatibleBitmap(target,width,height);
        if(buffer&&surface){
          HGDIOBJ previous=SelectObject(buffer,surface);
          Paint(hwnd,buffer);
          BitBlt(target,0,0,width,height,buffer,0,0,SRCCOPY);
          if(previous)SelectObject(buffer,previous);
        }else{
          Paint(hwnd,target);
        }
        if(surface)DeleteObject(surface);
        if(buffer)DeleteDC(buffer);
      }
      EndPaint(hwnd,&ps);
      return 0;
    }
    case WM_ERASEBKGND:return 1;
    case WM_CTLCOLOREDIT:SetTextColor((HDC)wp,RGB(231,241,250));SetBkColor((HDC)wp,RGB(20,31,47));return (LRESULT)g_editBrush;
    case WM_COMMAND: {
      const UINT command = LOWORD(wp);
      if (command >= ID_MENU_BOOKMARK_BASE &&
          command < ID_MENU_BOOKMARK_BASE + 100) {
        AppState::Get().OpenBookmark(command - ID_MENU_BOOKMARK_BASE);
        return 0;
      }
      if (command >= ID_MENU_HISTORY_BASE &&
          command < ID_MENU_HISTORY_BASE + 200) {
        AppState::Get().OpenHistory(command - ID_MENU_HISTORY_BASE);
        return 0;
      }
      switch(command) {
        case ID_MENU_BOOKMARK_THIS:
          if (!AppState::Get().AddBookmark()) MessageBeep(MB_ICONWARNING);
          InvalidateRect(hwnd, nullptr, FALSE);
          return 0;
        case ID_MENU_OPEN_DOWNLOADS:OpenDownloadsFolder(hwnd);return 0;
        case ID_MENU_CLEAR_HISTORY:AppState::Get().ClearHistory();return 0;
        case ID_MENU_NEW_TAB:AppState::Get().CreateTab();return 0;
        case ID_MENU_RELOAD:AppState::Get().Reload();return 0;
        case ID_MENU_CLOSE_TAB:AppState::Get().CloseActiveTab();return 0;
        case ID_MENU_ABOUT:MessageBoxW(hwnd,L"VEYRA 0.2\\nBrowse beyond.\\n\\nPowered by Chromium Embedded Framework.",L"About VEYRA",MB_OK|MB_ICONINFORMATION);return 0;
        case ID_MENU_EXIT:SendMessageW(hwnd,WM_CLOSE,0,0);return 0;
      }
      break;
    }
    case WM_LBUTTONUP:{
      POINT pt{GET_X_LPARAM(lp),GET_Y_LPARAM(lp)};
      for(auto it=g_hits.rbegin();it!=g_hits.rend();++it)if(Contains(it->rect,pt)){Act(hwnd,*it);return 0;}
      break;
    }
    case WM_VEYRA_NEW_TAB:AppState::Get().CreateTab();return 0;
    case WM_VEYRA_CLOSE_TAB:AppState::Get().CloseActiveTab();return 0;
    case WM_VEYRA_FOCUS_ADDRESS:AppState::Get().FocusAddress();return 0;
    case WM_VEYRA_BACK:AppState::Get().GoBack();return 0;
    case WM_VEYRA_FORWARD:AppState::Get().GoForward();return 0;
    case WM_VEYRA_RELOAD:AppState::Get().Reload();return 0;
    case WM_VEYRA_HOME:AppState::Get().GoHome();return 0;
    case WM_VEYRA_NAVIGATE:{
      if(g_address){
        int n=GetWindowTextLengthW(g_address);std::wstring text((size_t)n+1,L'\0');
        if(n>0)GetWindowTextW(g_address,text.data(),n+1);
        text.resize((size_t)n);
        AppState::Get().NavigateActive(text);AppState::Get().FocusBrowser();
      }return 0;
    }
    case WM_VEYRA_BOOKMARK:
      if (!AppState::Get().AddBookmark()) MessageBeep(MB_ICONWARNING);
      InvalidateRect(hwnd, nullptr, FALSE);
      return 0;
    case WM_VEYRA_CLOSE_READY:DestroyWindow(hwnd);return 0;
    case WM_CLOSE:AppState::Get().BeginShutdown();return 0;
    case WM_DESTROY:
      SvgIcons::Shutdown();
      if(g_gdiplusToken){GdiplusShutdown(g_gdiplusToken);g_gdiplusToken=0;}
      PostQuitMessage(0);
      return 0;
    case WM_GETMINMAXINFO: {
      auto* m = reinterpret_cast<MINMAXINFO*>(lp);
      m->ptMinTrackSize.x = 760;
      m->ptMinTrackSize.y = 520;
      MONITORINFO monitor{sizeof(monitor)};
      if (GetMonitorInfoW(MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST), &monitor)) {
        m->ptMaxPosition.x = monitor.rcWork.left - monitor.rcMonitor.left;
        m->ptMaxPosition.y = monitor.rcWork.top - monitor.rcMonitor.top;
        m->ptMaxSize.x = monitor.rcWork.right - monitor.rcWork.left;
        m->ptMaxSize.y = monitor.rcWork.bottom - monitor.rcWork.top;
      }
      return 0;
    }
  }
  return DefWindowProcW(hwnd,msg,wp,lp);
}
}

RECT GetBrowserContentRect(HWND hwnd) {
  RECT r{};GetClientRect(hwnd,&r);return RECT{0,kChromeHeight,r.right,std::max<LONG>(kChromeHeight,r.bottom)};
}
void LayoutAddressEdit(HWND hwnd) {
  if(!g_address)return;RECT r{};GetClientRect(hwnd,&r);
  SetWindowPos(g_address,nullptr,218,64,std::max(40,static_cast<int>(r.right)-330),25,
               SWP_NOZORDER|SWP_NOACTIVATE);
}
HWND CreateVeyraWindow(HINSTANCE instance,int showCommand) {
  INITCOMMONCONTROLSEX controls{sizeof(controls),ICC_STANDARD_CLASSES};InitCommonControlsEx(&controls);
  GdiplusStartupInput startup;GdiplusStartup(&g_gdiplusToken,&startup,nullptr);
  WNDCLASSEXW wc{};wc.cbSize=sizeof(wc);wc.style=CS_HREDRAW|CS_VREDRAW;wc.lpfnWndProc=Proc;
  wc.hInstance=instance;wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);wc.hIcon=LoadIconW(nullptr,IDI_APPLICATION);
  wc.hIconSm=wc.hIcon;wc.lpszClassName=kClassName;
  if(!RegisterClassExW(&wc)&&GetLastError()!=ERROR_CLASS_ALREADY_EXISTS)return nullptr;
  const DWORD windowStyle = WS_POPUP | WS_THICKFRAME | WS_MINIMIZEBOX |
                            WS_MAXIMIZEBOX | WS_SYSMENU;
  HWND hwnd=CreateWindowExW(WS_EX_APPWINDOW,kClassName,L"VEYRA — Browse beyond",
    windowStyle,CW_USEDEFAULT,CW_USEDEFAULT,1280,820,nullptr,nullptr,instance,nullptr);
  if(!hwnd)return nullptr;
  const BOOL dark=TRUE;DwmSetWindowAttribute(hwnd,20,&dark,sizeof(dark));
  const int corners=2;DwmSetWindowAttribute(hwnd,33,&corners,sizeof(corners));
  AppState::Get().SetWindow(hwnd);ShowWindow(hwnd,showCommand);UpdateWindow(hwnd);return hwnd;
}
