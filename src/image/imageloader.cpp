#include "imageloader.h"

#include "heifdecoder.h"
#include "imageformats.h"
#include "rawdecoder.h"

#include <QFileInfo>
#include <QImageReader>
#include <QMovie>
#include <QObject>
#include <QSvgRenderer>

namespace {

ImageLoader::LoadResult failure(const QString &message)
{
    ImageLoader::LoadResult result;
    result.errorMessage = message;
    return result;
}

QSize svgDefaultSize(QSvgRenderer *renderer)
{
    QSize defaultSize = renderer->defaultSize();
    if (defaultSize.isEmpty()) {
        const QRectF viewBox = renderer->viewBoxF();
        if (viewBox.isValid() && !viewBox.isEmpty()) {
            defaultSize = viewBox.size().toSize();
        }
    }
    if (defaultSize.isEmpty()) {
        defaultSize = QSize(1024, 1024);
    }
    return defaultSize;
}

} // namespace

ImageLoader::LoadResult ImageLoader::load(const QString &filePath)
{
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

        LoadResult result;
        result.success = true;
        result.kind = Kind::RawImage;
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

        LoadResult result;
        result.success = true;
        result.kind = Kind::HeifImage;
        result.image = decoded.image;
        result.heif.decoderInfo = decoded.decoderInfo;
        result.heif.sourceSize = decoded.sourceSize;
        result.heif.hasAlpha = decoded.hasAlpha;
        return result;
    }

    if (info.suffix().compare(QStringLiteral("gif"), Qt::CaseInsensitive) == 0) {
        auto movie = std::make_unique<QMovie>(filePath);
        movie->setCacheMode(QMovie::CacheAll);
        if (!movie->isValid()) {
            return failure(QObject::tr("图片打开失败，请检查文件是否损坏"));
        }

        LoadResult result;
        result.success = true;
        result.kind = Kind::GifImage;
        result.movie = std::move(movie);
        return result;
    }

    if (info.suffix().compare(QStringLiteral("svg"), Qt::CaseInsensitive) == 0) {
        auto renderer = std::make_unique<QSvgRenderer>(filePath);
        if (!renderer->isValid()) {
            return failure(QObject::tr("图片打开失败，请检查文件是否损坏"));
        }

        LoadResult result;
        result.success = true;
        result.kind = Kind::SvgImage;
        result.svgDefaultSize = svgDefaultSize(renderer.get());
        result.svgRenderer = std::move(renderer);
        return result;
    }

    QImageReader reader(filePath);
    reader.setAutoTransform(true);
    const QImage image = reader.read();
    if (image.isNull()) {
        return failure(QObject::tr("图片打开失败，请检查文件是否损坏"));
    }

    LoadResult result;
    result.success = true;
    result.kind = Kind::StaticImage;
    result.image = image;
    return result;
}
