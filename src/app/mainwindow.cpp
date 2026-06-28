#include "mainwindow.h"

#include "fileassociationdialog.h"
#include "fileassociationservice.h"
#include "imageinfodialog.h"
#include "imageformats.h"
#include "imageloader.h"
#include "imagetoolbar.h"
#include "imageviewerwidget.h"

#include <QApplication>
#include <QClipboard>
#include <QDesktopServices>
#include <QDir>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QImage>
#include <QKeyEvent>
#include <QMessageBox>
#include <QMimeData>
#include <QMovie>
#include <QPainter>
#include <QPainterPath>
#include <QProcess>
#include <QPushButton>
#include <QResizeEvent>
#include <QStandardPaths>
#include <QStringList>
#include <QStyle>
#include <QSvgRenderer>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrentRun>

#include <algorithm>

namespace {

class LoadingSpinner : public QWidget
{
public:
    explicit LoadingSpinner(QWidget *parent = nullptr)
        : QWidget(parent)
    {
        setFixedSize(48, 48);
        setAttribute(Qt::WA_TransparentForMouseEvents);
        connect(&m_timer, &QTimer::timeout, this, [this] {
            m_angle = (m_angle + 30) % 360;
            update();
        });
        m_timer.start(70);
    }

protected:
    void paintEvent(QPaintEvent *event) override
    {
        Q_UNUSED(event);

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);

        const QRectF rect = QRectF(8, 8, width() - 16, height() - 16);
        painter.setPen(QPen(QColor("#dbe3ec"), 4, Qt::SolidLine, Qt::RoundCap));
        painter.drawArc(rect, 0, 360 * 16);

        painter.setPen(QPen(QColor("#111827"), 4, Qt::SolidLine, Qt::RoundCap));
        painter.drawArc(rect, -m_angle * 16, -110 * 16);
    }

private:
    QTimer m_timer;
    int m_angle = 0;
};

QPixmap createEmptyIllustration(const QSize &size)
{
    QPixmap pixmap(size);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);

    const QRectF canvas(14, 10, size.width() - 28, size.height() - 20);
    painter.setPen(QPen(QColor("#cbd5e1"), 2));
    painter.setBrush(QColor("#f8fafc"));
    painter.drawRoundedRect(canvas, 14, 14);

    const QRectF imageFrame(42, 34, size.width() - 84, size.height() - 62);
    painter.setPen(QPen(QColor("#94a3b8"), 2));
    painter.setBrush(QColor("#ffffff"));
    painter.drawRoundedRect(imageFrame, 8, 8);

    painter.setBrush(QColor("#facc15"));
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(QPointF(imageFrame.left() + 30, imageFrame.top() + 28), 10, 10);

    QPainterPath mountains;
    mountains.moveTo(imageFrame.left() + 18, imageFrame.bottom() - 18);
    mountains.lineTo(imageFrame.left() + 62, imageFrame.top() + 54);
    mountains.lineTo(imageFrame.left() + 94, imageFrame.bottom() - 25);
    mountains.lineTo(imageFrame.left() + 122, imageFrame.top() + 66);
    mountains.lineTo(imageFrame.right() - 14, imageFrame.bottom() - 18);
    mountains.closeSubpath();
    painter.setBrush(QColor("#38bdf8"));
    painter.drawPath(mountains);

    return pixmap;
}

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setupUi();
    m_toastTimer = new QTimer(this);
    m_toastTimer->setSingleShot(true);
    m_toastTimer->setInterval(1800);
    connect(m_toastTimer, &QTimer::timeout, this, [this] {
        if (m_toastLabel) {
            m_toastLabel->hide();
        }
    });
    setAcceptDrops(true);
    setWindowTitle(tr("NGImageViewer"));
    resize(980, 680);
    showEmptyState();

    QTimer::singleShot(0, this, &MainWindow::showMissingFormatWarning);
}

MainWindow::~MainWindow()
{
}

bool MainWindow::openImage(const QString &filePath, bool showErrors)
{
    showMissingFormatWarning();

    const quint64 requestId = m_imageState.beginLoading(filePath);
    showLoadingState(filePath);
    startAsyncImageLoad(filePath, showErrors, requestId);
    return true;
}

void MainWindow::startAsyncImageLoad(const QString &filePath, bool showErrors, quint64 requestId)
{
    auto *watcher = new QFutureWatcher<ImageLoader::LoadResult>(this);
    connect(watcher, &QFutureWatcher<ImageLoader::LoadResult>::finished, this, [this, watcher, showErrors, requestId] {
        const ImageLoader::LoadResult result = watcher->result();
        watcher->deleteLater();
        applyLoadedImage(result, showErrors, requestId);
    });
    watcher->setFuture(QtConcurrent::run([filePath] {
        return ImageLoader::load(filePath);
    }));
}

bool MainWindow::applyLoadedImage(const ImageLoader::LoadResult &result, bool showErrors, quint64 requestId)
{
    if (!m_imageState.isActiveRequest(requestId)) {
        return false;
    }

    if (!result.success) {
        showPostLoadFailureState();
        if (showErrors) {
            showOpenError(result.errorMessage);
        }
        return false;
    }

    switch (result.kind) {
    case ImageLoader::Kind::StaticImage:
        m_viewer->setStaticImage(result.image);
        break;
    case ImageLoader::Kind::RawImage:
        m_viewer->setStaticImage(result.image);
        break;
    case ImageLoader::Kind::HeifImage:
        m_viewer->setStaticImage(result.image);
        break;
    case ImageLoader::Kind::SvgImage:
    {
        auto *renderer = new QSvgRenderer(result.filePath, this);
        if (!renderer->isValid()) {
            renderer->deleteLater();
            showPostLoadFailureState();
            if (showErrors) {
                showOpenError(tr("图片打开失败，请检查文件是否损坏"));
            }
            return false;
        }
        m_viewer->setSvgImage(renderer, result.svgDefaultSize);
        break;
    }
    case ImageLoader::Kind::GifImage:
    {
        auto *movie = new QMovie(result.filePath, QByteArray(), this);
        movie->setCacheMode(QMovie::CacheAll);
        if (!movie->isValid()) {
            movie->deleteLater();
            showPostLoadFailureState();
            if (showErrors) {
                showOpenError(tr("图片打开失败，请检查文件是否损坏"));
            }
            return false;
        }
        m_viewer->setGif(movie);
        break;
    }
    case ImageLoader::Kind::Invalid:
        showPostLoadFailureState();
        if (showErrors) {
            showOpenError(tr("图片打开失败，请检查文件是否损坏"));
        }
        return false;
    }

    ImageDocument document = ImageLoadState::documentFromLoadResult(result);
    document.displayedSize = m_viewer ? m_viewer->transformedImageSize() : QSize();
    m_imageState.completeLoading(document);
    m_stack->setCurrentWidget(m_imagePage);
    m_viewer->focusViewer();
    updateToolbarState();
    if (!result.warningMessage.isEmpty()) {
        showToast(result.warningMessage);
    }
    setWindowTitle(QFileInfo(result.filePath).fileName() + tr(" - NGImageViewer"));
    return true;
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    if (!event->mimeData()->hasUrls()) {
        return;
    }

    const QList<QUrl> urls = event->mimeData()->urls();
    if (!urls.isEmpty() && urls.first().isLocalFile()) {
        event->acceptProposedAction();
        setProperty("dragActive", true);
        style()->unpolish(this);
        style()->polish(this);
    }
}

void MainWindow::dragLeaveEvent(QDragLeaveEvent *event)
{
    QMainWindow::dragLeaveEvent(event);
    setProperty("dragActive", false);
    style()->unpolish(this);
    style()->polish(this);
}

void MainWindow::dropEvent(QDropEvent *event)
{
    setProperty("dragActive", false);
    style()->unpolish(this);
    style()->polish(this);

    const QList<QUrl> urls = event->mimeData()->urls();
    if (urls.isEmpty() || !urls.first().isLocalFile()) {
        return;
    }

    const QString path = urls.first().toLocalFile();
    if (!isSupportedFile(path)) {
        showOpenError(tr("不支持的格式"));
        return;
    }

    openImage(path, true);
    event->acceptProposedAction();
}

bool MainWindow::event(QEvent *event)
{
    if (hasImage() && m_viewer && m_viewer->handleWindowEvent(event)) {
        return true;
    }

    return QMainWindow::event(event);
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    if (handleKeyboardShortcut(event)) {
        return;
    }
    if (hasImage() && m_viewer && m_viewer->handleKeyboardPan(event)) {
        return;
    }

    QMainWindow::keyPressEvent(event);
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    repositionToast();
}

void MainWindow::setupUi()
{
    setFocusPolicy(Qt::StrongFocus);

    auto *central = new QWidget(this);
    central->setObjectName(QStringLiteral("root"));

    auto *layout = new QVBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_stack = new QStackedWidget(central);
    m_emptyPage = createEmptyPage();
    m_loadingPage = createLoadingPage();
    m_imagePage = createImagePage();
    m_stack->addWidget(m_emptyPage);
    m_stack->addWidget(m_loadingPage);
    m_stack->addWidget(m_imagePage);

    layout->addWidget(m_stack, 1);
    layout->addWidget(createToolbar(), 0);
    setCentralWidget(central);

    setStyleSheet(QStringLiteral(R"(
        QMainWindow {
            background: #f8fafc;
        }
        QMainWindow[dragActive="true"] #root {
            border: 2px solid #0ea5e9;
            background: #eef9ff;
        }
        QLabel#emptyTitle {
            color: #111827;
            font-size: 26px;
            font-weight: 700;
        }
        QLabel#emptySubtitle {
            color: #64748b;
            font-size: 14px;
        }
        QPushButton#emptyOpenButton {
            background: #111827;
            border: 1px solid #111827;
            border-radius: 6px;
            color: #ffffff;
            min-height: 34px;
            padding: 0 18px;
        }
        QPushButton#emptyOpenButton:hover {
            background: #1f2937;
        }
        QScrollArea {
            background: #f8fafc;
            border: none;
        }
        QWidget#toolbar {
            background: #ffffff;
            border-top: 1px solid #e2e8f0;
        }
        QToolButton {
            border: 1px solid transparent;
            border-radius: 6px;
            padding: 7px;
        }
        QToolButton:hover:enabled {
            background: #f1f5f9;
            border-color: #dbe3ec;
        }
        QToolButton:pressed:enabled {
            background: #e2e8f0;
        }
        QToolButton:disabled {
            color: #aeb8c5;
        }
        QToolButton::menu-indicator {
            image: none;
            width: 0;
        }
        QFrame#toolbarSeparator {
            color: #e2e8f0;
        }
        QLabel#toast {
            background: rgba(15, 23, 42, 220);
            border-radius: 7px;
            color: #ffffff;
            font-size: 13px;
            padding: 9px 14px;
        }
        QLabel#zoomStatus {
            background: rgba(15, 23, 42, 210);
            border-radius: 7px;
            color: #ffffff;
            font-size: 13px;
            font-weight: 600;
            padding: 7px 11px;
        }
    )"));

    m_toastLabel = new QLabel(central);
    m_toastLabel->setObjectName(QStringLiteral("toast"));
    m_toastLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_toastLabel->hide();
}

QWidget *MainWindow::createEmptyPage()
{
    auto *page = new QWidget(this);
    auto *layout = new QVBoxLayout(page);
    layout->setAlignment(Qt::AlignCenter);
    layout->setSpacing(16);

    m_emptyIllustration = new QLabel(page);
    m_emptyIllustration->setFixedSize(230, 170);
    m_emptyIllustration->setPixmap(createEmptyIllustration(m_emptyIllustration->size()));

    auto *title = new QLabel(tr("NGImageViewer"), page);
    title->setAlignment(Qt::AlignCenter);
    title->setObjectName(QStringLiteral("emptyTitle"));

    auto *subtitle = new QLabel(tr("打开或拖拽图片到窗口开始查看"), page);
    subtitle->setAlignment(Qt::AlignCenter);
    subtitle->setObjectName(QStringLiteral("emptySubtitle"));

    auto *openButton = new QPushButton(tr("打开图片"), page);
    openButton->setObjectName(QStringLiteral("emptyOpenButton"));
    connect(openButton, &QPushButton::clicked, this, &MainWindow::chooseImage);

    layout->addWidget(m_emptyIllustration, 0, Qt::AlignCenter);
    layout->addWidget(title);
    layout->addWidget(subtitle);
    layout->addWidget(openButton, 0, Qt::AlignCenter);
    return page;
}

QWidget *MainWindow::createLoadingPage()
{
    auto *page = new QWidget(this);
    auto *layout = new QVBoxLayout(page);
    layout->setAlignment(Qt::AlignCenter);

    auto *spinner = new LoadingSpinner(page);
    layout->addWidget(spinner, 0, Qt::AlignCenter);
    return page;
}

QWidget *MainWindow::createImagePage()
{
    auto *page = new QWidget(this);
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_viewer = new ImageViewerWidget(page);
    layout->addWidget(m_viewer);
    connect(m_viewer, &ImageViewerWidget::viewStateChanged, this, &MainWindow::updateToolbarState);
    connect(m_viewer, &ImageViewerWidget::previousRequested, this, &MainWindow::openPreviousImage);
    connect(m_viewer, &ImageViewerWidget::nextRequested, this, &MainWindow::openNextImage);

    return page;
}

QWidget *MainWindow::createToolbar()
{
    m_toolbar = new ImageToolbar(this);
    connect(m_toolbar, &ImageToolbar::openRequested, this, &MainWindow::chooseImage);
    connect(m_toolbar, &ImageToolbar::previousRequested, this, &MainWindow::openPreviousImage);
    connect(m_toolbar, &ImageToolbar::nextRequested, this, &MainWindow::openNextImage);
    connect(m_toolbar, &ImageToolbar::navigationUnavailableRequested, this, [this] {
        showToast(tr("没有更多了"));
    });
    connect(m_toolbar, &ImageToolbar::zoomInRequested, this, [this] {
        if (m_viewer) {
            m_viewer->zoomBy(1.25);
        }
    });
    connect(m_toolbar, &ImageToolbar::zoomOutRequested, this, [this] {
        if (m_viewer) {
            m_viewer->zoomBy(0.8);
        }
    });
    connect(m_toolbar, &ImageToolbar::fitRequested, this, [this] {
        if (m_viewer) {
            m_viewer->toggleFitMode();
        }
    });
    connect(m_toolbar, &ImageToolbar::actualSizeRequested, this, [this] {
        if (m_viewer) {
            m_viewer->showActualSize();
        }
    });
    connect(m_toolbar, &ImageToolbar::rotateClockwiseRequested, this, [this] {
        if (m_viewer) {
            m_viewer->rotateBy(90);
        }
    });
    connect(m_toolbar, &ImageToolbar::rotateCounterClockwiseRequested, this, [this] {
        if (m_viewer) {
            m_viewer->rotateBy(-90);
        }
    });
    connect(m_toolbar, &ImageToolbar::deleteRequested, this, &MainWindow::deleteCurrentImage);
    connect(m_toolbar, &ImageToolbar::infoRequested, this, &MainWindow::showImageInfoDialog);
    connect(m_toolbar, &ImageToolbar::copyImageRequested, this, &MainWindow::copyCurrentImageToClipboard);
    connect(m_toolbar, &ImageToolbar::copyPathRequested, this, &MainWindow::copyCurrentImagePathToClipboard);
    connect(m_toolbar, &ImageToolbar::revealRequested, this, &MainWindow::revealCurrentImageInFileManager);
    connect(m_toolbar, &ImageToolbar::associateFormatsRequested, this, &MainWindow::showFileAssociationsDialog);
    connect(m_toolbar, &ImageToolbar::aboutRequested, this, &MainWindow::showAboutDialog);

    updateToolbarState();
    return m_toolbar;
}

bool MainWindow::hasImage() const
{
    return m_imageState.viewerState() == ImageLoadState::ViewerState::ShowingImage
           && m_imageState.hasCurrent()
           && m_viewer
           && m_viewer->hasImage();
}

bool MainWindow::isSupportedFile(const QString &filePath) const
{
    return ImageFormats::isSupportedFile(filePath);
}

QStringList MainWindow::missingRequiredImageFormats() const
{
    return ImageFormats::missingRequiredRuntimeFormats();
}

void MainWindow::chooseImage()
{
    QString startDir;
    const QString currentPath = m_imageState.currentPath();
    if (!currentPath.isEmpty()) {
        startDir = QFileInfo(currentPath).absolutePath();
    } else {
#ifdef Q_OS_WIN
        startDir = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
#else
        startDir = QDir::homePath();
#endif
    }

    const QString filter = ImageFormats::openDialogFilter();
    const QString path = QFileDialog::getOpenFileName(this, tr("打开图片"), startDir, filter);
    if (path.isEmpty()) {
        return;
    }
    openImage(path, true);
}

void MainWindow::showOpenError(const QString &message)
{
    QMessageBox::warning(this, tr("无法打开图片"), message);
}

void MainWindow::showMissingFormatWarning()
{
    if (m_formatWarningShown) {
        return;
    }
    m_formatWarningShown = true;

    const QStringList missing = missingRequiredImageFormats();
    if (missing.isEmpty()) {
        return;
    }

    QMessageBox::warning(
        this,
        tr("图片格式支持不完整"),
        tr("当前 Qt 图片插件缺少以下强验收格式：%1。\n请安装对应 Qt 图片插件后再继续。")
            .arg(missing.join(QStringLiteral(", "))));
}

void MainWindow::showLoadingState(const QString &filePath)
{
    if (m_viewer) {
        m_viewer->hideTransientUi();
    }
    if (m_stack && m_loadingPage) {
        m_stack->setCurrentWidget(m_loadingPage);
    }
    setWindowTitle(QFileInfo(filePath).fileName() + tr(" - NGImageViewer"));
    updateToolbarState();
}

void MainWindow::showPostLoadFailureState()
{
    m_imageState.failLoading();
    if (m_imageState.hasCurrent() && m_viewer && m_viewer->hasImage()) {
        m_stack->setCurrentWidget(m_imagePage);
        setWindowTitle(QFileInfo(m_imageState.currentPath()).fileName() + tr(" - NGImageViewer"));
        m_viewer->focusViewer();
        updateToolbarState();
        return;
    }

    if (m_viewer) {
        m_viewer->clear();
    }
    showEmptyState();
}

void MainWindow::showEmptyState()
{
    m_stack->setCurrentWidget(m_emptyPage);
    setWindowTitle(tr("NGImageViewer"));
    if (m_viewer) {
        m_viewer->hideTransientUi();
    }
    updateToolbarState();
}

void MainWindow::clearCurrentImage()
{
    if (m_viewer) {
        m_viewer->clear();
    }
    m_imageState.clearCurrent();
    updateToolbarState();
}

void MainWindow::updateToolbarState()
{
    if (!m_toolbar) {
        return;
    }
    const bool imageAvailable = hasImage();
    const bool hasPrevious = imageAvailable && m_imageState.sequence().hasPrevious();
    const bool hasNext = imageAvailable && m_imageState.sequence().hasNext();
    if (m_viewer) {
        m_viewer->setNavigationAvailability(hasPrevious, hasNext);
    }
    m_toolbar->setState(imageAvailable,
                        hasPrevious,
                        hasNext,
                        m_viewer ? m_viewer->isFitToWindow() : true);
}

bool MainWindow::handleKeyboardShortcut(QKeyEvent *event)
{
    const Qt::KeyboardModifiers modifiers = event->modifiers();
    const bool commandModifier = modifiers.testFlag(Qt::ControlModifier)
                                 || modifiers.testFlag(Qt::MetaModifier);

    if (commandModifier && event->key() == Qt::Key_O) {
        chooseImage();
        event->accept();
        return true;
    }

    if (!hasImage()) {
        return false;
    }

    switch (event->key()) {
    case Qt::Key_Plus:
    case Qt::Key_Equal:
        if (m_viewer) {
            m_viewer->zoomBy(1.25);
        }
        event->accept();
        return true;
    case Qt::Key_Minus:
        if (m_viewer) {
            m_viewer->zoomBy(0.8);
        }
        event->accept();
        return true;
    case Qt::Key_Delete:
        deleteCurrentImage();
        event->accept();
        return true;
    default:
        return false;
    }
}

void MainWindow::openPreviousImage()
{
    const QString previousPath = m_imageState.sequence().previousPath();
    if (!previousPath.isEmpty()) {
        openImage(previousPath, true);
    }
}

void MainWindow::openNextImage()
{
    const QString nextPath = m_imageState.sequence().nextPath();
    if (!nextPath.isEmpty()) {
        openImage(nextPath, true);
    }
}

void MainWindow::deleteCurrentImage()
{
    if (!hasImage()) {
        return;
    }

    const QString path = m_imageState.currentPath();
    const bool wasGif = m_viewer && m_viewer->isGif();
    const auto answer = QMessageBox::question(
        this,
        tr("删除图片"),
        tr("确定要删除当前图片吗？将优先移动到系统回收站。"),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);
    if (answer != QMessageBox::Yes) {
        return;
    }

    if (wasGif) {
        m_viewer->clear();
    }

    if (QFile::moveToTrash(path)) {
        openNeighborAfterDelete();
        return;
    }

    if (!confirmPermanentDelete(path) && wasGif && QFileInfo::exists(path)) {
        openImage(path, false);
    }
}

bool MainWindow::confirmPermanentDelete(const QString &path)
{
    QMessageBox box(this);
    box.setIcon(QMessageBox::Warning);
    box.setWindowTitle(tr("永久删除图片"));
    box.setTextFormat(Qt::RichText);
    box.setText(tr("无法移动到系统回收站。<br/><span style='color:#d93025;font-weight:700;'>永久</span>删除后无法恢复，是否继续？"));
    box.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    box.setDefaultButton(QMessageBox::No);
    if (box.exec() != QMessageBox::Yes) {
        return false;
    }

    if (!QFile::remove(path)) {
        QMessageBox::warning(this, tr("删除失败"), tr("无法删除该图片，请检查文件权限后重试"));
        return false;
    }

    openNeighborAfterDelete();
    return true;
}

void MainWindow::openNeighborAfterDelete()
{
    const QString nextPath = m_imageState.sequence().nextPathAfterRemovingCurrent(m_imageState.currentPath());
    clearCurrentImage();
    if (!nextPath.isEmpty()) {
        openImage(nextPath, true);
        return;
    }

    showEmptyState();
}

void MainWindow::copyCurrentImageToClipboard()
{
    if (!hasImage()) {
        return;
    }

    const QImage image = m_viewer ? m_viewer->imageForClipboard() : QImage();
    if (!image.isNull()) {
        QApplication::clipboard()->setImage(image);
        showToast(tr("已复制到剪切板"));
    }
}

void MainWindow::copyCurrentImagePathToClipboard()
{
    const QString currentPath = m_imageState.currentPath();
    if (currentPath.isEmpty()) {
        return;
    }

    QApplication::clipboard()->setText(QDir::toNativeSeparators(currentPath));
    showToast(tr("已复制图片路径"));
}

void MainWindow::revealCurrentImageInFileManager()
{
    const QString currentPath = m_imageState.currentPath();
    if (currentPath.isEmpty()) {
        return;
    }

#ifdef Q_OS_MAC
    QProcess::startDetached(QStringLiteral("open"), {QStringLiteral("-R"), currentPath});
#elif defined(Q_OS_WIN)
    QProcess::startDetached(QStringLiteral("explorer.exe"),
                            {QStringLiteral("/select,%1").arg(QDir::toNativeSeparators(currentPath))});
#else
    QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(currentPath).absolutePath()));
#endif
}

void MainWindow::showImageInfoDialog()
{
    if (!hasImage()) {
        return;
    }

    const ImageDocument *document = m_imageState.current();
    if (!document) {
        return;
    }
    const QSize displayedSize = m_viewer ? m_viewer->transformedImageSize() : QSize();
    m_imageState.setCurrentDisplayedSize(displayedSize);

    ImageInfoDialog::Details details;
    details.filePath = document->filePath;
    details.imageSize = displayedSize;
    details.zoomText = m_viewer && m_viewer->isFitToWindow()
                           ? tr("Fit")
                           : tr("%1%").arg(qRound((m_viewer ? m_viewer->scaleFactor() : 1.0) * 100.0));
    details.isRaw = document->isRaw();
    details.isHeif = document->isHeif();
    details.rawDisplaySource = document->raw.displaySource;
    details.rawDecoderInfo = document->raw.decoderInfo;
    details.rawCameraInfo = document->raw.cameraInfo;
    details.rawSourceSize = document->raw.sourceSize;
    details.rawEmbeddedPreviewSize = document->raw.embeddedPreviewSize;
    details.heifDecoderInfo = document->heif.decoderInfo;
    details.heifSourceSize = document->heif.sourceSize;
    details.heifHasAlpha = document->heif.hasAlpha;
    ImageInfoDialog::show(this, details);
}

void MainWindow::showFileAssociationsDialog()
{
    if (!FileAssociationService::isAssociationSupported()) {
        QMessageBox::information(this, tr("关联图片格式"), tr("当前系统不支持自动关联文件格式"));
        return;
    }

    FileAssociationDialog dialog(this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    const FileAssociationService::AssociationResult result =
        FileAssociationService::applyAssociations(dialog.selectedExtensions());
    if (result.success) {
        showToast(result.message);
    } else {
        QMessageBox::warning(this, tr("关联失败"), result.message);
    }
}

void MainWindow::showToast(const QString &message)
{
    if (!m_toastLabel) {
        return;
    }

    m_toastLabel->setText(message);
    m_toastLabel->adjustSize();
    repositionToast();
    m_toastLabel->show();
    m_toastLabel->raise();
    if (m_toastTimer) {
        m_toastTimer->start();
    }
}

void MainWindow::repositionToast()
{
    if (!m_toastLabel || !centralWidget()) {
        return;
    }

    const int x = std::max(12, (centralWidget()->width() - m_toastLabel->width()) / 2);
    const int contentBottom = m_stack ? m_stack->height() : centralWidget()->height();
    const int y = std::max(12, contentBottom - m_toastLabel->height() - 18);
    m_toastLabel->move(x, y);
}

void MainWindow::showAboutDialog()
{
    QMessageBox::about(
        this,
        tr("关于 NGImageViewer"),
        tr("<b>NGImageViewer</b><br/>跨端图片查看器<br/>支持 JPG、PNG、BMP、GIF、WEBP、SVG、HEIC、HEIF 和常见 RAW 格式。"));
}
