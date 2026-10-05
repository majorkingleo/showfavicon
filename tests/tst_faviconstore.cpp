#include "faviconstore.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

using namespace ShowFavicon;

class TestFaviconStore : public QObject
{
    Q_OBJECT

private slots:
    void testNormalizesUrl();
    void testNormalizesUrl_data();

    void testKeyIsStable();
    void testKeyIsStable_data();

    void testKeyMatchesTheScriptImplementation();

    void testEntryPaths();

    void testWriteIsAtomic();
    void testWriteCreatesTheDirectory();
    void testWriteReportsFailure();

    void testHasFileRejectsEmptyFile();

    void testCachedFilesReportsWhatIsThere();
    void testCachedFilesIsEmptyWithoutFiles();
};

void TestFaviconStore::testNormalizesUrl_data()
{
    QTest::addColumn<QString>("raw");
    QTest::addColumn<QString>("expected");

    QTest::newRow("keeps a scheme") << "https://a.example/x" << "https://a.example/x";
    QTest::newRow("adds https") << "a.example" << "https://a.example";
    QTest::newRow("trims") << "  https://a.example/x  " << "https://a.example/x";
    QTest::newRow("keeps http") << "http://a.example" << "http://a.example";
    QTest::newRow("empty stays empty") << "   " << "";
}

void TestFaviconStore::testNormalizesUrl()
{
    QFETCH(QString, raw);
    QFETCH(QString, expected);

    QCOMPARE(normalizeUrlString(raw), expected);
}

void TestFaviconStore::testKeyIsStable_data()
{
    QTest::addColumn<QString>("first");
    QTest::addColumn<QString>("second");

    QTest::newRow("scheme is optional") << "a.example" << "https://a.example";
    QTest::newRow("padding is irrelevant") << "  a.example  " << "https://a.example";
}

// The two spellings have to end up on the same file, or the widget would fetch
// the icon twice and show whichever ran last.
void TestFaviconStore::testKeyIsStable()
{
    QFETCH(QString, first);
    QFETCH(QString, second);

    QCOMPARE(cacheKey(first), cacheKey(second));
}

// Measured against the first, script based implementation: same host, same
// digest. The key is what let the C++ version pick up the cache that script had
// left behind instead of starting from an empty directory.
void TestFaviconStore::testKeyMatchesTheScriptImplementation()
{
    QCOMPARE(cacheKey("https://serverhealthcheck.borger.co.at"),
             QStringLiteral("serverhealthcheck.borger.co.at-b33695b3"));
}

// Two URLs on one host must not share a name, and the name has to say which host
// it belongs to.
void TestFaviconStore::testEntryPaths()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const CacheEntry entry = entryFor("https://a.example/one", directory.path());
    QVERIFY(entry.key.startsWith(QStringLiteral("a.example-")));
    QCOMPARE(entry.colourPath, QDir(directory.path()).filePath(entry.key + ".png"));
    QCOMPARE(entry.grayPath, QDir(directory.path()).filePath(entry.key + ".gray.png"));

    const CacheEntry other = entryFor("https://a.example/two", directory.path());
    QVERIFY(other.key != entry.key);

    // entryFor has to go through the same key function, or the two spellings of
    // one URL would end up on two different files.
    QCOMPARE(cacheKey("https://a.example/one"), entry.key);
}

void TestFaviconStore::testWriteIsAtomic()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const QString path = QDir(directory.path()).filePath(QStringLiteral("icon.png"));
    const QByteArray bytes("not really a png, but bytes are bytes");

    QString error;
    const QString hash = writeAtomic(path, bytes, &error);

    QVERIFY2(error.isEmpty(), qPrintable(error));
    QCOMPARE(hash,
             QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha1).toHex()));
    QCOMPARE(sha1OfFile(path), hash);

    QFile file(path);
    QVERIFY(file.open(QIODevice::ReadOnly));
    QCOMPARE(file.readAll(), bytes);

    // Overwriting replaces the content rather than appending. QSaveFile does that
    // by swapping in a new file, so the handle above would still see the old
    // bytes -- the file has to be opened again.
    const QByteArray second("second");
    QVERIFY(!writeAtomic(path, second, &error).isEmpty());

    QFile reopened(path);
    QVERIFY(reopened.open(QIODevice::ReadOnly));
    QCOMPARE(reopened.readAll(), second);
}

// Creating the directory is the store's job. Leaving it to the caller is what
// made the first real run report "No such file or directory" after a successful
// fetch, because --cache-dir had never existed.
void TestFaviconStore::testWriteCreatesTheDirectory()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const QString path = QDir(directory.path()).filePath(QStringLiteral("nested/deeper/icon.png"));

    QString error;
    QVERIFY(!writeAtomic(path, QByteArrayLiteral("x"), &error).isEmpty());
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QVERIFY(hasFile(path));
}

// The writing half of the promise that a failed run leaves the old icon alone.
void TestFaviconStore::testWriteReportsFailure()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    // A regular file where the directory would have to go: mkpath cannot win
    // against that, so the write has to say so instead of pretending.
    const QString blocker = QDir(directory.path()).filePath(QStringLiteral("blocker"));
    QFile file(blocker);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.close();

    const QString path = QDir(blocker).filePath(QStringLiteral("icon.png"));

    QString error;
    const QString hash = writeAtomic(path, QByteArray("x"), &error);

    QVERIFY(hash.isEmpty());
    QVERIFY(!error.isEmpty());
    QVERIFY(!hasFile(path));
}

void TestFaviconStore::testHasFileRejectsEmptyFile()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const QString path = QDir(directory.path()).filePath(QStringLiteral("empty.png"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.close();

    // An empty file is what a truncated write would leave in a naive
    // implementation, so it must not count as a cached icon.
    QVERIFY(QFile::exists(path));
    QVERIFY(!hasFile(path));
}

void TestFaviconStore::testCachedFilesReportsWhatIsThere()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const CacheEntry entry = entryFor("https://a.example", directory.path());
    const QByteArray colour("colour");
    const QByteArray gray("gray");

    QVERIFY(!writeAtomic(entry.colourPath, colour, nullptr).isEmpty());
    QVERIFY(!writeAtomic(entry.grayPath, gray, nullptr).isEmpty());

    const CachedFiles files = cachedFiles(entry);
    QCOMPARE(files.colour, entry.colourPath);
    QCOMPARE(files.gray, entry.grayPath);
    QCOMPARE(files.hash,
             QString::fromLatin1(QCryptographicHash::hash(colour, QCryptographicHash::Sha1).toHex()));
}

void TestFaviconStore::testCachedFilesIsEmptyWithoutFiles()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const CacheEntry entry = entryFor("https://a.example", directory.path());
    const CachedFiles files = cachedFiles(entry);

    QVERIFY(files.colour.isEmpty());
    QVERIFY(files.gray.isEmpty());
    QVERIFY(files.hash.isEmpty());
}

QTEST_GUILESS_MAIN(TestFaviconStore)

#include "tst_faviconstore.moc"
