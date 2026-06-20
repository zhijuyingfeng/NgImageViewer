#include "imagewidgets.h"

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
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setFixedSize(168, 126);
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

    QRectF viewport(
        imageRect.left() + m_normalizedViewportRect.left() * imageRect.width(),
        imageRect.top() + m_normalizedViewportRect.top() * imageRect.height(),
        m_normalizedViewportRect.width() * imageRect.width(),
        m_normalizedViewportRect.height() * imageRect.height());
    viewport = viewport.intersected(imageRect);
    if (viewport.isEmpty()) {
        return;
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

    painter.setPen(QPen(QColor("#ffffff"), 2));
    painter.setBrush(QColor(255, 255, 255, 30));
    painter.drawRect(viewport.adjusted(1, 1, -1, -1));
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
