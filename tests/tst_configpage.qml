import QtQuick

// Headless check of the settings page.
//
// The page is the one thing `plasmawindowed` never loads: it opens the widget's
// own window and nothing else, so a page that fails to load costs the entire
// settings dialog and reports nothing at all. Loading it here catches that, and
// driving it through its own functions checks the list handling without Plasma.
//
// Run with:
//
//     qmlscene6 -platform offscreen tests/tst_configpage.qml
//
// qmlscene6 ignores Qt.exit(), so the verdict comes from the output:
// PASS_REGULAR_EXPRESSION / FAIL_REGULAR_EXPRESSION in the CTest entry.
//
// Two kinds of noise appear in that output and mean nothing. `i18n()` is
// installed by the Plasma applet context, so outside one every label raises
// ReferenceError -- in the panel the same call is fine, which is why main.qml can
// use it. And Kirigami's FormLayout complains about a null window when it is not
// inside an ApplicationWindow. The verdict is the PASS/FAIL lines.
Item {
    id: harness

    property int failures: 0

    function check(name, condition) {
        if (condition) {
            console.log("PASS " + name);
        } else {
            failures += 1;
            console.log("FAIL " + name);
        }
    }

    function checkEqual(name, actual, expected) {
        check(name, actual === expected);
        if (actual !== expected)
            console.log("     got " + JSON.stringify(actual) + ", want " + JSON.stringify(expected));
    }

    Loader {
        id: loader
        anchors.fill: parent

        source: Qt.resolvedUrl("../plasmoid/com.martin.showfavicon/contents/ui/config/ConfigPage.qml")

        onLoaded: harness.exercise()

        onStatusChanged: {
            if (status === Loader.Error) {
                harness.check("the config page loads", false);
                harness.finish();
            }
        }
    }

    function exercise() {
        var page = loader.item;
        if (!page) {
            check("the config page loads", false);
            finish();
            return;
        }
        check("the config page loads", true);

        page.cfg_sites = ["https://a.example", "https://b.example", "https://c.example"];
        checkEqual("three sites are shown", page.sites.length, 3);
        checkEqual("the order is kept", page.sites[0], "https://a.example");

        page.addSite();
        checkEqual("add appends a row", page.sites.length, 4);
        checkEqual("the row it appends is empty", page.sites[3], "");

        page.setSite(0, "  https://trimmed.example  ");
        checkEqual("an edit is trimmed", page.sites[0], "https://trimmed.example");

        page.removeSite(1);
        checkEqual("remove drops one", page.sites.length, 3);
        checkEqual("the rows after it move up", page.sites[1], "https://c.example");

        // Out of range has to be ignored rather than throw: a stale index after a
        // removal is exactly what a delegate can hand over.
        page.removeSite(99);
        page.setSite(-1, "https://nowhere.example");
        checkEqual("an out of range remove is ignored", page.sites.length, 3);
        checkEqual("an out of range edit is ignored", page.sites[0], "https://trimmed.example");

        page.cfg_sites = [];
        checkEqual("an empty list is fine", page.sites.length, 0);

        // A value that arrives as text rather than an array still fills the page,
        // which is what a hand-edited or half-migrated config produces.
        page.cfg_sites = "https://one.example\nhttps://two.example";
        checkEqual("a newline separated value is split", page.sites.length, 2);

        page.cfg_sites = "https://one.example, https://two.example";
        checkEqual("a comma separated value is split", page.sites.length, 2);

        finish();
    }

    function finish() {
        console.log(failures === 0 ? "ALL PASSED" : (failures + " CHECK(S) FAILED"));
        Qt.callLater(Qt.quit);
    }
}
