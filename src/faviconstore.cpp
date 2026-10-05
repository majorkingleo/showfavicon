#include "faviconstore.h"

#include "paths.h"

#include <QByteArray>
#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSaveFile>
#include <QUrl>

namespace ShowFavicon {
namespace {

/// How much of the digest ends up in a file name. Eight hex characters are
/// enough that a collision would need a deliberate attack, and short enough that
/// the name still shows the host.
constexpr int kKeyDigestLength = 8;

QString digest(const QByteArray &bytes, int length)
{
    const QByteArray hex = QCryptographicHash::hash(bytes, QCryptographicHash::Sha1).toHex();
    return QString::fromLatin1(length > 0 ? hex.left(length) : hex);
}

/// Everything a file name may not carry, plus the characters that would make the
/// name look like a path. A host cannot contain any of them, but the function
/// does not depend on that staying true.
QString sanitiseHost(const QString &host)
{
    static const QRegularExpression unwanted(QStringLiteral("[^A-Za-z0-9._-]"));
    QString safe = host;
    safe.remove(unwanted);
    return safe.isEmpty() ? QStringLiteral("site") : safe;
}

QString hostOf(const QString &normalizedUrl)
{
    // The text is already normalised, so it has a scheme and QUrl parses it the
    // same way every time.
    return QUrl(normalizedUrl).host();
}

} // namespace

QString normalizeUrlString(const QString &raw)
{
    const QString trimmed = raw.trimmed();
    if (trimmed.isEmpty())
        return QString();

    static const QRegularExpression hasScheme(QStringLiteral("^[A-Za-z][A-Za-z0-9+.-]*://"));
    if (hasScheme.match(trimmed).hasMatch())
        return trimmed;

    return QStringLiteral("https://") + trimmed;
}

QString cacheKey(const QString &urlText)
{
    const QString normalized = normalizeUrlString(urlText);
    const QString host = sanitiseHost(hostOf(normalized));
    return host + QLatin1Char('-') + digest(normalized.toUtf8(), kKeyDigestLength);
}

CacheEntry entryFor(const QString &urlText, const QString &cacheDir)
{
    const QString normalized = normalizeUrlString(urlText);
    const QDir directory(cacheDir.isEmpty() ? Paths::dataDir() : cacheDir);

    CacheEntry entry;
    entry.key = cacheKey(normalized);
    entry.colourPath = directory.filePath(entry.key + QStringLiteral(".png"));
    entry.grayPath = directory.filePath(entry.key + QStringLiteral(".gray.png"));
    return entry;
}

QString writeAtomic(const QString &path, const QByteArray &bytes, QString *error)
{
    if (error)
        error->clear();

    // The caller names a directory it may not have created yet: `--cache-dir` is
    // whatever the user typed, and on the very first run the parent is missing.
    // Leaving that to the caller made the first run report "No such file or
    // directory" after a successful fetch, which is the least useful moment.
    const QString parent = QFileInfo(path).absolutePath();
    if (!parent.isEmpty() && !QDir().mkpath(parent)) {
        if (error)
            *error = QStringLiteral("cannot create %1").arg(parent);
        return QString();
    }

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        if (error)
            *error = QStringLiteral("cannot open %1: %2").arg(path, file.errorString());
        return QString();
    }

    if (file.write(bytes) != bytes.size()) {
        if (error)
            *error = QStringLiteral("short write to %1: %2").arg(path, file.errorString());
        file.cancelWriting();
        return QString();
    }

    if (!file.commit()) {
        if (error)
            *error = QStringLiteral("cannot commit %1: %2").arg(path, file.errorString());
        return QString();
    }

    return digest(bytes, 0);
}

QString sha1OfFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return QString();
    return digest(file.readAll(), 0);
}

bool hasFile(const QString &path)
{
    if (path.isEmpty())
        return false;

    const QFileInfo info(path);
    return info.exists() && info.isFile() && info.size() > 0;
}

CachedFiles cachedFiles(const CacheEntry &entry)
{
    CachedFiles files;
    if (hasFile(entry.colourPath)) {
        files.colour = entry.colourPath;
        files.hash = sha1OfFile(entry.colourPath);
    }
    if (hasFile(entry.grayPath))
        files.gray = entry.grayPath;
    return files;
}

} // namespace ShowFavicon
