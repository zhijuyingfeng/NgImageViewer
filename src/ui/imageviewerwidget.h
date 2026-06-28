#ifndef IMAGEVIEWERWIDGET_H
#define IMAGEVIEWERWIDGET_H

#include <QImage>
#include <QPoint>
#include <QPointF>
#include <QPixmap>
#include <QSize>
#include <QWidget>

class ImageLabel;
class ImageOverview;
class QEvent;
class QGraphicsOpacityEffect;
class QKeyEvent;
class QLabel;
class QMovie;
class QNativeGestureEvent;
class QObject;
class QPropertyAnimation;
class QResizeEvent;
class QScrollArea;
class QSvgRenderer;
class QTimer;
class QWheelEvent;

class ImageViewerWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ImageViewerWidget(QWidget *parent = nullptr);
    ~ImageViewerWidget() override;

    bool hasImage() const;
    bool isGif() const;
    bool isFitToWindow() const;
    double scaleFactor() const;

    void setStaticImage(const QImage &image);
    void setSvgImage(QSvgRenderer *renderer, const QSize &defaultSize);
    void setGif(QMovie *movie);
    void clear();
    void hideTransientUi();
    void focusViewer();
    void setNavigationAvailability(bool hasPrevious, bool hasNext);

    QSize transformedImageSize() const;
    QImage imageForClipboard() const;

    void zoomBy(double factor);
    void toggleFitMode();
    void showActualSize();
    void rotateBy(int degrees);
    bool handleKeyboardPan(QKeyEvent *event);
    bool handleWindowEvent(QEvent *event);

signals:
    void viewStateChanged();
    void previousRequested();
    void nextRequested();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void updateImageView();
    bool updateTiledImageView(const QImage &source, const QSize &targetSize, Qt::TransformationMode mode);
    void requestImageViewUpdate();
    void updateZoomStatus(bool restartHideTimer = true);
    void repositionZoomStatus();
    void invalidateOverviewPreview();
    void updateOverviewIndicator();
    void repositionOverviewIndicator();
    void moveViewportFromOverview(const QPointF &normalizedCenter);
    QPixmap createOverviewPixmap(const QSize &targetSize);
    double currentFitScale() const;
    double maximumManualScale() const;
    bool isZoomedBeyondFit() const;
    bool handleMousePanEvent(QObject *watched, QEvent *event);
    bool handleWheelZoomEvent(QObject *watched, QWheelEvent *event);
    void zoomAt(double factor, const QPoint &viewportPosition);
    bool handleNativeZoomGesture(QNativeGestureEvent *event);
    void stopMovie();
    void stopSvgRenderer();
    void resetViewState();

    ImageLabel *m_imageLabel = nullptr;
    ImageOverview *m_overviewIndicator = nullptr;
    QScrollArea *m_scrollArea = nullptr;
    QTimer *m_renderTimer = nullptr;
    QTimer *m_qualityRenderTimer = nullptr;
    QTimer *m_zoomStatusTimer = nullptr;
    QLabel *m_zoomStatusLabel = nullptr;
    QGraphicsOpacityEffect *m_zoomStatusOpacity = nullptr;
    QPropertyAnimation *m_zoomStatusFadeAnimation = nullptr;

    QImage m_originalImage;
    QMovie *m_movie = nullptr;
    QSvgRenderer *m_svgRenderer = nullptr;
    QSize m_svgDefaultSize;
    bool m_isGif = false;
    bool m_isSvg = false;
    bool m_fitToWindow = true;
    double m_scaleFactor = 1.0;
    int m_rotation = 0;
    bool m_hasPrevious = false;
    bool m_hasNext = false;

    bool m_pendingScrollAnchor = false;
    QPointF m_pendingAnchorImage;
    QPoint m_pendingAnchorViewport;
    QPixmap m_overviewPreviewCache;
    QSize m_overviewPreviewCacheSize;
    bool m_overviewPreviewDirty = true;
    QSize m_lastTiledTargetSize;
    Qt::TransformationMode m_lastTiledMode = Qt::SmoothTransformation;
    bool m_mousePanning = false;
    QPoint m_lastPanPosition;
};

#endif // IMAGEVIEWERWIDGET_H
