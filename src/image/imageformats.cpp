#include "imageformats.h"

#include <QFileInfo>
#include <QImageReader>
#include <QSet>

namespace {

const QList<ImageFormatDescriptor> &formatDescriptors()
{
    static const QList<ImageFormatDescriptor> formats = {
        {QStringLiteral("jpg"), QStringLiteral("JPEG"), {QStringLiteral("image/jpeg")}},
        {QStringLiteral("jpeg"), QStringLiteral("JPEG"), {QStringLiteral("image/jpeg")}},
        {QStringLiteral("png"), QStringLiteral("PNG"), {QStringLiteral("image/png")}},
        {QStringLiteral("bmp"), QStringLiteral("BMP"), {QStringLiteral("image/bmp"), QStringLiteral("image/x-ms-bmp")}},
        {QStringLiteral("gif"), QStringLiteral("GIF"), {QStringLiteral("image/gif")}},
        {QStringLiteral("webp"), QStringLiteral("WEBP"), {QStringLiteral("image/webp")}},
        {QStringLiteral("svg"), QStringLiteral("SVG"), {QStringLiteral("image/svg+xml")}},
        {QStringLiteral("heic"), QStringLiteral("HEIC"), {QStringLiteral("image/heic"), QStringLiteral("image/heif")}},
        {QStringLiteral("heif"), QStringLiteral("HEIF"), {QStringLiteral("image/heif"), QStringLiteral("image/heic")}},
        {QStringLiteral("3fr"), QStringLiteral("Hasselblad RAW"), {QStringLiteral("image/x-hasselblad-3fr")}},
        {QStringLiteral("arw"), QStringLiteral("Sony RAW"), {QStringLiteral("image/x-sony-arw")}},
        {QStringLiteral("bay"), QStringLiteral("Casio RAW"), {QStringLiteral("image/x-casio-bay")}},
        {QStringLiteral("cr2"), QStringLiteral("Canon RAW"), {QStringLiteral("image/x-canon-cr2")}},
        {QStringLiteral("cr3"), QStringLiteral("Canon RAW"), {QStringLiteral("image/x-canon-cr3")}},
        {QStringLiteral("crw"), QStringLiteral("Canon RAW"), {QStringLiteral("image/x-canon-crw")}},
        {QStringLiteral("dcr"), QStringLiteral("Kodak RAW"), {QStringLiteral("image/x-kodak-dcr")}},
        {QStringLiteral("dng"), QStringLiteral("Adobe DNG"), {QStringLiteral("image/x-adobe-dng")}},
        {QStringLiteral("erf"), QStringLiteral("Epson RAW"), {QStringLiteral("image/x-epson-erf")}},
        {QStringLiteral("kdc"), QStringLiteral("Kodak RAW"), {QStringLiteral("image/x-kodak-kdc")}},
        {QStringLiteral("mos"), QStringLiteral("Leaf RAW"), {QStringLiteral("image/x-leaf-mos")}},
        {QStringLiteral("mrw"), QStringLiteral("Minolta RAW"), {QStringLiteral("image/x-minolta-mrw")}},
        {QStringLiteral("nef"), QStringLiteral("Nikon RAW"), {QStringLiteral("image/x-nikon-nef")}},
        {QStringLiteral("nrw"), QStringLiteral("Nikon RAW"), {QStringLiteral("image/x-nikon-nrw")}},
        {QStringLiteral("orf"), QStringLiteral("Olympus RAW"), {QStringLiteral("image/x-olympus-orf")}},
        {QStringLiteral("pef"), QStringLiteral("Pentax RAW"), {QStringLiteral("image/x-pentax-pef")}},
        {QStringLiteral("raf"), QStringLiteral("Fujifilm RAW"), {QStringLiteral("image/x-fuji-raf")}},
        {QStringLiteral("raw"), QStringLiteral("RAW"), {QStringLiteral("image/x-raw")}},
        {QStringLiteral("rw2"), QStringLiteral("Panasonic RAW"), {QStringLiteral("image/x-panasonic-rw2")}},
        {QStringLiteral("rwl"), QStringLiteral("Leica RAW"), {QStringLiteral("image/x-leica-rwl")}},
        {QStringLiteral("sr2"), QStringLiteral("Sony RAW"), {QStringLiteral("image/x-sony-sr2")}},
        {QStringLiteral("srf"), QStringLiteral("Sony RAW"), {QStringLiteral("image/x-sony-srf")}},
        {QStringLiteral("srw"), QStringLiteral("Samsung RAW"), {QStringLiteral("image/x-samsung-srw")}},
        {QStringLiteral("x3f"), QStringLiteral("Sigma RAW"), {QStringLiteral("image/x-sigma-x3f")}},
    };
    return formats;
}

const QSet<QString> &requiredFormats()
{
    static const QSet<QString> formats = [] {
        QSet<QString> values;
        for (const ImageFormatDescriptor &descriptor : formatDescriptors()) {
            values.insert(descriptor.extension);
        }
        return values;
    }();
    return formats;
}

const QStringList &requiredRuntimeFormats()
{
    static const QStringList formats = {
        QStringLiteral("gif"),
        QStringLiteral("webp"),
        QStringLiteral("svg"),
    };
    return formats;
}

} // namespace

namespace ImageFormats {

bool isSupportedFile(const QString &filePath)
{
    const QString suffix = QFileInfo(filePath).suffix().toLower();
    return requiredFormats().contains(suffix);
}

bool isRawFile(const QString &filePath)
{
    const QString suffix = QFileInfo(filePath).suffix().toLower();
    return requiredFormats().contains(suffix)
           && !QSet<QString>({
                  QStringLiteral("jpg"),
                  QStringLiteral("jpeg"),
                  QStringLiteral("png"),
                  QStringLiteral("bmp"),
                  QStringLiteral("gif"),
                  QStringLiteral("webp"),
                  QStringLiteral("svg"),
                  QStringLiteral("heic"),
                  QStringLiteral("heif"),
              }).contains(suffix);
}

bool isHeifFile(const QString &filePath)
{
    const QString suffix = QFileInfo(filePath).suffix().toLower();
    return suffix == QStringLiteral("heic") || suffix == QStringLiteral("heif");
}

QList<ImageFormatDescriptor> supportedFormatDescriptors()
{
    return formatDescriptors();
}

QStringList supportedExtensions()
{
    QStringList extensions;
    for (const ImageFormatDescriptor &descriptor : formatDescriptors()) {
        extensions << descriptor.extension;
    }
    return extensions;
}

QStringList imageNameFilters()
{
    QStringList filters;
    for (const QString &extension : supportedExtensions()) {
        filters << QStringLiteral("*.%1").arg(extension);
    }
    return filters;
}

QStringList missingRequiredRuntimeFormats()
{
    QSet<QString> available;
    const QList<QByteArray> supportedFormats = QImageReader::supportedImageFormats();
    for (const QByteArray &format : supportedFormats) {
        available.insert(QString::fromLatin1(format).toLower());
    }

    QStringList missing;
    for (const QString &format : requiredRuntimeFormats()) {
        if (!available.contains(format)) {
            missing << format;
        }
    }
    return missing;
}

QString openDialogFilter()
{
    QStringList filters;
    for (const QString &extension : supportedExtensions()) {
        filters << QStringLiteral("*.%1").arg(extension);
    }
    return QStringLiteral("Images (%1)").arg(filters.join(QLatin1Char(' ')));
}

} // namespace ImageFormats
