#include "paths.h"

#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>

namespace ShowFavicon::Paths {

QString dataDir()
{
    const QString base = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    const QString root = base.isEmpty() ? QDir::homePath() + QStringLiteral("/.local/share") : base;
    return QDir(root).filePath(QStringLiteral("showfavicon"));
}

QString resolvePath(const QString &path)
{
    if (path.isEmpty())
        return path;

    const QFileInfo info(path);
    const QString canonical = info.canonicalFilePath();
    if (!canonical.isEmpty())
        return canonical;

    return QDir::cleanPath(info.absoluteFilePath());
}

} // namespace ShowFavicon::Paths
