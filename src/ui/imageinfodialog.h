#ifndef IMAGEINFODIALOG_H
#define IMAGEINFODIALOG_H

#include <QSize>
#include <QString>

class QWidget;

namespace ImageInfoDialog {

struct Details
{
    QString filePath;
    QSize imageSize;
    QString zoomText;
    bool isRaw = false;
    bool isHeif = false;

    QString rawDisplaySource;
    QString rawDecoderInfo;
    QString rawCameraInfo;
    QSize rawSourceSize;
    QSize rawEmbeddedPreviewSize;

    QString heifDecoderInfo;
    QSize heifSourceSize;
    bool heifHasAlpha = false;
};

void show(QWidget *parent, const Details &details);

} // namespace ImageInfoDialog

#endif // IMAGEINFODIALOG_H
