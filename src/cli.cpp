#include "cli.h"

#include "faviconstore.h"
#include "log.h"
#include "version.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QTextStream>

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
int reportCached(const QString &urlText, const QString &cacheDir)
{
    const QString normalized = normalizeUrlString(urlText);
    const CacheEntry entry = entryFor(normalized, cacheDir);
    const CachedFiles cached = cachedFiles(entry);

    Log::step(QStringLiteral("cache"),
              QStringLiteral("%1 -> %2").arg(entry.key, entry.colourPath));
    if (!cached.colour.isEmpty()) {
        Log::step(QStringLiteral("cache"),
                  QStringLiteral("keeping the cached icon from the last successful run"));
    }

    out() << replyJson(normalized, false, cached.colour, cached.gray, cached.hash,
                       QStringLiteral("not implemented: no fetch yet"))
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

    return reportCached(urlText, cacheDir);
}

} // namespace ShowFavicon
