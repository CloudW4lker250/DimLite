#pragma once
#include <windowsx.h>
#include <algorithm>
#include <cmath>

// One-row OSD. Mouse tracking keeps it open during use, with no hover polling.
namespace BrightnessHud {
inline constexpr int BaseWidth=300, BaseHeight=64;
inline constexpr float TrackLeft=56.0f, TrackRight=224.0f, TrackY=32.0f;
inline HWND window=nullptr,controller=nullptr,returnFocus=nullptr;
inline UINT levelMessage=0,wheelMessage=0,drawingDpi=96;
inline HDC surface=nullptr;
inline HBITMAP bitmap=nullptr;
inline HGDIOBJ previousBitmap=nullptr;
inline void* pixels=nullptr;
inline int width=0,height=0,currentLevel=100;
inline BYTE alpha=255;
inline bool rendered=false,dragging=false,hovered=false,trackingMouse=false;
inline bool hideTimerActive=false,fading=false,currentLocked=false,currentDark=false;
inline ULONGLONG fadeStart=0,hideAfter=0;

inline void Initialize(HWND receiver,UINT setLevelMessage,UINT adjustMessage) {
    controller=receiver;levelMessage=setLevelMessage;wheelMessage=adjustMessage;
}
inline void FreeSurface() {
    if(surface&&previousBitmap)SelectObject(surface,previousBitmap);
    if(bitmap)DeleteObject(bitmap);if(surface)DeleteDC(surface);
    surface=nullptr;bitmap=nullptr;previousBitmap=nullptr;pixels=nullptr;width=height=0;
}
inline bool PrepareSurface() {
    const int w=MulDiv(BaseWidth,drawingDpi,96),h=MulDiv(BaseHeight,drawingDpi,96);
    if(surface&&width==w&&height==h)return true;
    FreeSurface();BITMAPINFO info={};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth=w;info.bmiHeader.biHeight=-h;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;
    surface=CreateCompatibleDC(nullptr);bitmap=CreateDIBSection(nullptr,&info,DIB_RGB_COLORS,&pixels,nullptr,0);
    if(!surface||!bitmap){FreeSurface();return false;}
    previousBitmap=SelectObject(surface,bitmap);width=w;height=h;return true;
}
inline bool Present() {
    if(!window||!surface)return false;
    RECT rect={};GetWindowRect(window,&rect);POINT destination={rect.left,rect.top},origin={};SIZE size={width,height};
    BLENDFUNCTION blend={AC_SRC_OVER,0,alpha,AC_SRC_ALPHA};
    return UpdateLayeredWindow(window,nullptr,&destination,&size,surface,&origin,0,&blend,ULW_ALPHA)!=0;
}
inline void UpdateTitle() {
    if(!window)return;
    wchar_t text[96];StringCchPrintfW(text,ARRAYSIZE(text),currentLocked ? L"DimLite — яркость %d%% (зафиксирована)" : L"DimLite — яркость %d%%",currentLevel);
    SetWindowTextW(window,text);
}
inline bool Render() {
    if(!PrepareSurface())return false;
    {
        Gdiplus::Bitmap buffer(width,height,width*4,PixelFormat32bppPARGB,static_cast<BYTE*>(pixels));
        Gdiplus::Graphics g(&buffer);g.Clear(Gdiplus::Color(0,0,0,0));
        const float scale=static_cast<float>(drawingDpi)/96.0f;g.ScaleTransform(scale,scale);
        g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);g.SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAliasGridFit);
        Gdiplus::GraphicsPath outline;
        outline.AddArc(0,0,16,16,180,90);outline.AddArc(BaseWidth-16,0,16,16,270,90);
        outline.AddArc(BaseWidth-16,BaseHeight-16,16,16,0,90);outline.AddArc(0,BaseHeight-16,16,16,90,90);outline.CloseFigure();
        Gdiplus::SolidBrush background(currentDark?Gdiplus::Color(248,32,32,35):Gdiplus::Color(248,249,249,251));
        Gdiplus::SolidBrush ink(currentDark?Gdiplus::Color(255,242,242,245):Gdiplus::Color(255,30,31,35));
        Gdiplus::SolidBrush track(currentDark?Gdiplus::Color(255,75,85,101):Gdiplus::Color(255,208,216,229));
        const Gdiplus::Color accent=currentLocked?(currentDark?Gdiplus::Color(255,169,175,186):Gdiplus::Color(255,104,112,125))
            :currentDark?Gdiplus::Color(255,132,188,255):Gdiplus::Color(255,30,100,204);
        g.FillPath(&background,&outline);
        Gdiplus::Pen icon(&ink,1.5f);icon.SetStartCap(Gdiplus::LineCapRound);icon.SetEndCap(Gdiplus::LineCapRound);
        if(currentLocked){g.DrawArc(&icon,22,21,8,14,180,180);g.DrawRectangle(&icon,19,28,14,13);}
        else{
            g.DrawEllipse(&icon,22,28,8,8);
            for(int i=0;i<8;++i){const float a=static_cast<float>(i)*3.14159265f/4.0f;
                g.DrawLine(&icon,26+std::cos(a)*7,32+std::sin(a)*7,26+std::cos(a)*10,32+std::sin(a)*10);}
        }
        Gdiplus::FontFamily family(L"Segoe UI");Gdiplus::Font number(&family,14,Gdiplus::FontStyleBold,Gdiplus::UnitPixel);
        Gdiplus::StringFormat format(Gdiplus::StringFormatFlagsNoWrap);format.SetLineAlignment(Gdiplus::StringAlignmentCenter);format.SetAlignment(Gdiplus::StringAlignmentFar);
        wchar_t text[12];StringCchPrintfW(text,ARRAYSIZE(text),L"%d%%",currentLevel);
        g.DrawString(text,-1,&number,Gdiplus::RectF(240,16,44,32),&format,&ink);
        const float fraction=static_cast<float>(currentLevel-5)/95.0f;
        const float thumb=TrackLeft+(TrackRight-TrackLeft)*std::clamp(fraction,0.0f,1.0f);
        Gdiplus::Pen rail(&track,4),fill(accent,4);rail.SetStartCap(Gdiplus::LineCapRound);rail.SetEndCap(Gdiplus::LineCapRound);
        fill.SetStartCap(Gdiplus::LineCapRound);fill.SetEndCap(Gdiplus::LineCapRound);
        g.DrawLine(&rail,TrackLeft,TrackY,TrackRight,TrackY);g.DrawLine(&fill,TrackLeft,TrackY,thumb,TrackY);
        Gdiplus::SolidBrush knob(accent);g.FillEllipse(&knob,thumb-6,TrackY-6,12.0f,12.0f);
    }
    rendered=Present();return rendered;
}
inline bool PointerInside() {
    if(!window||!IsWindowVisible(window))return false;
    POINT p;RECT rect;GetCursorPos(&p);GetWindowRect(window,&rect);
    return PtInRect(&rect,p)&&GetAncestor(WindowFromPoint(p),GA_ROOT)==window;
}
inline void TrackPointer() {
    if(!trackingMouse){TRACKMOUSEEVENT track={sizeof(track),TME_LEAVE,window,0};trackingMouse=TrackMouseEvent(&track)!=0;}
}
inline void StopHideTimers() {
    if(hideTimerActive)KillTimer(window,1);if(fading)KillTimer(window,2);hideTimerActive=fading=false;
}
inline void CancelHide() {
    StopHideTimers();
    if(alpha!=255){alpha=255;Present();}
}
inline void ScheduleHide(UINT delay=2000) {
    CancelHide();hideAfter=GetTickCount64()+delay;hideTimerActive=SetTimer(window,1,delay,nullptr)!=0;
}
inline void PointerActivity(UINT delay=2000) {
    hovered=PointerInside();
    if(dragging||hovered){CancelHide();TrackPointer();}else ScheduleHide(delay);
}
inline void Hide() {
    if(!window)return;
    StopHideTimers();dragging=hovered=trackingMouse=false;
    if(GetCapture()==window)ReleaseCapture();
    const bool hadFocus=GetForegroundWindow()==window;
    SetWindowPos(window,nullptr,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE|SWP_HIDEWINDOW);alpha=0;
    if(hadFocus&&returnFocus&&IsWindow(returnFocus)&&IsWindowVisible(returnFocus))SetForegroundWindow(returnFocus);
}
inline void Update(int level,bool locked,bool dark) {
    level=std::clamp(level,5,100);const bool changed=currentLevel!=level||currentLocked!=locked||currentDark!=dark;
    currentLevel=level;currentLocked=locked;currentDark=dark;
    if(locked&&dragging){dragging=false;if(GetCapture()==window)ReleaseCapture();PointerActivity();}
    if(changed&&window){UpdateTitle();if(IsWindowVisible(window))Render();}
}
inline Gdiplus::PointF Point(LPARAM l) {
    const float scale=96.0f/static_cast<float>(drawingDpi);
    return{static_cast<float>(GET_X_LPARAM(l))*scale,static_cast<float>(GET_Y_LPARAM(l))*scale};
}
inline bool OverSlider(Gdiplus::PointF p){return p.X>=TrackLeft-12&&p.X<=TrackRight+12&&p.Y>=10&&p.Y<=54;}
inline void RequestLevel(int level) {
    level=std::clamp(level,5,100);if(!currentLocked&&level!=currentLevel&&controller&&levelMessage)SendMessageW(controller,levelMessage,static_cast<WPARAM>(level),0);
}
inline void DragTo(float x) {
    const float fraction=std::clamp((x-TrackLeft)/(TrackRight-TrackLeft),0.0f,1.0f);
    RequestLevel(5+static_cast<int>(std::lround(fraction*95.0f)));
}
inline LRESULT CALLBACK Proc(HWND hwnd,UINT message,WPARAM w,LPARAM l) {
    switch(message){
    case WM_NCHITTEST:return HTCLIENT;
    case WM_MOUSEACTIVATE:{const HWND foreground=GetForegroundWindow();if(foreground!=hwnd)returnFocus=foreground;return MA_ACTIVATE;}
    case WM_LBUTTONDOWN:
        CancelHide();TrackPointer();SetFocus(hwnd);
        if(!currentLocked&&OverSlider(Point(l))){dragging=true;SetCapture(hwnd);DragTo(Point(l).X);}return 0;
    case WM_MOUSEMOVE:
        hovered=PointerInside();if(dragging&&GetCapture()==hwnd)DragTo(Point(l).X);
        if(dragging||hovered){CancelHide();TrackPointer();}else if(!hideTimerActive&&!fading)ScheduleHide(500);return 0;
    case WM_LBUTTONUP:
        if(dragging)DragTo(Point(l).X);dragging=false;if(GetCapture()==hwnd)ReleaseCapture();PointerActivity(500);return 0;
    case WM_CAPTURECHANGED:
    case WM_CANCELMODE:
        if(dragging){dragging=false;if(message==WM_CANCELMODE&&GetCapture()==hwnd)ReleaseCapture();PointerActivity(500);}return 0;
    case WM_MOUSELEAVE:trackingMouse=hovered=false;if(!dragging)ScheduleHide(500);return 0;
    case WM_MOUSEWHEEL:
        if(!currentLocked&&controller&&wheelMessage)SendMessageW(controller,wheelMessage,static_cast<WPARAM>(static_cast<INT_PTR>(GET_WHEEL_DELTA_WPARAM(w))),0);
        PointerActivity();return 0;
    case WM_KEYDOWN:
        if(w==VK_ESCAPE){Hide();return 0;}
        if((GetKeyState(VK_CONTROL)&0x8000)||(GetKeyState(VK_MENU)&0x8000))break;
        if(w==VK_LEFT||w==VK_DOWN)RequestLevel(currentLevel-1);else if(w==VK_RIGHT||w==VK_UP)RequestLevel(currentLevel+1);
        else if(w==VK_PRIOR)RequestLevel(currentLevel+5);else if(w==VK_NEXT)RequestLevel(currentLevel-5);
        else if(w==VK_HOME)RequestLevel(5);else if(w==VK_END)RequestLevel(100);PointerActivity();return 0;
    case WM_DPICHANGED:{drawingDpi=HIWORD(w);if(!PrepareSurface())return 0;const auto*rect=reinterpret_cast<RECT*>(l);
        SetWindowPos(hwnd,HWND_TOPMOST,rect->left,rect->top,width,height,SWP_NOACTIVATE);Render();return 0;}
    case WM_TIMER:
        if((w==1&&!hideTimerActive)||(w==2&&!fading)||(w!=1&&w!=2))return 0;
        if(dragging||PointerInside()){hovered=PointerInside();CancelHide();TrackPointer();return 0;}
        if(const ULONGLONG now=GetTickCount64();w==1&&now<hideAfter){ScheduleHide(static_cast<UINT>(hideAfter-now));return 0;}
        if(w==1){KillTimer(hwnd,1);hideTimerActive=false;fadeStart=GetTickCount64();fading=SetTimer(hwnd,2,16,nullptr)!=0;if(!fading)Hide();}
        else if(w==2){const ULONGLONG elapsed=GetTickCount64()-fadeStart;if(elapsed>=160)Hide();else{alpha=static_cast<BYTE>(255-elapsed*255/160);Present();}}return 0;
    case WM_CLOSE:Hide();return 0;
    }
    return DefWindowProcW(hwnd,message,w,l);
}
inline void Show(int level,bool locked,bool dark) {
    if(!window){WNDCLASSEXW klass={sizeof(klass)};klass.hInstance=GetModuleHandleW(nullptr);klass.lpfnWndProc=Proc;
        klass.lpszClassName=L"DimLite.Indicator.1";klass.hCursor=LoadCursorW(nullptr,IDC_ARROW);
        if(!RegisterClassExW(&klass)&&GetLastError()!=ERROR_CLASS_ALREADY_EXISTS)return;
        window=CreateWindowExW(WS_EX_LAYERED|WS_EX_TOOLWINDOW|WS_EX_TOPMOST,klass.lpszClassName,L"DimLite — яркость",WS_POPUP,0,0,1,1,nullptr,nullptr,klass.hInstance,nullptr);if(!window)return;
    }
    const bool visible=IsWindowVisible(window)!=0,changed=currentLevel!=level||currentLocked!=locked||currentDark!=dark;
    const UINT oldDpi=drawingDpi;const bool faded=alpha!=255;CancelHide();
    HWND foreground=GetForegroundWindow();if(foreground&&foreground!=window)returnFocus=foreground;
    drawingDpi=foreground?GetDpiForWindow(foreground):GetDpiForSystem();currentLevel=std::clamp(level,5,100);currentLocked=locked;currentDark=dark;
    if(!PrepareSurface())return;UpdateTitle();MONITORINFO monitor={sizeof(monitor)};
    GetMonitorInfoW(MonitorFromWindow(foreground,MONITOR_DEFAULTTONEAREST),&monitor);
    SetWindowPos(window,HWND_TOPMOST,(monitor.rcWork.left+monitor.rcWork.right-width)/2,
        monitor.rcWork.bottom-height-MulDiv(36,drawingDpi,96),width,height,SWP_NOACTIVATE);
    alpha=255;if(!visible||changed||faded||oldDpi!=drawingDpi)Render();
    if(rendered){SetWindowPos(window,HWND_TOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE|SWP_SHOWWINDOW);PointerActivity();}
}
inline void Destroy() {
    if(window)DestroyWindow(window);window=nullptr;controller=nullptr;dragging=hovered=trackingMouse=hideTimerActive=fading=false;FreeSurface();
}
}
