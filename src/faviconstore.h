#pragma once

#include <QString>

namespace ShowFavicon {

/// The names one site's cached files are stored under.
///
/// The key is `<sanitised host>-<sha1(normalised url)[:8]>`: the host so the
/// cache can be read by eye, the digest so two sites on the same host -- and two
/// URLs that differ only in their query -- cannot collide.
struct CacheEntry {
    QString key;
    QString colourPath;
    QString grayPath;
};

/// What is on disk for one entry right now.
///
/// A run that could not fetch reports this instead of the files it would have
/// written, which is what lets the widget keep showing the previous icon.
struct CachedFiles {
    QString colour;
    QString gray;
    QString hash;
};

/// Trim, and prepend `https://` when there is no scheme.
///
/// The same normalisation the QML module does in `logic.js`, so both sides agree
/// on what a given entry means. The digest is taken over exactly this string.
QString normalizeUrlString(const QString &raw);

/// `<sanitised host>-<sha1(normalised url)[:8]>`.
///
/// The digest is taken over the normalised text, not over `QUrl`'s own
/// serialisation, so the key for `example.com` and `https://example.com` is the
/// same and so is the one the first, script based implementation produced.
QString cacheKey(const QString &urlText);

/// The entry for one URL. An empty `cacheDir` means `Paths::dataDir()`.
CacheEntry entryFor(const QString &urlText, const QString &cacheDir = QString());

/// Writes `bytes` and returns their full sha1, or an empty string with `error`
/// set.
///
/// QSaveFile, so a failure leaves the previous file untouched: the widget shows
/// the last icon while a site is unreachable, and half a PNG would replace it
/// with a broken one.
QString writeAtomic(const QString &path, const QByteArray &bytes, QString *error);

/// The full sha1 of a file, or an empty string when it cannot be read.
QString sha1OfFile(const QString &path);

/// True when the path names a file that is not empty.
bool hasFile(const QString &path);

/// What is cached for `entry`: the paths that exist, and the colour file's sha1.
CachedFiles cachedFiles(const CacheEntry &entry);

} // namespace ShowFavicon
