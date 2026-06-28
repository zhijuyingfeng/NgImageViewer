#ifndef IMAGELOADER_H
#define IMAGELOADER_H

#include <QImage>
#include <QMovie>
#include <QSize>
#include <QString>
#include <QSvgRenderer>

#include <memory>

class ImageLoader
{
public:
    enum class Kind {
        Invalid,
        StaticImage,
        RawImage,
        HeifImage,
        SvgImage,
        GifImage
    };

    struct RawMetadata
    {
        QString displaySource;
        QString decoderInfo;
        QString cameraInfo;
        QSize sourceSize;
        QSize embeddedPreviewSize;
    };

    struct HeifMetadata
    {
        QString decoderInfo;
        QSize sourceSize;
        bool hasAlpha = false;
    };

    struct LoadResult
    {
        bool success = false;
        Kind kind = Kind::Invalid;
        QString errorMessage;
        QString warningMessage;
        QImage image;
        std::unique_ptr<QSvgRenderer> svgRenderer;
        QSize svgDefaultSize;
        std::unique_ptr<QMovie> movie;
        RawMetadata raw;
        HeifMetadata heif;
    };

    static LoadResult load(const QString &filePath);
};

#endif // IMAGELOADER_H
