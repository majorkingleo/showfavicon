#include "faviconfetcher.h"

#include <QElapsedTimer>
#include <QHash>
#include <QHostAddress>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTest>

using namespace ShowFavicon;

/// A single-request HTTP server for the tests.
///
/// Enough HTTP to be a server: it reads the request line and the headers, answers
/// from a route table and closes. It does not try to be a general purpose
/// implementation, which is why it lives in the test and not in src/.
class StubServer : public QTcpServer
{
public:
    struct Route {
        int status = 200;
        QByteArray body;
        QByteArray contentType = QByteArrayLiteral("text/html");
        QString location;
        /// Accept the request and never answer, for the timeout case.
        bool hang = false;
    };

    explicit StubServer(QObject *parent = nullptr)
        : QTcpServer(parent)
    {
        listen(QHostAddress::LocalHost, 0);
    }

    void route(const QString &path, const Route &entry) { m_routes.insert(path, entry); }

    QString url(const QString &path) const
    {
        return QStringLiteral("http://127.0.0.1:%1%2").arg(serverPort()).arg(path);
    }

    QStringList requests() const { return m_requests; }
    QHash<QString, QString> lastHeaders() const { return m_lastHeaders; }

protected:
    void incomingConnection(qintptr handle) override
    {
        auto *socket = new QTcpSocket(this);
        socket->setSocketDescriptor(handle);

        connect(socket, &QTcpSocket::readyRead, this,
                [this, socket, buffer = QByteArray(), handled = false]() mutable {
                    if (handled)
                        return;

                    buffer.append(socket->readAll());
                    const int end = buffer.indexOf("\r\n\r\n");
                    if (end < 0)
                        return;
                    handled = true;

                    const QList<QByteArray> lines = buffer.left(end).split('\n');
                    const QList<QByteArray> requestLine = lines.value(0).trimmed().split(' ');
                    m_requests.append(QString::fromUtf8(requestLine.value(1)));

                    QHash<QString, QString> headers;
                    for (int index = 1; index < lines.size(); ++index) {
                        const QByteArray line = lines.at(index).trimmed();
                        const int colon = line.indexOf(':');
                        if (colon > 0) {
                            headers.insert(QString::fromUtf8(line.left(colon)).trimmed().toLower(),
                                           QString::fromUtf8(line.mid(colon + 1)).trimmed());
                        }
                    }
                    m_lastHeaders = headers;

                    const Route entry = m_routes.value(QString::fromUtf8(requestLine.value(1)));
                    if (entry.hang)
                        return;

                    QByteArray response = "HTTP/1.1 " + QByteArray::number(entry.status) + ' '
                        + reasonPhrase(entry.status) + "\r\n";
                    if (!entry.location.isEmpty())
                        response += "Location: " + entry.location.toUtf8() + "\r\n";
                    response += "Content-Type: " + entry.contentType + "\r\n";
                    response += "Content-Length: " + QByteArray::number(entry.body.size()) + "\r\n";
                    response += "Connection: close\r\n\r\n";
                    response += entry.body;

                    socket->write(response);
                    socket->flush();
                    socket->disconnectFromHost();
                });

        connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
    }

private:
    static QByteArray reasonPhrase(int status)
    {
        switch (status) {
        case 200:
            return QByteArrayLiteral("OK");
        case 301:
            return QByteArrayLiteral("Moved Permanently");
        case 302:
            return QByteArrayLiteral("Found");
        case 404:
            return QByteArrayLiteral("Not Found");
        case 500:
            return QByteArrayLiteral("Internal Server Error");
        default:
            return QByteArrayLiteral("Unknown");
        }
    }

    QHash<QString, Route> m_routes;
    QStringList m_requests;
    QHash<QString, QString> m_lastHeaders;
};

class TestFaviconFetcher : public QObject
{
    Q_OBJECT

private slots:
    void testFetchesTheBody();
    void testSendsABrowserUserAgent();
    void testSendsAnImageAcceptForIcons();
    void testFollowsARedirect();
    void testReportsNotFound();
    void testReportsConnectionRefused();
    void testTimesOut();
    void testKeepsBinaryBytesExactly();
    void testEmptyBodyIsASuccess();
};

void TestFaviconFetcher::testFetchesTheBody()
{
    StubServer server;
    QVERIFY(server.isListening());
    server.route(QStringLiteral("/a"),
                 {200, QByteArrayLiteral("<html>hi</html>"),
                  QByteArrayLiteral("text/html; charset=utf-8")});

    Fetcher fetcher(2000);
    const FetchResult result = fetcher.fetchPage(QUrl(server.url(QStringLiteral("/a"))));

    QVERIFY2(result.ok(), qPrintable(result.error));
    QCOMPARE(result.body, QByteArrayLiteral("<html>hi</html>"));
    QCOMPARE(result.status, 200);
    QVERIFY(result.contentType.contains(QStringLiteral("text/html")));
    QCOMPARE(result.finalUrl.toString(), server.url(QStringLiteral("/a")));
    QCOMPARE(server.requests().join(QLatin1Char(',')), QStringLiteral("/a"));
}

void TestFaviconFetcher::testSendsABrowserUserAgent()
{
    StubServer server;
    QVERIFY(server.isListening());
    server.route(QStringLiteral("/a"), {200, QByteArrayLiteral("x")});

    Fetcher fetcher(2000);
    QVERIFY(fetcher.fetchPage(QUrl(server.url(QStringLiteral("/a")))).ok());

    const QHash<QString, QString> headers = server.lastHeaders();
    QVERIFY2(headers.value(QStringLiteral("user-agent")).contains(QStringLiteral("ShowFavicon")),
             qPrintable(headers.value(QStringLiteral("user-agent"))));
    QVERIFY(headers.value(QStringLiteral("accept")).contains(QStringLiteral("text/html")));
}

void TestFaviconFetcher::testSendsAnImageAcceptForIcons()
{
    StubServer server;
    QVERIFY(server.isListening());
    server.route(QStringLiteral("/i.png"), {200, QByteArrayLiteral("x"), QByteArrayLiteral("image/png")});

    Fetcher fetcher(2000);
    QVERIFY(fetcher.fetchIcon(QUrl(server.url(QStringLiteral("/i.png")))).ok());

    QVERIFY(server.lastHeaders().value(QStringLiteral("accept")).contains(QStringLiteral("image/")));
}

// The icon href is relative to where the page ended up, so the fetcher has to
// report the last URL of the chain and not the first.
void TestFaviconFetcher::testFollowsARedirect()
{
    StubServer server;
    QVERIFY(server.isListening());
    server.route(QStringLiteral("/r"),
                 {302, QByteArray(), QByteArrayLiteral("text/html"), QStringLiteral("/b")});
    server.route(QStringLiteral("/b"), {200, QByteArrayLiteral("second")});

    Fetcher fetcher(2000);
    const FetchResult result = fetcher.fetchPage(QUrl(server.url(QStringLiteral("/r"))));

    QVERIFY2(result.ok(), qPrintable(result.error));
    QCOMPARE(result.body, QByteArrayLiteral("second"));
    QCOMPARE(result.finalUrl.toString(), server.url(QStringLiteral("/b")));
    QCOMPARE(server.requests().join(QLatin1Char(',')), QStringLiteral("/r,/b"));
}

void TestFaviconFetcher::testReportsNotFound()
{
    StubServer server;
    QVERIFY(server.isListening());
    server.route(QStringLiteral("/missing"), {404, QByteArrayLiteral("nope")});

    Fetcher fetcher(2000);
    const FetchResult result = fetcher.fetchPage(QUrl(server.url(QStringLiteral("/missing"))));

    QVERIFY(!result.ok());
    QVERIFY(!result.error.isEmpty());
    QCOMPARE(result.status, 404);
    QVERIFY(result.body.isEmpty());
}

// A site without a connection is the state the widget draws grayscale, so the
// failure has to arrive as an error and not as an exception or a hang.
void TestFaviconFetcher::testReportsConnectionRefused()
{
    // Listening and then closing is the one reliable way to get a port that
    // nothing answers on.
    QTcpServer closed;
    QVERIFY(closed.listen(QHostAddress::LocalHost, 0));
    const int port = closed.serverPort();
    closed.close();

    Fetcher fetcher(2000);
    const FetchResult result =
        fetcher.fetchPage(QUrl(QStringLiteral("http://127.0.0.1:%1/x").arg(port)));

    QVERIFY(!result.ok());
    QVERIFY(!result.error.isEmpty());
    QCOMPARE(result.status, 0);
}

void TestFaviconFetcher::testTimesOut()
{
    StubServer server;
    QVERIFY(server.isListening());
    server.route(QStringLiteral("/hang"),
                 {200, QByteArray(), QByteArrayLiteral("text/html"), QString(), true});

    QElapsedTimer clock;
    clock.start();

    // Short, because this is the one test that has to wait for a timeout.
    Fetcher fetcher(300);
    const FetchResult result = fetcher.fetchPage(QUrl(server.url(QStringLiteral("/hang"))));

    QVERIFY(!result.ok());
    QVERIFY2(result.error.contains(QStringLiteral("timed out")), qPrintable(result.error));
    // The hard stop is the safety net, not the mechanism: if it is what ended the
    // request, the transfer timeout did nothing.
    QVERIFY(clock.elapsed() < 4000);
    QCOMPARE(server.requests().join(QLatin1Char(',')), QStringLiteral("/hang"));
}

void TestFaviconFetcher::testKeepsBinaryBytesExactly()
{
    QByteArray binary;
    binary.append(char(0x00));
    binary.append(char(0x89));
    binary.append("PNG\r\n", 5);
    binary.append(char(0xFF));
    binary.append(char(0x00));

    StubServer server;
    QVERIFY(server.isListening());
    server.route(QStringLiteral("/i.png"),
                 {200, binary, QByteArrayLiteral("image/png")});

    Fetcher fetcher(2000);
    const FetchResult result = fetcher.fetchIcon(QUrl(server.url(QStringLiteral("/i.png"))));

    QVERIFY2(result.ok(), qPrintable(result.error));
    // An icon is bytes, not text: NUL and 0xFF have to survive the round trip.
    QCOMPARE(result.body.size(), binary.size());
    QCOMPARE(result.body, binary);
}

void TestFaviconFetcher::testEmptyBodyIsASuccess()
{
    StubServer server;
    QVERIFY(server.isListening());
    server.route(QStringLiteral("/empty"), {200, QByteArray()});

    Fetcher fetcher(2000);
    const FetchResult result = fetcher.fetchIcon(QUrl(server.url(QStringLiteral("/empty"))));

    // The exchange worked. What an empty body means is the caller's business: the
    // CLI turns it into "this site has no icon" rather than a network error.
    QVERIFY2(result.ok(), qPrintable(result.error));
    QCOMPARE(result.status, 200);
    QVERIFY(result.body.isEmpty());
}

QTEST_GUILESS_MAIN(TestFaviconFetcher)

#include "tst_faviconfetcher.moc"
