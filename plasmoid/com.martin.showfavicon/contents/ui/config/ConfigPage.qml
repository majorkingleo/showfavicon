import QtQuick
import QtQuick.Controls as QQC
import QtQuick.Layouts

import org.kde.kirigami as Kirigami

// One page of the configuration dialog. Plasma sets one `cfg_<entry>` property
// per entry in `contents/config/main.xml`, so the properties below are the whole
// interface: the name after `cfg_` has to match the entry name exactly, or the
// dialog opens on an empty page and the setting is never written.
//
// A list cannot be aliased to one control's text, so the page owns the array and
// rebuilds it on every add, remove and edit. That is also why a field commits on
// editingFinished instead of on every keystroke: replacing the array re-evaluates
// every row, and doing that mid-typing would take the focus with it.
//
// The file lives under `contents/ui/` because that is what the `source` in
// `contents/config/config.qml` is resolved against.
Kirigami.FormLayout {
    id: page

    property var cfg_sites: []
    property alias cfg_fetcher: fetcherField.text

    /// The sites as the rows see them.
    ///
    /// A value that arrives as anything other than an array is folded into one, so
    /// a hand-edited or half-migrated config leaves the page usable instead of
    /// empty.
    readonly property var sites: {
        var value = cfg_sites;
        if (Array.isArray(value))
            return value;
        if (typeof value === "string") {
            return value.split(/[\n,]/).filter(function (part) {
                return String(part).trim().length > 0;
            });
        }
        return [];
    }

    function setSite(index, value) {
        if (index < 0 || index >= sites.length)
            return;
        var copy = sites.slice();
        copy[index] = String(value).trim();
        cfg_sites = copy;
    }

    function removeSite(index) {
        if (index < 0 || index >= sites.length)
            return;
        var copy = sites.slice();
        copy.splice(index, 1);
        cfg_sites = copy;
    }

    function addSite() {
        var copy = sites.slice();
        copy.push("");
        cfg_sites = copy;
    }

    Repeater {
        model: page.sites.length

        delegate: RowLayout {
            Kirigami.FormData.label: index === 0 ? i18n("Websites:") : ""

            QQC.TextField {
                Layout.fillWidth: true
                Layout.minimumWidth: Kirigami.Units.gridUnit * 16
                text: page.sites[index] || ""
                placeholderText: "https://example.com"
                onEditingFinished: page.setSite(index, text)
            }

            QQC.Button {
                icon.name: "list-remove"
                text: i18n("Remove")
                onClicked: page.removeSite(index)
            }
        }
    }

    QQC.Button {
        icon.name: "list-add"
        text: i18n("Add website")
        Kirigami.FormData.label: " "
        onClicked: page.addSite()
    }

    QQC.Label {
        Kirigami.FormData.isSection: true
        wrapMode: Text.Wrap
        opacity: 0.7
        font: Kirigami.Theme.smallFont
        text: i18n("One panel icon per website. A URL can also be dropped onto the icons in the panel: on an icon it replaces that site, beside them it adds one.")
    }

    QQC.TextField {
        id: fetcherField
        Kirigami.FormData.label: i18n("Download command:")
        placeholderText: "showfavicon"
    }

    QQC.Label {
        wrapMode: Text.Wrap
        opacity: 0.7
        font: Kirigami.Theme.smallFont
        text: i18n("Must accept \"<command> <url>\" and print one JSON line. Placed in ~/.local/bin by the install step, which is on the PATH Plasma sees.")
    }
}
