.pragma library

// Pure decision helpers for the Show Favicon widget. Nothing in here touches the
// shell, so the whole file can be driven from tests/tst_logic.qml.
//
// Deliberately free of i18n(): the module has to load under `qmlscene6`, where
// i18n() only exists if the importing file pulls in org.kde.kirigami.

// Quote a value for the POSIX shell. The `executable` data engine reports only
// the command string it was connected with, so the widget never parses a path
// back out of a command; this only has to survive the shell.
function shellQuote(value) {
    return "'" + String(value).replace(/'/g, "'\\''") + "'";
}

// The exact command string the widget connects to the executable data engine.
function buildCommand(fetcher, url) {
    return String(fetcher) + " " + shellQuote(url);
}

// Lower-case host of a URL, without scheme, port or path.
function hostOf(url) {
    var match = String(url == null ? "" : url).match(/^[A-Za-z][A-Za-z0-9+.-]*:\/\/([^\/:?#]+)/);
    return match ? match[1].toLowerCase() : "";
}

// Trim a URL and give it a scheme when it is missing one.
function normalizeUrl(value) {
    var url = String(value == null ? "" : value).trim();
    if (url.length === 0) return "";
    if (!/^[A-Za-z][A-Za-z0-9+.-]*:\/\//.test(url)) url = "https://" + url;
    return url;
}

// The URL carried by a DropArea drop event, or "" when there is none.
function urlFromDrop(drop) {
    if (!drop) return "";
    if (drop.urls && drop.urls.length > 0) return normalizeUrl(String(drop.urls[0]));
    if (drop.hasText) {
        var lines = String(drop.text).split("\n");
        for (var i = 0; i < lines.length; i++) {
            var line = lines[i].trim();
            if (line.length > 0) return normalizeUrl(line);
        }
    }
    return "";
}

// Parse the fetcher's one-line JSON reply. Only a JSON object counts as a reply;
// an array, a number or a stray line is not a result and yields null.
function parseReply(stdout) {
    if (!stdout) return null;
    var lines = String(stdout).split("\n");
    for (var i = lines.length - 1; i >= 0; i--) {
        var line = lines[i].trim();
        if (line.length === 0) continue;
        var value;
        try {
            value = JSON.parse(line);
        } catch (error) {
            continue;
        }
        if (value && typeof value === "object" && !Array.isArray(value)) return value;
    }
    return null;
}

// Decide what a drop means. `sites` is the visible list of {url} objects and
// `targetIndex` the icon the drop landed on, or -1 when it landed beside them.
//
//   same host already monitored -> refresh that site
//   dropped on an icon          -> replace that icon
//   dropped beside the icons    -> append
function planDrop(sites, droppedUrl, targetIndex) {
    var url = normalizeUrl(droppedUrl);
    if (url.length === 0) return null;
    var host = hostOf(url);
    var list = sites || [];
    for (var i = 0; i < list.length; i++) {
        if (host.length > 0 && hostOf(list[i].url) === host)
            return { action: "refresh", index: i, url: url };
    }
    if (targetIndex >= 0 && targetIndex < list.length)
        return { action: "replace", index: targetIndex, url: url };
    return { action: "append", url: url };
}
