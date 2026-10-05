#pragma once

#include <QByteArray>
#include <QImage>
#include <QString>

namespace ShowFavicon {

/// The longest edge a cached icon is stored at.
///
/// A bound on the cache, not a target: a favicon larger than this is scaled down,
/// a smaller one is kept as it is. Growing a 16 px icon to 128 px would only add
/// blur and bytes, and the panel scales the result to its own size anyway.
constexpr int kIconSize = 128;

/// What one decoded favicon became.
struct DecodedIcon {
    /// The icon at the cache size, in the format it was served.
    QImage colour;
    /// The same image with the colour taken out, alpha intact.
    QImage gray;
    QString error;

    bool ok() const { return error.isEmpty() && !colour.isNull(); }
};

/// Decodes an icon, whether the site served a bitmap or a vector.
///
/// SVG goes through Qt6::Svg; PNG, JPEG, GIF, WebP and ICO go through the image
/// plugins, and for a multi-size ICO the largest frame wins. `contentType` is
/// only a hint: a site may serve SVG from a path that says nothing about it (a
/// `.php`, for instance), so the bytes are checked as well.
DecodedIcon decodeIcon(const QByteArray &bytes, const QString &contentType = QString());

/// The grayscale copy, alpha preserved.
///
/// `QImage::convertToFormat(Format_Grayscale8)` is deliberately not used: it
/// drops the alpha channel, and a transparent favicon would come out as a black
/// square. The RGB channels are replaced by the pixel's luma, the alpha is left
/// alone.
QImage toGrayscale(const QImage &image);

/// Scales down to `size` on the longer edge, keeping the aspect ratio. Never
/// enlarges.
QImage scaleIcon(const QImage &image, int size = kIconSize);

/// The PNG bytes of one image, or an empty array when it cannot be encoded.
QByteArray encodePng(const QImage &image);

} // namespace ShowFavicon
