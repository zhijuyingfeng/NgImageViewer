#ifndef IMAGELOADSTATE_H
#define IMAGELOADSTATE_H

#include "imageloader.h"
#include "imagesequence.h"

#include <QSize>
#include <QString>

#include <optional>

struct ImageDocument
{
    QString filePath;
    ImageLoader::Kind kind = ImageLoader::Kind::Invalid;
    QSize displayedSize;
    QString warningMessage;
    ImageLoader::RawMetadata raw;
    ImageLoader::HeifMetadata heif;

    bool isRaw() const;
    bool isHeif() const;
};

class ImageLoadState
{
public:
    enum class ViewerState {
        Empty,
        Loading,
        ShowingImage
    };

    quint64 beginLoading(const QString &filePath);
    bool isActiveRequest(quint64 requestId) const;
    void completeLoading(const ImageDocument &document);
    void failLoading();
    void clearCurrent();
    void setCurrentDisplayedSize(const QSize &size);

    bool hasCurrent() const;
    bool isLoading() const;
    ViewerState viewerState() const;
    const QString &pendingPath() const;
    const ImageDocument *current() const;
    ImageDocument *current();
    QString currentPath() const;

    const ImageSequence &sequence() const;
    ImageSequence &sequence();

    static ImageDocument documentFromLoadResult(const ImageLoader::LoadResult &result);

private:
    quint64 m_requestId = 0;
    QString m_pendingPath;
    std::optional<ImageDocument> m_current;
    bool m_loading = false;
    ViewerState m_viewerState = ViewerState::Empty;
    ImageSequence m_sequence;
};

#endif // IMAGELOADSTATE_H
