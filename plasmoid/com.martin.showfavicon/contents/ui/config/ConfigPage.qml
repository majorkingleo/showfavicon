import QtQuick
import QtQuick.Controls as QQC

import org.kde.kirigami as Kirigami

// One page of the configuration dialog. Plasma sets one `cfg_<entry>` property
// per entry in `contents/config/main.xml`, so the aliases below are the whole
// interface: the name after `cfg_` has to match the entry name exactly, or the
// dialog opens on an empty page and the setting is never written.
//
// The file lives under `contents/ui/` because that is what the `source` in
// `contents/config/config.qml` is resolved against.
Kirigami.FormLayout {
    id: page

    property alias cfg_site1: site1Field.text
    property alias cfg_site2: site2Field.text
    property alias cfg_fetcher: fetcherField.text

    QQC.TextField {
        id: site1Field
        Kirigami.FormData.label: i18n("Website 1:")
        placeholderText: "https://example.com"
    }

    QQC.TextField {
        id: site2Field
        Kirigami.FormData.label: i18n("Website 2 (optional):")
        placeholderText: i18n("Leave empty for a single icon")
    }

    QQC.Label {
        Kirigami.FormData.isSection: true
        wrapMode: Text.Wrap
        opacity: 0.7
        font: Kirigami.Theme.smallFont
        text: i18n("A URL can also be dropped straight onto the icon in the panel: a new host becomes a second icon, an existing one is refreshed.")
    }

    QQC.TextField {
        id: fetcherField
        Kirigami.FormData.label: i18n("Download command:")
        placeholderText: "showfavicon-fetch"
    }

    QQC.Label {
        wrapMode: Text.Wrap
        opacity: 0.7
        font: Kirigami.Theme.smallFont
        text: i18n("Must accept \"<command> <url>\" and print one JSON line. Installed by scripts/install.sh into ~/.local/bin, which is on the PATH Plasma sees.")
    }
}
