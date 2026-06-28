#ifndef FILEASSOCIATIONSERVICE_P_H
#define FILEASSOCIATIONSERVICE_P_H

#include "imageformats.h"

#include <QString>
#include <QStringList>

namespace FileAssociationService {

QString normalizedExtension(QString extension);
QStringList normalizedExtensionList(const QStringList &extensions);
ImageFormatDescriptor descriptorForExtension(const QString &extension);
QString appDisplayName();

bool platformIsAssociationSupported();
bool platformIsAssociated(const ImageFormatDescriptor &descriptor);
bool platformPrepareAssociations(const QStringList &normalizedSelectedExtensions, QString *errorMessage);
bool platformAssociateExtension(const ImageFormatDescriptor &descriptor);
bool platformDisassociateExtension(const ImageFormatDescriptor &descriptor);

} // namespace FileAssociationService

#endif // FILEASSOCIATIONSERVICE_P_H
