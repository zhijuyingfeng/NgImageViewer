#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QImage>
#include <QLabel>
#include <QMainWindow>
#include <QMovie>
#include <QPoint>
#include <QPointF>
#include <QScrollArea>
#include <QStackedWidget>
#include <QToolButton>

class QAction;
class QDragEnterEvent;
class QDragLeaveEvent;
class QDropEvent;
class QEvent;
class QGraphicsOpacityEffect;
class QKeyEvent;
class QMouseEvent;
class QNativeGestureEvent;
class QObject;
class QPropertyAnimation;
class QSvgRenderer;
class QTimer;
class QWheelEvent;
class ImageLabel;
class ImageOverview;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    bool openImage(const QString &filePath, bool showErrors = true);

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragLeaveEvent(QDragLeaveEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    bool event(QEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void setupUi();
    QWidget *createEmptyPage();
    QWidget *createImagePage();
    QWidget *createToolbar();
    QToolButton *createToolButton(const QString &iconPath, const QString &tooltip);

    bool hasImage() const;
    bool isSupportedFile(const QString &filePath) const;
    QStringList missingRequiredImageFormats() const;

    bool openStaticImage(const QString &filePath, bool showErrors);
    bool openRawImage(const QString &filePath, bool showErrors);
    bool openHeifImage(const QString &filePath, bool showErrors);
    bool openSvgImage(const QString &filePath, bool showErrors);
    bool openGif(const QString &filePath, bool showErrors);
    void chooseImage();
    void showOpenError(const QString &message);
    void showMissingFormatWarning();
    void showEmptyState();
    void clearCurrentImage();
    void stopMovie();
    void stopSvgRenderer();
    void clearRawMetadata();
    void clearHeifMetadata();

    void updateToolbarState();
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
    QSize transformedImageSize() const;
    double currentFitScale() const;
    double maximumManualScale() const;
    bool isZoomedBeyondFit() const;
    bool handleKeyboardShortcut(QKeyEvent *event);
    bool handleKeyboardPan(QKeyEvent *event);
    bool handleMousePanEvent(QObject *watched, QEvent *event);
    bool handleWheelZoomEvent(QObject *watched, QWheelEvent *event);
    void zoomBy(double factor);
    void zoomAt(double factor, const QPoint &viewportPosition);
    bool handleNativeZoomGesture(QNativeGestureEvent *event);
    void toggleFitMode();
    void showActualSize();
    void rotateBy(int degrees);

    void rebuildDirectorySequence(const QString &filePath);
    void openPreviousImage();
    void openNextImage();

    void deleteCurrentImage();
    bool confirmPermanentDelete(const QString &path);
    void openNeighborAfterDelete();

    void copyCurrentImageToClipboard();
    void copyCurrentImagePathToClipboard();
    void revealCurrentImageInFileManager();
    void showImageInfoDialog();
    void showToast(const QString &message);
    void repositionToast();
    void showAboutDialog();

    QStackedWidget *m_stack = nullptr;
    QWidget *m_emptyPage = nullptr;
    QWidget *m_imagePage = nullptr;
    QLabel *m_emptyIllustration = nullptr;
    ImageLabel *m_imageLabel = nullptr;
    QLabel *m_toastLabel = nullptr;
    QLabel *m_zoomStatusLabel = nullptr;
    QGraphicsOpacityEffect *m_zoomStatusOpacity = nullptr;
    ImageOverview *m_overviewIndicator = nullptr;
    QScrollArea *m_scrollArea = nullptr;
    QTimer *m_renderTimer = nullptr;
    QTimer *m_qualityRenderTimer = nullptr;
    QTimer *m_toastTimer = nullptr;
    QTimer *m_zoomStatusTimer = nullptr;
    QPropertyAnimation *m_zoomStatusFadeAnimation = nullptr;

    QToolButton *m_zoomInButton = nullptr;
    QToolButton *m_zoomOutButton = nullptr;
    QToolButton *m_fitButton = nullptr;
    QToolButton *m_actualSizeButton = nullptr;
    QToolButton *m_openButton = nullptr;
    QToolButton *m_previousButton = nullptr;
    QToolButton *m_nextButton = nullptr;
    QToolButton *m_rotateCwButton = nullptr;
    QToolButton *m_rotateCcwButton = nullptr;
    QToolButton *m_deleteButton = nullptr;
    QToolButton *m_moreButton = nullptr;
    QAction *m_copyAction = nullptr;
    QAction *m_infoAction = nullptr;
    QAction *m_copyPathAction = nullptr;
    QAction *m_revealAction = nullptr;

    QString m_currentFilePath;
    QImage m_originalImage;
    QMovie *m_movie = nullptr;
    QSvgRenderer *m_svgRenderer = nullptr;
    QSize m_svgDefaultSize;
    bool m_isGif = false;
    bool m_isSvg = false;
    bool m_isRaw = false;
    bool m_isHeif = false;
    QString m_rawDisplaySource;
    QString m_rawDecoderInfo;
    QString m_rawCameraInfo;
    QSize m_rawSourceSize;
    QSize m_rawEmbeddedPreviewSize;
    QString m_heifDecoderInfo;
    QSize m_heifSourceSize;
    bool m_heifHasAlpha = false;
    bool m_fitToWindow = true;
    bool m_formatWarningShown = false;
    double m_scaleFactor = 1.0;
    int m_rotation = 0;
    QStringList m_directoryImages;
    int m_currentIndex = -1;

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
#endif // MAINWINDOW_H
