#include "faviconstore.h"

#include <QDir>
#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QProcess>
#include <QTcpServer>
#include <QTemporaryDir>
#include <QTest>

using namespace ShowFavicon;

namespace {

/// A run of the installed-style binary, reduced to what the contract is about.
struct Run {
    int exitCode = -1;
    QByteArray out;
    QByteArray err;
    QString error; // empty when the process started and finished

    bool started() const { return error.isEmpty(); }
};

/// Runs the binary the test was built with, not one from the PATH: a stale
/// installed copy would otherwise hide a change.
Run runShowFavicon(const QStringList &arguments)
{
    Run run;

    QProcess process;
    process.start(QStringLiteral(SHOWFAVICON_BINARY), arguments);

    if (!process.waitForStarted(10000)) {
        run.error = QStringLiteral("could not start the binary: %1").arg(process.errorString());
        return run;
    }
    if (!process.waitForFinished(60000)) {
        process.kill();
        process.waitForFinished(5000);
        run.error = QStringLiteral("the binary did not finish");
        return run;
    }

    run.exitCode = process.exitCode();
    run.out = process.readAllStandardOutput();
    run.err = process.readAllStandardError();
    return run;
}

/// A port nothing is listening on: bind one, note the number, release it again.
///
/// A local closed port rather than a name that does not resolve, because a
/// resolver that answers everything with a portal page would turn the offline case
/// into an online one.
quint16 closedPort()
{
    QTcpServer server;
    if (!server.listen(QHostAddress::LocalHost, 0))
        return 0;

    const quint16 port = server.serverPort();
    server.close();
    return port;
}

QString urlOnPort(quint16 port)
{
    return QStringLiteral("http://127.0.0.1:%1/").arg(port);
}

} // namespace

class TestCli : public QObject
{
    Q_OBJECT

private slots:
    void testVersion();
    void testHelp();
    void testNoArgumentsIsAUsageError();
    void testUnknownOptionIsAUsageError();
    void testCacheDirWithoutAValueIsAUsageError();
    void testASecondUrlIsRejected();

    void testUnreachableSiteAnswersWithOneJsonLine();
    void testCachedFilesSurviveAFailedFetch();
    void testVerboseKeepsStdoutToOneLine();
};

void TestCli::testVersion()
{
    const Run run = runShowFavicon({QStringLiteral("--version")});

    QVERIFY2(run.started(), qPrintable(run.error));
    QCOMPARE(run.exitCode, 0);
    QVERIFY(run.out.startsWith("ShowFavicon "));
    // Nothing on stderr: this is the smoke test for a working binary.
    QVERIFY(run.err.trimmed().isEmpty());
}

void TestCli::testHelp()
{
    const Run run = runShowFavicon({QStringLiteral("--help")});

    QVERIFY2(run.started(), qPrintable(run.error));
    QCOMPARE(run.exitCode, 0);
    QVERIFY(run.out.contains("usage:"));
    QVERIFY(run.out.contains("--cache-dir"));
}

void TestCli::testNoArgumentsIsAUsageError()
{
    const Run run = runShowFavicon({});

    QVERIFY2(run.started(), qPrintable(run.error));
    QCOMPARE(run.exitCode, 2);
    QVERIFY(run.out.isEmpty());
    QVERIFY(run.err.contains("usage:"));
}

void TestCli::testUnknownOptionIsAUsageError()
{
    const Run run = runShowFavicon({QStringLiteral("--nope")});

    QVERIFY2(run.started(), qPrintable(run.error));
    QCOMPARE(run.exitCode, 2);
    QVERIFY(run.err.contains("--nope"));
}

void TestCli::testCacheDirWithoutAValueIsAUsageError()
{
    // Swallowing the argument would fetch into a directory named after the next
    // option, so it is worth an explicit error.
    const Run run = runShowFavicon({QStringLiteral("--cache-dir")});

    QVERIFY2(run.started(), qPrintable(run.error));
    QCOMPARE(run.exitCode, 2);
    QVERIFY(run.err.contains("--cache-dir"));
}

void TestCli::testASecondUrlIsRejected()
{
    // The widget runs one command per site, so a second URL is a mistake rather
    // than a second fetch.
    const Run run = runShowFavicon({QStringLiteral("https://a.example"),
                                    QStringLiteral("https://b.example")});

    QVERIFY2(run.started(), qPrintable(run.error));
    QCOMPARE(run.exitCode, 2);
    QVERIFY(run.err.contains("https://b.example"));
}

void TestCli::testUnreachableSiteAnswersWithOneJsonLine()
{
    QTemporaryDir cache;
    QVERIFY(cache.isValid());
    const quint16 port = closedPort();
    QVERIFY(port != 0);

    const Run run = runShowFavicon({urlOnPort(port), QStringLiteral("--cache-dir"), cache.path()});

    QVERIFY2(run.started(), qPrintable(run.error));
    // A site that cannot be reached is a successful run of the program: the
    // answer carries the truth in `ok`. The widget never looks at the exit code.
    QCOMPARE(run.exitCode, 0);

    // Exactly one line: the QML side parses the last line of stdout, so a second
    // one -- a stray warning, a progress line -- would break the parse.
    const QList<QByteArray> lines = run.out.split('\n');
    QCOMPARE(lines.size(), qsizetype(2));
    QVERIFY(lines.at(1).isEmpty());

    QJsonParseError parseError{};
    const QJsonObject reply = QJsonDocument::fromJson(lines.at(0), &parseError).object();
    QCOMPARE(parseError.error, QJsonParseError::NoError);

    QCOMPARE(reply.size(), qsizetype(6));
    for (const QString &key : {QStringLiteral("site"), QStringLiteral("ok"), QStringLiteral("file"),
                               QStringLiteral("grayFile"), QStringLiteral("hash"),
                               QStringLiteral("error")}) {
        QVERIFY2(reply.contains(key), qPrintable(key));
    }

    QCOMPARE(reply.value(QStringLiteral("ok")).toBool(), false);
    // Nothing was ever cached, so there is no icon to fall back to.
    QVERIFY(reply.value(QStringLiteral("file")).isNull());
    QVERIFY(reply.value(QStringLiteral("grayFile")).isNull());
    QVERIFY(!reply.value(QStringLiteral("error")).toString().isEmpty());
}

void TestCli::testCachedFilesSurviveAFailedFetch()
{
    QTemporaryDir cache;
    QVERIFY(cache.isValid());
    const quint16 port = closedPort();
    QVERIFY(port != 0);
    const QString url = urlOnPort(port);

    // Plant what a previous, successful run would have left behind. The bytes are
    // not a real image on purpose: a failed run reports the cached files, it never
    // decodes them.
    const CacheEntry entry = entryFor(url, cache.path());
    const QByteArray colour("pretend this is a png");
    QString writeError;
    const QString hash = writeAtomic(entry.colourPath, colour, &writeError);
    QVERIFY2(writeError.isEmpty(), qPrintable(writeError));
    QVERIFY(!writeAtomic(entry.grayPath, QByteArray("pretend this is the gray one"), &writeError)
                 .isEmpty());

    const Run run = runShowFavicon({url, QStringLiteral("--cache-dir"), cache.path()});

    QVERIFY2(run.started(), qPrintable(run.error));
    QCOMPARE(run.exitCode, 0);

    const QJsonObject reply = QJsonDocument::fromJson(run.out).object();
    QCOMPARE(reply.value(QStringLiteral("ok")).toBool(), false);

    // This is what lets the widget keep showing the last icon, dimmed, instead of
    // going blank -- the whole reason the grayscale copy is written at all.
    QCOMPARE(reply.value(QStringLiteral("file")).toString(), entry.colourPath);
    QCOMPARE(reply.value(QStringLiteral("grayFile")).toString(), entry.grayPath);
    QCOMPARE(reply.value(QStringLiteral("hash")).toString(), hash);
}

void TestCli::testVerboseKeepsStdoutToOneLine()
{
    QTemporaryDir cache;
    QVERIFY(cache.isValid());
    const quint16 port = closedPort();
    QVERIFY(port != 0);

    const Run run = runShowFavicon({urlOnPort(port), QStringLiteral("--cache-dir"), cache.path(),
                                    QStringLiteral("--verbose")});

    QVERIFY2(run.started(), qPrintable(run.error));
    QCOMPARE(run.exitCode, 0);

    // The log has to go to stderr, or the widget's parse of stdout breaks the
    // moment someone debugs a widget by turning the log on.
    QCOMPARE(run.out.count('\n'), qsizetype(1));
    QVERIFY(!run.err.trimmed().isEmpty());
    QVERIFY(QJsonDocument::fromJson(run.out).object().contains(QStringLiteral("ok")));
}

QTEST_GUILESS_MAIN(TestCli)

#include "tst_cli.moc"
