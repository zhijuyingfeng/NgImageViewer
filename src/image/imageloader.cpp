#include "imageloader.h"

#include "heifdecoder.h"
#include "imageformats.h"
#include "rawdecoder.h"

#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QObject>
#include <QXmlStreamReader>

namespace {

ImageLoader::LoadResult failure(const QString &message)
{
    ImageLoader::LoadResult result;
    result.errorMessage = message;
    return result;
}

double parseSvgNumber(QString value)
{
    value = value.trimmed();
    int length = 0;
    while (length < value.size()) {
        const QChar ch = value.at(length);
        if (!(ch.isDigit() || ch == QLatin1Char('.') || ch == QLatin1Char('-') || ch == QLatin1Char('+'))) {
            break;
        }
        ++length;
    }
    if (length == 0) {
        return 0.0;
    }
    bool ok = false;
    const double number = value.left(length).toDouble(&ok);
    return ok ? number : 0.0;
}

QSize parseSvgViewBox(const QString &value)
{
    QString normalized = value;
    normalized.replace(QLatin1Char(','), QLatin1Char(' '));
    const QStringList parts = normalized.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    if (parts.size() != 4) {
        return {};
    }

    const double width = parseSvgNumber(parts.at(2));
    const double height = parseSvgNumber(parts.at(3));
    if (width <= 0.0 || height <= 0.0) {
        return {};
    }
    return QSize(qRound(width), qRound(height));
}

QSize svgDefaultSize(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return QSize(1024, 1024);
    }

    QXmlStreamReader reader(&file);
    while (!reader.atEnd()) {
        reader.readNext();
        if (!reader.isStartElement() || reader.name() != QLatin1String("svg")) {
            continue;
        }

        const QXmlStreamAttributes attributes = reader.attributes();
        const double width = parseSvgNumber(attributes.value(QStringLiteral("width")).toString());
        const double height = parseSvgNumber(attributes.value(QStringLiteral("height")).toString());
        if (width > 0.0 && height > 0.0) {
            return QSize(qRound(width), qRound(height));
        }

        const QSize viewBoxSize = parseSvgViewBox(attributes.value(QStringLiteral("viewBox")).toString());
        if (!viewBoxSize.isEmpty()) {
            return viewBoxSize;
        }
        break;
    }

    return QSize(1024, 1024);
}

} // namespace

ImageLoader::LoadResult ImageLoader::load(const QString &filePath)
{
    LoadResult result;
    result.filePath = filePath;

    const QFileInfo info(filePath);
    if (!info.exists()) {
        return failure(QObject::tr("文件不存在或已被移动"));
    }
    if (!info.isReadable()) {
        return failure(QObject::tr("无法访问该图片，请检查文件权限后重试"));
    }
    if (!ImageFormats::isSupportedFile(filePath)) {
        return failure(QObject::tr("不支持的格式"));
    }

    if (ImageFormats::isRawFile(filePath)) {
        if (!RawDecoder::isAvailable()) {
            return failure(RawDecoder::unavailableMessage());
        }

        const RawDecoder::DecodeResult decoded = RawDecoder::decode(filePath);
        if (decoded.image.isNull()) {
            return failure(decoded.errorMessage.isEmpty()
                               ? QObject::tr("RAW 图片打开失败，请检查文件是否损坏")
                               : decoded.errorMessage);
        }

        result = LoadResult();
        result.success = true;
        result.kind = Kind::RawImage;
        result.filePath = filePath;
        result.warningMessage = decoded.warningMessage;
        result.image = decoded.image;
        result.raw.displaySource = decoded.displaySource;
        result.raw.decoderInfo = decoded.decoderInfo;
        result.raw.cameraInfo = decoded.cameraInfo;
        result.raw.sourceSize = decoded.rawSize;
        result.raw.embeddedPreviewSize = decoded.embeddedPreviewSize;
        return result;
    }

    if (ImageFormats::isHeifFile(filePath)) {
        if (!HeifDecoder::isAvailable()) {
            return failure(HeifDecoder::unavailableMessage());
        }

        const HeifDecoder::DecodeResult decoded = HeifDecoder::decode(filePath);
        if (decoded.image.isNull()) {
            return failure(decoded.errorMessage.isEmpty()
                               ? QObject::tr("HEIF/HEIC 图片打开失败，请检查文件是否损坏")
                               : decoded.errorMessage);
        }

        result = LoadResult();
        result.success = true;
        result.kind = Kind::HeifImage;
        result.filePath = filePath;
        result.image = decoded.image;
        result.heif.decoderInfo = decoded.decoderInfo;
        result.heif.sourceSize = decoded.sourceSize;
        result.heif.hasAlpha = decoded.hasAlpha;
        return result;
    }

    if (info.suffix().compare(QStringLiteral("gif"), Qt::CaseInsensitive) == 0) {
        result = LoadResult();
        result.success = true;
        result.kind = Kind::GifImage;
        result.filePath = filePath;
        return result;
    }

    if (info.suffix().compare(QStringLiteral("svg"), Qt::CaseInsensitive) == 0) {
        result = LoadResult();
        result.success = true;
        result.kind = Kind::SvgImage;
        result.filePath = filePath;
        result.svgDefaultSize = svgDefaultSize(filePath);
        return result;
    }

    QImageReader reader(filePath);
    reader.setAutoTransform(true);
    const QImage image = reader.read();
    if (image.isNull()) {
        return failure(QObject::tr("图片打开失败，请检查文件是否损坏"));
    }

    result = LoadResult();
    result.success = true;
    result.kind = Kind::StaticImage;
    result.filePath = filePath;
    result.image = image;
    return result;
}
