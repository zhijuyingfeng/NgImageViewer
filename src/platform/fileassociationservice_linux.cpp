#include "fileassociationservice_p.h"

#ifdef Q_OS_LINUX

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QIODevice>
#include <QObject>
#include <QProcess>
#include <QStandardPaths>
#include <QTextStream>

namespace FileAssociationService {
namespace {

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

} // namespace

bool platformIsAssociationSupported()
{
    return true;
}

bool platformIsAssociated(const ImageFormatDescriptor &descriptor)
{
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
}

bool platformPrepareAssociations(const QStringList &normalizedSelectedExtensions, QString *errorMessage)
{
    if (normalizedSelectedExtensions.isEmpty()) {
        return true;
    }
    if (ensureLinuxDesktopFile() && writeLinuxMimePackage()) {
        return true;
    }
    if (errorMessage) {
        *errorMessage = QObject::tr("无法写入本机应用关联配置");
    }
    return false;
}

bool platformAssociateExtension(const ImageFormatDescriptor &descriptor)
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

bool platformDisassociateExtension(const ImageFormatDescriptor &descriptor)
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

} // namespace FileAssociationService

#endif
