#include "cli.h"

#include "log.h"
#include "version.h"

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
    stream << "usage: showfavicon --version\n"
              "       showfavicon --help\n";
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
    QStringList unknown;

    for (const QString &argument : arguments) {
        if (argument == QLatin1String("--version") || argument == QLatin1String("-v")) {
            versionRequested = true;
        } else if (argument == QLatin1String("--help") || argument == QLatin1String("-h")) {
            helpRequested = true;
        } else if (!applyGlobalOption(argument)) {
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

    // Nothing to do yet: fetching a URL arrives with the next step.
    printUsage(err());
    return 2;
}

} // namespace ShowFavicon
