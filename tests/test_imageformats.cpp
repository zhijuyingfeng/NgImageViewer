#include "imageformats.h"

#include <QSet>
#include <QTest>

class ImageFormatsTest : public QObject
{
    Q_OBJECT

private slots:
    void supportedFilesAreMatchedCaseInsensitively();
    void rawAndHeifClassification();
    void filtersContainEverySupportedExtension();
    void descriptorsHaveUniqueExtensionsAndMimeTypes();
};

void ImageFormatsTest::supportedFilesAreMatchedCaseInsensitively()
{
    QVERIFY(ImageFormats::isSupportedFile(QStringLiteral("/tmp/photo.JPG")));
    QVERIFY(ImageFormats::isSupportedFile(QStringLiteral("/tmp/vector.SVG")));
    QVERIFY(ImageFormats::isSupportedFile(QStringLiteral("/tmp/raw.RW2")));
    QVERIFY(!ImageFormats::isSupportedFile(QStringLiteral("/tmp/readme.txt")));
    QVERIFY(!ImageFormats::isSupportedFile(QStringLiteral("/tmp/no-extension")));
}

void ImageFormatsTest::rawAndHeifClassification()
{
    QVERIFY(ImageFormats::isRawFile(QStringLiteral("/tmp/camera.cr3")));
    QVERIFY(ImageFormats::isRawFile(QStringLiteral("/tmp/camera.RW2")));
    QVERIFY(!ImageFormats::isRawFile(QStringLiteral("/tmp/photo.jpg")));
    QVERIFY(!ImageFormats::isRawFile(QStringLiteral("/tmp/photo.heic")));

    QVERIFY(ImageFormats::isHeifFile(QStringLiteral("/tmp/photo.heic")));
    QVERIFY(ImageFormats::isHeifFile(QStringLiteral("/tmp/photo.HEIF")));
    QVERIFY(!ImageFormats::isHeifFile(QStringLiteral("/tmp/photo.jpg")));
}

void ImageFormatsTest::filtersContainEverySupportedExtension()
{
    const QStringList extensions = ImageFormats::supportedExtensions();
    const QStringList nameFilters = ImageFormats::imageNameFilters();
    const QString openDialogFilter = ImageFormats::openDialogFilter();

    for (const QString &extension : extensions) {
        const QString wildcard = QStringLiteral("*.%1").arg(extension);
        QVERIFY2(nameFilters.contains(wildcard), qPrintable(QStringLiteral("Missing name filter %1").arg(wildcard)));
        QVERIFY2(openDialogFilter.contains(wildcard),
                 qPrintable(QStringLiteral("Missing open dialog filter %1").arg(wildcard)));
    }
}

void ImageFormatsTest::descriptorsHaveUniqueExtensionsAndMimeTypes()
{
    QSet<QString> extensions;
    const QList<ImageFormatDescriptor> descriptors = ImageFormats::supportedFormatDescriptors();
    QVERIFY(!descriptors.isEmpty());

    for (const ImageFormatDescriptor &descriptor : descriptors) {
        QVERIFY2(!descriptor.extension.isEmpty(), "Extension must not be empty");
        QVERIFY2(!descriptor.displayName.isEmpty(), qPrintable(descriptor.extension));
        QVERIFY2(!descriptor.mimeTypes.isEmpty(), qPrintable(descriptor.extension));
        QVERIFY2(!extensions.contains(descriptor.extension),
                 qPrintable(QStringLiteral("Duplicate extension %1").arg(descriptor.extension)));
        extensions.insert(descriptor.extension);
    }

    QCOMPARE(extensions.size(), ImageFormats::supportedExtensions().size());
}

QTEST_GUILESS_MAIN(ImageFormatsTest)
#include "test_imageformats.moc"
