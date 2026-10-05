#include "faviconimage.h"

#include <QBuffer>
#include <QImageReader>
#include <QTest>

using namespace ShowFavicon;

namespace {

QImage filled(int width, int height, QRgb colour)
{
    QImage image(width, height, QImage::Format_ARGB32);
    image.fill(colour);
    return image;
}

/// Encoded with QImage here rather than through encodePng(), so the test does not
/// end up checking the encoder against itself.
QByteArray pngOf(const QImage &image)
{
    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");
    return bytes;
}

QByteArray svgOf(int size)
{
    return QStringLiteral("<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"%1\" height=\"%1\" "
                          "viewBox=\"0 0 %1 %1\">"
                          "<rect width=\"%1\" height=\"%1\" fill=\"#ff0000\"/></svg>")
        .arg(size)
        .toUtf8();
}

/// A hand-built 1x1 ICO, because Qt's ICO plugin can read that format but not
/// write it.
///
/// Short enough to build exactly: a six byte directory header, one sixteen byte
/// entry, then a forty byte BITMAPINFOHEADER whose height is doubled to cover the
/// XOR image and the AND mask. The mask row is padded to four bytes.
QByteArray onePixelIco(quint8 red, quint8 green, quint8 blue)
{
    const quint32 dibSize = 40 + 4 + 4;

    QByteArray ico;
    const auto put16 = [&ico](quint32 value) {
        for (int shift = 0; shift < 16; shift += 8)
            ico.append(static_cast<char>((value >> shift) & 0xFF));
    };
    const auto put32 = [&ico](quint32 value) {
        for (int shift = 0; shift < 32; shift += 8)
            ico.append(static_cast<char>((value >> shift) & 0xFF));
    };

    put16(0);            // reserved
    put16(1);            // type: icon
    put16(1);            // one image
    ico.append(char(1)); // width
    ico.append(char(1)); // height
    ico.append(char(0)); // palette size
    ico.append(char(0)); // reserved
    put16(1);            // planes
    put16(32);           // bits per pixel
    put32(dibSize);      // bytes of the DIB
    put32(22);           // where the DIB starts

    put32(40);           // BITMAPINFOHEADER
    put32(1);            // width
    put32(2);            // height: XOR image plus AND mask
    put16(1);            // planes
    put16(32);           // bits per pixel
    put32(0);            // no compression
    put32(dibSize - 40); // pixel data plus mask
    put32(0);            // x pixels per meter
    put32(0);            // y pixels per meter
    put32(0);            // colours used
    put32(0);            // important colours

    ico.append(char(blue));
    ico.append(char(green));
    ico.append(char(red));
    ico.append(char(255));

    put32(0);            // AND mask row, padded to four bytes
    return ico;
}

} // namespace

class TestFaviconImage : public QObject
{
    Q_OBJECT

private slots:
    void testGrayscaleEqualisesChannels();
    void testGrayscalePreservesAlpha();
    void testGrayscaleKeepsTheSize();

    void testScaleDownKeepsTheAspectRatio();
    void testScaleNeverEnlarges();
    void testScaleIgnoresNullImages();

    void testDecodePng();
    void testDecodeIgnoresAMisleadingContentType();
    void testDecodeSvg();
    void testDecodeSniffsSvgFromTheBytes();
    void testDecodeIco();
    void testDecodeRejectsGarbage();
    void testDecodeRejectsEmptyInput();

    void testEncodePngRoundTrips();
};

void TestFaviconImage::testGrayscaleEqualisesChannels()
{
    const QImage gray = toGrayscale(filled(2, 1, qRgb(200, 40, 90)));
    const QRgb pixel = gray.pixel(0, 0);

    QCOMPARE(qRed(pixel), qGreen(pixel));
    QCOMPARE(qGreen(pixel), qBlue(pixel));
    QCOMPARE(qRed(pixel), qGray(200, 40, 90));
}

void TestFaviconImage::testGrayscalePreservesAlpha()
{
    QImage image(2, 1, QImage::Format_ARGB32);
    image.setPixel(0, 0, qRgba(10, 200, 30, 255));
    image.setPixel(1, 0, qRgba(10, 200, 30, 0));

    const QImage gray = toGrayscale(image);

    QCOMPARE(qAlpha(gray.pixel(0, 0)), 255);
    // The reason Format_Grayscale8 is not used: a transparent pixel has to stay
    // transparent instead of turning into a black one.
    QCOMPARE(qAlpha(gray.pixel(1, 0)), 0);
}

void TestFaviconImage::testGrayscaleKeepsTheSize()
{
    QCOMPARE(toGrayscale(filled(7, 3, qRgb(1, 2, 3))).size(), QSize(7, 3));
}

void TestFaviconImage::testScaleDownKeepsTheAspectRatio()
{
    QCOMPARE(scaleIcon(filled(300, 150, qRgb(0, 0, 0))).size(), QSize(128, 64));
}

void TestFaviconImage::testScaleNeverEnlarges()
{
    // A 16 px favicon blown up to 128 px is a blurry 128 px favicon, and the panel
    // scales the result to its own size anyway.
    QCOMPARE(scaleIcon(filled(16, 16, qRgb(0, 0, 0))).size(), QSize(16, 16));
}

void TestFaviconImage::testScaleIgnoresNullImages()
{
    QVERIFY(scaleIcon(QImage()).isNull());
    QVERIFY(scaleIcon(QImage(), 0).isNull());
}

void TestFaviconImage::testDecodePng()
{
    const DecodedIcon icon = decodeIcon(pngOf(filled(64, 64, qRgb(1, 2, 3))), "image/png");

    QVERIFY2(icon.ok(), qPrintable(icon.error));
    QCOMPARE(icon.colour.size(), QSize(64, 64));
    QVERIFY(!icon.gray.isNull());
    QCOMPARE(icon.gray.size(), QSize(64, 64));
}

void TestFaviconImage::testDecodeIgnoresAMisleadingContentType()
{
    // A server that says image/x-icon while sending a PNG. The bytes decide; the
    // type is only a hint.
    const DecodedIcon icon = decodeIcon(pngOf(filled(32, 32, qRgb(9, 9, 9))), "image/x-icon");

    QVERIFY2(icon.ok(), qPrintable(icon.error));
    QCOMPARE(icon.colour.size(), QSize(32, 32));
}

void TestFaviconImage::testDecodeSvg()
{
    const DecodedIcon icon = decodeIcon(svgOf(32), "image/svg+xml");

    QVERIFY2(icon.ok(), qPrintable(icon.error));
    // Declared at 32 px, so rendered at 32 px: the size is not padded up to the
    // cache bound.
    QCOMPARE(icon.colour.size(), QSize(32, 32));
    QVERIFY(!icon.gray.isNull());
}

void TestFaviconImage::testDecodeSniffsSvgFromTheBytes()
{
    // The example site serves its SVG from favicon.php. A site could serve one
    // under a type that says nothing about it, so the bytes are checked as well.
    const DecodedIcon icon = decodeIcon(svgOf(24), QStringLiteral("text/html"));

    QVERIFY2(icon.ok(), qPrintable(icon.error));
    QCOMPARE(icon.colour.size(), QSize(24, 24));
}

void TestFaviconImage::testDecodeIco()
{
    if (!QImageReader::supportedImageFormats().contains(QByteArrayLiteral("ico")))
        QSKIP("the qico image plugin is not installed");

    const DecodedIcon icon = decodeIcon(onePixelIco(0x99, 0x66, 0x33), "image/x-icon");

    QVERIFY2(icon.ok(), qPrintable(icon.error));
    QCOMPARE(icon.colour.size(), QSize(1, 1));
    QCOMPARE(qRed(icon.colour.pixel(0, 0)), 0x99);
    QCOMPARE(qGreen(icon.colour.pixel(0, 0)), 0x66);
    QCOMPARE(qBlue(icon.colour.pixel(0, 0)), 0x33);
    QVERIFY(!icon.gray.isNull());
}

void TestFaviconImage::testDecodeRejectsGarbage()
{
    const DecodedIcon icon =
        decodeIcon(QByteArrayLiteral("this is not an image at all"), "image/png");

    QVERIFY(!icon.ok());
    QVERIFY(!icon.error.isEmpty());
    QVERIFY(icon.colour.isNull());
}

void TestFaviconImage::testDecodeRejectsEmptyInput()
{
    const DecodedIcon icon = decodeIcon(QByteArray(), "image/png");

    QVERIFY(!icon.ok());
    QCOMPARE(icon.error, QStringLiteral("empty image"));
}

void TestFaviconImage::testEncodePngRoundTrips()
{
    const QImage image = filled(20, 10, qRgba(1, 2, 3, 4));
    const QByteArray png = encodePng(image);
    QVERIFY(!png.isEmpty());

    const QImage back = QImage::fromData(png, "PNG");
    QCOMPARE(back.size(), QSize(20, 10));
    QCOMPARE(back.pixel(0, 0), image.pixel(0, 0));

    QVERIFY(encodePng(QImage()).isEmpty());
}

QTEST_GUILESS_MAIN(TestFaviconImage)

#include "tst_faviconimage.moc"
