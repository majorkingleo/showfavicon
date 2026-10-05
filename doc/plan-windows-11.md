# Plan 2 — Windows 11

## Goal

A tray (notification area) application: one icon per site, click → open browser,
hourly refresh, grayed icon when offline, configurable via a settings window.

## Architecture

- **C# / .NET 8 (WinForms)** — simplest reliable `NotifyIcon`. One `NotifyIcon`
  per site (max 2 in v1). Self-contained single-file publish.
- Background timer: `System.Threading.Timer` / `PeriodicTimer` (1 h) plus a fetch
  on startup.
- Fetching: `HttpClient` with timeout; same resolution logic as the Python script
  (parse `<link rel=icon>`, fallback `/favicon.ico`, normalize to PNG). Cache in
  `%LOCALAPPDATA%\ShowFavicon\<host>.png`.
- Graying: `System.Drawing` `ColorMatrix` (grayscale) plus reduced alpha applied
  to the cached image when the last fetch failed.
- Click handling: `NotifyIcon.Click` →
  `Process.Start(new ProcessStartInfo(url) { UseShellExecute = true })`.

## Configuration & drag-and-drop

- Settings window (WinForms) with two URL fields and add/remove buttons.
- **Caveat:** Windows tray icons do **not** accept drag-and-drop. Alternatives:
  drag a URL onto the settings window's list (implement `AllowDrop` on the form),
  or a small always-visible "drop target" window. Recommended: drop onto the
  settings window plus a right-click "Add current clipboard URL" convenience item.
- Right-click context menu: *Open*, *Update now*, *Configure…*, *Exit*.

## Multi-site

One process hosts N `NotifyIcon`s. Note: Windows 11 hides tray icons in the
overflow by default — document that the user must drag them into the visible tray
area.

## Milestones

1. Single site, hourly fetch, cache, click-to-open.
2. Offline grayed rendering.
3. Settings window + drag-and-drop onto it.
4. Second site (second `NotifyIcon`).
5. Self-contained publish (`dotnet publish -r win-x64 --self-contained`),
   optional MSIX.
