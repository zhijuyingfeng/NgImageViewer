#ifndef FILEASSOCIATIONSERVICE_H
#define FILEASSOCIATIONSERVICE_H

#include <QString>
#include <QStringList>

namespace FileAssociationService {

struct AssociationResult
{
    bool success = false;
    QString message;
    QStringList failedExtensions;
};

bool isAssociationSupported();
bool isAssociated(const QString &extension);
AssociationResult applyAssociations(const QStringList &selectedExtensions);

} // namespace FileAssociationService

#endif // FILEASSOCIATIONSERVICE_H
