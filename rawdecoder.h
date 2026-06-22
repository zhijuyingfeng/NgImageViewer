#ifndef RAWDECODER_H
#define RAWDECODER_H

#include <QImage>
#include <QString>

namespace RawDecoder {

struct DecodeResult
{
    QImage image;
    QString errorMessage;
};

bool isAvailable();
DecodeResult decode(const QString &filePath);
QString unavailableMessage();

} // namespace RawDecoder

#endif // RAWDECODER_H
