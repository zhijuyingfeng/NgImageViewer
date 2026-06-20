#ifndef IMAGEWIDGETS_H
#define IMAGEWIDGETS_H

#include <QLabel>
#include <QPixmap>
#include <QRectF>
#include <QWidget>

class ImageLabel : public QLabel
{
public:
    explicit ImageLabel(QWidget *parent = nullptr);

    void setShowTransparencyGrid(bool show);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    bool m_showTransparencyGrid = false;
};

class ImageOverview : public QWidget
{
public:
    explicit ImageOverview(QWidget *parent = nullptr);

    void setPreview(const QPixmap &preview);
    void setViewportRect(const QRectF &normalizedRect);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QRectF overviewImageRect() const;
    void paintCheckerboard(QPainter *painter, const QRectF &area);

    QPixmap m_preview;
    QRectF m_normalizedViewportRect;
};

#endif // IMAGEWIDGETS_H
