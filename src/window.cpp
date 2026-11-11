#include "src/window.h"
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <dwmapi.h>
#include <gdiplus.h>
#include <algorithm>
#include <string>
#include <vector>
#include "src/app_state.h"
#include "src/messages.h"

using namespace Gdiplus;
namespace {
constexpr wchar_t kClassName[] = L"VEYRA.NativeBrowserWindow";
constexpr int kChromeHeight = 116;
enum class Action { Back, Forward, Reload, Home, NewTab, ActivateTab, CloseTab, Menu };
struct Hit { RECT rect; Action action; int tabId; };
std::vector<Hit> g_hits;
HWND g_address = nullptr;
HBRUSH g_editBrush = nullptr;
HFONT g_editFont = nullptr;
ULONG_PTR g_gdiplusToken = 0;

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
void Button(Graphics& g,int x,int y,const wchar_t* glyph,bool enabled) {
  Label(g,glyph,(float)x,(float)y,34,34,19,enabled?Color(255,213,229,243):Color(255,88,105,124));
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
  const int available=std::max(150,w-380);
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
    if(t.loading){Pen p(Color(255,111,225,207),2);g.DrawArc(&p,(REAL)x+12,17,13,13,35,245);}
    else {SolidBrush dot(selected?Color(255,112,231,207):Color(255,91,119,146));g.FillEllipse(&dot,(REAL)x+16,21,6,6);}
    Label(g,t.title.empty()?L"New tab":t.title,(float)x+34,8,(float)tw-70,32,10,
          selected?Color(255,232,241,250):Color(255,154,174,193));
    Label(g,L"×",(float)x+tw-30,8,24,31,16,Color(255,143,161,180));
    HitAdd(x,6,tw-5,36,Action::ActivateTab,t.id);
    HitAdd(x+tw-30,7,25,34,Action::CloseTab,t.id);
    x+=tw;
  }
  const int plus=std::min(w-46,x+4);
  Round(g,RectF((REAL)plus,10,28,28),9,Color(70,86,130,153));
  Label(g,L"+",(float)plus,8,28,30,20,Color(255,204,224,237));
  HitAdd(plus,8,30,32,Action::NewTab);

  const auto* tab=AppState::Get().ActiveTab();
  Button(g,17,58,L"‹",tab&&tab->can_go_back);
  Button(g,58,58,L"›",tab&&tab->can_go_forward);
  Button(g,99,58,tab&&tab->loading?L"×":L"↻",true);
  Button(g,140,58,L"⌂",true);
  HitAdd(17,57,34,36,Action::Back); HitAdd(58,57,34,36,Action::Forward);
  HitAdd(99,57,34,36,Action::Reload); HitAdd(140,57,34,36,Action::Home);

  const int left=184, right=std::max(left+180,w-76);
  Round(g,RectF((REAL)left,56,(REAL)(right-left),40),13,Color(255,20,31,47));
  Pen border(Color(255,42,61,81),1);
  GraphicsPath p; p.AddArc((REAL)left,56,26,26,180,90);p.AddArc((REAL)right-26,56,26,26,270,90);
  p.AddArc((REAL)right-26,70,26,26,0,90);p.AddArc((REAL)left,70,26,26,90,90);p.CloseFigure();g.DrawPath(&border,&p);
  if(tab&&tab->url.rfind(L"https://",0)==0){
    Pen lock(Color(255,111,224,204),1.5f);g.DrawArc(&lock,198,65,10,11,180,180);
    Round(g,RectF(197,72,13,10),2,Color(255,111,224,204));
  }else{
    Pen globe(Color(255,126,154,178),1.3f);g.DrawEllipse(&globe,197,65,14,14);g.DrawLine(&globe,197,72,211,72);
  }
  Label(g,L"★",(float)right-36,60,27,32,13,Color(255,139,161,184));
  Round(g,RectF((REAL)w-58,58,38,36),11,Color(255,23,36,52));
  Label(g,L"···",(float)w-58,54,38,38,16,Color(255,194,211,227));
  HitAdd(w-58,57,38,38,Action::Menu);
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
void ShowMenu(HWND hwnd) {
  HMENU menu=CreatePopupMenu();
  AppendMenuW(menu,MF_STRING,ID_MENU_NEW_TAB,L"New tab\tCtrl+T");
  AppendMenuW(menu,MF_STRING,ID_MENU_RELOAD,L"Reload page\tCtrl+R");
  AppendMenuW(menu,MF_STRING,ID_MENU_CLOSE_TAB,L"Close tab\tCtrl+W");
  AppendMenuW(menu,MF_SEPARATOR,0,nullptr);
  AppendMenuW(menu,MF_STRING,ID_MENU_ABOUT,L"About VEYRA");
  AppendMenuW(menu,MF_STRING,ID_MENU_EXIT,L"Exit");
  POINT p{};GetCursorPos(&p);SetForegroundWindow(hwnd);
  TrackPopupMenu(menu,TPM_RIGHTBUTTON|TPM_LEFTALIGN,p.x,p.y,0,hwnd,nullptr);DestroyMenu(menu);
}
void Act(HWND hwnd,const Hit& hit) {
  auto& a=AppState::Get();
  switch(hit.action){
    case Action::Back:a.GoBack();break; case Action::Forward:a.GoForward();break;
    case Action::Reload: if(auto* t=const_cast<BrowserTab*>(a.ActiveTab());t&&t->loading)a.Stop();else a.Reload();break;
    case Action::Home:a.GoHome();break; case Action::NewTab:a.CreateTab();break;
    case Action::ActivateTab:a.ActivateTab(hit.tabId);break; case Action::CloseTab:a.CloseTab(hit.tabId);break;
    case Action::Menu:ShowMenu(hwnd);break;
  }
}
LRESULT CALLBACK Proc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp) {
  switch(msg){
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
    case WM_PAINT:{PAINTSTRUCT ps{};HDC dc=BeginPaint(hwnd,&ps);Paint(hwnd,dc);EndPaint(hwnd,&ps);return 0;}
    case WM_ERASEBKGND:return 1;
    case WM_CTLCOLOREDIT:SetTextColor((HDC)wp,RGB(231,241,250));SetBkColor((HDC)wp,RGB(20,31,47));return (LRESULT)g_editBrush;
    case WM_COMMAND:
      switch(LOWORD(wp)){
        case ID_MENU_NEW_TAB:AppState::Get().CreateTab();return 0;
        case ID_MENU_RELOAD:AppState::Get().Reload();return 0;
        case ID_MENU_CLOSE_TAB:AppState::Get().CloseActiveTab();return 0;
        case ID_MENU_ABOUT:MessageBoxW(hwnd,L"VEYRA 0.1\nBrowse beyond.\n\nPowered by Chromium Embedded Framework.",L"About VEYRA",MB_OK|MB_ICONINFORMATION);return 0;
        case ID_MENU_EXIT:SendMessageW(hwnd,WM_CLOSE,0,0);return 0;
      }break;
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
        int n=GetWindowTextLengthW(g_address);std::wstring text((size_t)n,L'\0');
        if(n>0)GetWindowTextW(g_address,text.data(),n+1);
        AppState::Get().NavigateActive(text);SetFocus(hwnd);
      }return 0;
    }
    case WM_VEYRA_CLOSE_READY:DestroyWindow(hwnd);return 0;
    case WM_CLOSE:AppState::Get().BeginShutdown();return 0;
    case WM_DESTROY:PostQuitMessage(0);return 0;
    case WM_GETMINMAXINFO:{auto* m=(MINMAXINFO*)lp;m->ptMinTrackSize.x=760;m->ptMinTrackSize.y=520;return 0;}
  }
  return DefWindowProcW(hwnd,msg,wp,lp);
}
}

RECT GetBrowserContentRect(HWND hwnd) {
  RECT r{};GetClientRect(hwnd,&r);return RECT{0,kChromeHeight,r.right,std::max(kChromeHeight,r.bottom)};
}
void LayoutAddressEdit(HWND hwnd) {
  if(!g_address)return;RECT r{};GetClientRect(hwnd,&r);
  MoveWindow(g_address,218,64,std::max(40,r.right-344),25,TRUE);
}
HWND CreateVeyraWindow(HINSTANCE instance,int showCommand) {
  INITCOMMONCONTROLSEX controls{sizeof(controls),ICC_STANDARD_CLASSES};InitCommonControlsEx(&controls);
  GdiplusStartupInput startup;GdiplusStartup(&g_gdiplusToken,&startup,nullptr);
  WNDCLASSEXW wc{};wc.cbSize=sizeof(wc);wc.style=CS_HREDRAW|CS_VREDRAW;wc.lpfnWndProc=Proc;
  wc.hInstance=instance;wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);wc.hIcon=LoadIconW(nullptr,IDI_APPLICATION);
  wc.hIconSm=wc.hIcon;wc.lpszClassName=kClassName;
  if(!RegisterClassExW(&wc)&&GetLastError()!=ERROR_CLASS_ALREADY_EXISTS)return nullptr;
  HWND hwnd=CreateWindowExW(WS_EX_APPWINDOW,kClassName,L"VEYRA — Browse beyond",
    WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,1280,820,nullptr,nullptr,instance,nullptr);
  if(!hwnd)return nullptr;
  const BOOL dark=TRUE;DwmSetWindowAttribute(hwnd,20,&dark,sizeof(dark));
  const int corners=2;DwmSetWindowAttribute(hwnd,33,&corners,sizeof(corners));
  AppState::Get().SetWindow(hwnd);ShowWindow(hwnd,showCommand);UpdateWindow(hwnd);return hwnd;
}
