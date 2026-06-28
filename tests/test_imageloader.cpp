#include "imageloader.h"

#include <QDir>
#include <QFile>
#include <QFuture>
#include <QImage>
#include <QStandardPaths>
#include <QTest>
#include <QTextStream>
#include <QtConcurrent/QtConcurrentRun>

class ImageLoaderTest : public QObject
{
    Q_OBJECT

private slots:
    void rejectsMissingFile();
    void rejectsUnsupportedFile();
    void loadsStaticImage();
    void loadsSvgImage();
    void loadsRawImageFromWorkerWhenConfigured();

private:
    QString tempPath(const QString &fileName) const;
};

QString ImageLoaderTest::tempPath(const QString &fileName) const
{
    const QString root = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    QDir dir(QDir(root).filePath(QStringLiteral("ngimageviewer-tests")));
    if (!dir.exists()) {
        dir.mkpath(QStringLiteral("."));
    }
    return dir.filePath(fileName);
}

void ImageLoaderTest::rejectsMissingFile()
{
    const ImageLoader::LoadResult result = ImageLoader::load(tempPath(QStringLiteral("missing.png")));
    QVERIFY(!result.success);
    QCOMPARE(result.kind, ImageLoader::Kind::Invalid);
    QVERIFY(!result.errorMessage.isEmpty());
}

void ImageLoaderTest::rejectsUnsupportedFile()
{
    const QString path = tempPath(QStringLiteral("unsupported.txt"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate));
    file.write("not an image");
    file.close();

    const ImageLoader::LoadResult result = ImageLoader::load(path);
    QVERIFY(!result.success);
    QCOMPARE(result.kind, ImageLoader::Kind::Invalid);
    QVERIFY(!result.errorMessage.isEmpty());
}

void ImageLoaderTest::loadsStaticImage()
{
    const QString path = tempPath(QStringLiteral("static.png"));
    QImage image(QSize(12, 8), QImage::Format_ARGB32_Premultiplied);
    image.fill(QColor("#38bdf8"));
    QVERIFY(image.save(path));

    const ImageLoader::LoadResult result = ImageLoader::load(path);
    QVERIFY(result.success);
    QCOMPARE(result.kind, ImageLoader::Kind::StaticImage);
    QCOMPARE(result.image.size(), QSize(12, 8));
}

void ImageLoaderTest::loadsSvgImage()
{
    const QString path = tempPath(QStringLiteral("vector.svg"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate));
    QTextStream out(&file);
    out << "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"24\" height=\"16\" viewBox=\"0 0 24 16\">";
    out << "<rect width=\"24\" height=\"16\" fill=\"#38bdf8\"/>";
    out << "</svg>";
    file.close();

    ImageLoader::LoadResult result = ImageLoader::load(path);
    QVERIFY(result.success);
    QCOMPARE(result.kind, ImageLoader::Kind::SvgImage);
    QCOMPARE(result.svgDefaultSize, QSize(24, 16));
}

void ImageLoaderTest::loadsRawImageFromWorkerWhenConfigured()
{
    const QString path = QString::fromUtf8(qgetenv("NGIMAGEVIEWER_TEST_RAW_FILE"));
    if (path.isEmpty()) {
        QSKIP("Set NGIMAGEVIEWER_TEST_RAW_FILE to run the optional RAW loader worker test.");
    }

    QFuture<ImageLoader::LoadResult> future = QtConcurrent::run([path] {
        return ImageLoader::load(path);
    });
    future.waitForFinished();

    const ImageLoader::LoadResult result = future.result();
    QVERIFY2(result.success, qPrintable(result.errorMessage));
    QVERIFY(!result.image.isNull());
}

QTEST_MAIN(ImageLoaderTest)

#include "test_imageloader.moc"
