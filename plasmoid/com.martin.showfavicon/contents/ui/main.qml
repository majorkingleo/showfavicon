import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC

import org.kde.plasma.plasmoid
import org.kde.plasma.plasma5support as P5Support
import org.kde.kirigami as Kirigami

import "logic.js" as Logic

// Show Favicon: one panel icon per monitored website, refreshed once an hour.
// A click opens the site, a drop of a URL adds or replaces a site, and a site
// that cannot be reached keeps its last icon, drawn grayscale.
PlasmoidItem {
    id: root

    readonly property int refreshIntervalMs: 60 * 60 * 1000
    readonly property string placeholderIcon: Qt.resolvedUrl("../icons/placeholder.svg")

    // The configured URLs in order: blank entries skipped, duplicates dropped.
    // Reading through this instead of the raw array means the config tidies itself
    // up the first time a drop writes to it.
    readonly property var siteUrls: {
        var list = [];
        var configured = Plasmoid.configuration.sites || [];
        for (var i = 0; i < configured.length; i++) {
            var url = Logic.normalizeUrl(configured[i]);
            if (url.length > 0 && list.indexOf(url) < 0)
                list.push(url);
        }
        return list;
    }

    // Roles of sitesModel. Kept as a list so updateRow() can refuse unknown keys.
    readonly property var rowRoles: ["url", "file", "grayFile", "online",
                                     "lastUpdateMs", "hash", "rev", "error"]

    ListModel { id: sitesModel }

    Component.onCompleted: {
        rebuildModel();
        Qt.callLater(refreshAll);
    }

    onSiteUrlsChanged: {
        rebuildModel();
        Qt.callLater(refreshAll);
    }

    // ---------------------------------------------------------------- fetching

    // Command string -> row index, so a reply can never be attributed to the
    // wrong site. The data engine reports only the command it was handed.
    property var pending: ({})

    Timer {
        interval: root.refreshIntervalMs
        repeat: true
        running: true
        onTriggered: root.refreshAll()
    }

    P5Support.DataSource {
        id: executable
        engine: "executable"
        connectedSources: []

        onNewData: function (source, data) {
            executable.disconnectSource(source);
            var index = root.pending[source];
            if (index === undefined)
                return;
            delete root.pending[source];
            root.applyReply(index, data);
        }
    }

    function refreshAll() {
        for (var i = 0; i < sitesModel.count; i++)
            refreshSite(i);
    }

    function refreshSite(index) {
        var row = sitesModel.get(index);
        if (!row)
            return;
        var command = Logic.buildCommand(Plasmoid.configuration.fetcher, row.url);
        pending[command] = index;
        // connectSource() re-runs a command that is already connected, which is
        // what a manual refresh wants; a previous reply is disconnected first.
        executable.connectSource(command);
    }

    function applyReply(index, data) {
        var row = sitesModel.get(index);
        if (!row)
            return;

        var stderr = String(data["stderr"] || "").trim();
        var reply = Logic.parseReply(data["stdout"]);
        if (reply === null) {
            // The command itself failed (missing fetcher, bad path, ...). Keep
            // the last icon and surface the shell noise so it can be debugged.
            updateRow(index, { online: false, error: stderr });
            return;
        }

        var online = reply.ok === true;
        var props = { online: online, error: online ? "" : String(reply.error || stderr || "") };
        if (reply.file) props.file = String(reply.file);
        if (reply.grayFile) props.grayFile = String(reply.grayFile);

        var newHash = reply.hash ? String(reply.hash) : "";
        if (newHash.length > 0 && newHash !== row.hash) {
            props.hash = newHash;
            // Bumping `rev` changes the Image URL and forces a reload; without
            // it QML keeps showing the pixmap it cached for the same path.
            props.rev = row.rev + 1;
        }
        if (online)
            props.lastUpdateMs = Date.now();

        updateRow(index, props);
    }

    function updateRow(index, props) {
        for (var key in props) {
            if (rowRoles.indexOf(key) >= 0)
                sitesModel.setProperty(index, key, props[key]);
        }
    }

    // Rebuild the visible list from the configuration, carrying the runtime
    // state of URLs that stay monitored across the rebuild.
    function rebuildModel() {
        // The rows have to be copied into plain objects first: clear() destroys
        // the objects get() handed out, and their roles then read as undefined.
        var previous = {};
        for (var i = 0; i < sitesModel.count; i++) {
            var row = sitesModel.get(i);
            previous[row.url] = {
                file: row.file,
                grayFile: row.grayFile,
                online: row.online,
                lastUpdateMs: row.lastUpdateMs,
                hash: row.hash,
                rev: row.rev,
                error: row.error
            };
        }
        sitesModel.clear();
        for (var j = 0; j < siteUrls.length; j++) {
            var url = siteUrls[j];
            var old = previous[url];
            sitesModel.append({
                url: url,
                file: old ? old.file : "",
                grayFile: old ? old.grayFile : "",
                online: old ? old.online : true,
                lastUpdateMs: old ? old.lastUpdateMs : 0,
                hash: old ? old.hash : "",
                rev: old ? old.rev : 0,
                error: old ? old.error : ""
            });
        }
    }

    // ------------------------------------------------------------------ drops

    function sitesArray() {
        var list = [];
        for (var i = 0; i < sitesModel.count; i++)
            list.push({ url: sitesModel.get(i).url });
        return list;
    }

    function handleDrop(targetIndex, drop) {
        var plan = Logic.planDrop(sitesArray(), Logic.urlFromDrop(drop), targetIndex);
        if (plan === null)
            return;
        if (plan.action === "refresh")
            refreshSite(plan.index);
        else if (plan.action === "append")
            appendSite(plan.url);
        else if (plan.action === "replace")
            replaceSite(plan.index, plan.url);
    }

    // Both writers rebuild the whole array from the visible list, so blank and
    // duplicated entries disappear the first time a drop changes anything.
    function appendSite(url) {
        var sites = siteUrls.slice();
        sites.push(url);
        storeSites(sites);
    }

    function replaceSite(siteIndex, url) {
        var sites = siteUrls.slice();
        if (siteIndex < 0 || siteIndex >= sites.length) {
            appendSite(url);
            return;
        }
        sites[siteIndex] = url;
        storeSites(sites);
    }

    function storeSites(sites) {
        Plasmoid.configuration.sites = sites;
        persistConfiguration();
    }

    function persistConfiguration() {
        // The configuration object is backed by KConfigSkeleton, which writes on
        // the config dialog's accept. A programmatic write still reaches the
        // running widget; ask for a flush when the object offers one.
        try {
            if (typeof Plasmoid.configuration.writeConfig === "function")
                Plasmoid.configuration.writeConfig();
        } catch (error) {
            // Not fatal: the value already reached the running widget.
        }
    }

    // Which icon a drop landed on, from its x position inside the widget, or -1
    // when it landed beside them -- which is what appends a site.
    function dropTargetIndex(x) {
        if (sitesModel.count === 0)
            return -1;

        var slotWidth = Kirigami.Units.iconSizes.smallMedium + Kirigami.Units.smallSpacing;
        var local = x - iconRow.x;
        if (local < 0)
            return 0;

        var index = Math.floor(local / slotWidth);
        if (index >= sitesModel.count)
            return -1;
        return index;
    }

    // ------------------------------------------------------------------ text

    function statusText(online, lastUpdateMs, error) {
        var parts = [];
        if (lastUpdateMs > 0)
            parts.push(i18n("Updated %1", Qt.formatTime(new Date(lastUpdateMs), "hh:mm")));
        parts.push(online ? i18n("Reachable") : i18n("Unreachable — showing the last icon"));
        if (error && error.length > 0)
            parts.push(error);
        return parts.join(" · ");
    }

    function tooltipText(url, online, lastUpdateMs, error) {
        var lines = [Logic.hostOf(url) || url];
        if (lastUpdateMs > 0)
            lines.push(i18n("Updated %1", Qt.formatTime(new Date(lastUpdateMs), "hh:mm")));
        lines.push(online ? i18n("Reachable") : i18n("Unreachable — showing the last icon"));
        if (error && error.length > 0)
            lines.push(error);
        return lines.join("\n");
    }

    // -------------------------------------------------------- panel (compact)

    compactRepresentation: Item {
        id: compact

        implicitWidth: iconRow.implicitWidth + 2 * Kirigami.Units.smallSpacing
        implicitHeight: Kirigami.Units.iconSizes.smallMedium

        Row {
            id: iconRow
            anchors.centerIn: parent
            spacing: Kirigami.Units.smallSpacing

            Repeater {
                model: sitesModel
                delegate: faviconSlot
            }

            // Only shown while nothing is configured, so a fresh widget can be
            // found in the panel and configured.
            Item {
                width: sitesModel.count === 0 ? Kirigami.Units.iconSizes.smallMedium : 0
                height: Kirigami.Units.iconSizes.smallMedium

                Image {
                    anchors.fill: parent
                    source: root.placeholderIcon
                    sourceSize.width: width
                    sourceSize.height: height
                    opacity: 0.5
                }

                MouseArea {
                    anchors.fill: parent
                    hoverEnabled: true
                    acceptedButtons: Qt.LeftButton | Qt.MiddleButton
                    onClicked: root.expanded = !root.expanded
                    QQC.ToolTip.visible: containsMouse
                    QQC.ToolTip.text: i18n("No website configured yet — drag one here or right-click to configure.")
                }
            }
        }

        // The compact representation is the only thing on screen in a panel, so
        // this is the drop target that matters.
        DropArea {
            anchors.fill: parent
            keys: ["text/uri-list", "text/plain", "text/x-moz-url"]
            onDropped: function (drop) { root.handleDrop(root.dropTargetIndex(drop.x), drop) }
        }
    }

    Component {
        id: faviconSlot

        Item {
            id: slot

            width: Kirigami.Units.iconSizes.smallMedium
            height: width

            Image {
                id: iconImage
                anchors.fill: parent
                sourceSize.width: width
                sourceSize.height: height
                fillMode: Image.PreserveAspectFit
                // The file content changes in place, so the built-in pixmap
                // cache must not be used; `rev` in the URL forces the reload.
                cache: false
                source: {
                    var base = (!model.online && model.grayFile) ? model.grayFile : model.file;
                    if (!base) return "";
                    return "file://" + base + "?v=" + model.rev;
                }
                // Without a grayscale copy (no ImageMagick), dimming is the
                // fallback way to show that the icon is stale.
                opacity: (!model.online && !model.grayFile) ? 0.35 : 1.0
                visible: status === Image.Ready
            }

            Image {
                anchors.fill: parent
                source: root.placeholderIcon
                sourceSize.width: width
                sourceSize.height: height
                visible: iconImage.status !== Image.Ready
                opacity: model.online ? 0.55 : 0.25
            }

            MouseArea {
                id: slotMouse
                anchors.fill: parent
                hoverEnabled: true
                acceptedButtons: Qt.LeftButton | Qt.MiddleButton
                onClicked: function (mouse) {
                    if (mouse.button === Qt.MiddleButton) {
                        root.expanded = !root.expanded;
                        return;
                    }
                    Qt.openUrlExternally(model.url);
                }
                QQC.ToolTip.visible: slotMouse.containsMouse
                QQC.ToolTip.text: root.tooltipText(model.url, model.online, model.lastUpdateMs, model.error)
                QQC.ToolTip.delay: Kirigami.Units.toolTipDelay
            }
        }
    }

    // ------------------------------------------------------------- popup view

    fullRepresentation: Item {
        id: popup

        implicitWidth: Kirigami.Units.gridUnit * 22
        implicitHeight: popupLayout.implicitHeight + 2 * Kirigami.Units.largeSpacing

        ColumnLayout {
            id: popupLayout
            anchors.fill: parent
            anchors.margins: Kirigami.Units.largeSpacing
            spacing: Kirigami.Units.smallSpacing

            QQC.Label {
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                visible: sitesModel.count === 0
                text: i18n("No website configured yet.")
            }

            Repeater {
                model: sitesModel

                delegate: RowLayout {
                    Layout.fillWidth: true
                    spacing: Kirigami.Units.smallSpacing

                    Image {
                        Layout.preferredWidth: Kirigami.Units.iconSizes.smallMedium
                        Layout.preferredHeight: Kirigami.Units.iconSizes.smallMedium
                        sourceSize.width: width
                        sourceSize.height: height
                        fillMode: Image.PreserveAspectFit
                        cache: false
                        source: {
                            var base = (!model.online && model.grayFile) ? model.grayFile : model.file;
                            return base ? "file://" + base + "?v=" + model.rev : root.placeholderIcon;
                        }
                        opacity: (!model.online && !model.grayFile) ? 0.35 : 1.0
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 0

                        QQC.Label {
                            Layout.fillWidth: true
                            elide: Text.ElideMiddle
                            text: Logic.hostOf(model.url) || model.url
                        }

                        QQC.Label {
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                            opacity: 0.7
                            font: Kirigami.Theme.smallFont
                            text: root.statusText(model.online, model.lastUpdateMs, model.error)
                        }
                    }

                    QQC.Button {
                        icon.name: "view-refresh"
                        text: i18n("Refresh")
                        onClicked: root.refreshSite(index)
                    }

                    QQC.Button {
                        icon.name: "document-open"
                        text: i18n("Open")
                        onClicked: Qt.openUrlExternally(model.url)
                    }
                }
            }

            QQC.Label {
                Layout.fillWidth: true
                Layout.topMargin: Kirigami.Units.smallSpacing
                wrapMode: Text.Wrap
                opacity: 0.7
                font: Kirigami.Theme.smallFont
                text: i18n("Drag a URL onto an icon to add or replace a site. Left-click opens the site, middle-click opens this popup, right-click configures the widget.")
            }
        }

        // A drop works whether or not the popup happens to be open.
        DropArea {
            anchors.fill: parent
            keys: ["text/uri-list", "text/plain", "text/x-moz-url"]
            onDropped: function (drop) { root.handleDrop(-1, drop) }
        }
    }
}
