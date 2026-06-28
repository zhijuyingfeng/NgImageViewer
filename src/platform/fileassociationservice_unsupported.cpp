#include "fileassociationservice_p.h"

#if !defined(Q_OS_WIN) && !defined(Q_OS_MAC) && !defined(Q_OS_LINUX)

#include <QtGlobal>

namespace FileAssociationService {

bool platformIsAssociationSupported()
{
    return false;
}

bool platformIsAssociated(const ImageFormatDescriptor &descriptor)
{
    Q_UNUSED(descriptor);
    return false;
}

bool platformPrepareAssociations(const QStringList &normalizedSelectedExtensions, QString *errorMessage)
{
    Q_UNUSED(normalizedSelectedExtensions);
    Q_UNUSED(errorMessage);
    return false;
}

bool platformAssociateExtension(const ImageFormatDescriptor &descriptor)
{
    Q_UNUSED(descriptor);
    return false;
}

bool platformDisassociateExtension(const ImageFormatDescriptor &descriptor)
{
    Q_UNUSED(descriptor);
    return false;
}

} // namespace FileAssociationService

#endif
