# Plan 1 — KDE Plasma 6 (Linux)

A C++ / Qt 6 program that fetches and caches website favicons, plus a thin QML
plasmoid that shows them in the panel. Built with CMake like InvoiceDrop and
installed per user with `cmake --install` and `kpackagetool6`.

## Progress

Implementation order, with what is already in the tree.

- [x] **Step 1 — build skeleton.** CMake project, `showfavicon --version`,
      `--verbose` / `--debug` / `--no-color`, and `log` / `paths` adapted from
      InvoiceDrop. Configure and build are green and `Run: version` prints
      `ShowFavicon 0.1.0`.
- [x] **Step 2 — CLI contract and cache paths.** `showfavicon <url>` answers with
      one JSON line and reports the cached files of a run that cannot fetch yet;
      `faviconstore` (cache key, atomic write, sha1) and `tests/CMakeLists.txt`
      with `tst_faviconstore`. The key matches the script's, so the cache the
      widget already has stays valid.
- [x] **Step 3 — `faviconresolver`** plus `tst_faviconresolver` (54 cases): the
      `rel` and format ranking, relative and absolute hrefs, and the example
      page's own link. That page declares a *relative* SVG href and answers 404
      for `/favicon.ico`, so the resolver is load bearing, not a nicety.
- [x] **Step 4 — `faviconfetcher`** plus `tst_faviconfetcher` (9 cases) against a
      stub HTTP server in the test: body and content type, the browser
      User-Agent, a redirect with the final URL reported, 404, a refused
      connection, a timeout, binary bytes and an empty body.
- [ ] **Step 5 — `faviconimage`** plus `tst_faviconimage`, and the fetch wired
      end to end.
- [ ] **Step 6 — `tst_cli`**, the QML switched to `showfavicon`, a panel test, and
      `tools/showfavicon-fetch` deleted.

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

Because the core is plain Qt, the same C++ code can back the Windows 11 tray
application from Plan 2; only the shell around it changes.

## Implementation language: C++

Everything with a decision or I/O is C++: HTTP, the HTML scan, image decoding,
scaling, the grayscale copy, caching, hashing and the JSON reply. This replaces
the Python helper from the first draft, and the repository becomes a CMake
project in the shape of InvoiceDrop.

One part stays QML, and that is not a choice: a Plasma panel applet *is* a QML
package. The panel item — its icons, clicks and drop areas — is QML, a thin shell
with no network, no filesystem and no decisions beyond hit-testing. That is the
same split InvoiceDrop uses: a C++ binary plus a QML front end that calls it.

Python, Pillow and ImageMagick all disappear as dependencies: Qt decodes PNG,
JPEG and ICO through `QImageReader`, SVG through `Qt6::Svg` / `QSvgRenderer`, and
`QImage` produces the grayscale copy.

## Reused from InvoiceDrop

| File | Use here |
| --- | --- |
| `CMakeLists.txt`, `src/CMakeLists.txt` | the project, target and install layout |
| `src/log.{h,cpp}` | stderr diagnostics that never touch stdout's JSON line — copied, the model/note parts dropped |
| `src/paths.{h,cpp}` | `dataDir()` and `resolvePath()` — the invoice-specific accessors are gone |
| `src/json.{h,cpp}` | *pattern only*: it is bill-specific and depends on the analysis types, so the reply is built with `QJsonObject` / `QJsonDocument` directly |
| `src/version.h.in` | `showfavicon --version` |
| `tests/CMakeLists.txt` | QTest registration plus the `qmlscene6` and `plasmoid-config` tests |
| `.clang-format`, `.gitignore`, `.vscode/*` | house style and the build tasks |

Not copied: the daemon, the systemd user unit and the D-Bus service. ShowFavicon
has no long-running process — the widget runs the binary once an hour.

## Package layout

```
showfavicon/
  CMakeLists.txt                     project, Qt6 (Core Network Gui Svg Test), install rules
  src/CMakeLists.txt                 libshowfavicon + the showfavicon executable
  src/
    main.cpp  cli.{h,cpp}            argument parsing, one-line JSON on stdout
    faviconresolver.{h,cpp}          scan <link rel=icon>, resolve, /favicon.ico fallback
    faviconfetcher.{h,cpp}           QNetworkAccessManager GET, redirects, timeout, User-Agent
    faviconimage.{h,cpp}             decode PNG/JPEG/ICO/SVG, scale, grayscale with alpha
    faviconstore.{h,cpp}             cache paths, atomic QSaveFile write, sha1
    log.{h,cpp}  paths.{h,cpp}  json.{h,cpp}     copied from InvoiceDrop
    version.h.in
  tests/
    CMakeLists.txt
    tst_faviconresolver.cpp  tst_faviconimage.cpp  tst_faviconstore.cpp
    tst_faviconfetcher.cpp   tst_cli.cpp
    tst_plasmoid.qml         check_plasmoid_config.cmake
  plasmoid/com.martin.showfavicon/   thin QML: panel icons, clicks, drops, settings
  .vscode/tasks.json                 configure / build / run / install / plasma / test
```

The QML half is already built and stays: `metadata.json`, `contents/config/*`,
`contents/ui/config/ConfigPage.qml`, `contents/ui/main.qml`,
`contents/ui/logic.js` and `contents/icons/placeholder.svg`. `logic.js` keeps the
UI-only decisions (which icon a drop hit, which slot to fill), exactly as
InvoiceDrop keeps its `invoicelogic.js`.

## Data flow

1. A QML `Timer` (3600000 ms) and the start-up path each call
   `Plasmoid.configuration.fetcher` — by default `showfavicon` — once per site.
2. The binary does the work and prints exactly one JSON line on stdout:
   `showfavicon <url>` → `{"site":…, "ok":true, "file":…, "grayFile":…, "hash":…, "error":""}`.
   The contract is unchanged from today's script, so `logic.js` and `main.qml`
   keep working as they are. `QJsonObject` writes its keys alphabetically, so the
   line starts with `error`; the widget reads by key, not by position.
   - `faviconfetcher` GETs the page (15 s timeout, browser User-Agent, redirects
     followed); `faviconresolver` picks the best `<link rel="icon">` and falls
     back to `/favicon.ico`.
   - `faviconimage` decodes, scales to 128 px and writes the colour PNG plus a
     grayscale PNG that keeps the alpha channel.
   - `faviconstore` writes both atomically to
     `~/.local/share/showfavicon/<host>-<sha1(url)[:8]>.png` and reports their
     absolute paths and the sha1 of the colour PNG.
   - On any failure it returns `ok:false` together with the *existing* cached
     paths, so the widget shows the previous icon grayed out.
3. `main.qml` maps the command string back to its row (never parsing the path out
   of the command) and the reply updates that row.
4. Display: the colour PNG while the site answers, the grayscale PNG when it does
   not, the placeholder SVG when nothing is cached. `rev` in the image URL forces
   a reload when `hash` changes, because the file is overwritten in place.

## Multi-site (2 icons)

Primary approach: **one plasmoid renders a `Row` of N icons** (N = configured
sites, 1–2). The compact representation is a custom `Row` of icon slots; each
slot is its own `MouseArea` + `DropArea`. Left-click → `Qt.openUrlExternally(url)`.
Settings via right-click → Plasma context menu → *Configure*. The
`fullRepresentation` is minimal (status list + "right-click → Configure" hint).

Fallback if the panel clips a wide compact representation: two instances of the
same plasmoid, each configured with one site.

## Configuration

- `main.xml`: `site1`, `site2` (empty = not shown) and `fetcher` (the command,
  default `showfavicon`). Site 1 defaults to
  `https://serverhealthcheck.borger.co.at`.
- Config page: two URL fields plus the fetcher command.
- Drag-and-drop: each panel icon is a `DropArea`; `logic.js` decides whether the
  drop refreshes the same host, fills the free slot, or replaces the icon it
  landed on, and the result is written back through `Plasmoid.configuration`.

## Key risks to verify

- `cfg_` aliases must match the `main.xml` entries exactly; `config.qml` must be a
  `ConfigModel`; `ConfigCategory.source` resolves against `contents/ui/`.
  `./scripts/check-package.sh` checks all three.
- Programmatic config writes must actually persist (KConfigSkeleton) — accept a
  drop, then reopen the widget and the settings dialog.
- ICO and SVG both need a Qt plugin, and both are present here (`libqico`,
  `Qt6Svg`); a missing one draws an empty square with no error, so the resolver
  ranks PNG over ICO when a page offers both.
- `ListModel.clear()` invalidates the rows `get()` handed out — copy the values
  into plain objects before rebuilding the model.

## Testing & milestones

1. `CMake: configure (Debug)` and `CMake: build (Debug)` from the tasks;
   `Run: version` links.
2. `tst_faviconresolver`, `tst_faviconimage` and `tst_faviconstore` green in
   `ctest`; they need no network.
3. `tst_faviconfetcher` against a local `QTcpServer` stub: a 200 with a `<link>`,
   a redirect, a 404 that falls back to `/favicon.ico`, and a timeout.
4. `tst_cli`: exactly one JSON line on stdout, and `ok:false` offline with the
   cached paths still reported.
5. `./scripts/check-package.sh` and the `qmlscene6` harness (`tst_plasmoid.qml`)
   for the QML side.
6. `Install: local`, add the widget to the panel, then `Plasma: reload widget`;
   check clicks and drag-and-drop on the panel icon, not only in
   `plasmawindowed`.
7. Delete `tools/showfavicon-fetch` once the C++ CLI passes the same checks.
