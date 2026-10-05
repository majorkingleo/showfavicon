# Plan 3 — Android

## Goal

A persistent status-bar presence per site: click → open browser, hourly refresh,
grayed icon when offline. Android has no desktop tray; the equivalent is a
**foreground service with a persistent notification**.

## Architecture

- **Kotlin + Jetpack**, min SDK 26+.
- A **foreground service** (declared type, e.g. `specialUse` or `dataSync`) owns
  the notifications — keeps them persistent and clickable.
- Fetching: **WorkManager periodic work** (hourly; its 15-minute minimum is no
  issue) or a coroutine loop inside the service. WorkManager preferred for
  battery-friendliness.
- Cache: app internal storage `filesDir/showfavicon/<host>.png`.
- Click: notification `contentIntent` = `ACTION_VIEW` `PendingIntent` to the URL.
- Graying: render a `Bitmap` through a `ColorMatrix` (saturation 0 plus reduced
  alpha) when the last fetch failed.

## Multi-site

One notification per site (each with its own channel and id), or one summary
notification with expandable lines. Recommended: one notification per site for
independent icons.

## Configuration & "drag-and-drop"

- Configuration via a main `Activity`: a list of sites with add/remove, plus
  validation. **No drag-and-drop on the status bar** — instead support: paste
  URL, and a "share from browser → ShowFavicon" intent to add a site quickly.
- Permissions: `POST_NOTIFICATIONS` runtime permission (Android 13+),
  `FOREGROUND_SERVICE` plus `FOREGROUND_SERVICE_SPECIAL_USE`/`DATA_SYNC`, and
  `INTERNET`.
- Note: modern Android renders status-bar small icons as **monochrome** masks —
  the full-color favicon appears in the notification's large icon / expanded
  view; document this limitation.

## Milestones

1. Foreground service + one persistent notification, click-to-open.
2. WorkManager hourly fetch + PNG cache.
3. Offline grayed large icon.
4. Multi-site (two notifications), config activity, share-intent add.
5. AAB release build.
