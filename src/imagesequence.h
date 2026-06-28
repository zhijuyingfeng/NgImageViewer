#ifndef IMAGESEQUENCE_H
#define IMAGESEQUENCE_H

#include <QString>
#include <QStringList>

class ImageSequence
{
public:
    void rebuild(const QString &filePath);
    void clear();

    bool hasPrevious() const;
    bool hasNext() const;
    QString previousPath() const;
    QString nextPath() const;
    QString nextPathAfterRemovingCurrent(const QString &currentFilePath);

private:
    QStringList m_images;
    int m_currentIndex = -1;
};

#endif // IMAGESEQUENCE_H
