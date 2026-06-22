#include "imageformats.h"

#include <QFileInfo>
#include <QImageReader>
#include <QSet>

namespace {

const QSet<QString> &requiredFormats()
{
    static const QSet<QString> formats = {
        QStringLiteral("jpg"),
        QStringLiteral("jpeg"),
        QStringLiteral("png"),
        QStringLiteral("bmp"),
        QStringLiteral("gif"),
        QStringLiteral("webp"),
        QStringLiteral("svg"),
        QStringLiteral("heic"),
        QStringLiteral("heif"),
        QStringLiteral("3fr"),
        QStringLiteral("arw"),
        QStringLiteral("bay"),
        QStringLiteral("cr2"),
        QStringLiteral("cr3"),
        QStringLiteral("crw"),
        QStringLiteral("dcr"),
        QStringLiteral("dng"),
        QStringLiteral("erf"),
        QStringLiteral("kdc"),
        QStringLiteral("mos"),
        QStringLiteral("mrw"),
        QStringLiteral("nef"),
        QStringLiteral("nrw"),
        QStringLiteral("orf"),
        QStringLiteral("pef"),
        QStringLiteral("raf"),
        QStringLiteral("raw"),
        QStringLiteral("rw2"),
        QStringLiteral("rwl"),
        QStringLiteral("sr2"),
        QStringLiteral("srf"),
        QStringLiteral("srw"),
        QStringLiteral("x3f"),
    };
    return formats;
}

const QStringList &rawNameFilters()
{
    static const QStringList filters = {
        QStringLiteral("*.3fr"),
        QStringLiteral("*.arw"),
        QStringLiteral("*.bay"),
        QStringLiteral("*.cr2"),
        QStringLiteral("*.cr3"),
        QStringLiteral("*.crw"),
        QStringLiteral("*.dcr"),
        QStringLiteral("*.dng"),
        QStringLiteral("*.erf"),
        QStringLiteral("*.kdc"),
        QStringLiteral("*.mos"),
        QStringLiteral("*.mrw"),
        QStringLiteral("*.nef"),
        QStringLiteral("*.nrw"),
        QStringLiteral("*.orf"),
        QStringLiteral("*.pef"),
        QStringLiteral("*.raf"),
        QStringLiteral("*.raw"),
        QStringLiteral("*.rw2"),
        QStringLiteral("*.rwl"),
        QStringLiteral("*.sr2"),
        QStringLiteral("*.srf"),
        QStringLiteral("*.srw"),
        QStringLiteral("*.x3f"),
    };
    return filters;
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

QStringList imageNameFilters()
{
    QStringList filters = {
        QStringLiteral("*.jpg"),
        QStringLiteral("*.jpeg"),
        QStringLiteral("*.png"),
        QStringLiteral("*.bmp"),
        QStringLiteral("*.gif"),
        QStringLiteral("*.webp"),
        QStringLiteral("*.svg"),
        QStringLiteral("*.heic"),
        QStringLiteral("*.heif"),
    };
    filters.append(rawNameFilters());
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
    QStringList filters = {
        QStringLiteral("*.jpg"),
        QStringLiteral("*.jpeg"),
        QStringLiteral("*.png"),
        QStringLiteral("*.bmp"),
        QStringLiteral("*.gif"),
        QStringLiteral("*.webp"),
        QStringLiteral("*.svg"),
        QStringLiteral("*.heic"),
        QStringLiteral("*.heif"),
    };
    filters.append(rawNameFilters());
    return QStringLiteral("Images (%1)").arg(filters.join(QLatin1Char(' ')));
}

} // namespace ImageFormats
