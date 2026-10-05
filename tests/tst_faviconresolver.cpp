#include "faviconresolver.h"

#include <QTest>

using namespace ShowFavicon;

class TestFaviconResolver : public QObject
{
    Q_OBJECT

private slots:
    void testRelRank_data();
    void testRelRank();

    void testFormatRank_data();
    void testFormatRank();

    void testResolveIconUrl_data();
    void testResolveIconUrl();
};

void TestFaviconResolver::testRelRank_data()
{
    QTest::addColumn<QString>("rel");
    QTest::addColumn<int>("expected");

    QTest::newRow("empty") << "" << -1;
    QTest::newRow("stylesheet") << "stylesheet" << -1;
    QTest::newRow("preload") << "preload" << -1;
    QTest::newRow("icon") << "icon" << 0;
    QTest::newRow("icon upper case") << "ICON" << 0;
    QTest::newRow("shortcut icon") << "shortcut icon" << 1;
    QTest::newRow("icon shortcut") << "icon shortcut" << 1;
    QTest::newRow("shortcut icon spaced oddly") << "  shortcut   icon  " << 1;
    QTest::newRow("apple-touch-icon") << "apple-touch-icon" << 2;
    QTest::newRow("apple-touch-icon-precomposed") << "apple-touch-icon-precomposed" << 3;
    QTest::newRow("mask-icon") << "mask-icon" << 4;
    QTest::newRow("sized apple-touch-icon") << "apple-touch-icon-120x120" << 5;
    QTest::newRow("fluid-icon") << "fluid-icon" << 5;
    QTest::newRow("alternate icon") << "alternate icon" << 5;
}

void TestFaviconResolver::testRelRank()
{
    QFETCH(QString, rel);
    QFETCH(int, expected);

    QCOMPARE(relRank(rel), expected);
}

void TestFaviconResolver::testFormatRank_data()
{
    QTest::addColumn<QString>("value");
    QTest::addColumn<int>("expected");

    QTest::newRow("nothing") << "" << 3;
    QTest::newRow("png type") << "image/png" << 0;
    QTest::newRow("png path") << "/i.png" << 0;
    QTest::newRow("svg type") << "image/svg+xml" << 1;
    QTest::newRow("svg path") << "/i.svg" << 1;
    QTest::newRow("jpeg") << "image/jpeg" << 2;
    QTest::newRow("jpg path") << "/i.jpg" << 2;
    QTest::newRow("webp") << "image/webp" << 2;
    QTest::newRow("gif") << "image/gif" << 2;
    QTest::newRow("ico type") << "image/x-icon" << 4;
    QTest::newRow("ico type, microsoft") << "image/vnd.microsoft.icon" << 4;
    QTest::newRow("ico path") << "/favicon.ico" << 4;
    QTest::newRow("unknown") << "application/octet-stream" << 3;
}

void TestFaviconResolver::testFormatRank()
{
    QFETCH(QString, value);
    QFETCH(int, expected);

    QCOMPARE(formatRank(value), expected);
}

void TestFaviconResolver::testResolveIconUrl_data()
{
    QTest::addColumn<QByteArray>("page");
    QTest::addColumn<QString>("base");
    QTest::addColumn<QString>("expected");

    const QString base = QStringLiteral("https://a.example/");
    const QString deep = QStringLiteral("https://a.example/deep/page");

    QTest::newRow("simple")
        << QByteArray("<link rel=\"icon\" href=\"/icon.png\">") << base
        << "https://a.example/icon.png";
    QTest::newRow("relative href against the page path")
        << QByteArray("<link rel=\"icon\" href=\"icon.png\">") << deep
        << "https://a.example/deep/icon.png";
    QTest::newRow("absolute href onto another host")
        << QByteArray("<link rel=\"icon\" href=\"https://cdn.example/i.png\">") << base
        << "https://cdn.example/i.png";
    QTest::newRow("query is kept")
        << QByteArray("<link rel=\"icon\" href=\"/i.png?v=3\">") << base
        << "https://a.example/i.png?v=3";
    QTest::newRow("shortcut icon")
        << QByteArray("<link rel=\"shortcut icon\" href=\"/s.ico\">") << base
        << "https://a.example/s.ico";
    QTest::newRow("href before rel")
        << QByteArray("<link href=\"/x.png\" rel=\"icon\">") << base
        << "https://a.example/x.png";
    QTest::newRow("tag and attribute names upper case")
        << QByteArray("<LINK REL=\"ICON\" HREF=\"/up.png\">") << base
        << "https://a.example/up.png";
    QTest::newRow("single quotes")
        << QByteArray("<link rel='icon' href='/q.png'>") << base
        << "https://a.example/q.png";
    QTest::newRow("unquoted values")
        << QByteArray("<link rel=icon href=/u.png>") << base
        << "https://a.example/u.png";
    QTest::newRow("attributes spread over lines")
        << QByteArray("<link\n    rel=\"icon\"\n    href=\"/m.png\"\n>") << base
        << "https://a.example/m.png";
    QTest::newRow("a stylesheet is not an icon")
        << QByteArray("<link rel=\"stylesheet\" href=\"/s.css\">") << base
        << "https://a.example/favicon.ico";
    QTest::newRow("a data icon is not fetchable")
        << QByteArray("<link rel=\"icon\" href=\"data:image/png;base64,AAAA\">") << base
        << "https://a.example/favicon.ico";
    QTest::newRow("no icon named at all")
        << QByteArray("<html><head><title>t</title></head></html>") << base
        << "https://a.example/favicon.ico";
    QTest::newRow("empty page")
        << QByteArray("") << base << "https://a.example/favicon.ico";
    QTest::newRow("fallback is root relative, not page relative")
        << QByteArray("<html></html>") << deep << "https://a.example/favicon.ico";

    // The base has no path here, which is how the widget hands a configured site
    // over: `https://a.example`, not `https://a.example/`. An empty path has to
    // behave like `/`, or every relative href lands on the wrong URL.
    QTest::newRow("relative href against a base without a path")
        << QByteArray("<link rel=\"icon\" href=\"favicon.php?ts=1\">")
        << QStringLiteral("https://a.example") << "https://a.example/favicon.php?ts=1";

    // Measured on the site this was written for: the only icon link is relative,
    // its type says SVG while the path says nothing, and /favicon.ico answers 404,
    // so the fallback would not save it.
    QTest::newRow("the real example page")
        << QByteArray("<link rel=\"icon\" href=\"favicon.php?ts=1791230991\" "
                      "type=\"image/svg+xml\">")
        << QStringLiteral("https://serverhealthcheck.borger.co.at")
        << "https://serverhealthcheck.borger.co.at/favicon.php?ts=1791230991";

    // The two orderings that decide what a browser would show.
    QTest::newRow("png beats ico")
        << QByteArray("<link rel=\"icon\" type=\"image/x-icon\" href=\"/a.ico\">"
                      "<link rel=\"icon\" type=\"image/png\" href=\"/b.png\">")
        << base << "https://a.example/b.png";
    QTest::newRow("a declared type beats the extension")
        << QByteArray("<link rel=\"icon\" type=\"image/png\" href=\"/x.ico\">") << base
        << "https://a.example/x.ico";
    QTest::newRow("icon beats apple-touch-icon")
        << QByteArray("<link rel=\"apple-touch-icon\" href=\"/a.png\">"
                      "<link rel=\"icon\" href=\"/b.png\">")
        << base << "https://a.example/b.png";
    QTest::newRow("apple-touch-icon beats the fallback")
        << QByteArray("<link rel=\"apple-touch-icon\" href=\"/a.png\">") << base
        << "https://a.example/a.png";
    QTest::newRow("mask-icon beats the fallback")
        << QByteArray("<link rel=\"mask-icon\" href=\"/m.svg\">") << base
        << "https://a.example/m.svg";
    QTest::newRow("the first of two equals wins")
        << QByteArray("<link rel=\"icon\" href=\"/first.png\">"
                      "<link rel=\"icon\" href=\"/second.png\">")
        << base << "https://a.example/first.png";
    QTest::newRow("an icon without href is skipped")
        << QByteArray("<link rel=\"icon\"><link rel=\"icon\" href=\"/only.png\">") << base
        << "https://a.example/only.png";
    QTest::newRow("other attributes are tolerated")
        << QByteArray("<link rel=\"icon\" sizes=\"32x32\" crossorigin=\"anonymous\" "
                      "type=\"image/png\" href=\"/sz.png\">")
        << base << "https://a.example/sz.png";
}

void TestFaviconResolver::testResolveIconUrl()
{
    QFETCH(QByteArray, page);
    QFETCH(QString, base);
    QFETCH(QString, expected);

    QCOMPARE(resolveIconUrl(page, QUrl(base)).toString(), expected);
}

QTEST_GUILESS_MAIN(TestFaviconResolver)

#include "tst_faviconresolver.moc"
