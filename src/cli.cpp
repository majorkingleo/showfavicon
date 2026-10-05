#include "cli.h"

#include "faviconfetcher.h"
#include "faviconimage.h"
#include "faviconresolver.h"
#include "faviconstore.h"
#include "log.h"
#include "version.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QTextStream>
#include <QUrl>

namespace ShowFavicon {
namespace {

/// The JSON reply goes here and nowhere else: the widget parses the last line of
/// stdout, so a diagnostic printed alongside it would break that parse.
QTextStream &out()
{
    static QTextStream stream(stdout);
    return stream;
}

QTextStream &err()
{
    static QTextStream stream(stderr);
    return stream;
}

void printUsage(QTextStream &stream)
{
    stream << "usage: showfavicon <url> [--cache-dir DIR] [--verbose|--debug] [--no-color]\n"
              "       showfavicon --version\n";
}

/// The one JSON line the widget parses. Built in one place so the shape cannot
/// drift from the six keys `logic.js` reads.
///
/// An absent path is null, not an empty string: the QML module treats a missing
/// key as undefined and would dim an icon that is not there.
QString replyJson(const QString &site, bool ok, const QString &colour, const QString &gray,
                  const QString &hash, const QString &error)
{
    QJsonObject reply;
    reply.insert(QStringLiteral("site"), site);
    reply.insert(QStringLiteral("ok"), ok);
    reply.insert(QStringLiteral("file"), colour.isEmpty() ? QJsonValue() : QJsonValue(colour));
    reply.insert(QStringLiteral("grayFile"), gray.isEmpty() ? QJsonValue() : QJsonValue(gray));
    reply.insert(QStringLiteral("hash"), hash.isEmpty() ? QJsonValue() : QJsonValue(hash));
    reply.insert(QStringLiteral("error"), error);
    return QString::fromUtf8(QJsonDocument(reply).toJson(QJsonDocument::Compact));
}

/// Answers one URL.
///
/// Fetching arrives with the next step, so for now the reply says what is already
/// cached and nothing else. The shape is the final one, which is what makes it
/// worth wiring up before the network is in place.
/// The outcome of one refresh attempt.
struct Refresh {
    bool ok = false;
    QString hash;
    QString error;
};

/// Fetches the page, picks its icon, decodes it and writes both copies.
Refresh refreshIcon(const QUrl &site, const CacheEntry &entry)
{
    Refresh refresh;
    Fetcher fetcher;

    // The page is read for the icon it advertises and for nothing else. When it
    // cannot be fetched the icon is tried anyway: plenty of sites answer 403 for
    // the page and serve the icon happily.
    const FetchResult page = fetcher.fetchPage(site);

    QUrl iconUrl = site.resolved(QUrl(QStringLiteral("/favicon.ico")));
    if (page.ok() && !page.body.isEmpty()) {
        iconUrl = resolveIconUrl(page.body, page.finalUrl);
    } else if (!page.ok()) {
        Log::warn(QStringLiteral("page"), page.error);
    }

    const FetchResult icon = fetcher.fetchIcon(iconUrl);
    if (!icon.ok()) {
        refresh.error = icon.error;
        return refresh;
    }
    if (icon.body.isEmpty()) {
        refresh.error = QStringLiteral("the site served an empty icon");
        return refresh;
    }

    const DecodedIcon decoded = decodeIcon(icon.body, icon.contentType);
    if (!decoded.ok()) {
        refresh.error = QStringLiteral("cannot decode the icon: %1").arg(decoded.error);
        return refresh;
    }

    const QByteArray colour = encodePng(decoded.colour);
    const QByteArray gray = encodePng(decoded.gray);
    if (colour.isEmpty() || gray.isEmpty()) {
        refresh.error = QStringLiteral("cannot encode the icon as PNG");
        return refresh;
    }

    // Both files or neither in practice: the colour one commits first, and a
    // failure on the gray copy leaves the previous pair in place rather than a new
    // colour next to an old gray.
    QString writeError;
    const QString digest = writeAtomic(entry.colourPath, colour, &writeError);
    if (digest.isEmpty()) {
        refresh.error = writeError;
        return refresh;
    }
    if (writeAtomic(entry.grayPath, gray, &writeError).isEmpty()) {
        refresh.error = writeError;
        return refresh;
    }

    Log::step(QStringLiteral("write"),
              QStringLiteral("cached a %1x%2 icon, %3 bytes")
                  .arg(decoded.colour.width())
                  .arg(decoded.colour.height())
                  .arg(colour.size()));

    refresh.ok = true;
    refresh.hash = digest;
    return refresh;
}

/// Answers one URL with exactly one JSON line.
///
/// The exit code stays 0 even when the site could not be reached: that is a
/// successful run of the program that found the site down, and the answer carries
/// the truth in `ok`. The widget never looks at the code, only at the line.
int fetchAndReport(const QString &urlText, const QString &cacheDir)
{
    const QString normalized = normalizeUrlString(urlText);
    if (normalized.isEmpty()) {
        err() << "not a URL: " << urlText << Qt::endl;
        return 2;
    }

    const CacheEntry entry = entryFor(normalized, cacheDir);
    Log::step(QStringLiteral("cache"), QStringLiteral("%1 -> %2").arg(entry.key, entry.colourPath));

    const Refresh refresh = refreshIcon(QUrl(normalized), entry);
    if (refresh.ok) {
        out() << replyJson(normalized, true, entry.colourPath, entry.grayPath, refresh.hash,
                           QString())
              << Qt::endl;
        return 0;
    }

    // Failed. Report what is already cached, so the widget keeps the last icon and
    // draws it grayscale instead of going blank -- which is the whole point of the
    // grayscale copy.
    const CachedFiles cached = cachedFiles(entry);
    if (!cached.colour.isEmpty())
        Log::step(QStringLiteral("cache"), QStringLiteral("keeping the icon from the last run"));
    Log::warn(QStringLiteral("fetch"), refresh.error);

    out() << replyJson(normalized, false, cached.colour, cached.gray, cached.hash, refresh.error)
          << Qt::endl;
    return 0;
}

/// The global options that are not tied to fetching. Returns false when the
/// argument is not one of them, so the caller can report it.
bool applyGlobalOption(const QString &argument)
{
    if (argument == QLatin1String("--verbose")) {
        Log::setLevel(Log::Level::Steps);
        return true;
    }
    if (argument == QLatin1String("--debug")) {
        Log::setLevel(Log::Level::Detail);
        return true;
    }
    if (argument == QLatin1String("--no-color")) {
        Log::setColour(Log::Colour::Never);
        return true;
    }
    return false;
}

} // namespace

int runCli(const QStringList &arguments)
{
    // Everything is parsed before anything runs, so a wrong option is reported
    // instead of half a run having happened.
    bool versionRequested = false;
    bool helpRequested = false;
    QString cacheDir;
    QString urlText;
    QStringList unknown;

    for (int index = 0; index < arguments.size(); ++index) {
        const QString &argument = arguments.at(index);

        if (argument == QLatin1String("--version") || argument == QLatin1String("-v")) {
            versionRequested = true;
        } else if (argument == QLatin1String("--help") || argument == QLatin1String("-h")) {
            helpRequested = true;
        } else if (argument == QLatin1String("--cache-dir")) {
            if (index + 1 >= arguments.size()) {
                err() << "--cache-dir needs a directory" << Qt::endl;
                printUsage(err());
                return 2;
            }
            cacheDir = arguments.at(++index);
        } else if (argument.startsWith(QLatin1Char('-'))) {
            if (!applyGlobalOption(argument))
                unknown.append(argument);
        } else if (urlText.isEmpty()) {
            urlText = argument;
        } else {
            // A second URL is not a second fetch: the widget runs one command per
            // site, so this is a mistake worth naming rather than ignoring.
            unknown.append(argument);
        }
    }

    Log::detail(QStringLiteral("cli"),
                QStringLiteral("parsed %1 argument(s): %2")
                    .arg(arguments.size())
                    .arg(arguments.isEmpty() ? QStringLiteral("(none)")
                                             : arguments.join(QLatin1Char(' '))));

    if (helpRequested) {
        printUsage(out());
        return 0;
    }

    if (versionRequested) {
        out() << SHOWFAVICON_NAME << ' ' << SHOWFAVICON_VERSION << Qt::endl;
        return 0;
    }

    if (!unknown.isEmpty()) {
        err() << "unrecognised argument: " << unknown.first() << Qt::endl;
        printUsage(err());
        return 2;
    }

    if (urlText.isEmpty()) {
        printUsage(err());
        return 2;
    }

    return fetchAndReport(urlText, cacheDir);
}

} // namespace ShowFavicon
