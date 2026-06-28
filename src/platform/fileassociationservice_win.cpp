#include "fileassociationservice_p.h"

#ifdef Q_OS_WIN

#include <QCoreApplication>
#include <QDir>

#include <qt_windows.h>
#include <shlobj_core.h>
#include <shellapi.h>

namespace FileAssociationService {
namespace {

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

} // namespace

bool platformIsAssociationSupported()
{
    return true;
}

bool platformIsAssociated(const ImageFormatDescriptor &descriptor)
{
    const QString defaultProgId =
        registryDefault(HKEY_CURRENT_USER,
                        QStringLiteral("Software\\Classes\\.%1").arg(descriptor.extension));
    return defaultProgId.compare(progIdForExtension(descriptor.extension), Qt::CaseInsensitive) == 0;
}

bool platformPrepareAssociations(const QStringList &normalizedSelectedExtensions, QString *errorMessage)
{
    Q_UNUSED(normalizedSelectedExtensions);
    Q_UNUSED(errorMessage);
    return true;
}

bool platformAssociateExtension(const ImageFormatDescriptor &descriptor)
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

bool platformDisassociateExtension(const ImageFormatDescriptor &descriptor)
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

} // namespace FileAssociationService

#endif
