#include "cli.h"
#include "version.h"

#include <QCoreApplication>
#include <QStringList>

int main(int argc, char *argv[])
{
    QCoreApplication application(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral(SHOWFAVICON_NAME));
    QCoreApplication::setApplicationVersion(QStringLiteral(SHOWFAVICON_VERSION));
    QCoreApplication::setOrganizationName(QStringLiteral("ShowFavicon"));

    QStringList arguments;
    arguments.reserve(argc - 1);
    for (int index = 1; index < argc; ++index)
        arguments.append(QString::fromLocal8Bit(argv[index]));

    return ShowFavicon::runCli(arguments);
}
