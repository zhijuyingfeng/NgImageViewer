#include "mainwindow.h"

#include "imageformats.h"
#include "imagewidgets.h"
#include "rawdecoder.h"

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QGraphicsOpacityEffect>
#include <QHBoxLayout>
#include <QImageReader>
#include <QInputDevice>
#include <QKeyEvent>
#include <QLocale>
#include <QMenu>
#include <QMessageBox>
#include <QMimeData>
#include <QMouseEvent>
#include <QNativeGestureEvent>
#include <QPainter>
#include <QPainterPath>
#include <QProcess>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QEasingCurve>
#include <QResizeEvent>
#include <QScrollBar>
#include <QStandardPaths>
#include <QStyle>
#include <QSvgRenderer>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>

namespace {

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

QFrame *createSeparator(QWidget *parent)
{
    auto *line = new QFrame(parent);
    line->setFrameShape(QFrame::VLine);
    line->setFixedHeight(22);
    line->setObjectName(QStringLiteral("toolbarSeparator"));
    return line;
}

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setupUi();
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
    connect(m_qualityRenderTimer, &QTimer::timeout, this, &MainWindow::updateImageView);
    m_toastTimer = new QTimer(this);
    m_toastTimer->setSingleShot(true);
    m_toastTimer->setInterval(1800);
    connect(m_toastTimer, &QTimer::timeout, this, [this] {
        if (m_toastLabel) {
            m_toastLabel->hide();
        }
    });
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

    if (ImageFormats::isRawFile(filePath)) {
        return openRawImage(filePath, showErrors);
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

    if (event->type() == QEvent::KeyPress) {
        auto *keyEvent = static_cast<QKeyEvent *>(event);
        if (handleKeyboardShortcut(keyEvent)) {
            return true;
        }
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
        return QMainWindow::eventFilter(watched, event);
    }

    auto *wheel = static_cast<QWheelEvent *>(event);
    const auto *device = wheel->pointingDevice();
    const bool fromTouchPad = device && device->type() == QInputDevice::DeviceType::TouchPad;
    const bool ctrlZoom = wheel->modifiers().testFlag(Qt::ControlModifier);
    const bool mouseVerticalWheel = !fromTouchPad
                                    && (wheel->angleDelta().y() != 0
                                        || wheel->pixelDelta().y() != 0);
    if (!ctrlZoom && !mouseVerticalWheel) {
        return QMainWindow::eventFilter(watched, event);
    }

    double steps = 0.0;
    if (ctrlZoom && !wheel->pixelDelta().isNull()) {
        const int delta = wheel->pixelDelta().y() != 0
                              ? wheel->pixelDelta().y()
                              : wheel->pixelDelta().x();
        steps = -delta / 80.0;
    } else if (ctrlZoom && wheel->angleDelta().y() != 0) {
        steps = wheel->angleDelta().y() / static_cast<double>(QWheelEvent::DefaultDeltasPerStep);
    } else if (mouseVerticalWheel && wheel->angleDelta().y() != 0) {
        steps = wheel->angleDelta().y() / static_cast<double>(QWheelEvent::DefaultDeltasPerStep);
    } else if (mouseVerticalWheel && wheel->pixelDelta().y() != 0) {
        steps = wheel->pixelDelta().y() / 80.0;
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

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    if (handleKeyboardShortcut(event)) {
        return;
    }
    if (handleKeyboardPan(event)) {
        return;
    }

    QMainWindow::keyPressEvent(event);
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    if (m_fitToWindow && hasImage()) {
        updateImageView();
    }
    repositionOverviewIndicator();
    updateOverviewIndicator();
    repositionToast();
    repositionZoomStatus();
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

    m_zoomStatusLabel = new QLabel(central);
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

QWidget *MainWindow::createImagePage()
{
    auto *page = new QWidget(this);
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_scrollArea = new QScrollArea(page);
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

    m_overviewIndicator = new ImageOverview(page);
    m_overviewIndicator->hide();
    m_overviewIndicator->raise();
    connect(m_overviewIndicator, &ImageOverview::viewportCenterChanged,
            this, &MainWindow::moveViewportFromOverview);
    connect(m_scrollArea->horizontalScrollBar(), &QScrollBar::valueChanged,
            this, &MainWindow::updateOverviewIndicator);
    connect(m_scrollArea->verticalScrollBar(), &QScrollBar::valueChanged,
            this, &MainWindow::updateOverviewIndicator);

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
    m_infoAction = menu->addAction(tr("更多信息"));
    m_copyAction = menu->addAction(tr("复制到剪切板"));
    m_copyPathAction = menu->addAction(tr("复制图片路径"));
#ifdef Q_OS_MAC
    m_revealAction = menu->addAction(tr("在 Finder 中显示"));
#elif defined(Q_OS_WIN)
    m_revealAction = menu->addAction(tr("在 Windows 文件资源管理器中显示"));
#else
    m_revealAction = menu->addAction(tr("在文件管理器中显示"));
#endif
    menu->addSeparator();
    auto *aboutAction = menu->addAction(tr("关于 NGImageViewer"));
    m_moreButton->setMenu(menu);
    m_moreButton->setPopupMode(QToolButton::InstantPopup);
    connect(m_infoAction, &QAction::triggered, this, &MainWindow::showImageInfoDialog);
    connect(m_copyAction, &QAction::triggered, this, &MainWindow::copyCurrentImageToClipboard);
    connect(m_copyPathAction, &QAction::triggered, this, &MainWindow::copyCurrentImagePathToClipboard);
    connect(m_revealAction, &QAction::triggered, this, &MainWindow::revealCurrentImageInFileManager);
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
    button->setFocusPolicy(Qt::NoFocus);
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
    return ImageFormats::isSupportedFile(filePath);
}

QStringList MainWindow::missingRequiredImageFormats() const
{
    return ImageFormats::missingRequiredRuntimeFormats();
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
    invalidateOverviewPreview();
    m_isGif = false;
    m_isSvg = false;
    m_originalImage = image;
    m_currentFilePath = filePath;
    m_rotation = 0;
    m_fitToWindow = true;
    m_scaleFactor = 1.0;
    m_pendingScrollAnchor = false;
    rebuildDirectorySequence(filePath);
    m_stack->setCurrentWidget(m_imagePage);
    m_scrollArea->setFocus(Qt::OtherFocusReason);
    updateImageView();
    QTimer::singleShot(0, this, &MainWindow::updateImageView);
    updateToolbarState();
    updateZoomStatus();
    setWindowTitle(QFileInfo(filePath).fileName() + tr(" - NGImageViewer"));
    return true;
}

bool MainWindow::openRawImage(const QString &filePath, bool showErrors)
{
    if (!RawDecoder::isAvailable()) {
        if (showErrors) {
            showOpenError(RawDecoder::unavailableMessage());
        }
        return false;
    }

    const RawDecoder::DecodeResult result = RawDecoder::decode(filePath);
    if (result.image.isNull()) {
        if (showErrors) {
            showOpenError(result.errorMessage.isEmpty()
                              ? tr("RAW 图片打开失败，请检查文件是否损坏")
                              : result.errorMessage);
        }
        return false;
    }

    stopMovie();
    stopSvgRenderer();
    invalidateOverviewPreview();
    m_isGif = false;
    m_isSvg = false;
    m_originalImage = result.image;
    m_currentFilePath = filePath;
    m_rotation = 0;
    m_fitToWindow = true;
    m_scaleFactor = 1.0;
    m_pendingScrollAnchor = false;
    rebuildDirectorySequence(filePath);
    m_stack->setCurrentWidget(m_imagePage);
    m_scrollArea->setFocus(Qt::OtherFocusReason);
    updateImageView();
    QTimer::singleShot(0, this, &MainWindow::updateImageView);
    updateToolbarState();
    updateZoomStatus();
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
    invalidateOverviewPreview();
    m_svgRenderer = renderer;
    m_svgDefaultSize = defaultSize;
    m_isSvg = true;
    m_isGif = false;
    m_originalImage = QImage();
    m_currentFilePath = filePath;
    m_rotation = 0;
    m_fitToWindow = true;
    m_scaleFactor = 1.0;
    m_pendingScrollAnchor = false;
    rebuildDirectorySequence(filePath);
    m_stack->setCurrentWidget(m_imagePage);
    m_scrollArea->setFocus(Qt::OtherFocusReason);
    updateImageView();
    QTimer::singleShot(0, this, &MainWindow::updateImageView);
    updateToolbarState();
    updateZoomStatus();
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
    m_pendingScrollAnchor = false;

    connect(m_movie, &QMovie::frameChanged, this, [this] {
        invalidateOverviewPreview();
        updateImageView();
    });
    m_movie->start();
    rebuildDirectorySequence(filePath);
    m_stack->setCurrentWidget(m_imagePage);
    m_scrollArea->setFocus(Qt::OtherFocusReason);
    updateImageView();
    QTimer::singleShot(0, this, &MainWindow::updateImageView);
    updateToolbarState();
    updateZoomStatus();
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

void MainWindow::showEmptyState()
{
    m_stack->setCurrentWidget(m_emptyPage);
    setWindowTitle(tr("NGImageViewer"));
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
    updateToolbarState();
}

void MainWindow::clearCurrentImage()
{
    stopMovie();
    stopSvgRenderer();
    invalidateOverviewPreview();
    m_isGif = false;
    m_isSvg = false;
    m_originalImage = QImage();
    m_currentFilePath.clear();
    m_rotation = 0;
    m_fitToWindow = true;
    m_scaleFactor = 1.0;
    m_pendingScrollAnchor = false;
    m_directoryImages.clear();
    m_currentIndex = -1;
    m_svgDefaultSize = QSize();
    if (m_imageLabel) {
        m_imageLabel->clear();
        m_imageLabel->setShowTransparencyGrid(false);
        m_imageLabel->resize(0, 0);
    }
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
    if (m_infoAction) {
        m_infoAction->setEnabled(imageAvailable);
    }
    if (m_copyPathAction) {
        m_copyPathAction->setEnabled(imageAvailable);
    }
    if (m_revealAction) {
        m_revealAction->setEnabled(imageAvailable);
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

    QTransform transform;
    transform.rotate(m_rotation);
    const QImage display = source.transformed(transform, Qt::SmoothTransformation);

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
                                                          ? Qt::FastTransformation
                                                          : Qt::SmoothTransformation;
    const QPixmap pixmap = QPixmap::fromImage(display).scaled(
        targetSize,
        Qt::KeepAspectRatio,
        transformationMode);
    m_imageLabel->setPixmap(pixmap);
    m_imageLabel->setShowTransparencyGrid(display.hasAlphaChannel());
    m_imageLabel->resize(pixmap.size());
    updateOverviewIndicator();
    updateZoomStatus(false);
}

void MainWindow::requestImageViewUpdate()
{
    if (!m_renderTimer) {
        updateImageView();
        return;
    }

    if (!m_renderTimer->isActive()) {
        m_renderTimer->start();
    }
}

void MainWindow::updateZoomStatus(bool restartHideTimer)
{
    if (!m_zoomStatusLabel || !hasImage()) {
        return;
    }

    if (m_fitToWindow) {
        m_zoomStatusLabel->setText(tr("Fit"));
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

void MainWindow::repositionZoomStatus()
{
    if (!m_zoomStatusLabel || !centralWidget()) {
        return;
    }

    const int margin = 14;
    const int contentBottom = m_stack ? m_stack->height() : centralWidget()->height();
    const int y = std::max(margin, contentBottom - m_zoomStatusLabel->height() - margin);
    m_zoomStatusLabel->move(margin, y);
}

void MainWindow::invalidateOverviewPreview()
{
    m_overviewPreviewCache = QPixmap();
    m_overviewPreviewCacheSize = QSize();
    m_overviewPreviewDirty = true;
}

void MainWindow::updateOverviewIndicator()
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

void MainWindow::repositionOverviewIndicator()
{
    if (!m_overviewIndicator || !m_imagePage) {
        return;
    }

    const int margin = 18;
    m_overviewIndicator->move(
        std::max(margin, m_imagePage->width() - m_overviewIndicator->width() - margin),
        margin);
}

void MainWindow::moveViewportFromOverview(const QPointF &normalizedCenter)
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

QPixmap MainWindow::createOverviewPixmap(const QSize &targetSize)
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

    QTransform transform;
    transform.rotate(m_rotation);
    const QImage display = source.transformed(transform, Qt::SmoothTransformation);
    m_overviewPreviewCache = QPixmap::fromImage(display).scaled(
        previewSize,
        Qt::KeepAspectRatio,
        Qt::SmoothTransformation);
    m_overviewPreviewCacheSize = targetSize;
    m_overviewPreviewDirty = false;
    return m_overviewPreviewCache;
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
    const double fitScale = currentFitScale();
    if (m_isSvg) {
        return std::max(256.0, fitScale * 4.0);
    }

    const QSize imageSize = transformedImageSize();
    if (imageSize.isEmpty()) {
        return std::max(1.0, fitScale);
    }

    constexpr double kMaxRenderedPixels = 48000000.0;
    constexpr double kMaxRenderedSide = 12000.0;
    const double sourcePixels = imageSize.width() * static_cast<double>(imageSize.height());
    const double pixelLimit = std::sqrt(kMaxRenderedPixels / sourcePixels);
    const double sideLimit = kMaxRenderedSide / std::max(imageSize.width(), imageSize.height());
    const double budgetLimit = std::min(pixelLimit, sideLimit);
    const double requestedLimit = std::max(20.0, fitScale * 4.0);
    return std::max(fitScale, std::min(requestedLimit, budgetLimit));
}

bool MainWindow::isZoomedBeyondFit() const
{
    const double fitScale = currentFitScale();
    return hasImage()
           && !m_fitToWindow
           && !qFuzzyIsNull(fitScale)
           && m_scaleFactor / fitScale > 1.0;
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
        zoomBy(1.25);
        event->accept();
        return true;
    case Qt::Key_Minus:
        zoomBy(0.8);
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

bool MainWindow::handleKeyboardPan(QKeyEvent *event)
{
    if (!hasImage()) {
        return false;
    }

    if (event->key() == Qt::Key_Escape && isZoomedBeyondFit()) {
        m_fitToWindow = true;
        m_scaleFactor = 1.0;
        m_pendingScrollAnchor = false;
        updateImageView();
        updateToolbarState();
        updateZoomStatus();
        event->accept();
        return true;
    }

    if (!isZoomedBeyondFit()) {
        if (event->key() == Qt::Key_Left && m_currentIndex > 0) {
            openPreviousImage();
            event->accept();
            return true;
        }
        if (event->key() == Qt::Key_Right
            && m_currentIndex >= 0
            && m_currentIndex + 1 < m_directoryImages.size()) {
            openNextImage();
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

bool MainWindow::handleMousePanEvent(QObject *watched, QEvent *event)
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
    QPointF labelPosition = m_imageLabel->pos();
    if (m_pendingScrollAnchor && !m_fitToWindow) {
        labelPosition = QPointF(m_pendingAnchorViewport) - m_pendingAnchorImage * oldScale;
    }
    const QPointF imageAnchor = (QPointF(viewportPosition) - labelPosition) / oldScale;

    m_fitToWindow = false;
    m_scaleFactor = std::clamp(oldScale * factor, 0.05, maximumManualScale());
    m_pendingScrollAnchor = true;
    m_pendingAnchorImage = imageAnchor;
    m_pendingAnchorViewport = viewportPosition;
    requestImageViewUpdate();
    if (m_qualityRenderTimer) {
        m_qualityRenderTimer->start();
    }
    updateToolbarState();
    updateZoomStatus();
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
    m_pendingScrollAnchor = false;
    if (!m_fitToWindow) {
        m_scaleFactor = std::min(1.0, maximumManualScale());
    }
    updateImageView();
    updateToolbarState();
    updateZoomStatus();
}

void MainWindow::showActualSize()
{
    if (!hasImage()) {
        return;
    }
    m_fitToWindow = false;
    m_scaleFactor = std::min(1.0, maximumManualScale());
    m_pendingScrollAnchor = false;
    updateImageView();
    updateToolbarState();
    updateZoomStatus();
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
    m_pendingScrollAnchor = false;
    invalidateOverviewPreview();
    updateImageView();
}

void MainWindow::rebuildDirectorySequence(const QString &filePath)
{
    const QFileInfo current(filePath);
    QDir dir(current.absolutePath());
    const QFileInfoList files = dir.entryInfoList(
        ImageFormats::imageNameFilters(),
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
        showToast(tr("已复制到剪切板"));
    }
}

void MainWindow::copyCurrentImagePathToClipboard()
{
    if (m_currentFilePath.isEmpty()) {
        return;
    }

    QApplication::clipboard()->setText(QDir::toNativeSeparators(m_currentFilePath));
    showToast(tr("已复制图片路径"));
}

void MainWindow::revealCurrentImageInFileManager()
{
    if (m_currentFilePath.isEmpty()) {
        return;
    }

#ifdef Q_OS_MAC
    QProcess::startDetached(QStringLiteral("open"), {QStringLiteral("-R"), m_currentFilePath});
#elif defined(Q_OS_WIN)
    QProcess::startDetached(QStringLiteral("explorer.exe"),
                            {QStringLiteral("/select,%1").arg(QDir::toNativeSeparators(m_currentFilePath))});
#else
    QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(m_currentFilePath).absolutePath()));
#endif
}

void MainWindow::showImageInfoDialog()
{
    if (!hasImage()) {
        return;
    }

    const QFileInfo info(m_currentFilePath);
    const QSize imageSize = transformedImageSize();
    const QString sizeText = imageSize.isEmpty()
                                 ? tr("未知")
                                 : tr("%1 x %2").arg(imageSize.width()).arg(imageSize.height());
    const QString zoomText = m_fitToWindow
                                 ? tr("Fit")
                                 : tr("%1%").arg(qRound(m_scaleFactor * 100.0));
    const QString fileSize = QLocale().formattedDataSize(info.size());
    const QString modified = QLocale().toString(info.lastModified(), QLocale::ShortFormat);

    QMessageBox box(this);
    box.setIcon(QMessageBox::Information);
    box.setWindowTitle(tr("更多信息"));
    box.setTextFormat(Qt::RichText);
    box.setText(tr("<b>%1</b>").arg(info.fileName().toHtmlEscaped()));
    box.setInformativeText(
        tr("格式：%1<br/>尺寸：%2<br/>文件大小：%3<br/>修改时间：%4<br/>当前缩放：%5<br/>路径：%6")
            .arg(info.suffix().toUpper().toHtmlEscaped(),
                 sizeText.toHtmlEscaped(),
                 fileSize.toHtmlEscaped(),
                 modified.toHtmlEscaped(),
                 zoomText.toHtmlEscaped(),
                 QDir::toNativeSeparators(m_currentFilePath).toHtmlEscaped()));
    box.exec();
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
        tr("<b>NGImageViewer</b><br/>跨端图片查看器<br/>支持 JPG、PNG、BMP、GIF、WEBP、SVG、HEIC、HEIF。"));
}
