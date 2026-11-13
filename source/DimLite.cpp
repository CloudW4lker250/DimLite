// DimLite 1.0 - native, event-driven desktop dimming with no network access.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <shellapi.h>
#include <shlobj.h>
#include <strsafe.h>
#include <magnification.h>
#include <dwmapi.h>
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>
#include <gdiplus.h>
#include "BrightnessHud.h"
#include "AppProfiles.h"

constexpr wchar_t MainClass[] = L"DimLite.Controller.1";
constexpr wchar_t FlyoutClass[] = L"DimLite.Flyout.1";
constexpr wchar_t MenuClass[] = L"DimLite.Menu.1";
constexpr wchar_t VersionText[] = L"DimLite 1.0.0";
constexpr wchar_t RunKey[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
constexpr UINT TrayMessage = WM_APP + 1, SetLevelMessage = WM_APP + 2;
constexpr UINT ShowMessage = WM_APP + 3, HideMessage = WM_APP + 4, QuitMessage = WM_APP + 5;
constexpr UINT FlyoutMessage = WM_APP + 7, ThemeMessage = WM_APP + 8;
constexpr UINT MenuMessage = WM_APP + 9;
constexpr UINT TrayWheelMessage = WM_APP + 10, RefreshTrayBoundsMessage = WM_APP + 11;
constexpr UINT ResetOnlyMessage = WM_APP + 12, IconModeMessage = WM_APP + 13;
constexpr UINT LockMessage = WM_APP + 14, ProfilesMessage = WM_APP + 15, ForegroundAppMessage = WM_APP + 16;
constexpr UINT EmergencyResetMessage = WM_APP + 17;
constexpr ULONG_PTR StatusSignature = 0x44494D31;
constexpr int SliderId = 101, StartupId = 102, ExitId = 104;
constexpr int NightId = 120, EveningId = 121, ResetId = 122, OpenId = 123, ThemeId = 130;
constexpr int AutoThemeId = 131, PercentageIconId = 132;
constexpr int LockId = 133, ProfilesId = 134, AppProfileId = 135;
constexpr int SettingsId = 136, BackId = 137, LightThemeId = 138, DarkThemeId = 139, VersionId = 140;
constexpr UINT_PTR SaveTimer = 1, DisplayTimer = 2, FlyoutLeaveTimer = 3, FinishTrayTimer = 4;
constexpr UINT_PTR TransitionTimer = 5, TrayUpdateTimer = 6;

struct Control { HWND window; int x, y, w, h; };
struct RecoveryState { MAGCOLOREFFECT original; volatile LONG armed; };
HINSTANCE instance;
HWND mainWindow, slider, valueLabel, shortcutLabel, startupBox;
HWND flyout, flyoutSlider, flyoutVerticalSlider, flyoutValue, tooltips, menuPanel;
HWND settingsHeading;
bool settingsVisible = false;
HICON appIcon, trayIcon, windowIcon, taskbarIcon;
HBRUSH backgroundBrush;
HFONT bodyFont, headingFont, valueFont;
HFONT flyoutBodyFont, flyoutHeadingFont, flyoutValueFont, stripValueFont;
UINT taskbarCreated, dpi = 96, flyoutDpi = 96;
int brightness = 100;
int manualBrightness = 100;
float appliedBrightness = 100.0f;
bool transitionActive = false, brightnessLocked = false, profilesEnabled = true;
ULONGLONG lastTransitionTick = 0, lastTargetChange = 0;
HWINEVENTHOOK foregroundHook = nullptr;
std::wstring activeAppPath, activeAppName;
bool systemTrayDark = false, trayUpdatePending = false;
ULONGLONG lastTrayUpdate = 0;
bool darkMode = false, saving = false, magInitialized = false, originalCaptured = false, hotkeyRegistered = false, mainPresented = false;
bool trayMinimizeRequested = false;
enum class ThemeMode { Automatic = 0, Light = 1, Dark = 2 };
ThemeMode themeMode = ThemeMode::Automatic;
bool percentageIcon = true, adjustHotkeysRegistered = false;
int cachedTrayLevel = -1, cachedTrayMode = -1, wheelRemainder = 0;
bool cachedTrayDark = false;
UINT adjustHotkeyUp = VK_UP, adjustHotkeyDown = VK_DOWN;
UINT adjustHotkeyModifiers = MOD_CONTROL | MOD_ALT;
wchar_t adjustHotkeyText[96] = L"Ctrl+Alt+↑ / ↓ — яркость ±5%";
SRWLOCK trayBoundsLock = SRWLOCK_INIT;
RECT trayMouseBounds = {};
HWND trayMouseRoot = nullptr, confirmedTrayRoot = nullptr;
bool trayBoundsValid = false;
HANDLE mouseThread = nullptr, mouseReady = nullptr;
DWORD mouseThreadId = 0;
volatile LONG mouseHookActive = 0, nativeWheelEvents = 0, nativeMiddleEvents = 0;
ULONGLONG lastTrayBoundsRefresh = 0;
ULONGLONG lastFlyoutDismissal = 0;
ULONGLONG lastTraySelect = 0, ignoreTraySelectUntil = 0;
POINT lastTraySelectPoint = {};
std::wstring settingsPath;
std::vector<Control> controls, flyoutControls, settingsControls;
MAGCOLOREFFECT originalEffect = {};
HANDLE recoveryMapping, recoveryStop, guardProcess;
RecoveryState* recoveryState;
ULONG_PTR graphicsToken;
constexpr int PanelWidth = 380, PanelHeight = 206, FlyoutWidth = 340, FlyoutHeight = 168;
constexpr int SettingsHeight = 426;
constexpr int StripWidth = 64, StripHeight = 220;
bool activeStrip = false;
bool lastFadeAnimated = false;
DWORD lastFadeDurationMs = 0;
constexpr wchar_t HoverProperty[] = L"DimLite.Hover";
LRESULT CALLBACK ButtonProc(HWND, UINT, WPARAM, LPARAM, UINT_PTR, DWORD_PTR);
LRESULT CALLBACK CheckboxProc(HWND, UINT, WPARAM, LPARAM, UINT_PTR, DWORD_PTR);
LRESULT PaintContextButton(const DRAWITEMSTRUCT*);
void CenterMainWindow();
void KeepFlyoutOpen();
void CheckFlyoutMouseLeave();
void HideMenuPanel();
bool ApplyLevel(int requested, bool force = false, bool fromProfile = false);
void UpdateMenuAppearance();
void UpdateLockControls();
void SetBrightnessLock(bool locked, bool indicate = true);
void ResetBrightness();
void SetProfilesEnabled(bool enabled);
void TrackForegroundApp(HWND window, bool apply = true);
void SyncSettings();
constexpr int MenuWidth = 196, MenuHeight = 156;
UINT menuDpi = 96;
int menuPixelWidth = MenuWidth;
HFONT menuFont;
HBRUSH menuBackgroundBrush;
bool menuDark = false, menuHighContrast = false, menuKeyboard = false;
std::vector<Control> menuControls;

int Scale(int n) { return MulDiv(n, static_cast<int>(dpi), 96); }
int FlyScale(int n) { return MulDiv(n, static_cast<int>(flyoutDpi), 96); }
COLORREF Background() { return darkMode ? RGB(32, 32, 35) : RGB(249, 249, 251); }
COLORREF Foreground() { return darkMode ? RGB(242, 242, 245) : RGB(30, 31, 35); }
COLORREF Accent() { return darkMode ? RGB(132, 188, 255) : RGB(30, 100, 204); }
COLORREF Divider() { return darkMode ? RGB(55, 55, 60) : RGB(225, 226, 231); }
Gdiplus::Color Color(COLORREF color) { return Gdiplus::Color(255, GetRValue(color), GetGValue(color), GetBValue(color)); }

bool SystemMenuDark() {
    DWORD light = 1, bytes = sizeof(light);
    RegGetValueW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
        L"SystemUsesLightTheme", RRF_RT_REG_DWORD, nullptr, &light, &bytes);
    return light == 0;
}

bool SystemAppsDark() {
    DWORD light = 1, bytes = sizeof(light);
    RegGetValueW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
        L"AppsUseLightTheme", RRF_RT_REG_DWORD, nullptr, &light, &bytes);
    return light == 0;
}

HICON MakePercentageIcon(bool onDark) {
    const int size = std::max(16, GetSystemMetricsForDpi(SM_CXSMICON, GetDpiForSystem()));
    constexpr int supersample = 4;
    wchar_t text[8]; StringCchPrintfW(text, ARRAYSIZE(text), L"%d", brightness);
    Gdiplus::FontFamily family(L"Segoe UI");
    Gdiplus::StringFormat format(Gdiplus::StringFormat::GenericTypographic());
    Gdiplus::GraphicsPath glyphs;
    if (glyphs.AddString(text, -1, &family, Gdiplus::FontStyleBold, 100.0f,
        Gdiplus::PointF(0, 0), &format) != Gdiplus::Ok) return nullptr;
    Gdiplus::RectF bounds;
    glyphs.GetBounds(&bounds);
    if (bounds.Width <= 0 || bounds.Height <= 0) return nullptr;
    const float targetHeight = static_cast<float>(size) * 0.84f;
    const float scaleY = targetHeight / bounds.Height;
    const float scaleX = std::min(scaleY, static_cast<float>(size - 1) / bounds.Width);
    const float x = (static_cast<float>(size) - bounds.Width * scaleX) / 2.0f - bounds.X * scaleX;
    const float y = (static_cast<float>(size) - bounds.Height * scaleY) / 2.0f - bounds.Y * scaleY;
    Gdiplus::Matrix transform(scaleX * supersample, 0, 0, scaleY * supersample, x * supersample, y * supersample);
    glyphs.Transform(&transform);
    Gdiplus::Bitmap large(size * supersample, size * supersample, PixelFormat32bppPARGB);
    {
        Gdiplus::Graphics graphics(&large);
        graphics.Clear(Gdiplus::Color(0, 0, 0, 0));
        graphics.SetSmoothingMode(Gdiplus::SmoothingModeHighQuality);
        graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);
        Gdiplus::SolidBrush ink(onDark ? Gdiplus::Color(255,255,255,255) : Gdiplus::Color(255,20,20,20));
        graphics.FillPath(&ink, &glyphs);
    }
    Gdiplus::Bitmap bitmap(size, size, PixelFormat32bppPARGB);
    {
        Gdiplus::Graphics graphics(&bitmap);
        graphics.Clear(Gdiplus::Color(0, 0, 0, 0));
        graphics.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
        graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);
        Gdiplus::ImageAttributes edges; edges.SetWrapMode(Gdiplus::WrapModeTileFlipXY);
        graphics.DrawImage(&large, Gdiplus::Rect(0,0,size,size), 0,0,size * supersample,size * supersample,
            Gdiplus::UnitPixel, &edges);
    }
    HICON icon = nullptr;
    return bitmap.GetHICON(&icon) == Gdiplus::Ok ? icon : nullptr;
}

void RefreshTrayIcon() {
    const bool onDark = systemTrayDark;
    HICON updated = percentageIcon ? MakePercentageIcon(onDark) : nullptr;
    if (!updated) updated = static_cast<HICON>(LoadImageW(instance,
        MAKEINTRESOURCEW(onDark ? 2 : 3), IMAGE_ICON, 0, 0, LR_DEFAULTSIZE));
    if (updated) {
        if (trayIcon) DestroyIcon(trayIcon);
        trayIcon = updated; cachedTrayLevel = brightness;
        cachedTrayMode = percentageIcon ? 1 : 0; cachedTrayDark = onDark;
    }
}

void RefreshWindowIcon() {
    if (!mainWindow) return;
    HICON updated = static_cast<HICON>(LoadImageW(instance,
        MAKEINTRESOURCEW(darkMode ? 2 : 3), IMAGE_ICON, 0, 0, LR_DEFAULTSIZE));
    if (updated) {
        SendMessageW(mainWindow, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(updated));
        if (windowIcon) DestroyIcon(windowIcon);
        windowIcon = updated;
    }
    HICON taskbarUpdated = static_cast<HICON>(LoadImageW(instance,
        MAKEINTRESOURCEW(systemTrayDark ? 2 : 3), IMAGE_ICON, 0, 0, LR_DEFAULTSIZE));
    if (taskbarUpdated) {
        SendMessageW(mainWindow, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(taskbarUpdated));
        if (taskbarIcon) DestroyIcon(taskbarIcon);
        taskbarIcon = taskbarUpdated;
    }
}

std::wstring RecoveryName(DWORD pid, const wchar_t* suffix) {
    return L"Local\\DimLite.Recovery." + std::to_wstring(pid) + suffix;
}

// The recovery process blocks on kernel handles, using no polling or timers.
// It restores the original desktop transform even if the UI process is killed.
int RunRecoveryGuard(DWORD parentPid) {
    HANDLE parent = OpenProcess(SYNCHRONIZE, FALSE, parentPid);
    HANDLE mapping = OpenFileMappingW(FILE_MAP_READ | FILE_MAP_WRITE, FALSE,
                                      RecoveryName(parentPid, L".state").c_str());
    HANDLE ready = OpenEventW(EVENT_MODIFY_STATE, FALSE, RecoveryName(parentPid, L".ready").c_str());
    HANDLE stop = OpenEventW(SYNCHRONIZE, FALSE, RecoveryName(parentPid, L".stop").c_str());
    auto* state = mapping ? static_cast<RecoveryState*>(MapViewOfFile(mapping,
        FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, sizeof(RecoveryState))) : nullptr;
    int result = 1;
    if (parent && state && ready && stop) {
        const MAGCOLOREFFECT baseline = state->original;
        SetEvent(ready);
        HANDLE waits[] = {parent, stop};
        if (WaitForMultipleObjects(2, waits, FALSE, INFINITE) != WAIT_FAILED) {
            result = 0;
            if (InterlockedCompareExchange(&state->armed, 0, 0) && MagInitialize()) {
                MAGCOLOREFFECT restore = baseline;
                result = MagSetFullscreenColorEffect(&restore) ? 0 : 2;
                MagUninitialize();
            }
        }
    }
    if (state) UnmapViewOfFile(state);
    for (HANDLE handle : {parent, mapping, ready, stop}) if (handle) CloseHandle(handle);
    return result;
}

bool StartRecoveryGuard() {
    const DWORD pid = GetCurrentProcessId();
    recoveryMapping = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE,
        0, sizeof(RecoveryState), RecoveryName(pid, L".state").c_str());
    if (!recoveryMapping) return false;
    recoveryState = static_cast<RecoveryState*>(MapViewOfFile(recoveryMapping,
        FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, sizeof(RecoveryState)));
    if (!recoveryState) return false;
    recoveryState->original = originalEffect;
    recoveryState->armed = 0;
    HANDLE ready = CreateEventW(nullptr, TRUE, FALSE, RecoveryName(pid, L".ready").c_str());
    recoveryStop = CreateEventW(nullptr, TRUE, FALSE, RecoveryName(pid, L".stop").c_str());
    if (!ready || !recoveryStop) { if (ready) CloseHandle(ready); return false; }
    wchar_t executable[32768] = {};
    GetModuleFileNameW(nullptr, executable, ARRAYSIZE(executable));
    std::wstring command = L"\"" + std::wstring(executable) + L"\" --guard=" + std::to_wstring(pid);
    STARTUPINFOW startup = {};
    startup.cb = sizeof(startup); startup.dwFlags = STARTF_USESHOWWINDOW; startup.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION child = {};
    const BOOL started = CreateProcessW(executable, command.data(), nullptr, nullptr, FALSE,
        CREATE_NO_WINDOW, nullptr, nullptr, &startup, &child);
    bool ok = false;
    if (started) {
        CloseHandle(child.hThread);
        guardProcess = child.hProcess;
        HANDLE waits[] = {ready, guardProcess};
        ok = WaitForMultipleObjects(2, waits, FALSE, 5000) == WAIT_OBJECT_0;
    }
    CloseHandle(ready);
    return ok;
}

void CloseRecovery() {
    if (recoveryStop) SetEvent(recoveryStop);
    if (guardProcess) WaitForSingleObject(guardProcess, 1500);
    if (recoveryState) UnmapViewOfFile(recoveryState);
    for (HANDLE handle : {guardProcess, recoveryStop, recoveryMapping}) if (handle) CloseHandle(handle);
    recoveryState = nullptr; guardProcess = recoveryStop = recoveryMapping = nullptr;
}

MAGCOLOREFFECT EffectAt(float level) {
    MAGCOLOREFFECT effect = originalEffect;
    // Scale the RGB output columns, preserving any original color transform.
    for (int row = 0; row < 5; ++row)
        for (int column = 0; column < 3; ++column)
            effect.transform[row][column] *= level / 100.0f;
    return effect;
}

void RestoreDesktop() {
    if (mainWindow) KillTimer(mainWindow, TransitionTimer);
    transitionActive = false;
    if (magInitialized) {
        if (originalCaptured && MagSetFullscreenColorEffect(&originalEffect) && recoveryState)
            InterlockedExchange(&recoveryState->armed, 0);
        MagUninitialize(); magInitialized = false;
    }
    CloseRecovery();
}

void SaveSettings() {
    if (DimProfiles::dirty) DimProfiles::Save();
    if (!saving) return;
    // The INI APIs cache reads; write only keys whose value actually changed.
    const auto save = [](const wchar_t* section, const wchar_t* key, int value) {
        wchar_t text[16], old[16];
        StringCchPrintfW(text, ARRAYSIZE(text), L"%d", value);
        GetPrivateProfileStringW(section, key, L"", old, ARRAYSIZE(old), settingsPath.c_str());
        return lstrcmpW(text, old) == 0 || WritePrivateProfileStringW(section, key, text, settingsPath.c_str()) != 0;
    };
    bool ok = save(L"Display", L"Brightness", brightness);
    ok &= save(L"Display", L"BaseBrightness", manualBrightness);
    ok &= save(L"Appearance", L"Dark", darkMode ? 1 : 0);
    ok &= save(L"Appearance", L"ThemeMode", static_cast<int>(themeMode));
    ok &= save(L"Appearance", L"PercentageIcon", percentageIcon ? 1 : 0);
    ok &= save(L"Behavior", L"Locked", brightnessLocked ? 1 : 0);
    ok &= save(L"Behavior", L"Profiles", profilesEnabled ? 1 : 0);
    if (ok) saving = false;
}

bool StartupEnabled() {
    wchar_t command[32768] = {}, executable[32768] = {};
    DWORD bytes = sizeof(command);
    if (RegGetValueW(HKEY_CURRENT_USER, RunKey, L"DimLite", RRF_RT_REG_SZ,
                    nullptr, command, &bytes) != ERROR_SUCCESS) return false;
    GetModuleFileNameW(nullptr, executable, ARRAYSIZE(executable));
    const std::wstring expected = L"\"" + std::wstring(executable) + L"\" --tray";
    return _wcsicmp(command, expected.c_str()) == 0;
}

bool SetStartup(bool enabled) {
    HKEY key;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, RunKey, 0, nullptr, 0, KEY_SET_VALUE,
                        nullptr, &key, nullptr) != ERROR_SUCCESS) return false;
    LSTATUS result;
    if (enabled) {
        wchar_t executable[32768] = {};
        GetModuleFileNameW(nullptr, executable, ARRAYSIZE(executable));
        const std::wstring command = L"\"" + std::wstring(executable) + L"\" --tray";
        result = RegSetValueExW(key, L"DimLite", 0, REG_SZ,
            reinterpret_cast<const BYTE*>(command.c_str()),
            static_cast<DWORD>((command.size() + 1) * sizeof(wchar_t)));
    } else {
        result = RegDeleteValueW(key, L"DimLite");
        if (result == ERROR_FILE_NOT_FOUND) result = ERROR_SUCCESS;
    }
    RegCloseKey(key);
    return result == ERROR_SUCCESS;
}

void UpdateTray(bool add = false) {
    // Coalesce slider bursts. Explorer/icon rendering stays outside the 60 Hz
    // matrix animation, and the final target is always published.
    const ULONGLONG now = GetTickCount64();
    if (!add && lastTrayUpdate && now - lastTrayUpdate < 70) {
        if (!trayUpdatePending) { trayUpdatePending = true; SetTimer(mainWindow, TrayUpdateTimer, 70, nullptr); }
        return;
    }
    if (mainWindow) KillTimer(mainWindow, TrayUpdateTimer);
    trayUpdatePending = false; lastTrayUpdate = now;
    if (!trayIcon || cachedTrayMode != (percentageIcon ? 1 : 0) || cachedTrayDark != systemTrayDark ||
        (percentageIcon && cachedTrayLevel != brightness)) RefreshTrayIcon();
    NOTIFYICONDATAW data = {};
    data.cbSize = sizeof(data); data.hWnd = mainWindow; data.uID = 1;
    data.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP | NIF_SHOWTIP;
    data.hIcon = trayIcon ? trayIcon : appIcon; data.uCallbackMessage = TrayMessage;
    StringCchPrintfW(data.szTip, ARRAYSIZE(data.szTip), brightnessLocked ? L"DimLite — %d%%, зафиксировано" : L"DimLite — яркость %d%%", brightness);
    if (Shell_NotifyIconW(add ? NIM_ADD : NIM_MODIFY, &data) && add) {
        data.uVersion = NOTIFYICON_VERSION_4;
        Shell_NotifyIconW(NIM_SETVERSION, &data);
    }
}

// Cache the icon hit area on shell notifications, never on a polling timer or
// clicks in unrelated apps. The mouse hook only checks this small rectangle.
void RefreshTrayBounds(bool shellHover = false) {
    NOTIFYICONIDENTIFIER icon = {}; icon.cbSize = sizeof(icon); icon.hWnd = mainWindow; icon.uID = 1;
    RECT bounds = {};
    bool valid = SUCCEEDED(Shell_NotifyIconGetRect(&icon, &bounds));
    HWND root = nullptr;
    if (valid) {
        const POINT center = {(bounds.left + bounds.right) / 2, (bounds.top + bounds.bottom) / 2};
        root = GetAncestor(WindowFromPoint(center), GA_ROOT);
        if (shellHover) {
            POINT cursor; GetCursorPos(&cursor);
            DWORD rootPid = 0, shellPid = 0;
            GetWindowThreadProcessId(root, &rootPid);
            GetWindowThreadProcessId(GetShellWindow(), &shellPid);
            wchar_t klass[96] = {}; GetClassNameW(root, klass, ARRAYSIZE(klass));
            if (PtInRect(&bounds, cursor) && rootPid && rootPid == shellPid &&
                lstrcmpW(klass, L"Progman") != 0 && lstrcmpW(klass, L"WorkerW") != 0)
                confirmedTrayRoot = root;
        }
        valid = root && root == confirmedTrayRoot;
    }
    AcquireSRWLockExclusive(&trayBoundsLock);
    trayMouseBounds = bounds; trayMouseRoot = root; trayBoundsValid = valid;
    ReleaseSRWLockExclusive(&trayBoundsLock);
    lastTrayBoundsRefresh = GetTickCount64();
}

bool MouseOverCachedTray(POINT cursor) {
    AcquireSRWLockShared(&trayBoundsLock);
    const RECT bounds = trayMouseBounds; const HWND root = trayMouseRoot; const bool valid = trayBoundsValid;
    ReleaseSRWLockShared(&trayBoundsLock);
    return valid && PtInRect(&bounds, cursor) && GetAncestor(WindowFromPoint(cursor), GA_ROOT) == root;
}

LRESULT CALLBACK TrayMouseHook(int code, WPARAM message, LPARAM data) {
    static bool middleCaptured = false;
    if (code == HC_ACTION) {
        const auto* mouse = reinterpret_cast<const MSLLHOOKSTRUCT*>(data);
        if (message == WM_MOUSEWHEEL && MouseOverCachedTray(mouse->pt)) {
            const short delta = static_cast<short>(HIWORD(mouse->mouseData));
            InterlockedIncrement(&nativeWheelEvents);
            PostMessageW(mainWindow, TrayWheelMessage, static_cast<WPARAM>(static_cast<INT_PTR>(delta)), 0);
            return 1;
        }
        if (message == WM_MBUTTONDOWN && MouseOverCachedTray(mouse->pt)) { middleCaptured = true; return 1; }
        if (message == WM_MBUTTONUP && middleCaptured) {
            middleCaptured = false;
            if (MouseOverCachedTray(mouse->pt)) {
                InterlockedIncrement(&nativeMiddleEvents);
                PostMessageW(mainWindow, ResetOnlyMessage, 0, 0);
            }
            return 1;
        }
    }
    return CallNextHookEx(nullptr, code, message, data);
}

DWORD WINAPI TrayMouseThread(void*) {
    MSG message;
    PeekMessageW(&message, nullptr, 0, 0, PM_NOREMOVE);
    HHOOK hook = SetWindowsHookExW(WH_MOUSE_LL, TrayMouseHook, instance, 0);
    InterlockedExchange(&mouseHookActive, hook ? 1 : 0);
    SetEvent(mouseReady);
    if (hook) {
        while (GetMessageW(&message, nullptr, 0, 0) > 0) { TranslateMessage(&message); DispatchMessageW(&message); }
        UnhookWindowsHookEx(hook);
    }
    InterlockedExchange(&mouseHookActive, 0);
    return 0;
}

void StartTrayMouse() {
    mouseReady = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!mouseReady) return;
    mouseThread = CreateThread(nullptr, 0, TrayMouseThread, nullptr, 0, &mouseThreadId);
    if (mouseThread) WaitForSingleObject(mouseReady, 3000);
}

void StopTrayMouse() {
    if (mouseThread) {
        PostThreadMessageW(mouseThreadId, WM_QUIT, 0, 0);
        WaitForSingleObject(mouseThread, 3000);
        CloseHandle(mouseThread); mouseThread = nullptr;
    }
    if (mouseReady) { CloseHandle(mouseReady); mouseReady = nullptr; }
}

void RegisterAdjustHotkeys() {
    for (int variant = 0; variant < 3; ++variant) {
        const UINT modifiers = MOD_CONTROL | MOD_ALT | (variant == 2 ? MOD_SHIFT : 0);
        const UINT up = variant == 1 ? VK_PRIOR : VK_UP, down = variant == 1 ? VK_NEXT : VK_DOWN;
        const BOOL a = RegisterHotKey(mainWindow, 2, modifiers | MOD_NOREPEAT, up);
        const BOOL b = RegisterHotKey(mainWindow, 3, modifiers | MOD_NOREPEAT, down);
        if (a && b) {
            adjustHotkeysRegistered = true; adjustHotkeyUp = up; adjustHotkeyDown = down; adjustHotkeyModifiers = modifiers;
            StringCchCopyW(adjustHotkeyText, ARRAYSIZE(adjustHotkeyText), variant == 1 ? L"Ctrl+Alt+PageUp / PageDown — яркость ±5%"
                : variant == 2 ? L"Ctrl+Alt+Shift+↑ / ↓ — яркость ±5%" : L"Ctrl+Alt+↑ / ↓ — яркость ±5%");
            return;
        }
        if (a) UnregisterHotKey(mainWindow, 2);
        if (b) UnregisterHotKey(mainWindow, 3);
    }
    StringCchCopyW(adjustHotkeyText, ARRAYSIZE(adjustHotkeyText), L"Колёсико мыши — яркость ±5%");
}

void ApplyWheelDelta(int delta) {
    if (brightnessLocked) { wheelRemainder = 0; BrightnessHud::Show(brightness, true, darkMode); return; }
    wheelRemainder += delta;
    const int steps = wheelRemainder / WHEEL_DELTA;
    wheelRemainder %= WHEEL_DELTA;
    if (steps) { ApplyLevel(brightness + steps * 5); BrightnessHud::Show(brightness, false, darkMode); }
}

void SyncSlider(HWND control) {
    const int position = control == flyoutVerticalSlider ? 105 - brightness : brightness;
    if (control && SendMessageW(control, TBM_GETPOS, 0, 0) != position)
        SendMessageW(control, TBM_SETPOS, TRUE, position);
}

bool WriteBrightness(float level) {
    MAGCOLOREFFECT effect = EffectAt(level);
    if (recoveryState && level < 100.0f) InterlockedExchange(&recoveryState->armed, 1);
    if (!MagSetFullscreenColorEffect(&effect)) return false;
    if (recoveryState && level >= 100.0f) InterlockedExchange(&recoveryState->armed, 0);
    appliedBrightness = level;
    return true;
}

void SyncBrightnessControls() {
    SyncSlider(slider); SyncSlider(flyoutSlider); SyncSlider(flyoutVerticalSlider);
    wchar_t text[12]; StringCchPrintfW(text, ARRAYSIZE(text), L"%d%%", brightness);
    if (valueLabel) SetWindowTextW(valueLabel, text);
    if (flyoutValue) SetWindowTextW(flyoutValue, text);
    UpdateTray();
    BrightnessHud::Update(brightness, brightnessLocked, darkMode);
}

void RememberLevel(int level) {
    if (profilesEnabled && DimProfiles::Find(activeAppPath) >= 0) DimProfiles::Set(activeAppPath, level);
    else manualBrightness = level;
}

void TickBrightness() {
    const ULONGLONG now = GetTickCount64();
    const float elapsed = static_cast<float>(std::min<ULONGLONG>(now - lastTransitionTick, 64));
    lastTransitionTick = now;
    const float target = static_cast<float>(brightness);
    float next = appliedBrightness + (target - appliedBrightness) * (1.0f - std::exp(-elapsed / 36.0f));
    const bool finished = std::fabs(next - target) < 0.035f || now - lastTargetChange >= 240;
    if (finished) next = target;
    const bool ok = WriteBrightness(next);
    if (finished || !ok) {
        KillTimer(mainWindow, TransitionTimer); transitionActive = false;
        if (!ok) {
            brightness = std::clamp(static_cast<int>(std::lround(appliedBrightness)), 5, 100);
            RememberLevel(brightness); SyncBrightnessControls();
        }
    }
}

bool ApplyLevel(int requested, bool force, bool fromProfile) {
    const int level = std::clamp(requested, 5, 100);
    if (brightnessLocked && !force) { SyncBrightnessControls(); return false; }
    if (!force && level == brightness) return true;
    if (force) {
        KillTimer(mainWindow, TransitionTimer); transitionActive = false;
        if (!WriteBrightness(static_cast<float>(level))) { SyncBrightnessControls(); return false; }
    } else {
        lastTargetChange = GetTickCount64();
        if (!transitionActive) {
            lastTransitionTick = lastTargetChange;
            transitionActive = SetTimer(mainWindow, TransitionTimer, 16, nullptr) != 0;
            if (!transitionActive && !WriteBrightness(static_cast<float>(level))) return false;
        }
    }
    brightness = level;
    if (!force && !fromProfile) RememberLevel(level);
    SyncBrightnessControls();
    saving = true; SetTimer(mainWindow, SaveTimer, 400, nullptr);
    return true;
}

void UpdateLockControls() {
    for (HWND control : {slider, flyoutSlider, flyoutVerticalSlider}) if (control) {
        EnableWindow(control, !brightnessLocked); InvalidateRect(control, nullptr, FALSE);
    }
    UpdateTray();
    BrightnessHud::Update(brightness, brightnessLocked, darkMode);
    if (settingsVisible) SyncSettings();
}

void SetBrightnessLock(bool locked, bool indicate) {
    if (brightnessLocked != locked) {
        brightnessLocked = locked; wheelRemainder = 0; UpdateLockControls();
        if (!locked) ApplyLevel(profilesEnabled ? DimProfiles::Resolve(activeAppPath, manualBrightness) : manualBrightness, false, true);
        saving = true; SetTimer(mainWindow, SaveTimer, 400, nullptr);
    }
    if (indicate) BrightnessHud::Show(brightness, brightnessLocked, darkMode);
}

void ResetBrightness() {
    brightnessLocked = false; wheelRemainder = 0;
    manualBrightness = 100; RememberLevel(100);
    ApplyLevel(100, true); UpdateLockControls();
}

void SetProfilesEnabled(bool enabled) {
    profilesEnabled = enabled;
    if (!brightnessLocked) ApplyLevel(enabled ? DimProfiles::Resolve(activeAppPath, manualBrightness) : manualBrightness, false, true);
    saving = true; SetTimer(mainWindow, SaveTimer, 400, nullptr);
    if (settingsVisible) SyncSettings();
}

void TrackForegroundApp(HWND window, bool apply) {
    std::wstring path, name;
    if (!DimProfiles::QueryApp(window, path, name)) return;
    if (CompareStringOrdinal(activeAppPath.c_str(), -1, path.c_str(), -1, TRUE) == CSTR_EQUAL) return;
    activeAppPath = std::move(path); activeAppName = std::move(name);
    if (apply && profilesEnabled && !brightnessLocked)
        ApplyLevel(DimProfiles::Resolve(activeAppPath, manualBrightness), false, true);
    if (settingsVisible) SyncSettings();
}

void CALLBACK ForegroundChanged(HWINEVENTHOOK, DWORD, HWND window, LONG, LONG, DWORD, DWORD) {
    if (window && mainWindow) PostMessageW(mainWindow, ForegroundAppMessage, reinterpret_cast<WPARAM>(window), 0);
}

HFONT Font(int size, UINT atDpi, int weight = FW_NORMAL) {
    return CreateFontW(-MulDiv(size, static_cast<int>(atDpi), 96), 0, 0, 0, weight,
        FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
}

void ApplyFonts() {
    for (HFONT font : {bodyFont, headingFont, valueFont}) if (font) DeleteObject(font);
    bodyFont = Font(14, dpi);
    headingFont = Font(20, dpi, FW_SEMIBOLD); valueFont = Font(28, dpi, FW_SEMIBOLD);
    for (const auto& c : controls) SendMessageW(c.window, WM_SETFONT, reinterpret_cast<WPARAM>(bodyFont), TRUE);
    for (const auto& c : settingsControls) SendMessageW(c.window, WM_SETFONT, reinterpret_cast<WPARAM>(bodyFont), TRUE);
    if (settingsHeading) SendMessageW(settingsHeading, WM_SETFONT, reinterpret_cast<WPARAM>(headingFont), TRUE);
    if (!controls.empty()) SendMessageW(controls[0].window, WM_SETFONT, reinterpret_cast<WPARAM>(headingFont), TRUE);
    if (valueLabel) SendMessageW(valueLabel, WM_SETFONT, reinterpret_cast<WPARAM>(valueFont), TRUE);
}

void ApplyFlyoutFonts() {
    for (HFONT font : {flyoutBodyFont, flyoutHeadingFont, flyoutValueFont, stripValueFont}) if (font) DeleteObject(font);
    flyoutBodyFont = Font(14, flyoutDpi); flyoutHeadingFont = Font(16, flyoutDpi, FW_SEMIBOLD);
    flyoutValueFont = Font(24, flyoutDpi, FW_SEMIBOLD);
    stripValueFont = Font(20, flyoutDpi, FW_SEMIBOLD);
    for (const auto& c : flyoutControls) SendMessageW(c.window, WM_SETFONT, reinterpret_cast<WPARAM>(flyoutBodyFont), TRUE);
    if (!flyoutControls.empty()) SendMessageW(flyoutControls[0].window, WM_SETFONT, reinterpret_cast<WPARAM>(flyoutHeadingFont), TRUE);
    if (flyoutValue) SendMessageW(flyoutValue, WM_SETFONT, reinterpret_cast<WPARAM>(flyoutValueFont), TRUE);
}

HWND AddControl(bool compact, const wchar_t* klass, const wchar_t* text, DWORD style,
                int x, int y, int width, int height, int id = 0) {
    const UINT atDpi = compact ? flyoutDpi : dpi;
    HWND window = CreateWindowExW(0, klass, text, WS_CHILD | WS_VISIBLE | style,
        MulDiv(x, atDpi, 96), MulDiv(y, atDpi, 96), MulDiv(width, atDpi, 96), MulDiv(height, atDpi, 96),
        compact ? flyout : mainWindow, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), instance, nullptr);
    (compact ? flyoutControls : controls).push_back({window, x, y, width, height});
    if ((style & BS_TYPEMASK) == BS_OWNERDRAW && lstrcmpW(klass, L"BUTTON") == 0)
        SetWindowSubclass(window, ButtonProc, 1, 0);
    return window;
}

void Layout(bool compact) {
    const UINT atDpi = compact ? flyoutDpi : dpi;
    for (const auto& c : compact ? flyoutControls : controls)
        MoveWindow(c.window, MulDiv(c.x, atDpi, 96), MulDiv(c.y, atDpi, 96),
            MulDiv(c.w, atDpi, 96), MulDiv(c.h, atDpi, 96), TRUE);
    if (!compact) for (const auto& c : settingsControls)
        MoveWindow(c.window, Scale(c.x), Scale(c.y), Scale(c.w), Scale(c.h), TRUE);
}

HWND AddSetting(const wchar_t* klass, const wchar_t* text, DWORD style,
                int x, int y, int width, int height, int id = 0) {
    HWND window = CreateWindowExW(0, klass, text, WS_CHILD | style,
        Scale(x), Scale(y), Scale(width), Scale(height), mainWindow,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), instance, nullptr);
    settingsControls.push_back({window, x, y, width, height});
    if (lstrcmpW(klass, L"BUTTON") == 0) {
        if ((style & BS_TYPEMASK) == BS_OWNERDRAW) SetWindowSubclass(window, ButtonProc, 1, 0);
        else if ((style & BS_TYPEMASK) == BS_AUTOCHECKBOX) SetWindowSubclass(window, CheckboxProc, 1, 0);
    }
    return window;
}

void LayoutFlyout() {
    Layout(true);
    for (const auto& control : flyoutControls) {
        const bool show = activeStrip ? (control.window == flyoutValue || control.window == flyoutVerticalSlider || GetDlgCtrlID(control.window) == ThemeId)
                                      : (control.window != flyoutVerticalSlider);
        ShowWindow(control.window, show ? SW_SHOWNOACTIVATE : SW_HIDE);
    }
    LONG_PTR style = GetWindowLongPtrW(flyoutValue, GWL_STYLE);
    style = (style & ~static_cast<LONG_PTR>(SS_TYPEMASK)) | (activeStrip ? SS_CENTER : SS_RIGHT);
    SetWindowLongPtrW(flyoutValue, GWL_STYLE, style);
    SendMessageW(flyoutValue, WM_SETFONT, reinterpret_cast<WPARAM>(activeStrip ? stripValueFont : flyoutValueFont), TRUE);
    if (activeStrip) {
        const int width = FlyScale(StripWidth), padding = FlyScale(6);
        MoveWindow(flyoutValue, padding, FlyScale(12), width - 2 * padding, FlyScale(30), TRUE);
        MoveWindow(flyoutVerticalSlider, FlyScale(22), FlyScale(48), FlyScale(32), FlyScale(128), TRUE);
        RECT thumb = {};
        SendMessageW(flyoutVerticalSlider, TBM_GETTHUMBRECT, 0, reinterpret_cast<LPARAM>(&thumb));
        const int centeredLeft = width / 2 - (thumb.left + thumb.right) / 2;
        MoveWindow(flyoutVerticalSlider, centeredLeft, FlyScale(48), FlyScale(32), FlyScale(128), TRUE);
        MoveWindow(GetDlgItem(flyout, ThemeId), (width - FlyScale(28)) / 2, FlyScale(184), FlyScale(28), FlyScale(28), TRUE);
    }
    RedrawWindow(flyout, nullptr, nullptr, RDW_INVALIDATE | RDW_ALLCHILDREN);
}

void RoundPath(Gdiplus::GraphicsPath& path, Gdiplus::RectF rect, float radius) {
    const float diameter = std::min(radius * 2.0f, std::min(rect.Width, rect.Height));
    path.AddArc(rect.X, rect.Y, diameter, diameter, 180.0f, 90.0f);
    path.AddArc(rect.GetRight() - diameter, rect.Y, diameter, diameter, 270.0f, 90.0f);
    path.AddArc(rect.GetRight() - diameter, rect.GetBottom() - diameter, diameter, diameter, 0.0f, 90.0f);
    path.AddArc(rect.X, rect.GetBottom() - diameter, diameter, diameter, 90.0f, 90.0f);
    path.CloseFigure();
}

void FilledRoundRect(HDC dc, RECT rect, COLORREF fill, COLORREF border, int radius) {
    Gdiplus::Graphics graphics(dc);
    graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    Gdiplus::GraphicsPath path;
    RoundPath(path, Gdiplus::RectF(static_cast<float>(rect.left) + 0.5f, static_cast<float>(rect.top) + 0.5f,
        static_cast<float>(rect.right - rect.left) - 1.0f, static_cast<float>(rect.bottom - rect.top) - 1.0f), static_cast<float>(radius));
    Gdiplus::SolidBrush brush(Color(fill));
    Gdiplus::Pen pen(Color(border), 1.0f);
    graphics.FillPath(&brush, &path); graphics.DrawPath(&pen, &path);
}

void PaintIcon(HDC dc, RECT rect, UINT atDpi, bool settings, COLORREF fill) {
    const float scale = static_cast<float>(atDpi) / 96.0f;
    const float x = static_cast<float>(rect.left + rect.right) / 2.0f;
    const float y = static_cast<float>(rect.top + rect.bottom) / 2.0f;
    Gdiplus::Graphics graphics(dc);
    graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    Gdiplus::Pen pen(Color(Foreground()), 1.5f * scale);
    pen.SetStartCap(Gdiplus::LineCapRound); pen.SetEndCap(Gdiplus::LineCapRound);
    if (settings) {
        Gdiplus::GraphicsPath gear;
        for (int i = 0; i < 32; ++i) {
            const float angle = static_cast<float>(i * 3.141592653589793 / 16.0);
            const float radius = ((i % 4 == 1 || i % 4 == 2) ? 8.5f : 6.5f) * scale;
            const Gdiplus::PointF point(x + std::cos(angle) * radius, y + std::sin(angle) * radius);
            if (i == 0) gear.StartFigure();
            else {
                const float previousAngle = static_cast<float>((i - 1) * 3.141592653589793 / 16.0);
                const float previousRadius = (((i - 1) % 4 == 1 || (i - 1) % 4 == 2) ? 8.5f : 6.5f) * scale;
                gear.AddLine(Gdiplus::PointF(x + std::cos(previousAngle) * previousRadius,
                    y + std::sin(previousAngle) * previousRadius), point);
            }
        }
        gear.CloseFigure(); graphics.DrawPath(&pen, &gear);
        graphics.DrawEllipse(&pen, x - 2.6f * scale, y - 2.6f * scale, 5.2f * scale, 5.2f * scale);
    } else if (darkMode) {
        graphics.DrawEllipse(&pen, x - 4.0f * scale, y - 4.0f * scale, 8.0f * scale, 8.0f * scale);
        for (int i = 0; i < 8; ++i) {
            const float angle = static_cast<float>(i * 3.141592653589793 / 4.0);
            graphics.DrawLine(&pen, x + std::cos(angle) * 7.0f * scale, y + std::sin(angle) * 7.0f * scale,
                x + std::cos(angle) * 9.0f * scale, y + std::sin(angle) * 9.0f * scale);
        }
    } else {
        Gdiplus::SolidBrush moon(Color(Foreground())), cutout(Color(fill));
        graphics.FillEllipse(&moon, x - 7.0f * scale, y - 7.0f * scale, 14.0f * scale, 14.0f * scale);
        graphics.FillEllipse(&cutout, x - 2.0f * scale, y - 10.0f * scale, 14.0f * scale, 14.0f * scale);
    }
}

LRESULT CALLBACK ButtonProc(HWND window, UINT message, WPARAM w, LPARAM l, UINT_PTR, DWORD_PTR) {
    if (message == WM_MOUSEMOVE && GetParent(window) == flyout) KeepFlyoutOpen();
    if (message == WM_MOUSEMOVE && GetParent(window) == menuPanel && menuKeyboard) {
        menuKeyboard = false;
        for (const auto& control : menuControls) InvalidateRect(control.window, nullptr, FALSE);
    }
    if (message == WM_MOUSEMOVE && !GetPropW(window, HoverProperty)) {
        SetPropW(window, HoverProperty, reinterpret_cast<HANDLE>(1));
        TRACKMOUSEEVENT track = {sizeof(track), TME_LEAVE, window, 0};
        TrackMouseEvent(&track); InvalidateRect(window, nullptr, FALSE);
    }
    if (message == WM_MOUSELEAVE) {
        RemovePropW(window, HoverProperty); InvalidateRect(window, nullptr, FALSE);
        if (GetParent(window) == flyout) CheckFlyoutMouseLeave();
    }
    if (message == WM_NCDESTROY) { RemovePropW(window, HoverProperty); RemoveWindowSubclass(window, ButtonProc, 1); }
    const LRESULT result = DefSubclassProc(window, message, w, l);
    if ((message == WM_LBUTTONUP || message == WM_CAPTURECHANGED) && GetParent(window) == flyout)
        CheckFlyoutMouseLeave();
    return result;
}

LRESULT PaintButton(const DRAWITEMSTRUCT* item) {
    if (GetParent(item->hwndItem) == menuPanel) return PaintContextButton(item);
    RECT rect = item->rcItem;
    const UINT atDpi = GetDpiForWindow(item->hwndItem);
    HBRUSH background = CreateSolidBrush(Background());
    FillRect(item->hDC, &rect, background); DeleteObject(background);
    const bool pressed = (item->itemState & ODS_SELECTED) != 0;
    const bool hover = GetPropW(item->hwndItem, HoverProperty) != nullptr;
    const bool icon = item->CtlID == ThemeId || item->CtlID == SettingsId || item->CtlID == BackId;
    const bool selectedTheme = (item->CtlID == AutoThemeId && themeMode == ThemeMode::Automatic) ||
        (item->CtlID == LightThemeId && themeMode == ThemeMode::Light) ||
        (item->CtlID == DarkThemeId && themeMode == ThemeMode::Dark);
    const bool keyboardFocus = (item->itemState & ODS_FOCUS) &&
        !(SendMessageW(item->hwndItem, WM_QUERYUISTATE, 0, 0) & UISF_HIDEFOCUS);
    const COLORREF fill = selectedTheme ? (darkMode ? RGB(43,62,88) : RGB(228,238,252)) : icon && !hover && !pressed ? Background()
        : (darkMode ? (pressed ? RGB(66, 67, 74) : hover ? RGB(59, 60, 67) : RGB(48, 49, 55))
                    : (pressed ? RGB(224, 227, 234) : hover ? RGB(235, 237, 242) : RGB(243, 244, 247)));
    const COLORREF border = keyboardFocus || selectedTheme ? Accent() : icon ? fill : (darkMode ? RGB(66, 67, 74) : RGB(218, 221, 228));
    if (!icon || hover || pressed || keyboardFocus)
        FilledRoundRect(item->hDC, rect, fill, border, MulDiv(7, atDpi, 96));
    if (item->CtlID == BackId) {
        const float scale = static_cast<float>(atDpi) / 96.0f;
        const float x = static_cast<float>(rect.left + rect.right) / 2.0f;
        const float y = static_cast<float>(rect.top + rect.bottom) / 2.0f;
        Gdiplus::Graphics graphics(item->hDC); graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        Gdiplus::Pen pen(Color(Foreground()), 1.6f * scale);
        pen.SetStartCap(Gdiplus::LineCapRound); pen.SetEndCap(Gdiplus::LineCapRound);
        graphics.DrawLine(&pen, x + 6 * scale, y, x - 6 * scale, y);
        graphics.DrawLine(&pen, x - 6 * scale, y, x - 1 * scale, y - 5 * scale);
        graphics.DrawLine(&pen, x - 6 * scale, y, x - 1 * scale, y + 5 * scale);
    }
    else if (icon) PaintIcon(item->hDC, rect, atDpi, item->CtlID == SettingsId, fill);
    else {
        wchar_t caption[128] = {};
        GetWindowTextW(item->hwndItem, caption, ARRAYSIZE(caption));
        HGDIOBJ old = SelectObject(item->hDC, reinterpret_cast<HFONT>(SendMessageW(item->hwndItem, WM_GETFONT, 0, 0)));
        SetTextColor(item->hDC, Foreground()); SetBkMode(item->hDC, TRANSPARENT);
        DrawTextW(item->hDC, caption, -1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        SelectObject(item->hDC, old);
    }
    return TRUE;
}

void PaintSlider(HWND window, HDC dc) {
    RECT client;
    GetClientRect(window, &client);
    FillRect(dc, &client, backgroundBrush);
    const UINT atDpi = GetDpiForWindow(window);
    RECT thumb = {}, channel = {};
    SendMessageW(window, TBM_GETTHUMBRECT, 0, reinterpret_cast<LPARAM>(&thumb));
    SendMessageW(window, TBM_GETCHANNELRECT, 0, reinterpret_cast<LPARAM>(&channel));
    const int centerX = (thumb.left + thumb.right) / 2, centerY = (thumb.top + thumb.bottom) / 2;
    const bool vertical = (GetWindowLongPtrW(window, GWL_STYLE) & TBS_VERT) != 0;
    const int half = MulDiv(2, atDpi, 96);
    RECT track;
    if (vertical) {
        const bool upright = channel.bottom - channel.top > channel.right - channel.left;
        const LONG top = upright ? channel.top : channel.left;
        const LONG bottom = upright ? channel.bottom : channel.right;
        track = {centerX - half, top, centerX + half, bottom};
    } else track = {channel.left, centerY - half, channel.right, centerY + half};
    const COLORREF unused = darkMode ? RGB(75, 85, 101) : RGB(208, 216, 229);
    FilledRoundRect(dc, track, unused, unused, MulDiv(4, atDpi, 96));
    if (vertical) track.top = std::clamp<LONG>(centerY, track.top, track.bottom - 1);
    else track.right = std::max<LONG>(track.left + 1, centerX);
    const COLORREF accent = IsWindowEnabled(window) ? Accent() : darkMode ? RGB(143,151,163) : RGB(118,128,143);
    FilledRoundRect(dc, track, accent, accent, MulDiv(4, atDpi, 96));
    const float radius = 7.0f * static_cast<float>(atDpi) / 96.0f;
    Gdiplus::Graphics graphics(dc);
    graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    Gdiplus::SolidBrush outside(Color(accent));
    const float cx = static_cast<float>(centerX), cy = static_cast<float>(centerY);
    graphics.FillEllipse(&outside, cx - radius, cy - radius, radius * 2.0f, radius * 2.0f);
}

struct SliderBuffer {
    HDC dc;
    HBITMAP bitmap;
    HGDIOBJ original;
    int width, height;
};
constexpr wchar_t SliderBufferProperty[] = L"DimLite.SliderBuffer";
HWND measuringSlider = nullptr;

LPARAM SliderClickPosition(HWND window, LPARAM point) {
    RECT thumb = {}; SendMessageW(window,TBM_GETTHUMBRECT,0,reinterpret_cast<LPARAM>(&thumb));
    const POINT cursor = {GET_X_LPARAM(point),GET_Y_LPARAM(point)};
    if (PtInRect(&thumb,cursor)) return point; // Preserve the native thumb grab offset.
    const bool vertical = (GetWindowLongPtrW(window,GWL_STYLE) & TBS_VERT) != 0;
    const int minimum = static_cast<int>(SendMessageW(window,TBM_GETRANGEMIN,0,0));
    const int maximum = static_cast<int>(SendMessageW(window,TBM_GETRANGEMAX,0,0));
    const int original = static_cast<int>(SendMessageW(window,TBM_GETPOS,0,0));
    RECT low = {}, high = {};
    // Read the native end positions; this accounts for DPI, thumb size and
    // vertical orientation without relying on channel-rectangle conventions.
    const HWND previousMeasurement = measuringSlider; measuringSlider = window;
    SendMessageW(window,TBM_SETPOS,TRUE,minimum);
    SendMessageW(window,TBM_GETTHUMBRECT,0,reinterpret_cast<LPARAM>(&low));
    SendMessageW(window,TBM_SETPOS,TRUE,maximum);
    SendMessageW(window,TBM_GETTHUMBRECT,0,reinterpret_cast<LPARAM>(&high));
    SendMessageW(window,TBM_SETPOS,TRUE,original);
    measuringSlider = previousMeasurement;
    const float start = vertical ? (low.top+low.bottom)/2.0f : (low.left+low.right)/2.0f;
    const float end = vertical ? (high.top+high.bottom)/2.0f : (high.left+high.right)/2.0f;
    if (start == end) return point;
    const int clicked = vertical ? cursor.y : cursor.x;
    const float fraction = std::clamp((static_cast<float>(clicked)-start)/(end-start),0.0f,1.0f);
    const int position = minimum+static_cast<int>(std::lround(fraction*static_cast<float>(maximum-minimum)));
    SendMessageW(window,TBM_SETPOS,TRUE,position);
    SendMessageW(GetParent(window),vertical ? WM_VSCROLL : WM_HSCROLL,
        MAKEWPARAM(TB_THUMBTRACK,position),reinterpret_cast<LPARAM>(window));
    SendMessageW(window,TBM_GETTHUMBRECT,0,reinterpret_cast<LPARAM>(&thumb));
    // Start the normal native drag at the new thumb centre. A click also works
    // above or below the narrow painted track, with no extra page-size step.
    return MAKELPARAM((thumb.left+thumb.right)/2,(thumb.top+thumb.bottom)/2);
}

LRESULT CALLBACK SliderProc(HWND window, UINT message, WPARAM w, LPARAM l, UINT_PTR, DWORD_PTR) {
    if (message == WM_MOUSEWHEEL) {
        PostMessageW(mainWindow, TrayWheelMessage, static_cast<WPARAM>(static_cast<INT_PTR>(GET_WHEEL_DELTA_WPARAM(w))), 0);
        return 0;
    }
    if (message == WM_LBUTTONDOWN && IsWindowEnabled(window)) l = SliderClickPosition(window,l);
    if (GetParent(window) == flyout && message == WM_MOUSEMOVE) {
        KeepFlyoutOpen();
        if (!GetPropW(window, HoverProperty)) {
            SetPropW(window, HoverProperty, reinterpret_cast<HANDLE>(1));
            TRACKMOUSEEVENT track = {sizeof(track), TME_LEAVE, window, 0};
            TrackMouseEvent(&track);
        }
    }
    if (message == WM_MOUSELEAVE) {
        RemovePropW(window, HoverProperty);
        if (GetParent(window) == flyout) CheckFlyoutMouseLeave();
    }
    if (message == WM_ERASEBKGND) return 1;
    if (message == WM_PRINTCLIENT) { PaintSlider(window, reinterpret_cast<HDC>(w)); return 0; }
    if (message == WM_PAINT) {
        if (window == measuringSlider) { PAINTSTRUCT paint; BeginPaint(window,&paint); EndPaint(window,&paint); return 0; }
        PAINTSTRUCT paint;
        HDC dc = BeginPaint(window, &paint);
        RECT rect; GetClientRect(window, &rect);
        auto* buffer = static_cast<SliderBuffer*>(GetPropW(window, SliderBufferProperty));
        if (!buffer) {
            buffer = static_cast<SliderBuffer*>(HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(SliderBuffer)));
            if (buffer && !SetPropW(window, SliderBufferProperty, buffer)) {
                HeapFree(GetProcessHeap(), 0, buffer); buffer = nullptr;
            }
        }
        const int width = std::max<LONG>(1, rect.right), height = std::max<LONG>(1, rect.bottom);
        if (buffer && !buffer->dc) buffer->dc = CreateCompatibleDC(dc);
        if (buffer && buffer->dc && (!buffer->bitmap || buffer->width != width || buffer->height != height)) {
            HBITMAP resized = CreateCompatibleBitmap(dc, width, height);
            if (resized) {
                HGDIOBJ previous = SelectObject(buffer->dc, resized);
                if (!buffer->original) buffer->original = previous;
                if (buffer->bitmap) DeleteObject(buffer->bitmap);
                buffer->bitmap = resized; buffer->width = width; buffer->height = height;
            }
        }
        if (buffer && buffer->bitmap && buffer->width == width && buffer->height == height) {
            PaintSlider(window, buffer->dc);
            BitBlt(dc, 0, 0, rect.right, rect.bottom, buffer->dc, 0, 0, SRCCOPY);
        } else PaintSlider(window, dc);
        EndPaint(window, &paint); return 0;
    }
    if (message == WM_NCDESTROY) {
        auto* buffer = static_cast<SliderBuffer*>(RemovePropW(window, SliderBufferProperty));
        if (buffer) {
            if (buffer->original) SelectObject(buffer->dc, buffer->original);
            if (buffer->bitmap) DeleteObject(buffer->bitmap);
            if (buffer->dc) DeleteDC(buffer->dc);
            HeapFree(GetProcessHeap(), 0, buffer);
        }
        RemovePropW(window, HoverProperty); RemoveWindowSubclass(window, SliderProc, 1);
    }
    const bool positionInput = (message == WM_MOUSEMOVE && GetCapture() == window) ||
        message == WM_LBUTTONDOWN || message == WM_LBUTTONUP || message == WM_KEYDOWN ||
        message == WM_KEYUP || message == TBM_SETPOS;
    const LRESULT previousPosition = positionInput ? SendMessageW(window, TBM_GETPOS, 0, 0) : 0;
    const LRESULT result = DefSubclassProc(window, message, w, l);
    if (message == WM_LBUTTONUP && GetParent(window) == flyout) CheckFlyoutMouseLeave();
    if ((positionInput && previousPosition != SendMessageW(window, TBM_GETPOS, 0, 0)) ||
        message == WM_SETFOCUS || message == WM_KILLFOCUS || message == WM_SIZE || message == WM_ENABLE)
        InvalidateRect(window, nullptr, FALSE);
    return result;
}

void DrawCheckbox(HWND window, HDC dc) {
        RECT rect;
        GetClientRect(window, &rect); FillRect(dc, &rect, backgroundBrush);
        const UINT atDpi = GetDpiForWindow(window);
        const int side = MulDiv(15, atDpi, 96), top = (rect.bottom - side) / 2;
        RECT box = {1, top, side + 1, top + side};
        const bool checked = SendMessageW(window, BM_GETCHECK, 0, 0) == BST_CHECKED;
        FilledRoundRect(dc, box, checked ? Accent() : Background(), checked ? Accent() : Foreground(), MulDiv(4, atDpi, 96));
        if (checked) {
            HPEN pen = CreatePen(PS_SOLID, std::max(1, MulDiv(2, atDpi, 96)), Background());
            HGDIOBJ old = SelectObject(dc, pen);
            MoveToEx(dc, box.left + side / 5, top + side / 2, nullptr);
            LineTo(dc, box.left + side * 2 / 5, top + side * 3 / 4);
            LineTo(dc, box.left + side * 4 / 5, top + side / 4);
            SelectObject(dc, old); DeleteObject(pen);
        }
        wchar_t text[128]; GetWindowTextW(window, text, ARRAYSIZE(text));
        rect.left = side + MulDiv(9, atDpi, 96);
        SetBkMode(dc, TRANSPARENT); SetTextColor(dc, IsWindowEnabled(window) ? Foreground() : darkMode ? RGB(142,142,149) : RGB(132,132,140));
        HGDIOBJ old = SelectObject(dc, reinterpret_cast<HFONT>(SendMessageW(window, WM_GETFONT, 0, 0)));
        DrawTextW(dc, text, -1, &rect, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        if (GetFocus() == window && !(SendMessageW(window, WM_QUERYUISTATE, 0, 0) & UISF_HIDEFOCUS)) DrawFocusRect(dc, &rect);
        SelectObject(dc, old);
}

LRESULT CALLBACK CheckboxProc(HWND window, UINT message, WPARAM w, LPARAM l, UINT_PTR, DWORD_PTR) {
    if (message == WM_ERASEBKGND) return 1;
    if (message == WM_PRINTCLIENT) { DrawCheckbox(window, reinterpret_cast<HDC>(w)); return 0; }
    if (message == WM_PAINT) {
        PAINTSTRUCT paint; HDC dc = BeginPaint(window, &paint);
        DrawCheckbox(window, dc); EndPaint(window, &paint); return 0;
    }
    if (message == WM_NCDESTROY) RemoveWindowSubclass(window, CheckboxProc, 1);
    return DefSubclassProc(window, message, w, l);
}

void ApplyTheme() {
    RefreshWindowIcon();
    HBRUSH old = backgroundBrush;
    backgroundBrush = CreateSolidBrush(Background());
    if (mainWindow) SetClassLongPtrW(mainWindow, GCLP_HBRBACKGROUND, reinterpret_cast<LONG_PTR>(backgroundBrush));
    if (flyout) SetClassLongPtrW(flyout, GCLP_HBRBACKGROUND, reinterpret_cast<LONG_PTR>(backgroundBrush));
    if (old) DeleteObject(old);
    const BOOL useDark = darkMode;
    const DWM_WINDOW_CORNER_PREFERENCE corners = DWMWCP_ROUND;
    const COLORREF backgroundColor = Background(), textColor = Foreground();
    const COLORREF borderColor = DWMWA_COLOR_NONE;
    for (HWND window : {mainWindow, flyout}) if (window) {
        DwmSetWindowAttribute(window, DWMWA_USE_IMMERSIVE_DARK_MODE, &useDark, sizeof(useDark));
        DwmSetWindowAttribute(window, DWMWA_WINDOW_CORNER_PREFERENCE, &corners, sizeof(corners));
        DwmSetWindowAttribute(window, DWMWA_CAPTION_COLOR, &backgroundColor, sizeof(backgroundColor));
        DwmSetWindowAttribute(window, DWMWA_TEXT_COLOR, &textColor, sizeof(textColor));
        DwmSetWindowAttribute(window, DWMWA_BORDER_COLOR, &borderColor, sizeof(borderColor));
    }
    for (const auto& list : {&controls, &flyoutControls})
        for (const auto& c : *list)
            if (GetDlgCtrlID(c.window) == ThemeId)
                SetWindowTextW(c.window, darkMode ? L"Включить светлую тему" : L"Включить тёмную тему");
    for (HWND window : {mainWindow, flyout}) if (window)
        RedrawWindow(window, nullptr, nullptr, RDW_ERASE | RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_UPDATENOW);
    BrightnessHud::Update(brightness, brightnessLocked, darkMode);
}

void SetTheme(bool dark) {
    themeMode = dark ? ThemeMode::Dark : ThemeMode::Light;
    darkMode = dark; ApplyTheme(); saving = true; SaveSettings();
}

void SetAutomaticTheme() {
    themeMode = ThemeMode::Automatic; darkMode = SystemAppsDark();
    ApplyTheme(); saving = true; SaveSettings();
}

void SetPercentageIcon(bool enabled) {
    percentageIcon = enabled; UpdateTray(); saving = true; SaveSettings();
    if (settingsVisible) SyncSettings();
}

bool AnimationsEnabled() {
    ANIMATIONINFO info = {sizeof(info), 1};
    SystemParametersInfoW(SPI_GETANIMATION, sizeof(info), &info, 0);
    return info.iMinAnimate != 0;
}

void FadeHideWindow(HWND window) {
    constexpr wchar_t hiding[] = L"DimLite.Fading";
    if (!window || !IsWindowVisible(window) || GetPropW(window, hiding)) return;
    SetPropW(window, hiding, reinterpret_cast<HANDLE>(1));
    const ULONGLONG started = GetTickCount64();
    lastFadeAnimated = AnimateWindow(window, 180, AW_BLEND | AW_HIDE) != FALSE;
    lastFadeDurationMs = static_cast<DWORD>(GetTickCount64() - started);
    if (!lastFadeAnimated)
        ShowWindow(window, SW_HIDE);
    RemovePropW(window, hiding);
}

void HideMenuPanel() { FadeHideWindow(menuPanel); }

void HideFlyout() {
    if (mainWindow) KillTimer(mainWindow, FlyoutLeaveTimer);
    if (flyout && IsWindowVisible(flyout)) {
        FadeHideWindow(flyout); lastFlyoutDismissal = GetTickCount64(); SaveSettings();
    }
}

bool PointerInsideFlyout() {
    POINT cursor; RECT rect;
    GetCursorPos(&cursor); GetWindowRect(flyout, &rect);
    return PtInRect(&rect, cursor) != FALSE;
}

bool DraggingFlyout() {
    const HWND capture = GetCapture();
    return capture && GetAncestor(capture, GA_ROOT) == flyout;
}

void KeepFlyoutOpen() { if (mainWindow) KillTimer(mainWindow, FlyoutLeaveTimer); }

void CheckFlyoutMouseLeave() {
    if (IsWindowVisible(flyout) && !PointerInsideFlyout() && !DraggingFlyout())
        SetTimer(mainWindow, FlyoutLeaveTimer, 180, nullptr);
}

void SyncSettings() {
    if (!mainWindow || settingsControls.empty()) return;
    for (const auto& c : settingsControls) {
        const int id = GetDlgCtrlID(c.window);
        bool check = false;
        if (id == StartupId) check = StartupEnabled();
        else if (id == PercentageIconId) check = percentageIcon;
        else if (id == LockId) check = brightnessLocked;
        else if (id == ProfilesId) check = profilesEnabled;
        else if (id == AppProfileId) check = DimProfiles::Find(activeAppPath) >= 0;
        else continue;
        SendMessageW(c.window, BM_SETCHECK, check ? BST_CHECKED : BST_UNCHECKED, 0);
        InvalidateRect(c.window, nullptr, FALSE);
    }
    std::wstring name = activeAppName;
    if (name.size() > 30) name = name.substr(0, 29) + L"…";
    const std::wstring caption = name.empty() ? L"Запоминать для приложения" : L"Запоминать: " + name;
    SetWindowTextW(GetDlgItem(mainWindow, AppProfileId), caption.c_str());
    EnableWindow(GetDlgItem(mainWindow, AppProfileId), !activeAppPath.empty() && profilesEnabled);
    for (int id : {AutoThemeId, LightThemeId, DarkThemeId})
        InvalidateRect(GetDlgItem(mainWindow, id), nullptr, FALSE);
}

void SwitchSettings(bool show) {
    if (settingsVisible == show) { if (show) SyncSettings(); return; }
    settingsVisible = show;
    // Hide the old page before changing the height; keep the window's centre.
    for (const auto& c : controls) ShowWindow(c.window, show ? SW_HIDE : SW_SHOWNOACTIVATE);
    for (const auto& c : settingsControls) ShowWindow(c.window, show ? SW_SHOWNOACTIVATE : SW_HIDE);
    RECT current; GetWindowRect(mainWindow, &current);
    RECT size = {0,0,Scale(PanelWidth),Scale(show ? SettingsHeight : PanelHeight)};
    AdjustWindowRectExForDpi(&size, static_cast<DWORD>(GetWindowLongPtrW(mainWindow, GWL_STYLE)), FALSE,
        static_cast<DWORD>(GetWindowLongPtrW(mainWindow, GWL_EXSTYLE)), dpi);
    const int width = size.right - size.left, height = size.bottom - size.top;
    SetWindowPos(mainWindow, nullptr, (current.left + current.right - width) / 2,
        (current.top + current.bottom - height) / 2, width, height, SWP_NOZORDER | SWP_NOACTIVATE);
    SetWindowTextW(mainWindow, show ? L"DimLite — настройки" : L"DimLite");
    SyncSettings(); RedrawWindow(mainWindow, nullptr, nullptr, RDW_INVALIDATE | RDW_ALLCHILDREN);
}

void OpenPanel(bool settings = false) {
    KillTimer(mainWindow, FinishTrayTimer);
    trayMinimizeRequested = false;
    HideFlyout();
    HideMenuPanel();
    if (IsIconic(mainWindow)) ShowWindow(mainWindow, SW_RESTORE);
    SwitchSettings(settings);
    MONITORINFO info = {sizeof(info)};
    if (GetMonitorInfoW(MonitorFromWindow(mainWindow, MONITOR_DEFAULTTONEAREST), &info)) {
        RECT current; GetWindowRect(mainWindow, &current);
        const int width = current.right - current.left, height = current.bottom - current.top;
        const int x = std::clamp(current.left, info.rcWork.left, std::max(info.rcWork.left, info.rcWork.right - width));
        const int y = std::clamp(current.top, info.rcWork.top, std::max(info.rcWork.top, info.rcWork.bottom - height));
        SetWindowPos(mainWindow, nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
    }
    SendMessageW(startupBox, BM_SETCHECK, StartupEnabled() ? BST_CHECKED : BST_UNCHECKED, 0);
    ShowWindow(mainWindow, SW_SHOWNORMAL);
    // A hidden background launch may override the first ShowWindow command.
    if (!IsWindowVisible(mainWindow)) ShowWindow(mainWindow, SW_SHOWNORMAL);
    if (!mainPresented) { CenterMainWindow(); mainPresented = true; }
    SetForegroundWindow(mainWindow);
    SetFocus(settings ? GetDlgItem(mainWindow, BackId) : brightnessLocked ? GetDlgItem(mainWindow, SettingsId) : slider);
}

void HidePanel() {
    SaveSettings(); HideFlyout(); HideMenuPanel();
    if (!IsWindowVisible(mainWindow)) return;
    trayMinimizeRequested = true;
    if (AnimationsEnabled() && !IsIconic(mainWindow)) {
        // Let DWM finish the normal Windows minimize transition before removing
        // the taskbar button; the notification-area icon remains available.
        ShowWindow(mainWindow, SW_MINIMIZE);
        SetTimer(mainWindow, FinishTrayTimer, 280, nullptr);
    } else {
        KillTimer(mainWindow, FinishTrayTimer);
        FadeHideWindow(mainWindow);
        trayMinimizeRequested = false;
    }
}

void CenterMainWindow() {
    POINT cursor; GetCursorPos(&cursor);
    MONITORINFO monitor = {sizeof(monitor)};
    if (!GetMonitorInfoW(MonitorFromPoint(cursor, MONITOR_DEFAULTTOPRIMARY), &monitor)) return;
    // A second placement accounts for a DPI change when moving between displays.
    for (int pass = 0; pass < 2; ++pass) {
        RECT rect; GetWindowRect(mainWindow, &rect);
        const int x = monitor.rcWork.left + ((monitor.rcWork.right - monitor.rcWork.left) - (rect.right - rect.left)) / 2;
        const int y = monitor.rcWork.top + ((monitor.rcWork.bottom - monitor.rcWork.top) - (rect.bottom - rect.top)) / 2;
        SetWindowPos(mainWindow, nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
    }
}

void AddTooltips() {
    tooltips = CreateWindowExW(WS_EX_TOPMOST, TOOLTIPS_CLASSW, nullptr,
        WS_POPUP | TTS_ALWAYSTIP | TTS_NOPREFIX, CW_USEDEFAULT, CW_USEDEFAULT,
        CW_USEDEFAULT, CW_USEDEFAULT, mainWindow, nullptr, instance, nullptr);
    if (!tooltips) return;
    SendMessageW(tooltips, TTM_SETMAXTIPWIDTH, 0, Scale(280));
    for (const auto* list : {&controls, &flyoutControls, &settingsControls}) for (const auto& control : *list) {
        const int id = GetDlgCtrlID(control.window);
        const wchar_t* text = id == SettingsId ? L"Настройки" : id == BackId ? L"Назад к яркости" : id == ThemeId ? L"Сменить тему"
            : id == ResetId ? L"Снять затемнение · Ctrl+Alt+F12"
            : (id == SliderId || id == SliderId + 1) ? adjustHotkeyText : nullptr;
        if (!text) continue;
        TOOLINFOW info = {};
        info.cbSize = sizeof(info); info.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
        info.hwnd = GetParent(control.window); info.uId = reinterpret_cast<UINT_PTR>(control.window);
        info.lpszText = const_cast<LPWSTR>(text);
        SendMessageW(tooltips, TTM_ADDTOOLW, 0, reinterpret_cast<LPARAM>(&info));
    }
}

void DrawPanel(HWND window, bool compact, HDC dc) {
    RECT rect; GetClientRect(window, &rect); FillRect(dc, &rect, backgroundBrush);
    if (compact && activeStrip) return;
    const int margin = compact ? FlyScale(20) : Scale(24);
    const int y = compact ? FlyScale(111) : Scale(settingsVisible ? 342 : 130);
    HPEN pen = CreatePen(PS_SOLID, 1, Divider());
    HGDIOBJ old = SelectObject(dc, pen);
    MoveToEx(dc, margin, y, nullptr); LineTo(dc, rect.right - margin, y);
    SelectObject(dc, old); DeleteObject(pen);
}

void PaintPanel(HWND window, bool compact) {
    PAINTSTRUCT paint; HDC dc = BeginPaint(window, &paint);
    DrawPanel(window, compact, dc); EndPaint(window, &paint);
}

void OpenFlyout() {
    ShowWindow(mainWindow, SW_HIDE);
    HideMenuPanel();
    NOTIFYICONIDENTIFIER icon = {};
    icon.cbSize = sizeof(icon); icon.hWnd = mainWindow; icon.uID = 1;
    RECT anchor = {};
    if (FAILED(Shell_NotifyIconGetRect(&icon, &anchor))) {
        POINT cursor; GetCursorPos(&cursor);
        anchor = {cursor.x, cursor.y, cursor.x + 1, cursor.y + 1};
    }
    APPBARDATA taskbar = {}; taskbar.cbSize = sizeof(taskbar);
    const POINT anchorCenter = {(anchor.left + anchor.right) / 2, (anchor.top + anchor.bottom) / 2};
    const bool taskbarFound = SHAppBarMessage(ABM_GETTASKBARPOS, &taskbar) != 0;
    activeStrip = taskbarFound && PtInRect(&taskbar.rc, anchorCenter);
    // An auto-hidden taskbar can report only its thin reveal edge. The icon
    // keeps its expanded coordinates, so include the normal taskbar thickness.
    APPBARDATA taskbarState = {}; taskbarState.cbSize = sizeof(taskbarState);
    if (!activeStrip && taskbarFound && (SHAppBarMessage(ABM_GETSTATE, &taskbarState) & ABS_AUTOHIDE)) {
        RECT band = taskbar.rc;
        const int minimum = 3 * GetSystemMetricsForDpi(SM_CYSMICON, flyoutDpi);
        if (band.right - band.left >= band.bottom - band.top) {
            if (anchorCenter.y <= band.top) band.top = std::min<LONG>(band.top, band.bottom - minimum);
            else band.bottom = std::max<LONG>(band.bottom, band.top + minimum);
        } else {
            if (anchorCenter.x <= band.left) band.left = std::min<LONG>(band.left, band.right - minimum);
            else band.right = std::max<LONG>(band.right, band.left + minimum);
        }
        activeStrip = PtInRect(&band, anchorCenter) != FALSE;
    }
    MONITORINFO monitor = {sizeof(monitor)};
    GetMonitorInfoW(MonitorFromRect(&anchor, MONITOR_DEFAULTTONEAREST), &monitor);
    // Moving the hidden window onto the target display establishes its DPI first.
    SetWindowPos(flyout, nullptr, anchor.left, anchor.top, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
    LayoutFlyout();
    const int width = FlyScale(activeStrip ? StripWidth : FlyoutWidth);
    const int height = FlyScale(activeStrip ? StripHeight : FlyoutHeight), gap = FlyScale(10);
    int x = (anchor.left + anchor.right - width) / 2, y = anchor.top - height - gap;
    if (anchor.bottom <= monitor.rcWork.top) y = anchor.bottom + gap;
    else if (anchor.left >= monitor.rcWork.right) { x = anchor.left - width - gap; y = anchor.bottom - height; }
    else if (anchor.right <= monitor.rcWork.left) { x = anchor.right + gap; y = anchor.bottom - height; }
    RECT safe = monitor.rcWork; InflateRect(&safe, -FlyScale(8), -FlyScale(8));
    x = std::clamp<int>(x, safe.left, std::max<LONG>(safe.left, safe.right - width));
    y = std::clamp<int>(y, safe.top, std::max<LONG>(safe.top, safe.bottom - height));
    SetWindowPos(flyout, HWND_TOPMOST, x, y, width, height, SWP_SHOWWINDOW);
    SetForegroundWindow(flyout); SetFocus(activeStrip ? flyoutVerticalSlider : flyoutSlider);
    if (!PointerInsideFlyout()) SetTimer(mainWindow, FlyoutLeaveTimer, 1200, nullptr);
}

void HandleCommand(int command) {
    switch (command) {
    case OpenId: OpenPanel(); break;
    case SettingsId: OpenPanel(true); break;
    case BackId: OpenPanel(); break;
    case NightId: ApplyLevel(20); break;
    case EveningId: ApplyLevel(50); break;
    case ResetId: ResetBrightness(); break;
    case ThemeId: SetTheme(!darkMode); break;
    case LightThemeId: SetTheme(false); break;
    case DarkThemeId: SetTheme(true); break;
    case AutoThemeId:
        SetAutomaticTheme();
        break;
    case PercentageIconId: SetPercentageIcon(!percentageIcon); break;
    case LockId: SetBrightnessLock(!brightnessLocked); break;
    case ProfilesId: SetProfilesEnabled(!profilesEnabled); break;
    case AppProfileId:
        if (!activeAppPath.empty()) {
            if (DimProfiles::Find(activeAppPath) >= 0) {
                DimProfiles::Remove(activeAppPath);
                if (profilesEnabled && !brightnessLocked) ApplyLevel(manualBrightness, false, true);
            } else if (DimProfiles::Set(activeAppPath, brightness)) profilesEnabled = true;
            saving = true; SetTimer(mainWindow, SaveTimer, 400, nullptr);
        }
        break;
    case StartupId:
        if (!SetStartup(!StartupEnabled())) MessageBoxW(mainWindow,
            L"Не удалось изменить автозапуск.", L"DimLite", MB_OK | MB_ICONINFORMATION);
        SendMessageW(startupBox, BM_SETCHECK, StartupEnabled() ? BST_CHECKED : BST_UNCHECKED, 0);
        InvalidateRect(startupBox, nullptr, FALSE);
        break;
    case ExitId: DestroyWindow(mainWindow); break;
    }
    if (command != ExitId && settingsVisible) SyncSettings();
}

void HandleMenuCommand(int command) {
    if (command == OpenId || command == SettingsId || command == ExitId) {
        HideMenuPanel();
        HandleCommand(command);
        return;
    }
    HandleCommand(command);
    // Settings and reset stay in place so several items can be changed in one
    // visit. Repaint checkmarks without rebuilding fonts or moving the menu.
    RedrawWindow(menuPanel, nullptr, nullptr, RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_UPDATENOW);
}

COLORREF MenuBackground() { return menuHighContrast ? GetSysColor(COLOR_MENU) : menuDark ? RGB(39, 39, 42) : RGB(250, 250, 252); }
COLORREF MenuText() { return menuHighContrast ? GetSysColor(COLOR_MENUTEXT) : menuDark ? RGB(244, 244, 246) : RGB(30, 31, 35); }

LRESULT PaintContextButton(const DRAWITEMSTRUCT* item) {
    const auto scale = [](int n) { return MulDiv(n, static_cast<int>(menuDpi), 96); };
    RECT rect = item->rcItem;
    HBRUSH brush = CreateSolidBrush(MenuBackground());
    FillRect(item->hDC, &rect, brush); DeleteObject(brush);
    COLORREF textColor = MenuText();
    const bool disabled = !IsWindowEnabled(item->hwndItem);
    const bool selected = !disabled && (GetPropW(item->hwndItem, HoverProperty) || (item->itemState & ODS_SELECTED) ||
        (menuKeyboard && GetFocus() == item->hwndItem));
    if (selected) {
        RECT selection = rect; InflateRect(&selection, 0, -scale(1));
        const COLORREF highlight = menuHighContrast ? GetSysColor(COLOR_HIGHLIGHT)
            : menuDark ? RGB(60, 60, 65) : RGB(232, 234, 240);
        FilledRoundRect(item->hDC, selection, highlight, highlight, scale(4));
        if (menuHighContrast) textColor = GetSysColor(COLOR_HIGHLIGHTTEXT);
    }
    rect.left += scale(14); rect.right -= scale(12);
    if (disabled) textColor = menuHighContrast ? GetSysColor(COLOR_GRAYTEXT) : menuDark ? RGB(142,142,149) : RGB(132,132,140);
    SetBkMode(item->hDC, TRANSPARENT); SetTextColor(item->hDC, textColor);
    HGDIOBJ old = SelectObject(item->hDC, menuFont);
    wchar_t caption[128] = {};
    GetWindowTextW(item->hwndItem, caption, ARRAYSIZE(caption));
    DrawTextW(item->hDC, caption, -1, &rect, DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_HIDEPREFIX | DT_END_ELLIPSIS);
    SelectObject(item->hDC, old); return TRUE;
}

void UpdateMenuAppearance() {
    menuDark = SystemMenuDark();
    HIGHCONTRASTW contrast = {sizeof(contrast)};
    SystemParametersInfoW(SPI_GETHIGHCONTRAST, sizeof(contrast), &contrast, 0);
    menuHighContrast = (contrast.dwFlags & HCF_HIGHCONTRASTON) != 0;
    NONCLIENTMETRICSW metrics = {sizeof(metrics)};
    HFONT oldFont = menuFont;
    menuFont = SystemParametersInfoForDpi(SPI_GETNONCLIENTMETRICS, sizeof(metrics), &metrics, 0, menuDpi)
        ? CreateFontIndirectW(&metrics.lfMenuFont) : Font(14, menuDpi);
    HDC measure = GetDC(menuPanel);
    HGDIOBJ previousFont = SelectObject(measure, menuFont);
    LONG widest = 0;
    for (const auto& control : menuControls) {
        wchar_t caption[128] = {}; SIZE extent = {};
        GetWindowTextW(control.window, caption, ARRAYSIZE(caption));
        GetTextExtentPoint32W(measure, caption, lstrlenW(caption), &extent);
        widest = std::max(widest, extent.cx);
    }
    SelectObject(measure, previousFont); ReleaseDC(menuPanel, measure);
    menuPixelWidth = std::max<int>(MulDiv(148, menuDpi, 96), widest + MulDiv(40, menuDpi, 96));
    for (const auto& control : menuControls) {
        SendMessageW(control.window, WM_SETFONT, reinterpret_cast<WPARAM>(menuFont), TRUE);
        MoveWindow(control.window, MulDiv(control.x, menuDpi, 96), MulDiv(control.y, menuDpi, 96),
            menuPixelWidth - MulDiv(12, menuDpi, 96), MulDiv(control.h, menuDpi, 96), TRUE);
    }
    if (oldFont) DeleteObject(oldFont);
    HBRUSH oldBrush = menuBackgroundBrush;
    menuBackgroundBrush = CreateSolidBrush(MenuBackground());
    SetClassLongPtrW(menuPanel, GCLP_HBRBACKGROUND, reinterpret_cast<LONG_PTR>(menuBackgroundBrush));
    if (oldBrush) DeleteObject(oldBrush);
    const BOOL useDark = menuDark;
    const DWM_WINDOW_CORNER_PREFERENCE corners = DWMWCP_ROUND;
    const COLORREF border = DWMWA_COLOR_NONE;
    DwmSetWindowAttribute(menuPanel, DWMWA_USE_IMMERSIVE_DARK_MODE, &useDark, sizeof(useDark));
    DwmSetWindowAttribute(menuPanel, DWMWA_WINDOW_CORNER_PREFERENCE, &corners, sizeof(corners));
    DwmSetWindowAttribute(menuPanel, DWMWA_BORDER_COLOR, &border, sizeof(border));
    RedrawWindow(menuPanel, nullptr, nullptr, RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_UPDATENOW);
}

void TrayMenu() {
    HideFlyout(); menuKeyboard = false;
    POINT point; GetCursorPos(&point);
    MONITORINFO monitor = {sizeof(monitor)};
    GetMonitorInfoW(MonitorFromPoint(point, MONITOR_DEFAULTTONEAREST), &monitor);
    SetWindowPos(menuPanel, nullptr, point.x, point.y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
    UpdateMenuAppearance();
    const int width = menuPixelWidth, height = MulDiv(MenuHeight, menuDpi, 96);
    const int gap = MulDiv(8, menuDpi, 96);
    int x = point.x, y = point.y;
    if (y + height > monitor.rcWork.bottom - gap) y = point.y - height;
    x = std::clamp<int>(x, monitor.rcWork.left + gap, std::max<LONG>(monitor.rcWork.left + gap, monitor.rcWork.right - gap - width));
    y = std::clamp<int>(y, monitor.rcWork.top + gap, std::max<LONG>(monitor.rcWork.top + gap, monitor.rcWork.bottom - gap - height));
    SetWindowPos(menuPanel, HWND_TOPMOST, x, y, width, height, SWP_SHOWWINDOW);
    SetForegroundWindow(menuPanel);
    if (!menuControls.empty()) SetFocus(menuControls[0].window);
}

void DrawMenuPanel(HWND window, HDC dc) {
    RECT rect; GetClientRect(window, &rect);
    if (menuBackgroundBrush) FillRect(dc, &rect, menuBackgroundBrush);
    HPEN pen = CreatePen(PS_SOLID, 1, menuDark ? RGB(62, 62, 67) : RGB(224, 225, 230));
    HGDIOBJ old = SelectObject(dc, pen);
    const int margin = MulDiv(14, menuDpi, 96);
    for (int position : {74, 114}) {
        const int y = MulDiv(position, menuDpi, 96);
        MoveToEx(dc, margin, y, nullptr); LineTo(dc, rect.right - margin, y);
    }
    SelectObject(dc, old); DeleteObject(pen);
}

LRESULT CALLBACK MenuProc(HWND window, UINT message, WPARAM w, LPARAM l) {
    switch (message) {
    case WM_NCCALCSIZE: return 0;
    case WM_NCHITTEST: return HTCLIENT;
    case WM_CREATE: {
        menuPanel = window;
        const int ids[] = {OpenId, SettingsId, ResetId, ExitId};
        const wchar_t* labels[] = {L"Открыть DimLite", L"Настройки", L"Сбросить яркость", L"Выход"};
        const int positions[] = {6, 38, 78, 118};
        for (int i = 0; i < 4; ++i) {
            HWND button = CreateWindowExW(0, L"BUTTON", labels[i], WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
                6, positions[i], MenuWidth - 12, 32, window,
                reinterpret_cast<HMENU>(static_cast<INT_PTR>(ids[i])), instance, nullptr);
            menuControls.push_back({button, 6, positions[i], MenuWidth - 12, 32});
            SetWindowSubclass(button, ButtonProc, 1, 0);
        }
        return 0;
    }
    case WM_PAINT: {
        PAINTSTRUCT paint; HDC dc = BeginPaint(window, &paint);
        DrawMenuPanel(window, dc); EndPaint(window, &paint); return 0;
    }
    case WM_PRINTCLIENT: DrawMenuPanel(window, reinterpret_cast<HDC>(w)); return 0;
    case WM_COMMAND:
        HandleMenuCommand(LOWORD(w)); return 0;
    case WM_DRAWITEM: return PaintContextButton(reinterpret_cast<DRAWITEMSTRUCT*>(l));
    case WM_ACTIVATE: if (LOWORD(w) == WA_INACTIVE) HideMenuPanel(); return 0;
    case WM_CLOSE: HideMenuPanel(); return 0;
    case WM_DPICHANGED: {
        menuDpi = HIWORD(w); UpdateMenuAppearance();
        const RECT* rect = reinterpret_cast<RECT*>(l);
        SetWindowPos(window, nullptr, rect->left, rect->top, menuPixelWidth,
            MulDiv(MenuHeight, menuDpi, 96), SWP_NOZORDER | SWP_NOACTIVATE); return 0;
    }
    }
    return DefWindowProcW(window, message, w, l);
}

bool WriteLiveStatus(const wchar_t* path) {
    MAGCOLOREFFECT actual = {}, expected = EffectAt(static_cast<float>(brightness));
    bool matches = MagGetFullscreenColorEffect(&actual) != 0;
    for (int row = 0; row < 5; ++row) for (int col = 0; col < 5; ++col)
        matches &= std::fabs(actual.transform[row][col] - expected.transform[row][col]) < 0.0001f;
    const MAGCOLOREFFECT applied = EffectAt(appliedBrightness);
    bool appliedMatches = true;
    for (int row = 0; row < 5; ++row) for (int col = 0; col < 5; ++col)
        appliedMatches &= std::fabs(actual.transform[row][col] - applied.transform[row][col]) < 0.0001f;
    const bool guardAlive = guardProcess && WaitForSingleObject(guardProcess, 0) == WAIT_TIMEOUT;
    RECT bounds = {}, work = {};
    GetWindowRect(mainWindow, &bounds);
    MONITORINFO monitor = {sizeof(monitor)};
    if (GetMonitorInfoW(MonitorFromWindow(mainWindow, MONITOR_DEFAULTTONEAREST), &monitor)) work = monitor.rcWork;
    DWM_WINDOW_CORNER_PREFERENCE corners = DWMWCP_DEFAULT;
    DwmGetWindowAttribute(flyout, DWMWA_WINDOW_CORNER_PREFERENCE, &corners, sizeof(corners));
    char text[4096];
    StringCchPrintfA(text, ARRAYSIZE(text),
        "{\n  \"brightness\": %d,\n  \"backend\": \"desktop_color_transform\",\n"
        "  \"matrix_matches_requested_level\": %s,\n  \"panel_visible\": %s,\n"
        "  \"flyout_visible\": %s,\n  \"main_slider\": %d,\n  \"flyout_slider\": %d,\n"
        "  \"theme\": \"%s\",\n  \"reset_hotkey\": %s,\n  \"recovery_guard_running\": %s,\n"
        "  \"menu_visible\": %s,\n  \"system_menu_theme\": \"%s\",\n"
        "  \"main_bounds\": [%ld,%ld,%ld,%ld],\n  \"main_work_area\": [%ld,%ld,%ld,%ld],\n"
        "  \"center_offset_twice\": [%ld,%ld],\n  \"flyout_corner_preference\": %d,\n"
        "  \"flyout_mode\": \"%s\",\n  \"vertical_slider_brightness\": %d,\n  \"menu_width\": %d,\n"
        "  \"last_fade_animated\": %s,\n  \"last_fade_duration_ms\": %lu,\n"
        "  \"theme_mode\": \"%s\",\n  \"percentage_icon\": %s,\n"
        "  \"wheel_hook_running\": %s,\n  \"native_wheel_events\": %ld,\n  \"native_middle_events\": %ld,\n"
        "  \"adjust_hotkeys_registered\": %s,\n  \"hotkey_up_vk\": %u,\n  \"hotkey_down_vk\": %u,\n  \"hotkey_modifiers\": %u,\n"
        "  \"window_minimized\": %s,\n  \"tray_minimize_requested\": %s,\n"
        "  \"applied_brightness\": %.3f,\n  \"transition_active\": %s,\n  \"matrix_matches_applied_level\": %s,\n"
        "  \"brightness_locked\": %s,\n  \"sliders_enabled\": %s,\n  \"base_brightness\": %d,\n"
        "  \"profiles_enabled\": %s,\n  \"profile_count\": %zu,\n  \"active_profile\": %s,\n  \"foreground_hook_running\": %s,\n"
        "  \"hud_visible\": %s,\n  \"hud_alpha\": %u,\n  \"hud_rendered\": %s,\n  \"hud_has_focus\": %s,\n"
        "  \"version\": \"1.0.0\",\n  \"settings_page\": %s,\n  \"tray_menu_items\": %zu,\n"
        "  \"hud_interactive\": %s,\n  \"hud_dragging\": %s,\n  \"hud_hide_timer\": %s,\n  \"hud_fading\": %s,\n  \"hud_level\": %d\n}\n",
        brightness, matches ? "true" : "false", IsWindowVisible(mainWindow) ? "true" : "false",
        IsWindowVisible(flyout) ? "true" : "false", static_cast<int>(SendMessageW(slider, TBM_GETPOS, 0, 0)),
        static_cast<int>(SendMessageW(flyoutSlider, TBM_GETPOS, 0, 0)), darkMode ? "dark" : "light",
        hotkeyRegistered ? "true" : "false", guardAlive ? "true" : "false",
        IsWindowVisible(menuPanel) ? "true" : "false", SystemMenuDark() ? "dark" : "light",
        bounds.left, bounds.top, bounds.right, bounds.bottom, work.left, work.top, work.right, work.bottom,
        bounds.left + bounds.right - work.left - work.right, bounds.top + bounds.bottom - work.top - work.bottom,
        static_cast<int>(corners), activeStrip ? "vertical" : "wide",
        105 - static_cast<int>(SendMessageW(flyoutVerticalSlider, TBM_GETPOS, 0, 0)), menuPixelWidth,
        lastFadeAnimated ? "true" : "false", lastFadeDurationMs,
        themeMode == ThemeMode::Automatic ? "auto" : themeMode == ThemeMode::Dark ? "dark" : "light",
        percentageIcon ? "true" : "false", InterlockedCompareExchange(&mouseHookActive, 0, 0) ? "true" : "false",
        InterlockedCompareExchange(&nativeWheelEvents, 0, 0), InterlockedCompareExchange(&nativeMiddleEvents, 0, 0),
        adjustHotkeysRegistered ? "true" : "false", adjustHotkeyUp, adjustHotkeyDown, adjustHotkeyModifiers,
        IsIconic(mainWindow) ? "true" : "false", trayMinimizeRequested ? "true" : "false",
        static_cast<double>(appliedBrightness), transitionActive ? "true" : "false", appliedMatches ? "true" : "false",
        brightnessLocked ? "true" : "false", IsWindowEnabled(slider) && IsWindowEnabled(flyoutSlider) && IsWindowEnabled(flyoutVerticalSlider) ? "true" : "false",
        manualBrightness, profilesEnabled ? "true" : "false", DimProfiles::entries.size(),
        DimProfiles::Find(activeAppPath) >= 0 ? "true" : "false", foregroundHook ? "true" : "false",
        BrightnessHud::window && IsWindowVisible(BrightnessHud::window) ? "true" : "false", static_cast<UINT>(BrightnessHud::alpha),
        BrightnessHud::rendered ? "true" : "false", BrightnessHud::window && GetForegroundWindow() == BrightnessHud::window ? "true" : "false",
        settingsVisible ? "true" : "false", menuControls.size(),
        BrightnessHud::window && !(GetWindowLongPtrW(BrightnessHud::window,GWL_EXSTYLE) & WS_EX_TRANSPARENT) ? "true" : "false",
        BrightnessHud::dragging ? "true" : "false", BrightnessHud::hideTimerActive ? "true" : "false", BrightnessHud::fading ? "true" : "false", BrightnessHud::currentLevel);
    HANDLE file = CreateFileW(path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;
    DWORD written = 0, length = static_cast<DWORD>(lstrlenA(text));
    const bool ok = WriteFile(file, text, length, &written, nullptr) && written == length;
    CloseHandle(file); return ok;
}

LRESULT ControlColors(WPARAM w) {
    const HDC dc = reinterpret_cast<HDC>(w);
    SetBkMode(dc, TRANSPARENT); SetTextColor(dc, Foreground());
    return reinterpret_cast<LRESULT>(backgroundBrush);
}

LRESULT CALLBACK FlyoutProc(HWND window, UINT message, WPARAM w, LPARAM l) {
    switch (message) {
    case WM_NCCALCSIZE: return 0;
    case WM_NCHITTEST: return HTCLIENT;
    case WM_CREATE:
        flyout = window;
        AddControl(true, L"STATIC", L"Яркость", 0, 20, 20, 150, 28);
        flyoutValue = AddControl(true, L"STATIC", L"100%", SS_RIGHT, 222, 14, 62, 36);
        AddControl(true, L"BUTTON", L"Сменить тему", WS_TABSTOP | BS_OWNERDRAW, 290, 15, 30, 30, ThemeId);
        flyoutSlider = AddControl(true, TRACKBAR_CLASSW, L"Яркость", WS_TABSTOP | TBS_NOTICKS, 16, 62, 308, 34, SliderId);
        SetWindowSubclass(flyoutSlider, SliderProc, 1, 0);
        SendMessageW(flyoutSlider, TBM_SETRANGE, TRUE, MAKELPARAM(5, 100));
        SendMessageW(flyoutSlider, TBM_SETPAGESIZE, 0, 5);
        AddControl(true, L"BUTTON", L"Сброс", WS_TABSTOP | BS_OWNERDRAW, 20, 124, 96, 30, ResetId);
        AddControl(true, L"BUTTON", L"Настройки", WS_TABSTOP | BS_OWNERDRAW, 290, 124, 30, 30, SettingsId);
        flyoutVerticalSlider = AddControl(true, TRACKBAR_CLASSW, L"Яркость", WS_TABSTOP | TBS_VERT | TBS_REVERSED | TBS_NOTICKS,
            22, 48, 32, 152, SliderId + 1);
        SetWindowSubclass(flyoutVerticalSlider, SliderProc, 1, 0);
        SendMessageW(flyoutVerticalSlider, TBM_SETRANGE, TRUE, MAKELPARAM(5, 100));
        SendMessageW(flyoutVerticalSlider, TBM_SETPAGESIZE, 0, 5);
        ApplyFlyoutFonts(); return 0;
    case WM_PAINT: PaintPanel(window, true); return 0;
    case WM_PRINTCLIENT: DrawPanel(window, true, reinterpret_cast<HDC>(w)); return 0;
    case WM_MOUSEMOVE: {
        KeepFlyoutOpen();
        TRACKMOUSEEVENT track = {sizeof(track), TME_LEAVE, window, 0};
        TrackMouseEvent(&track); return 0;
    }
    case WM_MOUSELEAVE: CheckFlyoutMouseLeave(); return 0;
    case WM_HSCROLL:
        if (reinterpret_cast<HWND>(l) == flyoutSlider) ApplyLevel(static_cast<int>(SendMessageW(flyoutSlider, TBM_GETPOS, 0, 0)));
        return 0;
    case WM_VSCROLL:
        if (reinterpret_cast<HWND>(l) == flyoutVerticalSlider)
            ApplyLevel(105 - static_cast<int>(SendMessageW(flyoutVerticalSlider, TBM_GETPOS, 0, 0)));
        return 0;
    case WM_COMMAND: HandleCommand(LOWORD(w)); return 0;
    case WM_ACTIVATE:
        if (LOWORD(w) == WA_INACTIVE) HideFlyout();
        return 0;
    case WM_CLOSE: HideFlyout(); return 0;
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORBTN: return ControlColors(w);
    case WM_DRAWITEM: return PaintButton(reinterpret_cast<DRAWITEMSTRUCT*>(l));
    case WM_DPICHANGED: {
        flyoutDpi = HIWORD(w); ApplyFlyoutFonts(); LayoutFlyout();
        const RECT* rect = reinterpret_cast<RECT*>(l);
        SetWindowPos(window, nullptr, rect->left, rect->top, FlyScale(activeStrip ? StripWidth : FlyoutWidth),
            FlyScale(activeStrip ? StripHeight : FlyoutHeight), SWP_NOZORDER | SWP_NOACTIVATE);
        return 0;
    }
    }
    return DefWindowProcW(window, message, w, l);
}

LRESULT CALLBACK MainProc(HWND window, UINT message, WPARAM w, LPARAM l) {
    if (taskbarCreated && message == taskbarCreated) { confirmedTrayRoot = nullptr; UpdateTray(true); RefreshTrayBounds(); return 0; }
    switch (message) {
    case WM_CREATE:
        mainWindow = window;
        AddControl(false, L"STATIC", L"Яркость", 0, 24, 24, 180, 30);
        valueLabel = AddControl(false, L"STATIC", L"100%", SS_RIGHT, 238, 18, 74, 38);
        AddControl(false, L"BUTTON", L"Сменить тему", WS_TABSTOP | BS_OWNERDRAW, 324, 22, 32, 32, ThemeId);
        slider = AddControl(false, TRACKBAR_CLASSW, L"Яркость", WS_TABSTOP | TBS_NOTICKS, 20, 76, 340, 36, SliderId);
        SetWindowSubclass(slider, SliderProc, 1, 0);
        SendMessageW(slider, TBM_SETRANGE, TRUE, MAKELPARAM(5, 100));
        SendMessageW(slider, TBM_SETPAGESIZE, 0, 5);
        AddControl(false, L"BUTTON", L"Настройки", WS_TABSTOP | BS_OWNERDRAW, 24, 146, 36, 36, SettingsId);
        AddControl(false, L"BUTTON", L"Сброс", WS_TABSTOP | BS_OWNERDRAW, 260, 146, 96, 36, ResetId);
        AddSetting(L"BUTTON", L"Назад", WS_TABSTOP | BS_OWNERDRAW, 20, 20, 32, 32, BackId);
        settingsHeading = AddSetting(L"STATIC", L"Настройки", 0, 64, 20, 292, 32);
        AddSetting(L"STATIC", L"Тема", 0, 24, 68, 332, 22);
        AddSetting(L"BUTTON", L"Системная", WS_TABSTOP | BS_OWNERDRAW, 24, 96, 104, 32, AutoThemeId);
        AddSetting(L"BUTTON", L"Светлая", WS_TABSTOP | BS_OWNERDRAW, 138, 96, 104, 32, LightThemeId);
        AddSetting(L"BUTTON", L"Тёмная", WS_TABSTOP | BS_OWNERDRAW, 252, 96, 104, 32, DarkThemeId);
        AddSetting(L"BUTTON", L"Процент на значке", WS_TABSTOP | BS_AUTOCHECKBOX, 24, 144, 332, 28, PercentageIconId);
        AddSetting(L"BUTTON", L"Зафиксировать яркость", WS_TABSTOP | BS_AUTOCHECKBOX, 24, 184, 332, 28, LockId);
        AddSetting(L"BUTTON", L"Профили приложений", WS_TABSTOP | BS_AUTOCHECKBOX, 24, 224, 332, 28, ProfilesId);
        AddSetting(L"BUTTON", L"Запоминать для приложения", WS_TABSTOP | BS_AUTOCHECKBOX, 44, 258, 312, 28, AppProfileId);
        startupBox = AddSetting(L"BUTTON", L"Запускать с Windows", WS_TABSTOP | BS_AUTOCHECKBOX, 24, 298, 332, 28, StartupId);
        AddSetting(L"STATIC", VersionText, 0, 24, 358, 332, 22, VersionId);
        shortcutLabel = AddSetting(L"STATIC", L"Снять затемнение: Ctrl+Alt+F12", 0, 24, 390, 332, 20);
        ApplyFonts(); return 0;
    case WM_PAINT: PaintPanel(window, false); return 0;
    case WM_PRINTCLIENT: DrawPanel(window, false, reinterpret_cast<HDC>(w)); return 0;
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORBTN: return ControlColors(w);
    case WM_DRAWITEM: return PaintButton(reinterpret_cast<DRAWITEMSTRUCT*>(l));
    case WM_HSCROLL:
        if (reinterpret_cast<HWND>(l) == slider) ApplyLevel(static_cast<int>(SendMessageW(slider, TBM_GETPOS, 0, 0)));
        return 0;
    case WM_COMMAND: HandleCommand(LOWORD(w)); return 0;
    case WM_HOTKEY:
        if (w == 1) { ResetBrightness(); OpenPanel(); }
        if (w == 2 || w == 3) {
            wheelRemainder = 0; ApplyLevel(brightness + (w == 2 ? 5 : -5));
            BrightnessHud::Show(brightness, brightnessLocked, darkMode);
        }
        return 0;
    case WM_TIMER:
        if (w == TransitionTimer) { TickBrightness(); return 0; }
        KillTimer(window, w);
        if (w == TrayUpdateTimer) { trayUpdatePending = false; UpdateTray(); }
        if (w == SaveTimer) SaveSettings();
        if (w == DisplayTimer) ApplyLevel(brightness, true);
        if (w == FlyoutLeaveTimer && IsWindowVisible(flyout) && !PointerInsideFlyout() && !DraggingFlyout()) HideFlyout();
        if (w == FinishTrayTimer && trayMinimizeRequested && IsIconic(window)) {
            ShowWindow(window, SW_HIDE); trayMinimizeRequested = false;
        }
        return 0;
    case WM_DISPLAYCHANGE: SetTimer(window, DisplayTimer, 250, nullptr); return 0;
    case WM_SETTINGCHANGE:
    case WM_THEMECHANGED:
        if (themeMode == ThemeMode::Automatic && darkMode != SystemAppsDark()) {
            darkMode = SystemAppsDark(); ApplyTheme();
        }
        if (systemTrayDark != SystemMenuDark()) {
            systemTrayDark = !systemTrayDark; RefreshWindowIcon(); UpdateTray();
        }
        RefreshTrayBounds(); return 0;
    case WM_POWERBROADCAST:
        if (w == PBT_APMRESUMEAUTOMATIC) SetTimer(window, DisplayTimer, 250, nullptr);
        return TRUE;
    case WM_DPICHANGED: {
        dpi = HIWORD(w); ApplyFonts(); Layout(false);
        const RECT* rect = reinterpret_cast<RECT*>(l);
        SetWindowPos(window, nullptr, rect->left, rect->top, rect->right - rect->left,
            rect->bottom - rect->top, SWP_NOZORDER | SWP_NOACTIVATE); return 0;
    }
    case WM_SIZE:
        if (w == SIZE_MINIMIZED && trayMinimizeRequested) {
            SaveSettings(); HideFlyout(); HideMenuPanel();
            if (AnimationsEnabled()) SetTimer(window, FinishTrayTimer, 280, nullptr);
            else ShowWindow(window, SW_HIDE);
        }
        if (w == SIZE_RESTORED) { trayMinimizeRequested = false; KillTimer(window, FinishTrayTimer); }
        return 0;
    case WM_SYSCOMMAND:
        if ((w & 0xFFF0) == SC_MINIMIZE) {
            POINT cursor; GetCursorPos(&cursor);
            const bool captionButton = GetAncestor(WindowFromPoint(cursor), GA_ROOT) == window &&
                DefWindowProcW(window, WM_NCHITTEST, 0, MAKELPARAM(cursor.x, cursor.y)) == HTMINBUTTON;
            if (captionButton) { HidePanel(); return 0; }
            // Taskbar clicks and ordinary Windows minimize commands keep their
            // taskbar button, so a second click can restore the running window.
            trayMinimizeRequested = false; KillTimer(window, FinishTrayTimer);
        }
        break;
    case WM_CLOSE: HidePanel(); return 0;
    case WM_QUERYENDSESSION: SaveSettings(); return TRUE;
    case TrayMessage:
        if ((LOWORD(l) == WM_MOUSEMOVE || LOWORD(l) == NIN_POPUPOPEN) &&
            (!trayBoundsValid || GetTickCount64() - lastTrayBoundsRefresh > 80)) RefreshTrayBounds(true);
        if (LOWORD(l) == WM_MBUTTONUP) {
            InterlockedIncrement(&nativeMiddleEvents); wheelRemainder = 0; ApplyLevel(100);
            BrightnessHud::Show(brightness, brightnessLocked, darkMode); return 0;
        }
        if (LOWORD(l) == WM_CONTEXTMENU) { lastTraySelect = 0; TrayMenu(); return 0; }
        if (LOWORD(l) == WM_LBUTTONDBLCLK) {
            lastTraySelect = 0;
            ignoreTraySelectUntil = GetTickCount64() + GetDoubleClickTime();
            OpenPanel(); return 0;
        }
        if (LOWORD(l) == NIN_SELECT || LOWORD(l) == NIN_KEYSELECT) {
            if (LOWORD(l) == NIN_SELECT) {
                const ULONGLONG now = GetTickCount64();
                if (now < ignoreTraySelectUntil) return 0;
                POINT cursor; GetCursorPos(&cursor);
                const bool doubleClick = lastTraySelect && now - lastTraySelect <= GetDoubleClickTime() &&
                    std::abs(cursor.x - lastTraySelectPoint.x) <= GetSystemMetrics(SM_CXDOUBLECLK) / 2 &&
                    std::abs(cursor.y - lastTraySelectPoint.y) <= GetSystemMetrics(SM_CYDOUBLECLK) / 2;
                if (doubleClick) {
                    lastTraySelect = 0; ignoreTraySelectUntil = now + GetDoubleClickTime();
                    OpenPanel(); return 0;
                }
                lastTraySelect = now; lastTraySelectPoint = cursor;
            }
            if (IsWindowVisible(flyout)) HideFlyout();
            else if (LOWORD(l) == NIN_KEYSELECT || GetTickCount64() - lastFlyoutDismissal > 200) OpenFlyout();
        }
        return 0;
    case SetLevelMessage: return ApplyLevel(static_cast<int>(w)) ? 1 : 0;
    case ShowMessage: OpenPanel(); return 1;
    case HideMessage: HidePanel(); return 1;
    case QuitMessage: DestroyWindow(window); return 1;
    case FlyoutMessage: OpenFlyout(); return 1;
    case ThemeMessage: if (w == 2) SetAutomaticTheme(); else SetTheme(w != 0); return 1;
    case MenuMessage: TrayMenu(); return 1;
    case TrayWheelMessage: ApplyWheelDelta(static_cast<int>(static_cast<INT_PTR>(w))); return 1;
    case RefreshTrayBoundsMessage: RefreshTrayBounds(); return 0;
    case ResetOnlyMessage:
        wheelRemainder = 0;
        { const bool ok = ApplyLevel(100); BrightnessHud::Show(brightness, brightnessLocked, darkMode); return ok ? 1 : 0; }
    case IconModeMessage: SetPercentageIcon(w != 0); return 1;
    case LockMessage: SetBrightnessLock(w != 0); return 1;
    case ProfilesMessage: SetProfilesEnabled(w != 0); return 1;
    case EmergencyResetMessage: ResetBrightness(); return 1;
    case ForegroundAppMessage: TrackForegroundApp(reinterpret_cast<HWND>(w)); return 0;
    case WM_COPYDATA: {
        const auto* data = reinterpret_cast<COPYDATASTRUCT*>(l);
        if (data->dwData == StatusSignature && data->lpData && data->cbData >= sizeof(wchar_t) &&
            data->cbData <= 65536 && data->cbData % sizeof(wchar_t) == 0) {
            const auto* path = static_cast<const wchar_t*>(data->lpData);
            if (path[data->cbData / sizeof(wchar_t) - 1] == L'\0') return WriteLiveStatus(path) ? 1 : 0;
        }
        return 0;
    }
    case WM_DESTROY: {
        SaveSettings();
        if (foregroundHook) { UnhookWinEvent(foregroundHook); foregroundHook = nullptr; }
        BrightnessHud::Destroy();
        StopTrayMouse();
        if (hotkeyRegistered) UnregisterHotKey(window, 1);
        if (adjustHotkeysRegistered) { UnregisterHotKey(window, 2); UnregisterHotKey(window, 3); }
        if (flyout) { DestroyWindow(flyout); flyout = nullptr; }
        if (menuPanel) { DestroyWindow(menuPanel); menuPanel = nullptr; }
        NOTIFYICONDATAW data = {}; data.cbSize = sizeof(data); data.hWnd = window; data.uID = 1;
        Shell_NotifyIconW(NIM_DELETE, &data);
        RestoreDesktop(); PostQuitMessage(0); return 0;
    }
    }
    return DefWindowProcW(window, message, w, l);
}

int WINAPI wWinMain(HINSTANCE app, HINSTANCE, PWSTR, int) {
    instance = app;
    bool trayOnly = false, quit = false, hide = false, reset = false, showFlyout = false, showMenu = false, normalMinimize = false;
    int requested = -1, requestedTheme = -1, requestedIcon = -1, requestedLock = -1, requestedProfiles = -1, argc = 0;
    int requestedWheel = 0, requestedOffset = 0;
    bool hasWheel = false, hasOffset = false, middleReset = false;
    DWORD guardParent = 0;
    std::wstring statusPath;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argv) return 2;
    for (int n = 1; n < argc; ++n) {
        const std::wstring arg = argv[n];
        if (arg == L"--tray") trayOnly = true;
        else if (arg == L"--hide") hide = true;
        else if (arg == L"--minimize") normalMinimize = true;
        else if (arg == L"--quit") quit = true;
        else if (arg == L"--flyout") showFlyout = true;
        else if (arg == L"--menu") showMenu = true;
        else if (arg == L"--reset") { requested = 100; reset = true; }
        else if (arg == L"--show") {}
        else if (arg == L"--theme=dark") requestedTheme = 1;
        else if (arg == L"--theme=light") requestedTheme = 0;
        else if (arg == L"--theme=auto") requestedTheme = 2;
        else if (arg == L"--icon=number") requestedIcon = 1;
        else if (arg == L"--icon=sun") requestedIcon = 0;
        else if (arg == L"--lock=on") requestedLock = 1;
        else if (arg == L"--lock=off") requestedLock = 0;
        else if (arg == L"--profiles=on") requestedProfiles = 1;
        else if (arg == L"--profiles=off") requestedProfiles = 0;
        else if (arg == L"--middle-reset") middleReset = true;
        else if (arg.rfind(L"--wheel=", 0) == 0 || arg.rfind(L"--offset=", 0) == 0) {
            const bool isWheel = arg.rfind(L"--wheel=", 0) == 0;
            const size_t prefix = isWheel ? 8 : 9;
            wchar_t* end = nullptr;
            const long parsed = wcstol(arg.c_str() + prefix, &end, 10);
            if (end == arg.c_str() + prefix || *end || parsed < -12000 || parsed > 12000) { LocalFree(argv); return 2; }
            if (isWheel) { requestedWheel = static_cast<int>(parsed); hasWheel = true; }
            else { requestedOffset = static_cast<int>(parsed); hasOffset = true; }
        }
        else if (arg.rfind(L"--guard=", 0) == 0) {
            wchar_t* end = nullptr;
            const unsigned long parsed = wcstoul(arg.c_str() + 8, &end, 10);
            if (argc != 2 || end == arg.c_str() + 8 || *end || parsed == 0) { LocalFree(argv); return 2; }
            guardParent = parsed;
        } else if (arg.rfind(L"--set=", 0) == 0) {
            wchar_t* end = nullptr;
            const long parsed = wcstol(arg.c_str() + 6, &end, 10);
            if (end == arg.c_str() + 6 || *end || parsed < 5 || parsed > 100) { LocalFree(argv); return 2; }
            requested = static_cast<int>(parsed);
        } else if (arg.rfind(L"--status=", 0) == 0 && arg.size() > 9) statusPath = arg.substr(9);
        else { LocalFree(argv); return 2; }
    }
    LocalFree(argv);
    if (guardParent) return RunRecoveryGuard(guardParent);
    HANDLE singleton = CreateMutexW(nullptr, FALSE, L"Local\\DimLite.Singleton.1");
    if (!singleton) return 1;
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        HWND existing = nullptr;
        for (int attempt = 0; attempt < 20 && !existing; ++attempt) {
            existing = FindWindowW(MainClass, nullptr); if (!existing) Sleep(50);
        }
        CloseHandle(singleton);
        if (!existing) return 3;
        auto send = [existing](UINT msg, WPARAM value = 0, LPARAM parameter = 0) {
            DWORD_PTR result = 0;
            return SendMessageTimeoutW(existing, msg, value, parameter, SMTO_ABORTIFHUNG, 5000, &result) && result;
        };
        if (!statusPath.empty()) {
            COPYDATASTRUCT data = {StatusSignature, static_cast<DWORD>((statusPath.size() + 1) * sizeof(wchar_t)), statusPath.data()};
            return send(WM_COPYDATA, 0, reinterpret_cast<LPARAM>(&data)) ? 0 : 3;
        }
        if (quit) return send(QuitMessage) ? 0 : 3;
        if (reset && !send(EmergencyResetMessage)) return 3;
        if (!reset && requested >= 0 && !send(SetLevelMessage, requested)) return 3;
        if (requestedTheme >= 0 && !send(ThemeMessage, requestedTheme)) return 3;
        if (requestedIcon >= 0 && !send(IconModeMessage, requestedIcon)) return 3;
        if (requestedLock >= 0 && !send(LockMessage, requestedLock)) return 3;
        if (requestedProfiles >= 0 && !send(ProfilesMessage, requestedProfiles)) return 3;
        if (hasWheel && !send(TrayWheelMessage, static_cast<WPARAM>(static_cast<INT_PTR>(requestedWheel)))) return 3;
        if (hasOffset) {
            // Reuse the same adjustment path as a complete wheel detent.
            if (requestedOffset % 5 != 0) return 2;
            if (!send(TrayWheelMessage, static_cast<WPARAM>(static_cast<INT_PTR>(requestedOffset / 5 * WHEEL_DELTA)))) return 3;
        }
        if (middleReset && !send(ResetOnlyMessage)) return 3;
        if (hide) return send(HideMessage) ? 0 : 3;
        if (normalMinimize) {
            DWORD_PTR ignored = 0;
            return SendMessageTimeoutW(existing, WM_SYSCOMMAND, SC_MINIMIZE, 0, SMTO_ABORTIFHUNG, 5000, &ignored) ? 0 : 3;
        }
        if (showFlyout) return send(FlyoutMessage) ? 0 : 3;
        if (showMenu) return PostMessageW(existing, MenuMessage, 0, 0) ? 0 : 3;
        if ((requested < 0 && requestedTheme < 0 && requestedIcon < 0 && requestedLock < 0 && requestedProfiles < 0 && !hasWheel && !hasOffset && !middleReset) || reset)
            return send(ShowMessage) ? 0 : 3;
        return 0;
    }
    if (quit || !statusPath.empty()) { CloseHandle(singleton); return quit ? 0 : 3; }
    PWSTR local = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_CREATE, nullptr, &local))) {
        const std::wstring folder = std::wstring(local) + L"\\DimLite";
        CoTaskMemFree(local); CreateDirectoryW(folder.c_str(), nullptr); settingsPath = folder + L"\\settings.ini";
        DimProfiles::Load(folder + L"\\profiles.ini");
    } else { CloseHandle(singleton); return 1; }
    const UINT saved = GetPrivateProfileIntW(L"Display", L"Brightness", 100, settingsPath.c_str());
    brightness = saved >= 5 && saved <= 100 ? static_cast<int>(saved) : 100;
    const UINT base = GetPrivateProfileIntW(L"Display", L"BaseBrightness", brightness, settingsPath.c_str());
    manualBrightness = base >= 5 && base <= 100 ? static_cast<int>(base) : 100;
    brightnessLocked = requestedLock >= 0 ? requestedLock != 0 : GetPrivateProfileIntW(L"Behavior", L"Locked", 0, settingsPath.c_str()) != 0;
    profilesEnabled = requestedProfiles >= 0 ? requestedProfiles != 0 : GetPrivateProfileIntW(L"Behavior", L"Profiles", 1, settingsPath.c_str()) != 0;
    TrackForegroundApp(GetForegroundWindow(), false);
    if (!brightnessLocked) brightness = profilesEnabled ? DimProfiles::Resolve(activeAppPath, manualBrightness) : manualBrightness;
    if (requested >= 0 && (!brightnessLocked || reset)) { brightness = requested; RememberLevel(brightness); }
    if (reset) { brightnessLocked = false; manualBrightness = 100; }
    const UINT themeSetting = GetPrivateProfileIntW(L"Appearance", L"ThemeMode", 0, settingsPath.c_str());
    themeMode = requestedTheme >= 0 ? (requestedTheme == 2 ? ThemeMode::Automatic : requestedTheme ? ThemeMode::Dark : ThemeMode::Light)
        : themeSetting <= 2 ? static_cast<ThemeMode>(themeSetting) : ThemeMode::Automatic;
    darkMode = themeMode == ThemeMode::Automatic ? SystemAppsDark() : themeMode == ThemeMode::Dark;
    percentageIcon = requestedIcon >= 0 ? requestedIcon != 0
        : GetPrivateProfileIntW(L"Appearance", L"PercentageIcon", 1, settingsPath.c_str()) != 0;
    if (hasOffset && !brightnessLocked) { brightness = std::clamp(brightness + requestedOffset, 5, 100); RememberLevel(brightness); }
    if (middleReset && !brightnessLocked) { brightness = 100; RememberLevel(brightness); }
    systemTrayDark = SystemMenuDark();
    Gdiplus::GdiplusStartupInput graphicsStartup;
    if (Gdiplus::GdiplusStartup(&graphicsToken, &graphicsStartup, nullptr) != Gdiplus::Ok) {
        CloseHandle(singleton); return 1;
    }
    magInitialized = MagInitialize() != 0;
    originalCaptured = magInitialized && MagGetFullscreenColorEffect(&originalEffect);
    if (!originalCaptured || !StartRecoveryGuard()) {
        RestoreDesktop(); CloseHandle(singleton);
        MessageBoxW(nullptr, L"Не удалось запустить системное затемнение. Перезапустите DimLite.", L"DimLite", MB_OK | MB_ICONINFORMATION);
        return 1;
    }
    dpi = GetDpiForSystem(); flyoutDpi = menuDpi = dpi;
    INITCOMMONCONTROLSEX common = {sizeof(common), ICC_BAR_CLASSES | ICC_STANDARD_CLASSES};
    InitCommonControlsEx(&common);
    backgroundBrush = CreateSolidBrush(Background());
    appIcon = static_cast<HICON>(LoadImageW(instance, MAKEINTRESOURCEW(1), IMAGE_ICON, 0, 0, LR_DEFAULTSIZE));
    if (!appIcon) appIcon = LoadIconW(nullptr, IDI_APPLICATION);
    RefreshTrayIcon();
    WNDCLASSEXW mainClass = {};
    mainClass.cbSize = sizeof(mainClass); mainClass.hInstance = instance; mainClass.lpfnWndProc = MainProc;
    mainClass.lpszClassName = MainClass; mainClass.hIcon = appIcon; mainClass.hIconSm = appIcon;
    mainClass.hCursor = LoadCursorW(nullptr, IDC_ARROW); mainClass.hbrBackground = backgroundBrush;
    WNDCLASSEXW smallClass = mainClass;
    smallClass.lpfnWndProc = FlyoutProc; smallClass.lpszClassName = FlyoutClass;
    WNDCLASSEXW contextClass = mainClass;
    contextClass.lpfnWndProc = MenuProc; contextClass.lpszClassName = MenuClass;
    contextClass.hbrBackground = nullptr;
    if (!RegisterClassExW(&mainClass) || !RegisterClassExW(&smallClass) || !RegisterClassExW(&contextClass)) {
        RestoreDesktop(); CloseHandle(singleton); return 1;
    }
    const DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_CLIPCHILDREN;
    const DWORD extended = WS_EX_CONTROLPARENT | WS_EX_COMPOSITED;
    RECT size = {0, 0, Scale(PanelWidth), Scale(PanelHeight)};
    AdjustWindowRectExForDpi(&size, style, FALSE, extended, dpi);
    mainWindow = CreateWindowExW(extended, MainClass, L"DimLite", style,
        CW_USEDEFAULT, CW_USEDEFAULT, size.right - size.left, size.bottom - size.top,
        nullptr, nullptr, instance, nullptr);
    flyout = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_CONTROLPARENT | WS_EX_COMPOSITED,
        FlyoutClass, L"DimLite — яркость", WS_POPUP | WS_THICKFRAME | WS_CAPTION | WS_CLIPCHILDREN,
        0, 0, FlyScale(FlyoutWidth), FlyScale(FlyoutHeight), nullptr, nullptr, instance, nullptr);
    menuPanel = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_CONTROLPARENT | WS_EX_COMPOSITED,
        MenuClass, L"DimLite — меню", WS_POPUP | WS_THICKFRAME | WS_CAPTION | WS_CLIPCHILDREN,
        0, 0, MulDiv(MenuWidth, menuDpi, 96), MulDiv(MenuHeight, menuDpi, 96), nullptr, nullptr, instance, nullptr);
    if (!mainWindow || !flyout || !menuPanel) { RestoreDesktop(); CloseHandle(singleton); return 1; }
    BrightnessHud::Initialize(mainWindow, SetLevelMessage, TrayWheelMessage);
    // Keep the client opaque: extending a glass frame by one pixel leaves
    // pale vertical seams around a narrow dark popup on some Windows builds.
    ApplyTheme(); UpdateMenuAppearance(); CenterMainWindow(); RegisterAdjustHotkeys(); AddTooltips();
    SendMessageW(mainWindow, WM_CHANGEUISTATE, MAKEWPARAM(UIS_INITIALIZE, 0), 0);
    taskbarCreated = RegisterWindowMessageW(L"TaskbarCreated"); UpdateTray(true);
    hotkeyRegistered = RegisterHotKey(mainWindow, 1, MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, VK_F12) != 0;
    if (!hotkeyRegistered) SetWindowTextW(shortcutLabel, L"Снять затемнение: «Сброс» или выход.");
    ApplyLevel(brightness, true); UpdateLockControls();
    foregroundHook = SetWinEventHook(EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND, nullptr,
        ForegroundChanged, 0, 0, WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);
    RefreshTrayBounds(); StartTrayMouse();
    if (hasWheel) ApplyWheelDelta(requestedWheel);
    if (showMenu) PostMessageW(mainWindow, MenuMessage, 0, 0);
    else if (showFlyout) OpenFlyout();
    else if (normalMinimize) { OpenPanel(); ShowWindow(mainWindow, SW_MINIMIZE); }
    else if (!trayOnly && !hide) OpenPanel();
    MSG message;
    BOOL result;
    while ((result = GetMessageW(&message, nullptr, 0, 0)) > 0) {
        if (message.message == WM_KEYDOWN && IsWindowVisible(menuPanel)) {
            const WPARAM key = message.wParam;
            if (key == VK_ESCAPE) { HideMenuPanel(); continue; }
            if (key == VK_UP || key == VK_DOWN || key == VK_HOME || key == VK_END || key == VK_RETURN || key == VK_SPACE) {
                size_t selected = 0;
                for (size_t n = 0; n < menuControls.size(); ++n)
                    if (GetFocus() == menuControls[n].window) selected = n;
                if (key == VK_RETURN || key == VK_SPACE) {
                    if (!IsWindowEnabled(menuControls[selected].window)) continue;
                    const int command = GetDlgCtrlID(menuControls[selected].window);
                    HandleMenuCommand(command); continue;
                }
                menuKeyboard = true;
                if (key == VK_UP) selected = (selected + menuControls.size() - 1) % menuControls.size();
                if (key == VK_DOWN) selected = (selected + 1) % menuControls.size();
                if (key == VK_HOME) selected = 0;
                if (key == VK_END) selected = menuControls.size() - 1;
                for (size_t attempt = 0; attempt < menuControls.size() && !IsWindowEnabled(menuControls[selected].window); ++attempt)
                    selected = (selected + (key == VK_UP || key == VK_END ? menuControls.size() - 1 : 1)) % menuControls.size();
                SetFocus(menuControls[selected].window);
                for (const auto& control : menuControls) InvalidateRect(control.window, nullptr, FALSE);
                continue;
            }
            if (key == VK_TAB) menuKeyboard = true;
        }
        if (message.message == WM_KEYDOWN && message.wParam == VK_ESCAPE) {
            if (message.hwnd == BrightnessHud::window) BrightnessHud::Hide();
            else if (IsWindowVisible(flyout)) HideFlyout();
            else if (IsWindowVisible(mainWindow)) { if (settingsVisible) OpenPanel(); else HidePanel(); }
            continue;
        }
        if (!IsDialogMessageW(menuPanel, &message) && !IsDialogMessageW(flyout, &message) && !IsDialogMessageW(mainWindow, &message)) {
            TranslateMessage(&message); DispatchMessageW(&message);
        }
    }
    RestoreDesktop();
    for (HFONT font : {bodyFont, headingFont, valueFont, flyoutBodyFont, flyoutHeadingFont, flyoutValueFont, stripValueFont, menuFont})
        if (font) DeleteObject(font);
    DeleteObject(backgroundBrush); DeleteObject(menuBackgroundBrush); CloseHandle(singleton);
    if (trayIcon) DestroyIcon(trayIcon);
    if (windowIcon) DestroyIcon(windowIcon);
    if (taskbarIcon) DestroyIcon(taskbarIcon);
    Gdiplus::GdiplusShutdown(graphicsToken);
    return result == -1 ? 1 : 0;
}

