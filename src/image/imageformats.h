#ifndef IMAGEFORMATS_H
#define IMAGEFORMATS_H

#include <QString>
#include <QStringList>
#include <QList>

struct ImageFormatDescriptor
{
    QString extension;
    QString displayName;
    QStringList mimeTypes;
};

namespace ImageFormats {

bool isSupportedFile(const QString &filePath);
bool isRawFile(const QString &filePath);
bool isHeifFile(const QString &filePath);
QList<ImageFormatDescriptor> supportedFormatDescriptors();
QStringList supportedExtensions();
QStringList imageNameFilters();
QStringList missingRequiredRuntimeFormats();
QString openDialogFilter();

} // namespace ImageFormats

#endif // IMAGEFORMATS_H
