#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "imagesequence.h"

#include <QLabel>
#include <QMainWindow>
#include <QStackedWidget>

class QDragEnterEvent;
class QDragLeaveEvent;
class QDropEvent;
class QEvent;
class QKeyEvent;
class QObject;
class QResizeEvent;
class QTimer;
class ImageToolbar;
class ImageViewerWidget;

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
    void keyPressEvent(QKeyEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void setupUi();
    QWidget *createEmptyPage();
    QWidget *createImagePage();
    QWidget *createToolbar();

    bool hasImage() const;
    bool isSupportedFile(const QString &filePath) const;
    QStringList missingRequiredImageFormats() const;

    void chooseImage();
    void showOpenError(const QString &message);
    void showMissingFormatWarning();
    void showEmptyState();
    void clearCurrentImage();
    void clearRawMetadata();
    void clearHeifMetadata();

    void updateToolbarState();
    bool handleKeyboardShortcut(QKeyEvent *event);

    void openPreviousImage();
    void openNextImage();

    void deleteCurrentImage();
    bool confirmPermanentDelete(const QString &path);
    void openNeighborAfterDelete();

    void copyCurrentImageToClipboard();
    void copyCurrentImagePathToClipboard();
    void revealCurrentImageInFileManager();
    void showImageInfoDialog();
    void showFileAssociationsDialog();
    void showToast(const QString &message);
    void repositionToast();
    void showAboutDialog();

    QStackedWidget *m_stack = nullptr;
    QWidget *m_emptyPage = nullptr;
    QWidget *m_imagePage = nullptr;
    QLabel *m_emptyIllustration = nullptr;
    ImageViewerWidget *m_viewer = nullptr;
    QLabel *m_toastLabel = nullptr;
    QTimer *m_toastTimer = nullptr;

    ImageToolbar *m_toolbar = nullptr;

    QString m_currentFilePath;
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
    bool m_formatWarningShown = false;
    ImageSequence m_sequence;
};
#endif // MAINWINDOW_H
