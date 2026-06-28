#include "imageviewerwidget.h"

#include "imagewidgets.h"

#include <QEasingCurve>
#include <QEvent>
#include <QGraphicsOpacityEffect>
#include <QKeyEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QMovie>
#include <QNativeGestureEvent>
#include <QPainter>
#include <QPropertyAnimation>
#include <QResizeEvent>
#include <QScrollArea>
#include <QScrollBar>
#include <QSizePolicy>
#include <QSvgRenderer>
#include <QTimer>
#include <QTransform>
#include <QVBoxLayout>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>

ImageViewerWidget::ImageViewerWidget(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setAlignment(Qt::AlignCenter);
    m_scrollArea->setWidgetResizable(false);
    m_scrollArea->setFocusPolicy(Qt::StrongFocus);
    m_scrollArea->viewport()->setFocusPolicy(Qt::StrongFocus);

    m_imageLabel = new ImageLabel(m_scrollArea);
    m_imageLabel->setObjectName(QStringLiteral("imageLabel"));
    m_imageLabel->setAlignment(Qt::AlignCenter);
    m_imageLabel->setBackgroundRole(QPalette::Base);
    m_imageLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);

    m_scrollArea->setWidget(m_imageLabel);
    m_scrollArea->installEventFilter(this);
    m_scrollArea->viewport()->installEventFilter(this);
    m_imageLabel->installEventFilter(this);
    layout->addWidget(m_scrollArea);

    m_overviewIndicator = new ImageOverview(this);
    m_overviewIndicator->hide();
    m_overviewIndicator->raise();
    connect(m_overviewIndicator, &ImageOverview::viewportCenterChanged,
            this, &ImageViewerWidget::moveViewportFromOverview);
    connect(m_scrollArea->horizontalScrollBar(), &QScrollBar::valueChanged,
            this, &ImageViewerWidget::updateOverviewIndicator);
    connect(m_scrollArea->verticalScrollBar(), &QScrollBar::valueChanged,
            this, &ImageViewerWidget::updateOverviewIndicator);

    m_renderTimer = new QTimer(this);
    m_renderTimer->setSingleShot(true);
    m_renderTimer->setInterval(16);
    connect(m_renderTimer, &QTimer::timeout, this, [this] {
        updateImageView();
        if (!m_pendingScrollAnchor) {
            return;
        }

        m_pendingScrollAnchor = false;
        const QPointF newAnchor = m_pendingAnchorImage * m_scaleFactor;
        m_scrollArea->horizontalScrollBar()->setValue(qRound(newAnchor.x() - m_pendingAnchorViewport.x()));
        m_scrollArea->verticalScrollBar()->setValue(qRound(newAnchor.y() - m_pendingAnchorViewport.y()));
    });

    m_qualityRenderTimer = new QTimer(this);
    m_qualityRenderTimer->setSingleShot(true);
    m_qualityRenderTimer->setInterval(120);
    connect(m_qualityRenderTimer, &QTimer::timeout, this, &ImageViewerWidget::updateImageView);

    m_zoomStatusLabel = new QLabel(this);
    m_zoomStatusLabel->setObjectName(QStringLiteral("zoomStatus"));
    m_zoomStatusLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_zoomStatusOpacity = new QGraphicsOpacityEffect(m_zoomStatusLabel);
    m_zoomStatusOpacity->setOpacity(1.0);
    m_zoomStatusLabel->setGraphicsEffect(m_zoomStatusOpacity);
    m_zoomStatusFadeAnimation = new QPropertyAnimation(m_zoomStatusOpacity, "opacity", this);
    m_zoomStatusFadeAnimation->setDuration(220);
    m_zoomStatusFadeAnimation->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_zoomStatusFadeAnimation, &QPropertyAnimation::finished, this, [this] {
        if (m_zoomStatusLabel && m_zoomStatusOpacity && qFuzzyIsNull(m_zoomStatusOpacity->opacity())) {
            m_zoomStatusLabel->hide();
            m_zoomStatusOpacity->setOpacity(1.0);
        }
    });
    m_zoomStatusLabel->hide();

    m_zoomStatusTimer = new QTimer(this);
    m_zoomStatusTimer->setSingleShot(true);
    m_zoomStatusTimer->setInterval(1000);
    connect(m_zoomStatusTimer, &QTimer::timeout, this, [this] {
        if (!m_zoomStatusLabel || !m_zoomStatusFadeAnimation) {
            return;
        }
        m_zoomStatusFadeAnimation->stop();
        m_zoomStatusFadeAnimation->setStartValue(1.0);
        m_zoomStatusFadeAnimation->setEndValue(0.0);
        m_zoomStatusFadeAnimation->start();
    });
}

ImageViewerWidget::~ImageViewerWidget()
{
    stopMovie();
    stopSvgRenderer();
}

bool ImageViewerWidget::hasImage() const
{
    if (m_isSvg) {
        return m_svgRenderer != nullptr && m_svgRenderer->isValid();
    }
    return m_isGif ? m_movie != nullptr : !m_originalImage.isNull();
}

bool ImageViewerWidget::isGif() const
{
    return m_isGif;
}

bool ImageViewerWidget::isFitToWindow() const
{
    return m_fitToWindow;
}

double ImageViewerWidget::scaleFactor() const
{
    return m_fitToWindow ? currentFitScale() : m_scaleFactor;
}

void ImageViewerWidget::setStaticImage(const QImage &image)
{
    stopMovie();
    stopSvgRenderer();
    resetViewState();
    m_originalImage = image;
    updateImageView();
    QTimer::singleShot(0, this, &ImageViewerWidget::updateImageView);
    updateZoomStatus();
    emit viewStateChanged();
}

void ImageViewerWidget::setSvgImage(QSvgRenderer *renderer, const QSize &defaultSize)
{
    stopMovie();
    stopSvgRenderer();
    resetViewState();
    m_svgRenderer = renderer;
    if (m_svgRenderer) {
        m_svgRenderer->setParent(this);
    }
    m_svgDefaultSize = defaultSize;
    m_isSvg = true;
    updateImageView();
    QTimer::singleShot(0, this, &ImageViewerWidget::updateImageView);
    updateZoomStatus();
    emit viewStateChanged();
}

void ImageViewerWidget::setGif(QMovie *movie)
{
    stopMovie();
    stopSvgRenderer();
    resetViewState();
    m_movie = movie;
    if (m_movie) {
        m_movie->setParent(this);
        m_movie->setCacheMode(QMovie::CacheAll);
        connect(m_movie, &QMovie::frameChanged, this, [this] {
            invalidateOverviewPreview();
            updateImageView();
        });
        m_movie->start();
    }
    m_isGif = m_movie != nullptr;
    updateImageView();
    QTimer::singleShot(0, this, &ImageViewerWidget::updateImageView);
    updateZoomStatus();
    emit viewStateChanged();
}

void ImageViewerWidget::clear()
{
    stopMovie();
    stopSvgRenderer();
    resetViewState();
    m_originalImage = QImage();
    m_svgDefaultSize = QSize();
    if (m_imageLabel) {
        m_imageLabel->clearDrawnPixmap();
        m_imageLabel->clear();
        m_imageLabel->setShowTransparencyGrid(false);
        m_imageLabel->resize(0, 0);
    }
    hideTransientUi();
    emit viewStateChanged();
}

void ImageViewerWidget::hideTransientUi()
{
    if (m_overviewIndicator) {
        m_overviewIndicator->hide();
    }
    if (m_zoomStatusLabel) {
        m_zoomStatusLabel->hide();
    }
    if (m_zoomStatusFadeAnimation) {
        m_zoomStatusFadeAnimation->stop();
    }
    if (m_zoomStatusOpacity) {
        m_zoomStatusOpacity->setOpacity(1.0);
    }
    if (m_zoomStatusTimer) {
        m_zoomStatusTimer->stop();
    }
}

void ImageViewerWidget::focusViewer()
{
    if (m_scrollArea) {
        m_scrollArea->setFocus(Qt::OtherFocusReason);
    }
}

void ImageViewerWidget::setNavigationAvailability(bool hasPrevious, bool hasNext)
{
    m_hasPrevious = hasPrevious;
    m_hasNext = hasNext;
}

QSize ImageViewerWidget::transformedImageSize() const
{
    QSize size;
    if (m_isSvg && m_svgRenderer) {
        size = m_svgDefaultSize;
    } else if (m_isGif && m_movie) {
        size = m_movie->currentImage().size();
    } else {
        size = m_originalImage.size();
    }

    if (size.isEmpty()) {
        return {};
    }

    if (m_rotation == 90 || m_rotation == 270) {
        size.transpose();
    }
    return size;
}

QImage ImageViewerWidget::imageForClipboard() const
{
    if (m_isSvg && m_svgRenderer) {
        const QSize imageSize = m_svgDefaultSize.isEmpty() ? QSize(1024, 1024) : m_svgDefaultSize;
        QImage image(imageSize, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        QPainter painter(&image);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setRenderHint(QPainter::TextAntialiasing);
        painter.setRenderHint(QPainter::SmoothPixmapTransform);
        m_svgRenderer->render(&painter, QRectF(QPointF(0, 0), QSizeF(imageSize)));
        return image;
    }

    if (m_isGif && m_movie) {
        return m_movie->currentImage();
    }

    return m_originalImage;
}

void ImageViewerWidget::zoomBy(double factor)
{
    if (!hasImage()) {
        return;
    }
    zoomAt(factor, m_scrollArea->viewport()->rect().center());
}

void ImageViewerWidget::toggleFitMode()
{
    if (!hasImage()) {
        return;
    }
    m_fitToWindow = !m_fitToWindow;
    m_pendingScrollAnchor = false;
    if (!m_fitToWindow) {
        m_scaleFactor = std::min(1.0, maximumManualScale());
    }
    updateImageView();
    updateZoomStatus();
    emit viewStateChanged();
}

void ImageViewerWidget::showActualSize()
{
    if (!hasImage()) {
        return;
    }
    m_fitToWindow = false;
    m_scaleFactor = std::min(1.0, maximumManualScale());
    m_pendingScrollAnchor = true;
    const QSize imageSize = transformedImageSize();
    m_pendingAnchorImage = QPointF(imageSize.width() / 2.0, imageSize.height() / 2.0);
    m_pendingAnchorViewport = m_scrollArea->viewport()->rect().center();
    requestImageViewUpdate();
    updateZoomStatus();
    emit viewStateChanged();
}

void ImageViewerWidget::rotateBy(int degrees)
{
    if (!hasImage()) {
        return;
    }
    m_rotation = (m_rotation + degrees) % 360;
    if (m_rotation < 0) {
        m_rotation += 360;
    }
    m_pendingScrollAnchor = false;
    invalidateOverviewPreview();
    updateImageView();
    emit viewStateChanged();
}

bool ImageViewerWidget::handleKeyboardPan(QKeyEvent *event)
{
    if (!hasImage()) {
        return false;
    }

    if (event->key() == Qt::Key_Escape && isZoomedBeyondFit()) {
        m_fitToWindow = true;
        m_scaleFactor = 1.0;
        m_pendingScrollAnchor = false;
        updateImageView();
        updateZoomStatus();
        emit viewStateChanged();
        event->accept();
        return true;
    }

    if (!isZoomedBeyondFit()) {
        if (event->key() == Qt::Key_Left && m_hasPrevious) {
            emit previousRequested();
            event->accept();
            return true;
        }
        if (event->key() == Qt::Key_Right && m_hasNext) {
            emit nextRequested();
            event->accept();
            return true;
        }
        return false;
    }

    int dx = 0;
    int dy = 0;
    switch (event->key()) {
    case Qt::Key_Left:
        dx = -1;
        break;
    case Qt::Key_Right:
        dx = 1;
        break;
    case Qt::Key_Up:
        dy = -1;
        break;
    case Qt::Key_Down:
        dy = 1;
        break;
    default:
        return false;
    }

    auto *horizontal = m_scrollArea->horizontalScrollBar();
    auto *vertical = m_scrollArea->verticalScrollBar();
    if ((dx != 0 && horizontal->maximum() == 0)
        || (dy != 0 && vertical->maximum() == 0)) {
        return false;
    }

    const int baseStep = std::max(48, m_scrollArea->viewport()->width() / 12);
    const int step = event->modifiers().testFlag(Qt::ShiftModifier) ? baseStep * 3 : baseStep;
    if (dx != 0) {
        horizontal->setValue(horizontal->value() + dx * step);
    }
    if (dy != 0) {
        vertical->setValue(vertical->value() + dy * step);
    }

    event->accept();
    return true;
}

bool ImageViewerWidget::handleWindowEvent(QEvent *event)
{
    if (!hasImage()) {
        return false;
    }

    if (event->type() == QEvent::NativeGesture) {
        auto *nativeGesture = static_cast<QNativeGestureEvent *>(event);
        if (handleNativeZoomGesture(nativeGesture)) {
            return true;
        }
    }

    if (event->type() == QEvent::Wheel) {
        auto *wheel = static_cast<QWheelEvent *>(event);
        if (handleWheelZoomEvent(this, wheel)) {
            return true;
        }
    }

    return false;
}

bool ImageViewerWidget::eventFilter(QObject *watched, QEvent *event)
{
    const bool imageViewportEvent = watched == m_scrollArea
                                    || watched == m_scrollArea->viewport()
                                    || watched == m_imageLabel;
    if (!imageViewportEvent || !hasImage()) {
        return QWidget::eventFilter(watched, event);
    }

    if (event->type() == QEvent::KeyPress) {
        auto *keyEvent = static_cast<QKeyEvent *>(event);
        if (handleKeyboardPan(keyEvent)) {
            return true;
        }
    }

    if (handleMousePanEvent(watched, event)) {
        return true;
    }

    if (event->type() == QEvent::NativeGesture) {
        auto *nativeGesture = static_cast<QNativeGestureEvent *>(event);
        if (handleNativeZoomGesture(nativeGesture)) {
            return true;
        }
    }

    if (event->type() != QEvent::Wheel) {
        return QWidget::eventFilter(watched, event);
    }

    auto *wheel = static_cast<QWheelEvent *>(event);
    if (handleWheelZoomEvent(watched, wheel)) {
        return true;
    }
    return QWidget::eventFilter(watched, event);
}

void ImageViewerWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    if (m_fitToWindow && hasImage()) {
        updateImageView();
    }
    repositionOverviewIndicator();
    updateOverviewIndicator();
    repositionZoomStatus();
}

bool ImageViewerWidget::handleMousePanEvent(QObject *watched, QEvent *event)
{
    if (!isZoomedBeyondFit() || !m_scrollArea || !m_imageLabel) {
        return false;
    }

    auto *horizontal = m_scrollArea->horizontalScrollBar();
    auto *vertical = m_scrollArea->verticalScrollBar();
    if (horizontal->maximum() == 0 && vertical->maximum() == 0) {
        return false;
    }

    auto viewportPosition = [this, watched](const QPoint &position) {
        if (auto *widget = qobject_cast<QWidget *>(watched)) {
            return widget->mapTo(m_scrollArea->viewport(), position);
        }
        return position;
    };

    if (event->type() == QEvent::MouseButtonPress) {
        auto *mouseEvent = static_cast<QMouseEvent *>(event);
        if (mouseEvent->button() != Qt::LeftButton) {
            return false;
        }

        m_mousePanning = true;
        m_lastPanPosition = viewportPosition(mouseEvent->position().toPoint());
        m_scrollArea->viewport()->setCursor(Qt::ClosedHandCursor);
        mouseEvent->accept();
        return true;
    }

    if (event->type() == QEvent::MouseMove && m_mousePanning) {
        auto *mouseEvent = static_cast<QMouseEvent *>(event);
        const QPoint currentPosition = viewportPosition(mouseEvent->position().toPoint());
        const QPoint delta = currentPosition - m_lastPanPosition;
        horizontal->setValue(horizontal->value() - delta.x());
        vertical->setValue(vertical->value() - delta.y());
        m_lastPanPosition = currentPosition;
        mouseEvent->accept();
        return true;
    }

    if ((event->type() == QEvent::MouseButtonRelease || event->type() == QEvent::Leave)
        && m_mousePanning) {
        if (event->type() == QEvent::MouseButtonRelease) {
            auto *mouseEvent = static_cast<QMouseEvent *>(event);
            if (mouseEvent->button() != Qt::LeftButton) {
                return false;
            }
            mouseEvent->accept();
        }
        m_mousePanning = false;
        m_scrollArea->viewport()->unsetCursor();
        return true;
    }

    return false;
}

bool ImageViewerWidget::handleWheelZoomEvent(QObject *watched, QWheelEvent *event)
{
    if (!hasImage() || !m_scrollArea
        || (event->angleDelta().isNull() && event->pixelDelta().isNull())) {
        return false;
    }

    const bool ctrlZoom = event->modifiers().testFlag(Qt::ControlModifier);
    double steps = 0.0;
    if (ctrlZoom && !event->pixelDelta().isNull()) {
        const int delta = event->pixelDelta().y() != 0
                              ? event->pixelDelta().y()
                              : event->pixelDelta().x();
        steps = -delta / 80.0;
    } else if (event->angleDelta().y() != 0) {
        steps = event->angleDelta().y() / static_cast<double>(QWheelEvent::DefaultDeltasPerStep);
    } else if (event->pixelDelta().y() != 0) {
        steps = event->pixelDelta().y() / 80.0;
    } else if (ctrlZoom && event->angleDelta().x() != 0) {
        steps = event->angleDelta().x() / static_cast<double>(QWheelEvent::DefaultDeltasPerStep);
    }

    if (qFuzzyIsNull(steps)) {
        return false;
    }

    QPoint viewportPosition = m_scrollArea->viewport()->mapFromGlobal(event->globalPosition().toPoint());
    if (!m_scrollArea->viewport()->rect().contains(viewportPosition)) {
        viewportPosition = m_scrollArea->viewport()->rect().center();
        if (auto *widget = qobject_cast<QWidget *>(watched)) {
            viewportPosition = widget->mapTo(m_scrollArea->viewport(), event->position().toPoint());
        }
    }

    zoomAt(std::pow(1.18, steps), viewportPosition);
    event->accept();
    return true;
}

void ImageViewerWidget::zoomAt(double factor, const QPoint &viewportPosition)
{
    if (!hasImage()) {
        return;
    }

    const double oldScale = m_fitToWindow ? currentFitScale() : m_scaleFactor;
    QPointF labelPosition = m_imageLabel->pos();
    if (m_pendingScrollAnchor && !m_fitToWindow) {
        labelPosition = QPointF(m_pendingAnchorViewport) - m_pendingAnchorImage * oldScale;
    }
    const double newScale = std::clamp(oldScale * factor, 0.05, maximumManualScale());
    if (qFuzzyCompare(newScale, oldScale)) {
        return;
    }

    const QPointF imageAnchor = (QPointF(viewportPosition) - labelPosition) / oldScale;

    m_fitToWindow = false;
    m_scaleFactor = newScale;
    m_pendingScrollAnchor = true;
    m_pendingAnchorImage = imageAnchor;
    m_pendingAnchorViewport = viewportPosition;
    requestImageViewUpdate();
    if (m_qualityRenderTimer) {
        m_qualityRenderTimer->start();
    }
    updateZoomStatus();
    emit viewStateChanged();
}

bool ImageViewerWidget::handleNativeZoomGesture(QNativeGestureEvent *event)
{
    if (event->gestureType() != Qt::ZoomNativeGesture || qFuzzyIsNull(event->value())) {
        return false;
    }

    const QPoint viewportPosition = m_scrollArea->viewport()->mapFromGlobal(event->globalPosition().toPoint());
    zoomAt(std::exp(event->value()), viewportPosition);
    event->accept();
    return true;
}

void ImageViewerWidget::updateImageView()
{
    if (m_isSvg && m_svgRenderer) {
        QSize targetSize = transformedImageSize();
        if (targetSize.isEmpty()) {
            return;
        }

        if (m_fitToWindow) {
            QSize viewport = m_scrollArea->viewport()->size() - QSize(24, 24);
            viewport = viewport.expandedTo(QSize(1, 1));
            targetSize.scale(viewport, Qt::KeepAspectRatio);
        } else {
            targetSize = QSize(
                std::max(1, qRound(targetSize.width() * m_scaleFactor)),
                std::max(1, qRound(targetSize.height() * m_scaleFactor)));
        }

        QSize renderSize = targetSize;
        if (m_rotation == 90 || m_rotation == 270) {
            renderSize.transpose();
        }

        QImage rendered(renderSize, QImage::Format_ARGB32_Premultiplied);
        rendered.fill(Qt::transparent);
        QPainter painter(&rendered);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setRenderHint(QPainter::TextAntialiasing);
        painter.setRenderHint(QPainter::SmoothPixmapTransform);
        m_svgRenderer->render(&painter, QRectF(QPointF(0, 0), QSizeF(renderSize)));
        painter.end();

        QImage display = rendered;
        if (m_rotation != 0) {
            QTransform transform;
            transform.rotate(m_rotation);
            display = rendered.transformed(transform, Qt::SmoothTransformation);
        }

        m_lastTiledTargetSize = QSize();
        m_imageLabel->clearDrawnPixmap();
        m_imageLabel->setPixmap(QPixmap::fromImage(display));
        m_imageLabel->setShowTransparencyGrid(true);
        m_imageLabel->resize(display.size());
        updateOverviewIndicator();
        updateZoomStatus(false);
        return;
    }

    QImage source;
    if (m_isGif && m_movie) {
        source = m_movie->currentImage();
    } else {
        source = m_originalImage;
    }
    if (source.isNull()) {
        return;
    }

    QImage display = source;
    if (m_rotation != 0) {
        QTransform transform;
        transform.rotate(m_rotation);
        display = source.transformed(transform, Qt::SmoothTransformation);
    }

    QSize targetSize = display.size();
    if (m_fitToWindow) {
        QSize viewport = m_scrollArea->viewport()->size() - QSize(24, 24);
        viewport = viewport.expandedTo(QSize(1, 1));
        targetSize.scale(viewport, Qt::KeepAspectRatio);
    } else {
        const double effectiveScale = std::min(m_scaleFactor, maximumManualScale());
        if (!qFuzzyCompare(effectiveScale, m_scaleFactor)) {
            m_scaleFactor = effectiveScale;
        }
        targetSize = QSize(
            std::max(1, qRound(display.width() * effectiveScale)),
            std::max(1, qRound(display.height() * effectiveScale)));
    }

    const Qt::TransformationMode transformationMode = m_pendingScrollAnchor
                                                          || (m_qualityRenderTimer
                                                              && m_qualityRenderTimer->isActive())
                                                          ? Qt::FastTransformation
                                                          : Qt::SmoothTransformation;
    if (updateTiledImageView(display, targetSize, transformationMode)) {
        updateOverviewIndicator();
        updateZoomStatus(false);
        return;
    }

    m_lastTiledTargetSize = QSize();
    const QPixmap pixmap = QPixmap::fromImage(display).scaled(
        targetSize,
        Qt::KeepAspectRatio,
        transformationMode);
    m_imageLabel->clearDrawnPixmap();
    m_imageLabel->setPixmap(pixmap);
    m_imageLabel->setShowTransparencyGrid(display.hasAlphaChannel());
    m_imageLabel->resize(pixmap.size());
    updateOverviewIndicator();
    updateZoomStatus(false);
}

bool ImageViewerWidget::updateTiledImageView(const QImage &source, const QSize &targetSize, Qt::TransformationMode mode)
{
    if (source.isNull() || targetSize.isEmpty() || m_fitToWindow || m_isGif || m_isSvg || !m_scrollArea) {
        return false;
    }

    const QSize viewportSize = m_scrollArea->viewport()->size();
    if (viewportSize.isEmpty()) {
        return false;
    }

    constexpr qint64 fullRenderPixelLimit = 18000000;
    const qint64 targetPixels = static_cast<qint64>(targetSize.width()) * targetSize.height();
    if (targetPixels <= fullRenderPixelLimit) {
        return false;
    }

    const double scaleX = targetSize.width() / static_cast<double>(source.width());
    const double scaleY = targetSize.height() / static_cast<double>(source.height());
    if (qFuzzyIsNull(scaleX) || qFuzzyIsNull(scaleY)) {
        return false;
    }

    const bool sameTiledState = m_lastTiledTargetSize == targetSize
                                && m_lastTiledMode == mode
                                && m_imageLabel->hasTiledImage(source, targetSize, mode);
    m_imageLabel->setShowTransparencyGrid(source.hasAlphaChannel());
    if (m_imageLabel->size() != targetSize) {
        m_imageLabel->resize(targetSize);
    }
    if (!sameTiledState) {
        m_imageLabel->setTiledImage(source, targetSize, mode);
        m_lastTiledTargetSize = targetSize;
        m_lastTiledMode = mode;
    }
    return true;
}

void ImageViewerWidget::requestImageViewUpdate()
{
    if (!m_renderTimer) {
        updateImageView();
        return;
    }

    if (!m_renderTimer->isActive()) {
        m_renderTimer->start();
    }
}

void ImageViewerWidget::updateZoomStatus(bool restartHideTimer)
{
    if (!m_zoomStatusLabel || !hasImage()) {
        return;
    }

    if (m_fitToWindow) {
        m_zoomStatusLabel->setText(tr("Fit (%1%)").arg(qRound(currentFitScale() * 100.0)));
    } else {
        m_zoomStatusLabel->setText(tr("%1%").arg(qRound(m_scaleFactor * 100.0)));
    }

    m_zoomStatusLabel->adjustSize();
    repositionZoomStatus();
    if (!restartHideTimer && !m_zoomStatusLabel->isVisible()) {
        return;
    }
    if (!restartHideTimer) {
        m_zoomStatusLabel->raise();
        return;
    }

    if (m_zoomStatusFadeAnimation) {
        m_zoomStatusFadeAnimation->stop();
    }
    if (m_zoomStatusOpacity) {
        m_zoomStatusOpacity->setOpacity(1.0);
    }
    m_zoomStatusLabel->show();
    m_zoomStatusLabel->raise();
    if (restartHideTimer && m_zoomStatusTimer) {
        m_zoomStatusTimer->start();
    }
}

void ImageViewerWidget::repositionZoomStatus()
{
    if (!m_zoomStatusLabel) {
        return;
    }

    const int margin = 14;
    const int y = std::max(margin, height() - m_zoomStatusLabel->height() - margin);
    m_zoomStatusLabel->move(margin, y);
}

void ImageViewerWidget::invalidateOverviewPreview()
{
    m_overviewPreviewCache = QPixmap();
    m_overviewPreviewCacheSize = QSize();
    m_overviewPreviewDirty = true;
    m_lastTiledTargetSize = QSize();
}

void ImageViewerWidget::updateOverviewIndicator()
{
    if (!m_overviewIndicator || !m_scrollArea || !m_imageLabel) {
        return;
    }

    const QSize labelSize = m_imageLabel->size();
    const QSize viewportSize = m_scrollArea->viewport()->size();
    const double displayedScaleRatio = m_fitToWindow || qFuzzyIsNull(currentFitScale())
                                           ? 1.0
                                           : m_scaleFactor / currentFitScale();
    const bool shouldShow = hasImage()
                            && !m_fitToWindow
                            && displayedScaleRatio >= 1.0
                            && !labelSize.isEmpty()
                            && (labelSize.width() > viewportSize.width()
                                || labelSize.height() > viewportSize.height());
    if (!shouldShow) {
        m_overviewIndicator->hide();
        return;
    }

    const QPixmap preview = createOverviewPixmap(QSize(152, 110));
    if (preview.isNull()) {
        m_overviewIndicator->hide();
        return;
    }

    const QRectF imageRect(QPointF(0, 0), QSizeF(labelSize));
    QRectF visibleRect(QPointF(-m_imageLabel->pos()), QSizeF(viewportSize));
    visibleRect = visibleRect.intersected(imageRect);
    if (visibleRect.isEmpty()) {
        m_overviewIndicator->hide();
        return;
    }

    m_overviewIndicator->setPreview(preview);
    m_overviewIndicator->setViewportRect(QRectF(
        visibleRect.left() / labelSize.width(),
        visibleRect.top() / labelSize.height(),
        visibleRect.width() / labelSize.width(),
        visibleRect.height() / labelSize.height()));

    repositionOverviewIndicator();
    m_overviewIndicator->show();
    m_overviewIndicator->raise();
}

void ImageViewerWidget::repositionOverviewIndicator()
{
    if (!m_overviewIndicator) {
        return;
    }

    const int margin = 18;
    m_overviewIndicator->move(
        std::max(margin, width() - m_overviewIndicator->width() - margin),
        margin);
}

void ImageViewerWidget::moveViewportFromOverview(const QPointF &normalizedCenter)
{
    if (!m_scrollArea || !m_imageLabel || !isZoomedBeyondFit()) {
        return;
    }

    const QSize labelSize = m_imageLabel->size();
    const QSize viewportSize = m_scrollArea->viewport()->size();
    if (labelSize.isEmpty() || viewportSize.isEmpty()) {
        return;
    }

    m_pendingScrollAnchor = false;
    auto *horizontal = m_scrollArea->horizontalScrollBar();
    auto *vertical = m_scrollArea->verticalScrollBar();
    horizontal->setValue(qRound(normalizedCenter.x() * labelSize.width() - viewportSize.width() / 2.0));
    vertical->setValue(qRound(normalizedCenter.y() * labelSize.height() - viewportSize.height() / 2.0));
}

QPixmap ImageViewerWidget::createOverviewPixmap(const QSize &targetSize)
{
    if (!m_overviewPreviewDirty
        && m_overviewPreviewCacheSize == targetSize
        && !m_overviewPreviewCache.isNull()) {
        return m_overviewPreviewCache;
    }

    const QSize imageSize = transformedImageSize();
    if (imageSize.isEmpty() || targetSize.isEmpty()) {
        return {};
    }

    QSize previewSize = imageSize;
    previewSize.scale(targetSize, Qt::KeepAspectRatio);
    previewSize = previewSize.expandedTo(QSize(1, 1));

    if (m_isSvg && m_svgRenderer) {
        QSize renderSize = previewSize;
        if (m_rotation == 90 || m_rotation == 270) {
            renderSize.transpose();
        }

        QImage rendered(renderSize, QImage::Format_ARGB32_Premultiplied);
        rendered.fill(Qt::transparent);
        QPainter painter(&rendered);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setRenderHint(QPainter::TextAntialiasing);
        painter.setRenderHint(QPainter::SmoothPixmapTransform);
        m_svgRenderer->render(&painter, QRectF(QPointF(0, 0), QSizeF(renderSize)));
        painter.end();

        if (m_rotation == 0) {
            m_overviewPreviewCache = QPixmap::fromImage(rendered);
        } else {
            QTransform transform;
            transform.rotate(m_rotation);
            m_overviewPreviewCache = QPixmap::fromImage(rendered.transformed(transform, Qt::SmoothTransformation));
        }
        m_overviewPreviewCacheSize = targetSize;
        m_overviewPreviewDirty = false;
        return m_overviewPreviewCache;
    }

    QImage source;
    if (m_isGif && m_movie) {
        source = m_movie->currentImage();
    } else {
        source = m_originalImage;
    }
    if (source.isNull()) {
        return {};
    }

    QImage display = source;
    if (m_rotation != 0) {
        QTransform transform;
        transform.rotate(m_rotation);
        display = source.transformed(transform, Qt::SmoothTransformation);
    }
    m_overviewPreviewCache = QPixmap::fromImage(display).scaled(
        previewSize,
        Qt::KeepAspectRatio,
        Qt::SmoothTransformation);
    m_overviewPreviewCacheSize = targetSize;
    m_overviewPreviewDirty = false;
    return m_overviewPreviewCache;
}

double ImageViewerWidget::currentFitScale() const
{
    const QSize imageSize = transformedImageSize();
    if (imageSize.isEmpty() || !m_scrollArea) {
        return 1.0;
    }

    QSize viewport = m_scrollArea->viewport()->size() - QSize(24, 24);
    viewport = viewport.expandedTo(QSize(1, 1));

    return std::min(
        viewport.width() / static_cast<double>(imageSize.width()),
        viewport.height() / static_cast<double>(imageSize.height()));
}

double ImageViewerWidget::maximumManualScale() const
{
    const double fitScale = currentFitScale();
    if (m_isSvg) {
        return std::max(256.0, fitScale * 4.0);
    }

    const QSize imageSize = transformedImageSize();
    if (imageSize.isEmpty()) {
        return std::max(1.0, fitScale);
    }

    const double sourcePixels = imageSize.width() * static_cast<double>(imageSize.height());
    const double longestSide = std::max(imageSize.width(), imageSize.height());
    if (m_isGif) {
        constexpr double kMaxGifRenderedPixels = 48000000.0;
        constexpr double kMaxGifRenderedSide = 12000.0;
        const double pixelLimit = std::sqrt(kMaxGifRenderedPixels / sourcePixels);
        const double sideLimit = kMaxGifRenderedSide / longestSide;
        const double budgetLimit = std::min(pixelLimit, sideLimit);
        const double requestedLimit = std::max(20.0, fitScale * 4.0);
        return std::max(fitScale, std::min(requestedLimit, budgetLimit));
    }

    constexpr double kMaxBitmapScale = 32.0;
    constexpr double kMaxBitmapRenderedSide = 131072.0;
    const double sideLimit = kMaxBitmapRenderedSide / longestSide;
    const double requestedLimit = std::max(kMaxBitmapScale, fitScale * 8.0);
    return std::max(fitScale, std::min(requestedLimit, sideLimit));
}

bool ImageViewerWidget::isZoomedBeyondFit() const
{
    const double fitScale = currentFitScale();
    return hasImage()
           && !m_fitToWindow
           && !qFuzzyIsNull(fitScale)
           && m_scaleFactor / fitScale > 1.0;
}

void ImageViewerWidget::stopMovie()
{
    if (!m_movie) {
        return;
    }
    m_movie->stop();
    m_movie->disconnect(this);
    delete m_movie;
    m_movie = nullptr;
}

void ImageViewerWidget::stopSvgRenderer()
{
    if (!m_svgRenderer) {
        return;
    }
    delete m_svgRenderer;
    m_svgRenderer = nullptr;
    m_svgDefaultSize = QSize();
}

void ImageViewerWidget::resetViewState()
{
    invalidateOverviewPreview();
    m_isGif = false;
    m_isSvg = false;
    m_fitToWindow = true;
    m_scaleFactor = 1.0;
    m_rotation = 0;
    m_pendingScrollAnchor = false;
    m_mousePanning = false;
    if (m_scrollArea) {
        m_scrollArea->viewport()->unsetCursor();
    }
}
