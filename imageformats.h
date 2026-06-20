#ifndef IMAGEFORMATS_H
#define IMAGEFORMATS_H

#include <QString>
#include <QStringList>

namespace ImageFormats {

bool isSupportedFile(const QString &filePath);
QStringList imageNameFilters();
QStringList missingRequiredRuntimeFormats();
QString openDialogFilter();

} // namespace ImageFormats

#endif // IMAGEFORMATS_H
