# Plan 1 — KDE Plasma 6 (Linux)

A plasmoid (panel widget) built with the existing `plasma-widget` skill. No C++, no
build system — a QML package installed via `kpackagetool6`.

## Goal

Show one favicon per monitored website directly in the panel (system tray).
Left-click an icon opens the site in the default browser. The favicon is
re-fetched hourly; if offline, the last good favicon stays but is shown grayed
out. 1–2 websites are supported; URLs are configurable and settable by
drag-and-drop.

## Why a Plasmoid (not a system-tray icon)

A system-tray icon (StatusNotifierItem / SNI) is the alternative way to put an
icon in the KDE panel. For this project a Plasmoid is the right choice, because
two requirements can only be met by a Plasmoid directly:

| Requirement | Plasmoid | System-tray icon (SNI) |
|---|---|---|
| 2 icons side by side in the panel | ✅ a `Row` of icons rendered directly in the panel | ❌ folded into the tray overflow, hidden by default |
| Drag-and-drop onto the icon | ✅ `DropArea` directly on the icon | ❌ SNI does not support drag-and-drop |
| Click → open browser | ✅ `Qt.openUrlExternally` | ✅ menu/click action |
| Configuration | ✅ built-in dialog (KConfigXT) | ⚠️ needs a separate settings window |
| Hourly fetch / grayed when offline | ✅ `Timer` + DataSource | ✅ own process |
| Desktop-independent | ❌ KDE only (sufficient here) | ✅ works on many desktops |

Deciding factors:

1. **Drag-and-drop** — a Plasmoid can accept a URL with a `DropArea` directly on
   the panel icon. A tray icon cannot.
2. **Two icons in the panel** — the Plasmoid draws the icons itself as a `Row`.
   SNI icons are collapsed into the system-tray overflow and are not visible side
   by side by default.

SNI would only be preferable for a cross-desktop solution (GNOME, XFCE, …) or a
classic daemon in the tray overflow — neither is required here. The existing
`plasma-widget` skill templates and scripts additionally reduce the effort.

## Package layout

```
plasmoid/com.martin.showfavicon/
  metadata.json                      KPlugin.Id = com.martin.showfavicon
  contents/config/main.xml           KConfigXT: sites list + names
  contents/config/config.qml         ConfigModel → ConfigPage
  contents/ui/config/ConfigPage.qml  one cfg_ alias per entry
  contents/ui/main.qml               PlasmoidItem (icons, timer, drop targets)
  contents/ui/logic.js               favicon state machine, command building
  contents/icons/placeholder.svg
```

Plus a helper script shipped in the repo (installed into `~/.local/bin`):

```
tools/fetch_favicon.py <url> <cacheDir>
```

## Data flow

1. QML `Timer` (interval 3600000 ms) plus an initial fetch on start.
2. `P5Support.DataSource { engine: "executable" }` runs `fetch_favicon.py` per
   site; the script prints one JSON line to stdout.
3. Script logic (dependency-light, Python 3 + Pillow for ICO/WebP → PNG):
   - GET the page (timeout ~10 s), parse
     `<link rel="icon|shortcut icon|apple-touch-icon">`, resolve relative URLs.
   - Fallback: `https://<host>/favicon.ico`.
   - Download, sniff image type, normalize to PNG, save to
     `<cacheDir>/<host>.png` (cacheDir =
     `QStandardPaths.GenericDataLocation + "/showfavicon"`, passed in as an
     argument).
   - On success print `{"site": "<url>", "ok": true, "file": "<abs-path>"}`; on
     any failure print `{"site": "<url>", "ok": false}` and leave the cached file
     untouched.
4. QML keeps per-site state `{url, file, ok}` in `logic.js` (testable with
   `qmlscene6`). `onNewData` reads the keys `stdout` and `exit code` and maps the
   command string → site (never re-parse the path from the command line).
5. Display: `Image` (or `Kirigami.Icon`) per site. `ok == false` →
   `Qt5Compat.GraphicalEffects.Desaturate` plus reduced `opacity` over the last
   cached file; a tooltip shows "last update: …".

## Multi-site (2 icons)

Primary approach: **one plasmoid renders a `Row` of N icons** (N = configured
sites, 1–2). The compact representation is a custom `Row` of icon slots; each
slot is its own `MouseArea` + `DropArea`. Left-click → `Qt.openUrlExternally(url)`.
Settings via right-click → Plasma context menu → *Configure*. The
`fullRepresentation` is minimal (status list + "right-click → Configure" hint).

Fallback if the panel clips a wide compact representation: two instances of the
same plasmoid, each configured with one site.

## Configuration

- `main.xml`: `StringList sites` (max 2 in v1) plus optional display names.
  Default = `https://serverhealthcheck.borger.co.at`.
- Config page: two URL text fields.
- Drag-and-drop: `DropArea` on each icon slot accepts `text/plain` /
  `text/uri-list`; parse the URL, write it into `plasmoid.configuration.sites`,
  persist via KConfigSkeleton. Dropping on an empty slot adds a site; dropping on
  an occupied slot replaces it.

## Key risks to verify

- `cfg_` aliases must match `entry name` exactly; `config.qml` must be a
  `ConfigModel`; `ConfigCategory.source` resolves against `contents/ui/`.
- Confirm programmatic config writes actually persist (KConfigSkeleton) — verify
  by re-opening the widget.
- Grayscale effect: `import Qt5Compat.GraphicalEffects` (Qt 6) for `Desaturate`.

## Testing & milestones

1. Scaffold package from `assets/`, run `plasmawindowed com.martin.showfavicon`
   until zero QML warnings.
2. Unit-test `fetch_favicon.py` standalone: the example site, a redirect, a site
   without `<link rel=icon>`, and an unreachable host.
3. Test offline path in `plasmawindowed` (non-routable host → grayed cached icon).
4. `./scripts/check-package.sh` for config wiring.
5. Install with `kpackagetool6 --type Plasma/Applet --install`, add to panel,
   test real click-to-open and drag-and-drop on the panel icon (not only in the
   window).
6. Reload via `./scripts/reload-plasmoid.sh`, confirm PIDs change.
