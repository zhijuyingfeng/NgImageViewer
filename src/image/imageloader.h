#ifndef IMAGELOADER_H
#define IMAGELOADER_H

#include <QImage>
#include <QSize>
#include <QString>

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
        QString filePath;
        QString errorMessage;
        QString warningMessage;
        QImage image;
        QSize svgDefaultSize;
        RawMetadata raw;
        HeifMetadata heif;
    };

    static LoadResult load(const QString &filePath);
};

#endif // IMAGELOADER_H
