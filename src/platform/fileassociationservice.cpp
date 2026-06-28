#include "fileassociationservice.h"

#include "fileassociationservice_p.h"

#include <QObject>

namespace FileAssociationService {

QString normalizedExtension(QString extension)
{
    extension = extension.trimmed().toLower();
    if (extension.startsWith(QLatin1Char('.'))) {
        extension.remove(0, 1);
    }
    return extension;
}

QStringList normalizedExtensionList(const QStringList &extensions)
{
    QStringList normalizedExtensions;
    for (const QString &extension : extensions) {
        const QString normalized = normalizedExtension(extension);
        if (!normalized.isEmpty() && !normalizedExtensions.contains(normalized)) {
            normalizedExtensions << normalized;
        }
    }
    return normalizedExtensions;
}

ImageFormatDescriptor descriptorForExtension(const QString &extension)
{
    const QString normalized = normalizedExtension(extension);
    for (const ImageFormatDescriptor &descriptor : ImageFormats::supportedFormatDescriptors()) {
        if (descriptor.extension == normalized) {
            return descriptor;
        }
    }
    return {};
}

QString appDisplayName()
{
    return QStringLiteral("NgImageViewer");
}

bool isAssociationSupported()
{
    return platformIsAssociationSupported();
}

bool isAssociated(const QString &extension)
{
    const ImageFormatDescriptor descriptor = descriptorForExtension(extension);
    return !descriptor.extension.isEmpty() && platformIsAssociated(descriptor);
}

AssociationResult applyAssociations(const QStringList &selectedExtensions)
{
    AssociationResult result;
    if (!isAssociationSupported()) {
        result.message = QObject::tr("当前系统不支持自动关联文件格式");
        return result;
    }

    const QStringList normalizedSelectedExtensions = normalizedExtensionList(selectedExtensions);
    QString prepareError;
    if (!platformPrepareAssociations(normalizedSelectedExtensions, &prepareError)) {
        result.message = prepareError.isEmpty()
                             ? QObject::tr("无法写入本机应用关联配置")
                             : prepareError;
        return result;
    }

    for (const ImageFormatDescriptor &descriptor : ImageFormats::supportedFormatDescriptors()) {
        const bool shouldAssociate = normalizedSelectedExtensions.contains(descriptor.extension);
        const bool ok = shouldAssociate
                            ? platformAssociateExtension(descriptor)
                            : platformDisassociateExtension(descriptor);
        if (!ok) {
            result.failedExtensions << descriptor.extension;
        }
    }

    result.success = result.failedExtensions.isEmpty();
    result.message = result.success
                         ? QObject::tr("已更新图片格式关联")
                         : QObject::tr("部分格式更新失败：%1").arg(result.failedExtensions.join(QStringLiteral(", ")));
    return result;
}

} // namespace FileAssociationService
