#include "fileassociationservice.h"

#include "imageformats.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QIODevice>
#include <QObject>
#include <QProcess>
#include <QStandardPaths>
#include <QTextStream>

#ifdef Q_OS_MAC
#include <CoreServices/CoreServices.h>
#endif

#ifdef Q_OS_WIN
#include <qt_windows.h>
#include <shellapi.h>
#endif

namespace {

QString normalizedExtension(QString extension)
{
    extension = extension.trimmed().toLower();
    if (extension.startsWith(QLatin1Char('.'))) {
        extension.remove(0, 1);
    }
    return extension;
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

#ifdef Q_OS_WIN

std::wstring toWide(const QString &value)
{
    return value.toStdWString();
}

QString progIdForExtension(const QString &extension)
{
    return QStringLiteral("NgImageViewer.%1").arg(normalizedExtension(extension));
}

bool setRegistryString(HKEY root, const QString &subKey, const wchar_t *valueName, const QString &value)
{
    HKEY key = nullptr;
    const std::wstring wideSubKey = toWide(subKey);
    const LONG createStatus = RegCreateKeyExW(root,
                                              wideSubKey.c_str(),
                                              0,
                                              nullptr,
                                              0,
                                              KEY_SET_VALUE,
                                              nullptr,
                                              &key,
                                              nullptr);
    if (createStatus != ERROR_SUCCESS) {
        return false;
    }

    const std::wstring wideValue = toWide(value);
    const auto *data = reinterpret_cast<const BYTE *>(wideValue.c_str());
    const DWORD dataSize = static_cast<DWORD>((wideValue.size() + 1) * sizeof(wchar_t));
    const LONG setStatus = RegSetValueExW(key, valueName, 0, REG_SZ, data, dataSize);
    RegCloseKey(key);
    return setStatus == ERROR_SUCCESS;
}

bool setRegistryDefault(HKEY root, const QString &subKey, const QString &value)
{
    return setRegistryString(root, subKey, nullptr, value);
}

bool setRegistryNamedString(HKEY root, const QString &subKey, const QString &valueName, const QString &value)
{
    const std::wstring wideValueName = toWide(valueName);
    return setRegistryString(root, subKey, wideValueName.c_str(), value);
}

QString registryDefault(HKEY root, const QString &subKey)
{
    HKEY key = nullptr;
    const std::wstring wideSubKey = toWide(subKey);
    const LONG openStatus = RegOpenKeyExW(root, wideSubKey.c_str(), 0, KEY_QUERY_VALUE, &key);
    if (openStatus != ERROR_SUCCESS) {
        return {};
    }

    wchar_t buffer[512] = {};
    DWORD size = sizeof(buffer);
    const LONG queryStatus = RegQueryValueExW(key, nullptr, nullptr, nullptr, reinterpret_cast<LPBYTE>(buffer), &size);
    RegCloseKey(key);
    if (queryStatus != ERROR_SUCCESS || size == 0) {
        return {};
    }
    return QString::fromWCharArray(buffer);
}

bool deleteRegistryValue(HKEY root, const QString &subKey, const wchar_t *valueName)
{
    HKEY key = nullptr;
    const std::wstring wideSubKey = toWide(subKey);
    const LONG openStatus = RegOpenKeyExW(root, wideSubKey.c_str(), 0, KEY_SET_VALUE, &key);
    if (openStatus == ERROR_FILE_NOT_FOUND) {
        return true;
    }
    if (openStatus != ERROR_SUCCESS) {
        return false;
    }

    const LONG deleteStatus = RegDeleteValueW(key, valueName);
    RegCloseKey(key);
    return deleteStatus == ERROR_SUCCESS || deleteStatus == ERROR_FILE_NOT_FOUND;
}

bool deleteRegistryNamedValue(HKEY root, const QString &subKey, const QString &valueName)
{
    const std::wstring wideValueName = toWide(valueName);
    return deleteRegistryValue(root, subKey, wideValueName.c_str());
}

bool associateWindowsExtension(const ImageFormatDescriptor &descriptor)
{
    const QString extension = normalizedExtension(descriptor.extension);
    const QString dotExtension = QStringLiteral(".%1").arg(extension);
    const QString progId = progIdForExtension(extension);
    const QString appPath = QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
    const QString command = QStringLiteral("\"%1\" \"%2\"").arg(appPath, QStringLiteral("%1"));
    const QString description = QStringLiteral("%1 Image").arg(descriptor.displayName.isEmpty()
                                                                   ? extension.toUpper()
                                                                   : descriptor.displayName);

    bool ok = true;
    ok = setRegistryDefault(HKEY_CURRENT_USER, QStringLiteral("Software\\Classes\\%1").arg(dotExtension), progId) && ok;
    ok = setRegistryDefault(HKEY_CURRENT_USER, QStringLiteral("Software\\Classes\\%1").arg(progId), description) && ok;
    ok = setRegistryDefault(HKEY_CURRENT_USER,
                            QStringLiteral("Software\\Classes\\%1\\DefaultIcon").arg(progId),
                            QStringLiteral("\"%1\",0").arg(appPath))
         && ok;
    ok = setRegistryDefault(HKEY_CURRENT_USER,
                            QStringLiteral("Software\\Classes\\%1\\shell\\open\\command").arg(progId),
                            command)
         && ok;

    const QString applicationKey = QStringLiteral("Software\\Classes\\Applications\\NgImageViewer.exe");
    ok = setRegistryDefault(HKEY_CURRENT_USER,
                            QStringLiteral("%1\\shell\\open\\command").arg(applicationKey),
                            command)
         && ok;
    ok = setRegistryNamedString(HKEY_CURRENT_USER,
                                QStringLiteral("Software\\NgImageViewer\\Capabilities"),
                                QStringLiteral("ApplicationName"),
                                appDisplayName())
         && ok;
    ok = setRegistryNamedString(HKEY_CURRENT_USER,
                                QStringLiteral("Software\\NgImageViewer\\Capabilities"),
                                QStringLiteral("ApplicationDescription"),
                                QStringLiteral("Image viewer"))
         && ok;
    ok = setRegistryNamedString(HKEY_CURRENT_USER,
                                QStringLiteral("Software\\NgImageViewer\\Capabilities\\FileAssociations"),
                                dotExtension,
                                progId)
         && ok;
    ok = setRegistryNamedString(HKEY_CURRENT_USER,
                                QStringLiteral("Software\\RegisteredApplications"),
                                QStringLiteral("NgImageViewer"),
                                QStringLiteral("Software\\NgImageViewer\\Capabilities"))
         && ok;

    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
    return ok;
}

bool disassociateWindowsExtension(const ImageFormatDescriptor &descriptor)
{
    const QString extension = normalizedExtension(descriptor.extension);
    const QString dotExtension = QStringLiteral(".%1").arg(extension);
    const QString extensionKey = QStringLiteral("Software\\Classes\\%1").arg(dotExtension);
    const QString currentProgId = registryDefault(HKEY_CURRENT_USER, extensionKey);

    bool ok = true;
    if (currentProgId.compare(progIdForExtension(extension), Qt::CaseInsensitive) == 0) {
        ok = deleteRegistryValue(HKEY_CURRENT_USER, extensionKey, nullptr) && ok;
    }
    ok = deleteRegistryNamedValue(HKEY_CURRENT_USER,
                                  QStringLiteral("Software\\NgImageViewer\\Capabilities\\FileAssociations"),
                                  dotExtension)
         && ok;

    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
    return ok;
}

#endif

#ifdef Q_OS_MAC

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

bool associateMacExtension(const ImageFormatDescriptor &descriptor)
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

bool disassociateMacExtension(const ImageFormatDescriptor &descriptor)
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

#endif

#ifdef Q_OS_LINUX

QString desktopFileId()
{
    return QStringLiteral("ngimageviewer.desktop");
}

QString escapedDesktopExecPath(const QString &path)
{
    QString escaped = path;
    escaped.replace(QLatin1Char('\\'), QStringLiteral("\\\\"));
    escaped.replace(QLatin1Char('"'), QStringLiteral("\\\""));
    return QStringLiteral("\"%1\"").arg(escaped);
}

QStringList allMimeTypes()
{
    QStringList values;
    for (const ImageFormatDescriptor &descriptor : ImageFormats::supportedFormatDescriptors()) {
        for (const QString &mime : descriptor.mimeTypes) {
            if (!values.contains(mime)) {
                values << mime;
            }
        }
    }
    return values;
}

bool ensureLinuxDesktopFile()
{
    const QString applicationsDir =
        QStandardPaths::writableLocation(QStandardPaths::ApplicationsLocation);
    if (applicationsDir.isEmpty()) {
        return false;
    }

    QDir dir(applicationsDir);
    if (!dir.exists() && !dir.mkpath(QStringLiteral("."))) {
        return false;
    }

    QFile file(dir.filePath(desktopFileId()));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        return false;
    }

    QTextStream out(&file);
    out << "[Desktop Entry]\n";
    out << "Type=Application\n";
    out << "Name=" << appDisplayName() << "\n";
    out << "GenericName=Image Viewer\n";
    out << "Comment=View images\n";
    out << "Exec=" << escapedDesktopExecPath(QCoreApplication::applicationFilePath()) << " %F\n";
    out << "Icon=ngimageviewer\n";
    out << "Terminal=false\n";
    out << "Categories=Graphics;Viewer;Photography;\n";
    out << "MimeType=" << allMimeTypes().join(QLatin1Char(';')) << ";\n";
    out << "StartupNotify=true\n";
    out << "StartupWMClass=NgImageViewer\n";
    return true;
}

bool writeLinuxMimePackage()
{
    const QString dataHome = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    if (dataHome.isEmpty()) {
        return false;
    }

    QDir packageDir(QDir(dataHome).filePath(QStringLiteral("mime/packages")));
    if (!packageDir.exists() && !packageDir.mkpath(QStringLiteral("."))) {
        return false;
    }

    QFile file(packageDir.filePath(QStringLiteral("ngimageviewer.xml")));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        return false;
    }

    QTextStream out(&file);
    out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    out << "<mime-info xmlns=\"http://www.freedesktop.org/standards/shared-mime-info\">\n";
    QStringList writtenMimeTypes;
    for (const ImageFormatDescriptor &descriptor : ImageFormats::supportedFormatDescriptors()) {
        if (descriptor.mimeTypes.isEmpty()) {
            continue;
        }
        const QString mimeType = descriptor.mimeTypes.first();
        if (writtenMimeTypes.contains(mimeType)) {
            continue;
        }
        writtenMimeTypes << mimeType;
        out << "  <mime-type type=\"" << mimeType << "\">\n";
        out << "    <glob pattern=\"*." << descriptor.extension << "\"/>\n";
        out << "    <glob pattern=\"*." << descriptor.extension.toUpper() << "\"/>\n";
        out << "  </mime-type>\n";
    }
    out << "</mime-info>\n";

    const QString updateMime = QStandardPaths::findExecutable(QStringLiteral("update-mime-database"));
    if (!updateMime.isEmpty()) {
        QProcess::execute(updateMime, {QDir(dataHome).filePath(QStringLiteral("mime"))});
    }
    return true;
}

bool associateLinuxExtension(const ImageFormatDescriptor &descriptor)
{
    if (descriptor.mimeTypes.isEmpty()) {
        return false;
    }

    bool ok = true;
    const QString xdgMime = QStandardPaths::findExecutable(QStringLiteral("xdg-mime"));
    if (xdgMime.isEmpty()) {
        return false;
    }

    for (const QString &mime : descriptor.mimeTypes) {
        const int exitCode = QProcess::execute(xdgMime, {QStringLiteral("default"), desktopFileId(), mime});
        ok = exitCode == 0 && ok;
    }
    return ok;
}

QStringList mimeAppsListPaths()
{
    QStringList paths;
    const QString configHome = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
    if (!configHome.isEmpty()) {
        paths << QDir(configHome).filePath(QStringLiteral("mimeapps.list"));
    }

    const QString dataHome = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    if (!dataHome.isEmpty()) {
        paths << QDir(dataHome).filePath(QStringLiteral("applications/mimeapps.list"));
    }
    return paths;
}

bool removeDesktopDefaultFromMimeAppsList(const QString &path, const QStringList &mimeTypes)
{
    QFile file(path);
    if (!file.exists()) {
        return true;
    }
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }

    const QString content = QString::fromUtf8(file.readAll());
    file.close();

    QStringList outputLines;
    bool inDefaultApplications = false;
    bool changed = false;
    const QStringList lines = content.split(QLatin1Char('\n'));
    for (const QString &line : lines) {
        if (line.startsWith(QLatin1Char('['))) {
            inDefaultApplications = line.trimmed() == QStringLiteral("[Default Applications]");
            outputLines << line;
            continue;
        }

        if (!inDefaultApplications) {
            outputLines << line;
            continue;
        }

        const int equalsIndex = line.indexOf(QLatin1Char('='));
        if (equalsIndex <= 0) {
            outputLines << line;
            continue;
        }

        const QString mimeType = line.left(equalsIndex).trimmed();
        if (!mimeTypes.contains(mimeType)) {
            outputLines << line;
            continue;
        }

        QStringList desktopFiles = line.mid(equalsIndex + 1).split(QLatin1Char(';'), Qt::SkipEmptyParts);
        const int removed = desktopFiles.removeAll(desktopFileId());
        if (removed == 0) {
            outputLines << line;
            continue;
        }

        changed = true;
        if (!desktopFiles.isEmpty()) {
            outputLines << QStringLiteral("%1=%2;").arg(mimeType, desktopFiles.join(QLatin1Char(';')));
        }
    }

    if (!changed) {
        return true;
    }

    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        return false;
    }
    QTextStream out(&file);
    out << outputLines.join(QLatin1Char('\n'));
    return true;
}

bool disassociateLinuxExtension(const ImageFormatDescriptor &descriptor)
{
    if (descriptor.mimeTypes.isEmpty()) {
        return false;
    }

    bool ok = true;
    for (const QString &path : mimeAppsListPaths()) {
        ok = removeDesktopDefaultFromMimeAppsList(path, descriptor.mimeTypes) && ok;
    }
    return ok;
}

#endif

} // namespace

namespace FileAssociationService {

bool isAssociationSupported()
{
#if defined(Q_OS_WIN) || defined(Q_OS_MAC) || defined(Q_OS_LINUX)
    return true;
#else
    return false;
#endif
}

bool isAssociated(const QString &extension)
{
    const ImageFormatDescriptor descriptor = descriptorForExtension(extension);
    if (descriptor.extension.isEmpty()) {
        return false;
    }

#ifdef Q_OS_WIN
    const QString defaultProgId =
        registryDefault(HKEY_CURRENT_USER,
                        QStringLiteral("Software\\Classes\\.%1").arg(descriptor.extension));
    return defaultProgId.compare(progIdForExtension(descriptor.extension), Qt::CaseInsensitive) == 0;
#elif defined(Q_OS_MAC)
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
#elif defined(Q_OS_LINUX)
    if (descriptor.mimeTypes.isEmpty()) {
        return false;
    }
    const QString xdgMime = QStandardPaths::findExecutable(QStringLiteral("xdg-mime"));
    if (xdgMime.isEmpty()) {
        return false;
    }

    QProcess process;
    process.start(xdgMime, {QStringLiteral("query"), QStringLiteral("default"), descriptor.mimeTypes.first()});
    if (!process.waitForFinished(1000) || process.exitCode() != 0) {
        return false;
    }
    const QString output = QString::fromUtf8(process.readAllStandardOutput()).trimmed();
    return output == desktopFileId();
#else
    Q_UNUSED(extension);
    return false;
#endif
}

AssociationResult applyAssociations(const QStringList &selectedExtensions)
{
    AssociationResult result;
    if (!isAssociationSupported()) {
        result.message = QObject::tr("当前系统不支持自动关联文件格式");
        return result;
    }

    const QStringList normalizedSelectedExtensions = normalizedExtensionList(selectedExtensions);

#ifdef Q_OS_LINUX
    if (!normalizedSelectedExtensions.isEmpty() && (!ensureLinuxDesktopFile() || !writeLinuxMimePackage())) {
        result.message = QObject::tr("无法写入本机应用关联配置");
        return result;
    }
#endif

    for (const ImageFormatDescriptor &descriptor : ImageFormats::supportedFormatDescriptors()) {
        const bool shouldAssociate = normalizedSelectedExtensions.contains(descriptor.extension);
        bool ok = false;
#ifdef Q_OS_WIN
        ok = shouldAssociate ? associateWindowsExtension(descriptor) : disassociateWindowsExtension(descriptor);
#elif defined(Q_OS_MAC)
        ok = shouldAssociate ? associateMacExtension(descriptor) : disassociateMacExtension(descriptor);
#elif defined(Q_OS_LINUX)
        ok = shouldAssociate ? associateLinuxExtension(descriptor) : disassociateLinuxExtension(descriptor);
#else
        ok = false;
#endif
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
