import QtQuick

import "../plasmoid/com.martin.showfavicon/contents/ui/logic.js" as Logic

// Headless harness for the pure helpers in contents/ui/logic.js. Run it with:
//
//     qmlscene6 -platform offscreen tests/tst_logic.qml
//
// qmlscene6 ignores Qt.exit(), so the process always ends with exit code 0 and
// the printed lines are the result. `Qt.callLater(Qt.quit)` is what ends it.
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

    Component.onCompleted: {
        // --- shellQuote / buildCommand ------------------------------------
        checkEqual("shellQuote plain", Logic.shellQuote("https://a.example/x"), "'https://a.example/x'");
        checkEqual("shellQuote apostrophe", Logic.shellQuote("it's"), "'it'\\''s'");
        checkEqual("buildCommand", Logic.buildCommand("showfavicon", "https://a.example/x"),
                   "showfavicon 'https://a.example/x'");

        // --- hostOf / normalizeUrl ----------------------------------------
        checkEqual("hostOf strips scheme and port", Logic.hostOf("https://Sub.Example.com:8443/p?q=1"), "sub.example.com");
        checkEqual("hostOf no scheme", Logic.hostOf("example.com/x"), "");
        checkEqual("normalizeUrl adds https", Logic.normalizeUrl("example.com"), "https://example.com");
        checkEqual("normalizeUrl trims", Logic.normalizeUrl("  https://a.example/x  "), "https://a.example/x");
        checkEqual("normalizeUrl keeps empty empty", Logic.normalizeUrl("   "), "");

        // --- urlFromDrop ---------------------------------------------------
        checkEqual("urlFromDrop uris", Logic.urlFromDrop({ urls: ["https://x.example/y"] }), "https://x.example/y");
        checkEqual("urlFromDrop text", Logic.urlFromDrop({ urls: [], hasText: true, text: "example.com\njunk" }),
                   "https://example.com");
        checkEqual("urlFromDrop nothing", Logic.urlFromDrop({ urls: [], hasText: false }), "");
        checkEqual("urlFromDrop null", Logic.urlFromDrop(null), "");

        // --- parseReply ----------------------------------------------------
        var good = Logic.parseReply('{"ok":true,"file":"/tmp/x.png"}');
        check("parseReply object", good !== null && good.ok === true && good.file === "/tmp/x.png");
        var last = Logic.parseReply('noise\n{"ok":false,"error":"offline"}\n');
        check("parseReply last line", last !== null && last.ok === false && last.error === "offline");
        checkEqual("parseReply number is not a reply", Logic.parseReply("42"), null);
        checkEqual("parseReply array is not a reply", Logic.parseReply("[1,2]"), null);
        checkEqual("parseReply garbage", Logic.parseReply("not json"), null);
        checkEqual("parseReply empty", Logic.parseReply(""), null);

        // --- planDrop ------------------------------------------------------
        var one = [{ url: "https://a.example" }];
        var two = [{ url: "https://a.example" }, { url: "https://b.example" }];

        var same = Logic.planDrop(one, "https://a.example/other", 0);
        check("planDrop same host refreshes", same !== null && same.action === "refresh" && same.index === 0);

        var replace = Logic.planDrop(two, "https://c.example", 1);
        check("planDrop on an icon replaces it", replace !== null && replace.action === "replace" && replace.index === 1);

        var append = Logic.planDrop(two, "https://c.example", -1);
        check("planDrop beside the icons appends", append !== null && append.action === "append");

        var beyond = Logic.planDrop(two, "https://c.example", 9);
        check("planDrop past the end appends", beyond !== null && beyond.action === "append");

        var first = Logic.planDrop([], "https://a.example", -1);
        check("planDrop into an empty widget appends", first !== null && first.action === "append");

        checkEqual("planDrop empty url", Logic.planDrop(one, "", 0), null);

        console.log(harness.failures === 0 ? "ALL PASSED" : (harness.failures + " CHECK(S) FAILED"));
        Qt.callLater(Qt.quit);
    }
}
