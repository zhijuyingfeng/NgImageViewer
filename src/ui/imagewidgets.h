#ifndef IMAGEWIDGETS_H
#define IMAGEWIDGETS_H

#include <QImage>
#include <QLabel>
#include <QPointF>
#include <QPixmap>
#include <QRectF>
#include <QSize>
#include <QWidget>

class QMouseEvent;

class ImageLabel : public QLabel
{
public:
    explicit ImageLabel(QWidget *parent = nullptr);

    void setShowTransparencyGrid(bool show);
    void setDrawnPixmap(const QPixmap &pixmap, const QPoint &offset = {});
    void clearDrawnPixmap();
    void setTiledImage(const QImage &image, const QSize &targetSize, Qt::TransformationMode mode);
    bool hasTiledImage(const QImage &image, const QSize &targetSize, Qt::TransformationMode mode) const;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    void paintCheckerboard(QPainter *painter, const QRect &area);

    bool m_showTransparencyGrid = false;
    QPixmap m_drawnPixmap;
    QPoint m_drawnPixmapOffset;
    QImage m_tiledImage;
    QSize m_tiledTargetSize;
    Qt::TransformationMode m_tiledMode = Qt::SmoothTransformation;
};

class ImageOverview : public QWidget
{
    Q_OBJECT

public:
    explicit ImageOverview(QWidget *parent = nullptr);

    void setPreview(const QPixmap &preview);
    void setViewportRect(const QRectF &normalizedRect);

signals:
    void viewportCenterChanged(const QPointF &normalizedCenter);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    QRectF overviewImageRect() const;
    QRectF viewportIndicatorRect() const;
    QPointF normalizedCenterFromPosition(const QPointF &position) const;
    void paintCheckerboard(QPainter *painter, const QRectF &area);

    QPixmap m_preview;
    QRectF m_normalizedViewportRect;
    bool m_draggingViewport = false;
    QPointF m_dragOffset;
};

#endif // IMAGEWIDGETS_H
