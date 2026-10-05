#pragma once

#include <QByteArray>
#include <QString>
#include <QUrl>

namespace ShowFavicon {

/// What one GET produced.
struct FetchResult {
    /// The body as it arrived. Empty on any error.
    QByteArray body;
    /// Where the body actually came from, after redirects. A relative icon href
    /// has to be resolved against this and not against the URL that was asked
    /// for, or a redirect to another directory breaks every icon URL.
    QUrl finalUrl;
    /// The `Content-Type` header, verbatim, charset and all.
    QString contentType;
    /// The HTTP status, or 0 when the request never got that far.
    int status = 0;
    /// Empty on success, otherwise why it failed.
    QString error;

    bool ok() const { return error.isEmpty(); }
};

/// A blocking HTTP GET.
///
/// One request at a time, because the whole program is one shot: the widget runs
/// this binary per site and the process ends with the answer. An asynchronous API
/// would buy nothing and cost the CLI an event loop of its own.
class Fetcher
{
public:
    /// `timeoutMs` bounds the whole exchange. It is a parameter so the tests can
    /// use a short one instead of waiting fifteen seconds for the timeout case.
    explicit Fetcher(int timeoutMs = 15000);

    /// The page itself, for scanning its icon links.
    FetchResult fetchPage(const QUrl &url);

    /// An image, for the icon the page pointed at.
    FetchResult fetchIcon(const QUrl &url);

    /// What both go through.
    FetchResult get(const QUrl &url, const QString &accept);

    int timeoutMs() const { return m_timeoutMs; }

private:
    int m_timeoutMs;
};

} // namespace ShowFavicon
