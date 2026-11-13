# DimLite 1.0.0

Lightweight native Windows screen dimmer.

## Highlights

- Compact one-row brightness OSD, 300×64 at 100% DPI, inspired by the Windows hardware indicator.
- Direct click-to-position and dragging on every brightness slider.
- Mouse wheel and keyboard controls inside the OSD.
- The OSD stays open while hovered or dragged and fades after the pointer leaves.
- Smooth transitions, application profiles and brightness lock.
- Dedicated settings page with theme, startup, icon and version controls.
- Native C++/Win32 implementation with no network access or telemetry.

## Assets

- `DimLite-1.0.0-win-x64.zip` — portable x64 build.
- `DimLite-1.0.0-win-x64.sha256` — SHA-256 checksum.

## Known limitations

- Exclusive full-screen games and protected Windows desktops may not be affected by the Magnification API.
- Hardware monitor brightness is not changed; DimLite applies software dimming to the desktop output.


