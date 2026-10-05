#include "faviconresolver.h"

#include <QHash>
#include <QRegularExpression>

namespace ShowFavicon {
namespace {

/// Every attribute of one tag, keys lower-cased.
///
/// Named groups rather than numbered ones: with three alternatives for the value
/// an unnamed group that did not take part and one that captured the empty string
/// are easy to confuse, and `hasCaptured()` settles it.
QHash<QString, QString> attributesOf(const QString &tag)
{
    static const QRegularExpression attribute(
        QStringLiteral("(?<key>[A-Za-z_:][-A-Za-z0-9_:.]*)\\s*=\\s*"
                       "(?:\"(?<double>[^\"]*)\"|'(?<single>[^']*)'|(?<bare>[^\\s\"'>]+))"));

    QHash<QString, QString> attributes;
    auto matches = attribute.globalMatch(tag);
    while (matches.hasNext()) {
        const QRegularExpressionMatch match = matches.next();

        QString value;
        if (match.hasCaptured(QStringLiteral("double")))
            value = match.captured(QStringLiteral("double"));
        else if (match.hasCaptured(QStringLiteral("single")))
            value = match.captured(QStringLiteral("single"));
        else
            value = match.captured(QStringLiteral("bare"));

        attributes.insert(match.captured(QStringLiteral("key")).toLower(), value);
    }
    return attributes;
}

bool isFetchable(const QUrl &url)
{
    const QString scheme = url.scheme();
    return scheme == QLatin1String("http") || scheme == QLatin1String("https");
}

} // namespace

int relRank(const QString &rel)
{
    // simplified() collapses runs of whitespace, so `shortcut  icon` and
    // `shortcut icon` are the same value.
    const QString value = rel.toLower().simplified();

    if (value == QLatin1String("icon"))
        return 0;
    if (value == QLatin1String("shortcut icon") || value == QLatin1String("icon shortcut"))
        return 1;
    if (value == QLatin1String("apple-touch-icon"))
        return 2;
    if (value == QLatin1String("apple-touch-icon-precomposed"))
        return 3;
    if (value == QLatin1String("mask-icon"))
        return 4;

    // The rest of the family -- `apple-touch-icon-120x120`, `fluid-icon`,
    // `alternate icon` -- still beats the fallback, just not the plain ones.
    if (value.contains(QLatin1String("icon")))
        return 5;

    return -1;
}

int formatRank(const QString &typeOrPath)
{
    const QString value = typeOrPath.toLower();
    if (value.isEmpty())
        return 3;

    if (value.contains(QLatin1String("png")))
        return 0;
    if (value.contains(QLatin1String("svg")))
        return 1;
    if (value.contains(QLatin1String("webp")) || value.contains(QLatin1String("jpeg"))
        || value.contains(QLatin1String("jpg")) || value.contains(QLatin1String("gif")))
        return 2;

    // `image/x-icon`, `image/vnd.microsoft.icon`, `/favicon.ico`.
    if (value.contains(QLatin1String("icon")) || value.endsWith(QLatin1String(".ico")))
        return 4;

    return 3;
}

QUrl resolveIconUrl(const QByteArray &pageBytes, const QUrl &baseUrl)
{
    const QUrl fallback = baseUrl.resolved(QUrl(QStringLiteral("/favicon.ico")));
    if (pageBytes.isEmpty())
        return fallback;

    // Latin-1 rather than UTF-8: every byte maps to exactly one code point, so
    // the ASCII tag names, attribute names and (percent-encoded) hrefs survive
    // unchanged whatever charset the page claims.
    const QString text = QString::fromLatin1(pageBytes);

    // A `>` inside an attribute value, or a tag inside a comment, would fool
    // this. Both are rare enough to accept: the cost is a wrong icon, not a
    // crash, and the fallback is still a valid URL.
    static const QRegularExpression linkTag(QStringLiteral("<link\\b[^>]*>"),
                                            QRegularExpression::CaseInsensitiveOption);

    int bestScore = -1;
    QUrl bestUrl;

    auto matches = linkTag.globalMatch(text);
    while (matches.hasNext()) {
        const QHash<QString, QString> attributes = attributesOf(matches.next().captured(0));

        const int rank = relRank(attributes.value(QStringLiteral("rel")));
        if (rank < 0)
            continue;

        const QString href = attributes.value(QStringLiteral("href")).trimmed();
        if (href.isEmpty())
            continue;

        const QUrl resolved = baseUrl.resolved(QUrl(href));
        if (!isFetchable(resolved))
            continue;

        // A declared type is the server's own statement about what it will send,
        // so it beats a guess from the path.
        const QString type = attributes.value(QStringLiteral("type")).trimmed();
        const int score = rank * 10 + formatRank(type.isEmpty() ? href : type);

        // Strict `<`, so the first of two equal candidates wins: the page's own
        // order is the tie breaker, as in a browser.
        if (bestScore < 0 || score < bestScore) {
            bestScore = score;
            bestUrl = resolved;
        }
    }

    return bestScore < 0 ? fallback : bestUrl;
}

} // namespace ShowFavicon
