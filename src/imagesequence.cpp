#include "imagesequence.h"

#include "imageformats.h"

#include <QDir>
#include <QFileInfo>
#include <algorithm>

void ImageSequence::rebuild(const QString &filePath)
{
    const QFileInfo current(filePath);
    QDir dir(current.absolutePath());
    const QFileInfoList files = dir.entryInfoList(
        ImageFormats::imageNameFilters(),
        QDir::Files | QDir::Readable,
        QDir::Name | QDir::IgnoreCase);

    m_images.clear();
    for (const QFileInfo &file : files) {
        m_images << file.absoluteFilePath();
    }
    m_currentIndex = m_images.indexOf(current.absoluteFilePath());
}

void ImageSequence::clear()
{
    m_images.clear();
    m_currentIndex = -1;
}

bool ImageSequence::hasPrevious() const
{
    return m_currentIndex > 0;
}

bool ImageSequence::hasNext() const
{
    return m_currentIndex >= 0 && m_currentIndex + 1 < m_images.size();
}

QString ImageSequence::previousPath() const
{
    return hasPrevious() ? m_images.at(m_currentIndex - 1) : QString();
}

QString ImageSequence::nextPath() const
{
    return hasNext() ? m_images.at(m_currentIndex + 1) : QString();
}

QString ImageSequence::nextPathAfterRemovingCurrent(const QString &currentFilePath)
{
    const int oldIndex = m_currentIndex;
    const QString oldDir = QFileInfo(currentFilePath).absolutePath();
    rebuild(QDir(oldDir).absoluteFilePath(QStringLiteral("__deleted__")));

    if (m_images.isEmpty()) {
        return {};
    }

    const int lastIndex = static_cast<int>(m_images.size()) - 1;
    const int nextIndex = std::clamp(oldIndex, 0, lastIndex);
    return m_images.at(nextIndex);
}
