#include "faviconimage.h"

#include <QBuffer>
#include <QImageReader>
#include <QPainter>
#include <QSvgRenderer>

namespace ShowFavicon {
namespace {

/// The first bytes of the payload, without a BOM and without surrounding
/// whitespace, for deciding whether this is SVG.
QByteArray sniffHead(const QByteArray &bytes)
{
    QByteArray head = bytes.left(512);

    static const QByteArray bom = QByteArrayLiteral("\xEF\xBB\xBF");
    if (head.startsWith(bom))
        head.remove(0, bom.size());

    return head.trimmed();
}

bool looksLikeSvg(const QByteArray &bytes, const QString &contentType)
{
    if (contentType.contains(QLatin1String("svg"), Qt::CaseInsensitive))
        return true;

    const QByteArray head = sniffHead(bytes);
    return head.startsWith("<?xml") || head.startsWith("<svg");
}

QImage rasteriseSvg(const QByteArray &bytes, QString *error)
{
    QSvgRenderer renderer(bytes);
    if (!renderer.isValid()) {
        *error = QStringLiteral("not a valid SVG");
        return QImage();
    }

    // Rendering straight to the target size is the point of a vector: crisp at
    // whatever resolution it is drawn, unlike a scaled bitmap.
    QSize size = renderer.defaultSize();
    if (!size.isValid() || size.isEmpty())
        size = QSize(kIconSize, kIconSize);

    // Only shrink. QSize::scale() grows as well, and an SVG that declares 32 px
    // should be rasterised at 32 px rather than blown up to the cache size.
    if (size.width() > kIconSize || size.height() > kIconSize)
        size.scale(kIconSize, kIconSize, Qt::KeepAspectRatio);

    if (size.isEmpty())
        size = QSize(kIconSize, kIconSize);

    QImage image(size, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    renderer.render(&painter);
    painter.end();
    return image;
}

/// The largest frame in a multi-size file.
///
/// A `.ico` holds several sizes and `read()` hands back the first, which is often
/// the 16 px one -- the size that suffers most from being scaled to a panel.
QImage readLargestFrame(QImageReader &reader)
{
    const int count = reader.imageCount();
    if (count <= 1)
        return reader.read();

    QImage best;
    for (int index = 0; index < count; ++index) {
        if (!reader.jumpToImage(index))
            continue;
        const QImage candidate = reader.read();
        if (candidate.isNull())
            continue;
        if (best.isNull()
            || candidate.width() * candidate.height() > best.width() * best.height()) {
            best = candidate;
        }
    }
    return best;
}

} // namespace

QImage toGrayscale(const QImage &image)
{
    if (image.isNull())
        return image;

    // ARGB32 rather than Grayscale8: the alpha channel has to survive, and the
    // scanline has to be RGBA to rewrite the colour channels in place.
    QImage result = image.convertToFormat(QImage::Format_ARGB32);
    for (int y = 0; y < result.height(); ++y) {
        auto *line = reinterpret_cast<QRgb *>(result.scanLine(y));
        for (int x = 0; x < result.width(); ++x) {
            const QRgb pixel = line[x];
            const int luma = qGray(pixel);
            line[x] = qRgba(luma, luma, luma, qAlpha(pixel));
        }
    }
    return result;
}

QImage scaleIcon(const QImage &image, int size)
{
    if (image.isNull() || size <= 0)
        return image;

    if (image.width() <= size && image.height() <= size)
        return image;

    const QSize target = image.size().scaled(size, size, Qt::KeepAspectRatio);
    if (!target.isValid() || target.isEmpty())
        return image;

    return image.scaled(target, Qt::KeepAspectRatio, Qt::SmoothTransformation);
}

QByteArray encodePng(const QImage &image)
{
    if (image.isNull())
        return QByteArray();

    QByteArray bytes;
    QBuffer buffer(&bytes);
    if (!buffer.open(QIODevice::WriteOnly))
        return QByteArray();

    if (!image.save(&buffer, "PNG"))
        return QByteArray();

    return bytes;
}

DecodedIcon decodeIcon(const QByteArray &bytes, const QString &contentType)
{
    DecodedIcon icon;
    if (bytes.isEmpty()) {
        icon.error = QStringLiteral("empty image");
        return icon;
    }

    if (looksLikeSvg(bytes, contentType)) {
        QString error;
        const QImage rasterised = rasteriseSvg(bytes, &error);
        if (rasterised.isNull()) {
            icon.error = error;
            return icon;
        }
        icon.colour = scaleIcon(rasterised);
    } else {
        QBuffer buffer;
        buffer.setData(bytes);
        if (!buffer.open(QIODevice::ReadOnly)) {
            icon.error = QStringLiteral("cannot read the image data");
            return icon;
        }

        QImageReader reader(&buffer);
        reader.setAutoTransform(true);

        const QImage image = readLargestFrame(reader);
        if (image.isNull()) {
            // The reader's own message names the format when the plugin for it is
            // missing, which is the difference between "not an image" and "this
            // build cannot read ICO".
            icon.error = reader.errorString();
            return icon;
        }
        icon.colour = scaleIcon(image);
    }

    icon.gray = toGrayscale(icon.colour);
    return icon;
}

} // namespace ShowFavicon
