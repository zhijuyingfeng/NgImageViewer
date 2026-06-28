#include "imageloadstate.h"

bool ImageDocument::isRaw() const
{
    return kind == ImageLoader::Kind::RawImage;
}

bool ImageDocument::isHeif() const
{
    return kind == ImageLoader::Kind::HeifImage;
}

quint64 ImageLoadState::beginLoading(const QString &filePath)
{
    ++m_requestId;
    m_pendingPath = filePath;
    m_loading = true;
    m_viewerState = ViewerState::Loading;
    return m_requestId;
}

bool ImageLoadState::isActiveRequest(quint64 requestId) const
{
    return requestId == m_requestId;
}

void ImageLoadState::completeLoading(const ImageDocument &document)
{
    m_current = document;
    m_pendingPath.clear();
    m_loading = false;
    m_viewerState = ViewerState::ShowingImage;
    m_sequence.rebuild(document.filePath);
}

void ImageLoadState::failLoading()
{
    m_pendingPath.clear();
    m_loading = false;
    m_viewerState = m_current.has_value() ? ViewerState::ShowingImage : ViewerState::Empty;
}

void ImageLoadState::clearCurrent()
{
    m_pendingPath.clear();
    m_current.reset();
    m_loading = false;
    m_viewerState = ViewerState::Empty;
    m_sequence.clear();
}

void ImageLoadState::setCurrentDisplayedSize(const QSize &size)
{
    if (m_current) {
        m_current->displayedSize = size;
    }
}

bool ImageLoadState::hasCurrent() const
{
    return m_current.has_value();
}

bool ImageLoadState::isLoading() const
{
    return m_loading;
}

ImageLoadState::ViewerState ImageLoadState::viewerState() const
{
    return m_viewerState;
}

const QString &ImageLoadState::pendingPath() const
{
    return m_pendingPath;
}

const ImageDocument *ImageLoadState::current() const
{
    return m_current ? &(*m_current) : nullptr;
}

ImageDocument *ImageLoadState::current()
{
    return m_current ? &(*m_current) : nullptr;
}

QString ImageLoadState::currentPath() const
{
    return m_current ? m_current->filePath : QString();
}

const ImageSequence &ImageLoadState::sequence() const
{
    return m_sequence;
}

ImageSequence &ImageLoadState::sequence()
{
    return m_sequence;
}

ImageDocument ImageLoadState::documentFromLoadResult(const ImageLoader::LoadResult &result)
{
    ImageDocument document;
    document.filePath = result.filePath;
    document.kind = result.kind;
    document.warningMessage = result.warningMessage;
    document.raw = result.raw;
    document.heif = result.heif;
    return document;
}
