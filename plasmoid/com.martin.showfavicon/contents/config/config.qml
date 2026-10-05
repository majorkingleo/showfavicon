import QtQuick

import org.kde.plasma.configuration

// The applet's configuration dialog. Plasma reads the pages from this model and
// from nowhere else: a `config.qml` that is a plain Item loads without a single
// warning and the dialog then has no page of its own.
//
// `source` is resolved against `contents/ui/`, not against this folder, which is
// why the page it names lives in `contents/ui/config/`.
ConfigModel {
    ConfigCategory {
        name: i18n("General")
        icon: "settings-configure"
        source: "config/ConfigPage.qml"
    }
}
