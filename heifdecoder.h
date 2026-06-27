#ifndef HEIFDECODER_H
#define HEIFDECODER_H

#include <QImage>
#include <QSize>
#include <QString>

namespace HeifDecoder {

struct DecodeResult
{
    QImage image;
    QString errorMessage;
    QString decoderInfo;
    QSize sourceSize;
    bool hasAlpha = false;
};

bool isAvailable();
DecodeResult decode(const QString &filePath);
QString unavailableMessage();

} // namespace HeifDecoder

#endif // HEIFDECODER_H
