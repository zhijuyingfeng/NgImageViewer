#include "fileassociationservice_p.h"

#ifdef Q_OS_MAC

#include <CoreServices/CoreServices.h>

namespace FileAssociationService {
namespace {

constexpr LSRolesMask kMacAssociationRole = kLSRolesAll;

CFStringRef createString(const QString &value)
{
    const QByteArray utf8 = value.toUtf8();
    return CFStringCreateWithBytes(kCFAllocatorDefault,
                                   reinterpret_cast<const UInt8 *>(utf8.constData()),
                                   static_cast<CFIndex>(utf8.size()),
                                   kCFStringEncodingUTF8,
                                   false);
}

QString bundleIdentifier()
{
    CFBundleRef bundle = CFBundleGetMainBundle();
    if (!bundle) {
        return {};
    }
    CFStringRef identifier = CFBundleGetIdentifier(bundle);
    if (!identifier) {
        return {};
    }
    char buffer[256] = {};
    if (!CFStringGetCString(identifier, buffer, sizeof(buffer), kCFStringEncodingUTF8)) {
        return {};
    }
    return QString::fromUtf8(buffer);
}

CFURLRef copyCurrentAppBundleUrl()
{
    CFBundleRef bundle = CFBundleGetMainBundle();
    if (!bundle) {
        return nullptr;
    }
    return CFBundleCopyBundleURL(bundle);
}

bool registerCurrentAppBundle()
{
    CFURLRef bundleUrl = copyCurrentAppBundleUrl();
    if (!bundleUrl) {
        return false;
    }
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#endif
    const OSStatus status = LSRegisterURL(bundleUrl, true);
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
    CFRelease(bundleUrl);
    return status == noErr;
}

CFStringRef createUtiForExtension(const QString &extension)
{
    CFStringRef cfExtension = createString(normalizedExtension(extension));
    if (!cfExtension) {
        return nullptr;
    }
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#endif
    CFStringRef uti = UTTypeCreatePreferredIdentifierForTag(kUTTagClassFilenameExtension, cfExtension, nullptr);
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
    CFRelease(cfExtension);
    return uti;
}

bool macHandlerMatches(CFStringRef uti, const QString &identifier, LSRolesMask role)
{
    CFStringRef handler = LSCopyDefaultRoleHandlerForContentType(uti, role);
    if (!handler) {
        return false;
    }

    char buffer[256] = {};
    const bool copied = CFStringGetCString(handler, buffer, sizeof(buffer), kCFStringEncodingUTF8);
    CFRelease(handler);
    return copied && identifier == QString::fromUtf8(buffer);
}

} // namespace

bool platformIsAssociationSupported()
{
    return true;
}

bool platformIsAssociated(const ImageFormatDescriptor &descriptor)
{
    const QString identifier = bundleIdentifier();
    if (identifier.isEmpty()) {
        return false;
    }
    if (!registerCurrentAppBundle()) {
        return false;
    }

    CFStringRef uti = createUtiForExtension(descriptor.extension);
    if (!uti) {
        return false;
    }
    const bool associated = macHandlerMatches(uti, identifier, kMacAssociationRole)
                            || macHandlerMatches(uti, identifier, kLSRolesViewer);
    CFRelease(uti);
    return associated;
}

bool platformPrepareAssociations(const QStringList &normalizedSelectedExtensions, QString *errorMessage)
{
    Q_UNUSED(normalizedSelectedExtensions);
    Q_UNUSED(errorMessage);
    return true;
}

bool platformAssociateExtension(const ImageFormatDescriptor &descriptor)
{
    const QString identifier = bundleIdentifier();
    if (identifier.isEmpty()) {
        return false;
    }
    if (!registerCurrentAppBundle()) {
        return false;
    }

    CFStringRef uti = createUtiForExtension(descriptor.extension);
    CFStringRef handler = createString(identifier);
    if (!uti || !handler) {
        if (uti) {
            CFRelease(uti);
        }
        if (handler) {
            CFRelease(handler);
        }
        return false;
    }

    const OSStatus status = LSSetDefaultRoleHandlerForContentType(uti, kMacAssociationRole, handler);
    CFRelease(uti);
    CFRelease(handler);
    return status == noErr;
}

bool platformDisassociateExtension(const ImageFormatDescriptor &descriptor)
{
    const QString identifier = bundleIdentifier();
    if (identifier.isEmpty()) {
        return false;
    }
    if (!registerCurrentAppBundle()) {
        return false;
    }

    CFStringRef uti = createUtiForExtension(descriptor.extension);
    if (!uti) {
        return false;
    }

    const bool currentlyAssociated = macHandlerMatches(uti, identifier, kMacAssociationRole)
                                     || macHandlerMatches(uti, identifier, kLSRolesViewer);
    if (!currentlyAssociated) {
        CFRelease(uti);
        return true;
    }

    CFArrayRef handlers = LSCopyAllRoleHandlersForContentType(uti, kMacAssociationRole);
    if (!handlers) {
        CFRelease(uti);
        return false;
    }

    OSStatus status = kLSApplicationNotFoundErr;
    const CFIndex count = CFArrayGetCount(handlers);
    for (CFIndex index = 0; index < count; ++index) {
        auto candidate = static_cast<CFStringRef>(const_cast<void *>(CFArrayGetValueAtIndex(handlers, index)));
        if (!candidate) {
            continue;
        }
        char buffer[256] = {};
        if (!CFStringGetCString(candidate, buffer, sizeof(buffer), kCFStringEncodingUTF8)) {
            continue;
        }
        if (identifier == QString::fromUtf8(buffer)) {
            continue;
        }

        status = LSSetDefaultRoleHandlerForContentType(uti, kMacAssociationRole, candidate);
        break;
    }

    CFRelease(handlers);
    CFRelease(uti);
    return status == noErr;
}

} // namespace FileAssociationService

#endif
