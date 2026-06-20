#include "imagewidgets.h"

#include <QMouseEvent>
#include <QPainter>

#include <algorithm>
#include <cmath>

ImageLabel::ImageLabel(QWidget *parent)
    : QLabel(parent)
{
}

void ImageLabel::setShowTransparencyGrid(bool show)
{
    if (m_showTransparencyGrid == show) {
        return;
    }
    m_showTransparencyGrid = show;
    update();
}

void ImageLabel::paintEvent(QPaintEvent *event)
{
    const QPixmap currentPixmap = pixmap();
    if (m_showTransparencyGrid && !currentPixmap.isNull()) {
        QPainter painter(this);
        const int cell = 12;
        for (int y = 0; y < height(); y += cell) {
            for (int x = 0; x < width(); x += cell) {
                const bool alternate = ((x / cell) + (y / cell)) % 2 == 0;
                painter.fillRect(QRect(x, y, cell, cell),
                                 alternate ? QColor("#f8fafc") : QColor("#dbe3ec"));
            }
        }
    }

    QLabel::paintEvent(event);
}

ImageOverview::ImageOverview(QWidget *parent)
    : QWidget(parent)
{
    setFixedSize(168, 126);
    setMouseTracking(true);
}

void ImageOverview::setPreview(const QPixmap &preview)
{
    m_preview = preview;
    update();
}

void ImageOverview::setViewportRect(const QRectF &normalizedRect)
{
    m_normalizedViewportRect = normalizedRect;
    update();
}

void ImageOverview::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    const QRectF outer = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    painter.setPen(QPen(QColor(15, 23, 42, 120), 1));
    painter.setBrush(QColor(15, 23, 42, 115));
    painter.drawRoundedRect(outer, 7, 7);

    if (m_preview.isNull()) {
        return;
    }

    const QRectF imageRect = overviewImageRect();
    paintCheckerboard(&painter, imageRect);
    painter.drawPixmap(imageRect.toRect(), m_preview);

    painter.setPen(QPen(QColor(255, 255, 255, 170), 1));
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(imageRect.adjusted(-0.5, -0.5, 0.5, 0.5));

    QRectF viewport = viewportIndicatorRect();
    if (viewport.isEmpty()) {
        return;
    }

    painter.setPen(QPen(QColor("#ffffff"), 2));
    painter.setBrush(QColor(255, 255, 255, 30));
    painter.drawRect(viewport.adjusted(1, 1, -1, -1));
}

void ImageOverview::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton || m_preview.isNull()) {
        QWidget::mousePressEvent(event);
        return;
    }

    const QRectF viewport = viewportIndicatorRect();
    if (!viewport.contains(event->position())) {
        QWidget::mousePressEvent(event);
        return;
    }

    m_draggingViewport = true;
    m_dragOffset = event->position() - viewport.center();
    setCursor(Qt::ClosedHandCursor);
    event->accept();
}

void ImageOverview::mouseMoveEvent(QMouseEvent *event)
{
    if (m_draggingViewport) {
        emit viewportCenterChanged(normalizedCenterFromPosition(event->position() - m_dragOffset));
        event->accept();
        return;
    }

    if (viewportIndicatorRect().contains(event->position())) {
        setCursor(Qt::OpenHandCursor);
    } else {
        unsetCursor();
    }
    QWidget::mouseMoveEvent(event);
}

void ImageOverview::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton || !m_draggingViewport) {
        QWidget::mouseReleaseEvent(event);
        return;
    }

    m_draggingViewport = false;
    if (viewportIndicatorRect().contains(event->position())) {
        setCursor(Qt::OpenHandCursor);
    } else {
        unsetCursor();
    }
    event->accept();
}

void ImageOverview::leaveEvent(QEvent *event)
{
    if (!m_draggingViewport) {
        unsetCursor();
    }
    QWidget::leaveEvent(event);
}

QRectF ImageOverview::overviewImageRect() const
{
    if (m_preview.isNull()) {
        return {};
    }

    const QRectF content = QRectF(rect()).adjusted(8, 8, -8, -8);
    QSizeF size = m_preview.size();
    size.scale(content.size(), Qt::KeepAspectRatio);
    return QRectF(
        QPointF(content.center().x() - size.width() / 2.0,
                content.center().y() - size.height() / 2.0),
        size);
}

QRectF ImageOverview::viewportIndicatorRect() const
{
    const QRectF imageRect = overviewImageRect();
    if (imageRect.isEmpty()) {
        return {};
    }

    QRectF viewport(
        imageRect.left() + m_normalizedViewportRect.left() * imageRect.width(),
        imageRect.top() + m_normalizedViewportRect.top() * imageRect.height(),
        m_normalizedViewportRect.width() * imageRect.width(),
        m_normalizedViewportRect.height() * imageRect.height());
    viewport = viewport.intersected(imageRect);
    if (viewport.isEmpty()) {
        return {};
    }
    if (viewport.width() < 4.0) {
        viewport.setWidth(4.0);
    }
    if (viewport.height() < 4.0) {
        viewport.setHeight(4.0);
    }
    if (viewport.right() > imageRect.right()) {
        viewport.moveRight(imageRect.right());
    }
    if (viewport.bottom() > imageRect.bottom()) {
        viewport.moveBottom(imageRect.bottom());
    }
    return viewport;
}

QPointF ImageOverview::normalizedCenterFromPosition(const QPointF &position) const
{
    const QRectF imageRect = overviewImageRect();
    if (imageRect.isEmpty()) {
        return {};
    }

    const double x = std::clamp(position.x(), imageRect.left(), imageRect.right());
    const double y = std::clamp(position.y(), imageRect.top(), imageRect.bottom());
    return QPointF(
        (x - imageRect.left()) / imageRect.width(),
        (y - imageRect.top()) / imageRect.height());
}

void ImageOverview::paintCheckerboard(QPainter *painter, const QRectF &area)
{
    painter->save();
    painter->setClipRect(area);
    painter->setPen(Qt::NoPen);

    const int cell = 8;
    const int startX = static_cast<int>(std::floor(area.left()));
    const int startY = static_cast<int>(std::floor(area.top()));
    for (int y = startY; y < area.bottom(); y += cell) {
        for (int x = startX; x < area.right(); x += cell) {
            const bool alternate = ((x / cell) + (y / cell)) % 2 == 0;
            painter->fillRect(QRect(x, y, cell, cell),
                              alternate ? QColor("#f8fafc") : QColor("#cbd5e1"));
        }
    }

    painter->restore();
}
