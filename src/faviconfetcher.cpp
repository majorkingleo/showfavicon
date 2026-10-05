#include "faviconfetcher.h"

#include "log.h"

#include <QEventLoop>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>

namespace ShowFavicon {
namespace {

/// Enough for the http-to-https and trailing-slash hops a site actually uses,
/// and few enough that a redirect loop ends instead of running to the timeout.
constexpr int kMaxRedirects = 5;

/// How long past the transfer timeout the hard stop waits before aborting. The
/// transfer timeout covers a stalled connection; this covers everything else.
constexpr int kGuardSlackMs = 5000;

QString browserUserAgent()
{
    // Not a Qt default: a handful of sites serve a different page -- or nothing
    // at all -- to a client that does not look like a browser.
    return QStringLiteral("Mozilla/5.0 (X11; Linux x86_64) ShowFavicon/1.0");
}

} // namespace

Fetcher::Fetcher(int timeoutMs)
    : m_timeoutMs(timeoutMs > 0 ? timeoutMs : 15000)
{
}

FetchResult Fetcher::fetchPage(const QUrl &url)
{
    return get(url, QStringLiteral("text/html,application/xhtml+xml,*/*;q=0.8"));
}

FetchResult Fetcher::fetchIcon(const QUrl &url)
{
    return get(url, QStringLiteral("image/*,*/*;q=0.8"));
}

FetchResult Fetcher::get(const QUrl &url, const QString &accept)
{
    FetchResult result;
    result.finalUrl = url;

    Log::step(QStringLiteral("fetch"), QStringLiteral("GET %1").arg(url.toString()));

    // One manager per request: it would hold connections open for a program that
    // is about to exit, and nothing here reuses a connection anyway.
    QNetworkAccessManager manager;

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, browserUserAgent());
    request.setRawHeader("Accept", accept.toUtf8());
    request.setTransferTimeout(m_timeoutMs);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setMaximumRedirectsAllowed(kMaxRedirects);

    QNetworkReply *reply = manager.get(request);

    bool timedOut = false;
    QEventLoop loop;
    QTimer guard;

    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(&guard, &QTimer::timeout, &loop, [&]() {
        timedOut = true;
        reply->abort();
    });

    guard.setSingleShot(true);
    guard.start(m_timeoutMs + kGuardSlackMs);
    loop.exec();
    guard.stop();

    result.finalUrl = reply->url();
    result.contentType = reply->header(QNetworkRequest::ContentTypeHeader).toString();
    result.status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    if (timedOut) {
        result.error = QStringLiteral("timed out after %1 ms").arg(m_timeoutMs);
    } else if (reply->error() != QNetworkReply::NoError) {
        result.error = reply->errorString();
    } else {
        result.body = reply->readAll();
    }

    if (result.ok()) {
        Log::detail(QStringLiteral("fetch"),
                    QStringLiteral("%1, %2 bytes, %3")
                        .arg(result.status)
                        .arg(result.body.size())
                        .arg(result.contentType.isEmpty() ? QStringLiteral("no content type")
                                                         : result.contentType));
        Log::detail(QStringLiteral("fetch"),
                    QStringLiteral("landed on %1").arg(result.finalUrl.toString()));
    } else {
        Log::warn(QStringLiteral("fetch"), result.error);
    }

    // The reply is a child of the manager, which is about to go out of scope.
    return result;
}

} // namespace ShowFavicon
