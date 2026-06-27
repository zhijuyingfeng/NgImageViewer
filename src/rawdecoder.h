#ifndef RAWDECODER_H
#define RAWDECODER_H

#include <QImage>
#include <QSize>
#include <QString>

namespace RawDecoder {

struct DecodeResult
{
    QImage image;
    QString errorMessage;
    QString warningMessage;
    QString displaySource;
    QString decoderInfo;
    QString cameraInfo;
    QSize rawSize;
    QSize embeddedPreviewSize;
    bool usedEmbeddedPreview = false;
    bool fullDecodeAttempted = false;
};

bool isAvailable();
DecodeResult decode(const QString &filePath);
QString unavailableMessage();

} // namespace RawDecoder

#endif // RAWDECODER_H
