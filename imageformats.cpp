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
    };
    return formats;
}

const QStringList &requiredRuntimeFormats()
{
    static const QStringList formats = {
        QStringLiteral("gif"),
        QStringLiteral("webp"),
        QStringLiteral("svg"),
        QStringLiteral("heic"),
        QStringLiteral("heif"),
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

QStringList imageNameFilters()
{
    return {
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
    return QStringLiteral("Images (*.jpg *.jpeg *.png *.bmp *.gif *.webp *.svg *.heic *.heif)");
}

} // namespace ImageFormats
