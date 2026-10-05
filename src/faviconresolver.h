#pragma once

#include <QByteArray>
#include <QString>
#include <QUrl>

namespace ShowFavicon {

/// The icon a page advertises, or `baseUrl`'s `/favicon.ico` when it names none.
///
/// `baseUrl` is the URL the page was actually fetched from, after redirects, so
/// that a relative href resolves against where the page really came from and not
/// against the URL that was asked for.
///
/// Only `http` and `https` are accepted: a `data:` icon is valid HTML but is not
/// something this program fetches, so a page that offers only that falls through
/// to `/favicon.ico`.
QUrl resolveIconUrl(const QByteArray &pageBytes, const QUrl &baseUrl);

/// How much the resolver wants a `rel` value; lower wins, `-1` means "not an
/// icon at all".
///
/// Exposed so the order can be checked without building a page per case.
int relRank(const QString &rel);

/// How much the resolver wants a declared type or file extension; lower wins.
///
/// PNG and SVG come before ICO because every Qt build decodes them, while ICO
/// needs the qico plugin -- and a missing image plugin draws an empty square with
/// no error at all.
int formatRank(const QString &typeOrPath);

} // namespace ShowFavicon
