#include "mainwindow.h"

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QDir>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QImageReader>
#include <QMenu>
#include <QMessageBox>
#include <QMimeData>
#include <QNativeGestureEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollBar>
#include <QSet>
#include <QStandardPaths>
#include <QStatusBar>
#include <QStyle>
#include <QSvgRenderer>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>

namespace {

const QSet<QString> kRequiredFormats = {
    QStringLiteral("jpg"),
    QStringLiteral("jpeg"),
    QStringLiteral("png"),
    QStringLiteral("bmp"),
    QStringLiteral("gif"),
    QStringLiteral("webp"),
    QStringLiteral("svg"),
};

const QStringList kImageNameFilters = {
    QStringLiteral("*.jpg"),
    QStringLiteral("*.jpeg"),
    QStringLiteral("*.png"),
    QStringLiteral("*.bmp"),
    QStringLiteral("*.gif"),
    QStringLiteral("*.webp"),
    QStringLiteral("*.svg"),
};

QPixmap createEmptyIllustration(const QSize &size)
{
    QPixmap pixmap(size);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);

    const QRectF canvas(10, 12, size.width() - 20, size.height() - 24);
    painter.setPen(QPen(QColor("#cbd5e1"), 2));
    painter.setBrush(QColor("#f8fafc"));
    painter.drawRoundedRect(canvas, 14, 14);

    const QRectF imageFrame(38, 36, size.width() - 76, size.height() - 68);
    painter.setPen(QPen(QColor("#94a3b8"), 2));
    painter.setBrush(QColor("#ffffff"));
    painter.drawRoundedRect(imageFrame, 8, 8);

    painter.setBrush(QColor("#facc15"));
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(QPointF(imageFrame.left() + 30, imageFrame.top() + 24), 9, 9);

    QPainterPath mountains;
    mountains.moveTo(imageFrame.left() + 18, imageFrame.bottom() - 16);
    mountains.lineTo(imageFrame.left() + 58, imageFrame.top() + 44);
    mountains.lineTo(imageFrame.left() + 88, imageFrame.bottom() - 22);
    mountains.lineTo(imageFrame.left() + 112, imageFrame.top() + 54);
    mountains.lineTo(imageFrame.right() - 14, imageFrame.bottom() - 16);
    mountains.closeSubpath();
    painter.setBrush(QColor("#38bdf8"));
    painter.drawPath(mountains);

    return pixmap;
}

QFrame *createSeparator(QWidget *parent)
{
    auto *line = new QFrame(parent);
    line->setFrameShape(QFrame::VLine);
    line->setFixedHeight(22);
    line->setObjectName(QStringLiteral("toolbarSeparator"));
    return line;
}

class ImageLabel : public QLabel
{
public:
    using QLabel::QLabel;

    void setShowTransparencyGrid(bool show)
    {
        if (m_showTransparencyGrid == show) {
            return;
        }
        m_showTransparencyGrid = show;
        update();
    }

protected:
    void paintEvent(QPaintEvent *event) override
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

private:
    bool m_showTransparencyGrid = false;
};

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setupUi();
    setAcceptDrops(true);
    setWindowTitle(tr("NGImageViewer"));
    resize(980, 680);
    showEmptyState();

    QTimer::singleShot(0, this, &MainWindow::showMissingFormatWarning);
}

MainWindow::~MainWindow()
{
    stopMovie();
    stopSvgRenderer();
}

bool MainWindow::openImage(const QString &filePath, bool showErrors)
{
    showMissingFormatWarning();

    const QFileInfo info(filePath);
    if (!info.exists()) {
        if (showErrors) {
            showOpenError(tr("文件不存在或已被移动"));
        }
        return false;
    }
    if (!info.isReadable()) {
        if (showErrors) {
            showOpenError(tr("无法访问该图片，请检查文件权限后重试"));
        }
        return false;
    }
    if (!isSupportedFile(filePath)) {
        if (showErrors) {
            showOpenError(tr("不支持的格式"));
        }
        return false;
    }

    if (info.suffix().compare(QStringLiteral("gif"), Qt::CaseInsensitive) == 0) {
        return openGif(filePath, showErrors);
    }
    if (info.suffix().compare(QStringLiteral("svg"), Qt::CaseInsensitive) == 0) {
        return openSvgImage(filePath, showErrors);
    }
    return openStaticImage(filePath, showErrors);
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
    if (event->type() == QEvent::NativeGesture && hasImage()) {
        auto *nativeGesture = static_cast<QNativeGestureEvent *>(event);
        if (handleNativeZoomGesture(nativeGesture)) {
            return true;
        }
    }

    return QMainWindow::event(event);
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    const bool imageViewportEvent = watched == m_scrollArea
                                    || watched == m_scrollArea->viewport()
                                    || watched == m_imageLabel;
    if (!imageViewportEvent || !hasImage()) {
        return QMainWindow::eventFilter(watched, event);
    }

    if (event->type() == QEvent::NativeGesture) {
        auto *nativeGesture = static_cast<QNativeGestureEvent *>(event);
        if (handleNativeZoomGesture(nativeGesture)) {
            return true;
        }
    }

    if (event->type() != QEvent::Wheel) {
        return QMainWindow::eventFilter(watched, event);
    }

    auto *wheel = static_cast<QWheelEvent *>(event);
    const bool ctrlZoom = wheel->modifiers().testFlag(Qt::ControlModifier);
    const bool mouseWheel = wheel->pixelDelta().isNull() && !wheel->angleDelta().isNull();
    if (!ctrlZoom && !mouseWheel) {
        return QMainWindow::eventFilter(watched, event);
    }

    double steps = 0.0;
    if (ctrlZoom && !wheel->pixelDelta().isNull()) {
        const int delta = wheel->pixelDelta().y() != 0
                              ? wheel->pixelDelta().y()
                              : wheel->pixelDelta().x();
        steps = -delta / 80.0;
    } else if (!wheel->angleDelta().isNull()) {
        const int delta = wheel->angleDelta().y() != 0
                              ? wheel->angleDelta().y()
                              : wheel->angleDelta().x();
        steps = delta / static_cast<double>(QWheelEvent::DefaultDeltasPerStep);
    }

    if (qFuzzyIsNull(steps)) {
        return QMainWindow::eventFilter(watched, event);
    }

    QPoint viewportPosition = m_scrollArea->viewport()->rect().center();
    if (auto *widget = qobject_cast<QWidget *>(watched)) {
        viewportPosition = widget->mapTo(m_scrollArea->viewport(), wheel->position().toPoint());
    }

    zoomAt(std::pow(1.18, steps), viewportPosition);
    wheel->accept();
    return true;
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    if (m_fitToWindow && hasImage()) {
        updateImageView();
    }
}

void MainWindow::setupUi()
{
    auto *central = new QWidget(this);
    central->setObjectName(QStringLiteral("root"));

    auto *layout = new QVBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_stack = new QStackedWidget(central);
    m_emptyPage = createEmptyPage();
    m_imagePage = createImagePage();
    m_stack->addWidget(m_emptyPage);
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
        QFrame#toolbarSeparator {
            color: #e2e8f0;
        }
    )"));
}

QWidget *MainWindow::createEmptyPage()
{
    auto *page = new QWidget(this);
    auto *layout = new QVBoxLayout(page);
    layout->setAlignment(Qt::AlignCenter);
    layout->setSpacing(16);

    m_emptyIllustration = new QLabel(page);
    m_emptyIllustration->setFixedSize(220, 140);
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

QWidget *MainWindow::createImagePage()
{
    auto *page = new QWidget(this);
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_scrollArea = new QScrollArea(page);
    m_scrollArea->setAlignment(Qt::AlignCenter);
    m_scrollArea->setWidgetResizable(false);

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

    return page;
}

QWidget *MainWindow::createToolbar()
{
    auto *toolbar = new QWidget(this);
    toolbar->setObjectName(QStringLiteral("toolbar"));
    toolbar->setFixedHeight(58);

    auto *layout = new QHBoxLayout(toolbar);
    layout->setContentsMargins(12, 8, 12, 8);
    layout->setSpacing(6);

    m_openButton = createToolButton(QStringLiteral(":/icons/resources/icons/open-file.svg"), tr("打开图片"));
    m_previousButton = createToolButton(QStringLiteral(":/icons/resources/icons/previous.svg"), tr("上一张"));
    m_nextButton = createToolButton(QStringLiteral(":/icons/resources/icons/next.svg"), tr("下一张"));
    m_zoomOutButton = createToolButton(QStringLiteral(":/icons/resources/icons/zoom-out.svg"), tr("缩小"));
    m_zoomInButton = createToolButton(QStringLiteral(":/icons/resources/icons/zoom-in.svg"), tr("放大"));
    m_fitButton = createToolButton(QStringLiteral(":/icons/resources/icons/fit-to-window.svg"), tr("适配窗口"));
    m_actualSizeButton = createToolButton(QStringLiteral(":/icons/resources/icons/actual-size.svg"), tr("原始比例"));
    m_rotateCcwButton = createToolButton(QStringLiteral(":/icons/resources/icons/rotate-ccw.svg"), tr("逆时针旋转"));
    m_rotateCwButton = createToolButton(QStringLiteral(":/icons/resources/icons/rotate-cw.svg"), tr("顺时针旋转"));
    m_deleteButton = createToolButton(QStringLiteral(":/icons/resources/icons/delete.svg"), tr("删除"));
    m_moreButton = createToolButton(QStringLiteral(":/icons/resources/icons/more.svg"), tr("更多"));

    layout->addStretch(1);
    layout->addWidget(m_openButton);
    layout->addWidget(createSeparator(toolbar));
    layout->addWidget(m_previousButton);
    layout->addWidget(m_nextButton);
    layout->addWidget(createSeparator(toolbar));
    layout->addWidget(m_zoomOutButton);
    layout->addWidget(m_zoomInButton);
    layout->addWidget(m_fitButton);
    layout->addWidget(m_actualSizeButton);
    layout->addWidget(createSeparator(toolbar));
    layout->addWidget(m_rotateCcwButton);
    layout->addWidget(m_rotateCwButton);
    layout->addWidget(createSeparator(toolbar));
    layout->addWidget(m_deleteButton);
    layout->addWidget(m_moreButton);
    layout->addStretch(1);

    connect(m_openButton, &QToolButton::clicked, this, &MainWindow::chooseImage);
    connect(m_previousButton, &QToolButton::clicked, this, &MainWindow::openPreviousImage);
    connect(m_nextButton, &QToolButton::clicked, this, &MainWindow::openNextImage);
    connect(m_zoomInButton, &QToolButton::clicked, this, [this] { zoomBy(1.25); });
    connect(m_zoomOutButton, &QToolButton::clicked, this, [this] { zoomBy(0.8); });
    connect(m_fitButton, &QToolButton::clicked, this, &MainWindow::toggleFitMode);
    connect(m_actualSizeButton, &QToolButton::clicked, this, &MainWindow::showActualSize);
    connect(m_rotateCwButton, &QToolButton::clicked, this, [this] { rotateBy(90); });
    connect(m_rotateCcwButton, &QToolButton::clicked, this, [this] { rotateBy(-90); });
    connect(m_deleteButton, &QToolButton::clicked, this, &MainWindow::deleteCurrentImage);

    auto *menu = new QMenu(this);
    m_copyAction = menu->addAction(tr("复制到剪切板"));
    auto *aboutAction = menu->addAction(tr("关于 NGImageViewer"));
    m_moreButton->setMenu(menu);
    m_moreButton->setPopupMode(QToolButton::InstantPopup);
    connect(m_copyAction, &QAction::triggered, this, &MainWindow::copyCurrentImageToClipboard);
    connect(aboutAction, &QAction::triggered, this, &MainWindow::showAboutDialog);

    updateToolbarState();
    return toolbar;
}

QToolButton *MainWindow::createToolButton(const QString &iconPath, const QString &tooltip)
{
    auto *button = new QToolButton(this);
    button->setIcon(QIcon(iconPath));
    button->setIconSize(QSize(22, 22));
    button->setToolTip(tooltip);
    button->setAccessibleName(tooltip);
    button->setFixedSize(38, 38);
    button->setAutoRaise(true);
    return button;
}

bool MainWindow::hasImage() const
{
    if (m_currentFilePath.isEmpty()) {
        return false;
    }
    if (m_isSvg) {
        return m_svgRenderer != nullptr && m_svgRenderer->isValid();
    }
    return m_isGif ? m_movie != nullptr : !m_originalImage.isNull();
}

bool MainWindow::isSupportedFile(const QString &filePath) const
{
    const QString suffix = QFileInfo(filePath).suffix().toLower();
    return kRequiredFormats.contains(suffix);
}

QStringList MainWindow::missingRequiredImageFormats() const
{
    QSet<QString> available;
    const QList<QByteArray> supportedFormats = QImageReader::supportedImageFormats();
    for (const QByteArray &format : supportedFormats) {
        available.insert(QString::fromLatin1(format).toLower());
    }

    QStringList missing;
    for (const QString &format : {QStringLiteral("gif"), QStringLiteral("webp"), QStringLiteral("svg")}) {
        if (!available.contains(format)) {
            missing << format;
        }
    }
    return missing;
}

bool MainWindow::openStaticImage(const QString &filePath, bool showErrors)
{
    QImageReader reader(filePath);
    reader.setAutoTransform(true);
    const QImage image = reader.read();
    if (image.isNull()) {
        if (showErrors) {
            showOpenError(tr("图片打开失败，请检查文件是否损坏"));
        }
        return false;
    }

    stopMovie();
    stopSvgRenderer();
    m_isGif = false;
    m_isSvg = false;
    m_originalImage = image;
    m_currentFilePath = filePath;
    m_rotation = 0;
    m_fitToWindow = true;
    m_scaleFactor = 1.0;
    rebuildDirectorySequence(filePath);
    m_stack->setCurrentWidget(m_imagePage);
    updateImageView();
    QTimer::singleShot(0, this, &MainWindow::updateImageView);
    updateToolbarState();
    setWindowTitle(QFileInfo(filePath).fileName() + tr(" - NGImageViewer"));
    return true;
}

bool MainWindow::openSvgImage(const QString &filePath, bool showErrors)
{
    auto *renderer = new QSvgRenderer(filePath, this);
    if (!renderer->isValid()) {
        renderer->deleteLater();
        if (showErrors) {
            showOpenError(tr("图片打开失败，请检查文件是否损坏"));
        }
        return false;
    }

    QSize defaultSize = renderer->defaultSize();
    if (defaultSize.isEmpty()) {
        const QRectF viewBox = renderer->viewBoxF();
        if (viewBox.isValid() && !viewBox.isEmpty()) {
            defaultSize = viewBox.size().toSize();
        }
    }
    if (defaultSize.isEmpty()) {
        defaultSize = QSize(1024, 1024);
    }

    stopMovie();
    stopSvgRenderer();
    m_svgRenderer = renderer;
    m_svgDefaultSize = defaultSize;
    m_isSvg = true;
    m_isGif = false;
    m_originalImage = QImage();
    m_currentFilePath = filePath;
    m_rotation = 0;
    m_fitToWindow = true;
    m_scaleFactor = 1.0;
    rebuildDirectorySequence(filePath);
    m_stack->setCurrentWidget(m_imagePage);
    updateImageView();
    QTimer::singleShot(0, this, &MainWindow::updateImageView);
    updateToolbarState();
    setWindowTitle(QFileInfo(filePath).fileName() + tr(" - NGImageViewer"));
    return true;
}

bool MainWindow::openGif(const QString &filePath, bool showErrors)
{
    auto *movie = new QMovie(filePath, QByteArray(), this);
    movie->setCacheMode(QMovie::CacheAll);
    if (!movie->isValid()) {
        movie->deleteLater();
        if (showErrors) {
            showOpenError(tr("图片打开失败，请检查文件是否损坏"));
        }
        return false;
    }

    stopMovie();
    stopSvgRenderer();
    m_movie = movie;
    m_isGif = true;
    m_isSvg = false;
    m_originalImage = QImage();
    m_currentFilePath = filePath;
    m_rotation = 0;
    m_fitToWindow = true;
    m_scaleFactor = 1.0;

    connect(m_movie, &QMovie::frameChanged, this, [this] { updateImageView(); });
    m_movie->start();
    rebuildDirectorySequence(filePath);
    m_stack->setCurrentWidget(m_imagePage);
    updateImageView();
    QTimer::singleShot(0, this, &MainWindow::updateImageView);
    updateToolbarState();
    setWindowTitle(QFileInfo(filePath).fileName() + tr(" - NGImageViewer"));
    return true;
}

void MainWindow::chooseImage()
{
    QString startDir;
    if (!m_currentFilePath.isEmpty()) {
        startDir = QFileInfo(m_currentFilePath).absolutePath();
    } else {
#ifdef Q_OS_WIN
        startDir = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
#else
        startDir = QDir::homePath();
#endif
    }

    const QString filter = tr("Images (*.jpg *.jpeg *.png *.bmp *.gif *.webp *.svg)");
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

void MainWindow::showEmptyState()
{
    m_stack->setCurrentWidget(m_emptyPage);
    setWindowTitle(tr("NGImageViewer"));
    updateToolbarState();
}

void MainWindow::clearCurrentImage()
{
    stopMovie();
    stopSvgRenderer();
    m_isGif = false;
    m_isSvg = false;
    m_originalImage = QImage();
    m_currentFilePath.clear();
    m_rotation = 0;
    m_fitToWindow = true;
    m_scaleFactor = 1.0;
    m_directoryImages.clear();
    m_currentIndex = -1;
    m_svgDefaultSize = QSize();
    if (m_imageLabel) {
        m_imageLabel->clear();
        static_cast<ImageLabel *>(m_imageLabel)->setShowTransparencyGrid(false);
        m_imageLabel->resize(0, 0);
    }
    updateToolbarState();
}

void MainWindow::stopMovie()
{
    if (!m_movie) {
        return;
    }
    m_movie->stop();
    m_movie->disconnect(this);
    delete m_movie;
    m_movie = nullptr;
}

void MainWindow::stopSvgRenderer()
{
    if (!m_svgRenderer) {
        return;
    }
    delete m_svgRenderer;
    m_svgRenderer = nullptr;
    m_svgDefaultSize = QSize();
}

void MainWindow::updateToolbarState()
{
    const bool imageAvailable = hasImage();
    m_openButton->setEnabled(true);
    m_zoomInButton->setEnabled(imageAvailable);
    m_zoomOutButton->setEnabled(imageAvailable);
    m_fitButton->setEnabled(imageAvailable);
    m_actualSizeButton->setEnabled(imageAvailable);
    m_rotateCwButton->setEnabled(imageAvailable);
    m_rotateCcwButton->setEnabled(imageAvailable);
    m_deleteButton->setEnabled(imageAvailable);
    m_previousButton->setEnabled(imageAvailable && m_currentIndex > 0);
    m_nextButton->setEnabled(imageAvailable && m_currentIndex >= 0
                             && m_currentIndex + 1 < m_directoryImages.size());
    m_moreButton->setEnabled(true);
    if (m_copyAction) {
        m_copyAction->setEnabled(imageAvailable);
    }
    if (m_fitButton) {
        m_fitButton->setToolTip(m_fitToWindow ? tr("切换到原始比例") : tr("适配窗口"));
    }
}

void MainWindow::updateImageView()
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

        m_imageLabel->setPixmap(QPixmap::fromImage(display));
        static_cast<ImageLabel *>(m_imageLabel)->setShowTransparencyGrid(true);
        m_imageLabel->resize(display.size());
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

    QTransform transform;
    transform.rotate(m_rotation);
    const QImage display = source.transformed(transform, Qt::SmoothTransformation);

    QSize targetSize = display.size();
    if (m_fitToWindow) {
        QSize viewport = m_scrollArea->viewport()->size() - QSize(24, 24);
        viewport = viewport.expandedTo(QSize(1, 1));
        targetSize.scale(viewport, Qt::KeepAspectRatio);
    } else {
        targetSize = QSize(
            std::max(1, qRound(display.width() * m_scaleFactor)),
            std::max(1, qRound(display.height() * m_scaleFactor)));
    }

    const QPixmap pixmap = QPixmap::fromImage(display).scaled(
        targetSize,
        Qt::KeepAspectRatio,
        Qt::SmoothTransformation);
    m_imageLabel->setPixmap(pixmap);
    static_cast<ImageLabel *>(m_imageLabel)->setShowTransparencyGrid(display.hasAlphaChannel());
    m_imageLabel->resize(pixmap.size());
}

QSize MainWindow::transformedImageSize() const
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

double MainWindow::currentFitScale() const
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

double MainWindow::maximumManualScale() const
{
    const double baseLimit = m_isSvg ? 256.0 : 20.0;
    return std::max(baseLimit, currentFitScale() * 4.0);
}

void MainWindow::zoomBy(double factor)
{
    if (!hasImage()) {
        return;
    }
    zoomAt(factor, m_scrollArea->viewport()->rect().center());
}

void MainWindow::zoomAt(double factor, const QPoint &viewportPosition)
{
    if (!hasImage()) {
        return;
    }

    const double oldScale = m_fitToWindow ? currentFitScale() : m_scaleFactor;
    const QPoint labelPosition = m_imageLabel->pos();
    const QPointF imageAnchor = (QPointF(viewportPosition - labelPosition)) / oldScale;

    m_fitToWindow = false;
    m_scaleFactor = std::clamp(oldScale * factor, 0.05, maximumManualScale());
    updateImageView();

    const QPointF newAnchor = imageAnchor * m_scaleFactor;
    m_scrollArea->horizontalScrollBar()->setValue(qRound(newAnchor.x() - viewportPosition.x()));
    m_scrollArea->verticalScrollBar()->setValue(qRound(newAnchor.y() - viewportPosition.y()));
    updateToolbarState();
}

bool MainWindow::handleNativeZoomGesture(QNativeGestureEvent *event)
{
    if (event->gestureType() != Qt::ZoomNativeGesture || qFuzzyIsNull(event->value())) {
        return false;
    }

    const QPoint viewportPosition = m_scrollArea->viewport()->mapFromGlobal(event->globalPosition().toPoint());
    zoomAt(std::exp(event->value()), viewportPosition);
    event->accept();
    return true;
}

void MainWindow::toggleFitMode()
{
    if (!hasImage()) {
        return;
    }
    m_fitToWindow = !m_fitToWindow;
    if (!m_fitToWindow) {
        m_scaleFactor = 1.0;
    }
    updateImageView();
    updateToolbarState();
}

void MainWindow::showActualSize()
{
    if (!hasImage()) {
        return;
    }
    m_fitToWindow = false;
    m_scaleFactor = 1.0;
    updateImageView();
    updateToolbarState();
}

void MainWindow::rotateBy(int degrees)
{
    if (!hasImage()) {
        return;
    }
    m_rotation = (m_rotation + degrees) % 360;
    if (m_rotation < 0) {
        m_rotation += 360;
    }
    updateImageView();
}

void MainWindow::rebuildDirectorySequence(const QString &filePath)
{
    const QFileInfo current(filePath);
    QDir dir(current.absolutePath());
    const QFileInfoList files = dir.entryInfoList(
        kImageNameFilters,
        QDir::Files | QDir::Readable,
        QDir::Name | QDir::IgnoreCase);

    m_directoryImages.clear();
    for (const QFileInfo &file : files) {
        m_directoryImages << file.absoluteFilePath();
    }
    m_currentIndex = m_directoryImages.indexOf(current.absoluteFilePath());
}

void MainWindow::openPreviousImage()
{
    if (m_currentIndex > 0) {
        openImage(m_directoryImages.at(m_currentIndex - 1), true);
    }
}

void MainWindow::openNextImage()
{
    if (m_currentIndex >= 0 && m_currentIndex + 1 < m_directoryImages.size()) {
        openImage(m_directoryImages.at(m_currentIndex + 1), true);
    }
}

void MainWindow::deleteCurrentImage()
{
    if (!hasImage()) {
        return;
    }

    const QString path = m_currentFilePath;
    const bool wasGif = m_isGif;
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
        stopMovie();
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
    const int oldIndex = m_currentIndex;
    const QString oldDir = QFileInfo(m_currentFilePath).absolutePath();
    rebuildDirectorySequence(QDir(oldDir).absoluteFilePath(QStringLiteral("__deleted__")));

    if (!m_directoryImages.isEmpty()) {
        const int lastIndex = static_cast<int>(m_directoryImages.size()) - 1;
        const int nextIndex = std::clamp(oldIndex, 0, lastIndex);
        openImage(m_directoryImages.at(nextIndex), true);
        return;
    }

    clearCurrentImage();
    showEmptyState();
}

void MainWindow::copyCurrentImageToClipboard()
{
    if (!hasImage()) {
        return;
    }

    QImage image;
    if (m_isSvg && m_svgRenderer) {
        const QSize imageSize = m_svgDefaultSize.isEmpty() ? QSize(1024, 1024) : m_svgDefaultSize;
        image = QImage(imageSize, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        QPainter painter(&image);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setRenderHint(QPainter::TextAntialiasing);
        painter.setRenderHint(QPainter::SmoothPixmapTransform);
        m_svgRenderer->render(&painter, QRectF(QPointF(0, 0), QSizeF(imageSize)));
    } else if (m_isGif && m_movie) {
        image = m_movie->currentImage();
    } else {
        image = m_originalImage;
    }

    if (!image.isNull()) {
        QApplication::clipboard()->setImage(image);
        statusBar()->showMessage(tr("已复制到剪切板"), 1800);
    }
}

void MainWindow::showAboutDialog()
{
    QMessageBox::about(
        this,
        tr("关于 NGImageViewer"),
        tr("<b>NGImageViewer</b><br/>跨端图片查看器<br/>支持 JPG、PNG、BMP、GIF、WEBP、SVG。"));
}
