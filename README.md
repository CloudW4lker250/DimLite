# DimLite

[Русский](#русский) · [English](#english)

Lightweight native screen dimmer for Windows 10/11. DimLite applies a software color transform to the whole desktop, so it can dim the taskbar and windows below the monitor's hardware brightness minimum.

The project is written in C++/Win32. It has no Electron, .NET runtime, network access or telemetry.

## Download

Download the latest `DimLite-*-win-x64.zip` from the [Releases](../../releases) page, extract `DimLite.exe` and run it. It is portable; installation and administrator rights are not required.

Releases also include `DimLite-*-Setup.exe`, which installs the app per-user and adds a normal Windows uninstaller. The ZIP remains available as the portable option.

The application starts in the notification area. A shortcut can be placed in Start, and automatic startup is available in Settings.

## Features

- Whole-desktop software dimming from 5% to 100%.
- Smooth changes from the main window, tray popup, wheel and hotkeys.
- Compact Windows-style brightness panel after `Ctrl+Alt+Up/Down` or tray input.
- The panel accepts a direct click, dragging, keyboard arrows and the mouse wheel.
- Application brightness profiles, with up to 32 explicitly saved executable profiles.
- Brightness lock to prevent accidental changes.
- Light, dark and Windows automatic themes.
- High-contrast percentage or monochrome tray icon.
- Startup with Windows, centered first launch and tray minimization.
- Recovery helper restores the original desktop transform after an unexpected exit.

## Quick controls

- `Ctrl+Alt+Up/Down` — change brightness by 5%.
- `Ctrl+Alt+F12` — unlock and reset to 100%.
- Mouse wheel over the tray icon — change by 5%.
- Middle-click on the tray icon — reset to 100% when unlocked.
- Double-click the tray icon — open the main window.
- `Escape` — close the active popup.

Clicking a slider track jumps directly to that position. The quick panel fades after the pointer leaves; it stays visible while hovered or dragged.

## Build from source

Requirements:

- Windows 10/11 x64.
- Visual Studio C++ Build Tools with the Windows 10/11 SDK.

From PowerShell:

```powershell
.\source\build.ps1
```

The output is `DimLite.exe` in the repository root. The script builds with MSVC `/W4 /WX`, static CRT and the Windows Magnification API.

## How dimming works

DimLite uses `MagSetFullscreenColorEffect` to scale the RGB output columns of the desktop color matrix. It does not change the monitor's hardware brightness, contrast or color temperature. Exclusive full-screen games and protected Windows desktops may be outside this API's scope.

## Resource profile

The app is event-driven. Idle operation does not poll the mouse or foreground application. Animation timers run only during a transition or fade. The tested 10-second idle sample for the UI and recovery processes was about 28 MiB working set and 0 ms accumulated CPU time on the test computer.

## License

DimLite is released under the [MIT License](LICENSE).

## Русский

Лёгкое нативное приложение для затемнения экрана в Windows 10/11. DimLite применяет программное цветовое преобразование всего рабочего стола, поэтому может затемнять окна и панель задач ниже аппаратного минимума монитора.

Проект написан на C++/Win32. В нём нет Electron, .NET, сетевых запросов и телеметрии.

### Возможности

- Затемнение всего рабочего стола от 5% до 100%.
- Плавное изменение яркости из окна, трея, колёсика и горячих клавиш.
- Компактная панель в стиле Windows с процентом и интерактивным ползунком.
- Щелчок по дорожке сразу переходит в выбранную точку; бегунок можно тянуть.
- Профили приложений, блокировка яркости и три режима темы.
- Настройки, автозапуск, процентный или монохромный значок в трее.
- Восстановление изображения после аварийного завершения.

### Управление

- `Ctrl+Alt+↑/↓` — изменить яркость на 5%.
- `Ctrl+Alt+F12` — снять блокировку и вернуть 100%.
- Колёсико над значком в трее — изменить на 5%.
- Средний щелчок по значку — вернуть 100%, если яркость не заблокирована.
- Двойной щелчок по значку — открыть главное окно.
- `Escape` — закрыть активную панель.

Сборка выполняется командой `.\source\build.ps1` на Windows 10/11 x64 с установленными MSVC C++ Build Tools и Windows SDK. Лицензия — [MIT](LICENSE).

В релизах также доступен `DimLite-*-Setup.exe`: он устанавливает приложение в профиль пользователя и добавляет обычное удаление через Windows. ZIP остаётся portable-вариантом.


