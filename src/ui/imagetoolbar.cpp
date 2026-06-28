#include "imagetoolbar.h"

#include <QAction>
#include <QFrame>
#include <QGraphicsOpacityEffect>
#include <QHBoxLayout>
#include <QIcon>
#include <QMenu>
#include <QSize>
#include <QToolButton>

namespace {

QFrame *createSeparator(QWidget *parent)
{
    auto *line = new QFrame(parent);
    line->setFrameShape(QFrame::VLine);
    line->setFixedHeight(22);
    line->setObjectName(QStringLiteral("toolbarSeparator"));
    return line;
}

} // namespace

ImageToolbar::ImageToolbar(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("toolbar"));
    setFixedHeight(58);

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(12, 8, 12, 8);
    layout->setSpacing(6);

    m_openButton = createToolButton(QStringLiteral(":/icons/resources/icons/open-file.svg"), tr("打开图片"));
    m_previousButton = createToolButton(QStringLiteral(":/icons/resources/icons/previous.svg"), tr("上一张"));
    m_nextButton = createToolButton(QStringLiteral(":/icons/resources/icons/next.svg"), tr("下一张"));
    m_previousOpacity = new QGraphicsOpacityEffect(m_previousButton);
    m_nextOpacity = new QGraphicsOpacityEffect(m_nextButton);
    m_previousButton->setGraphicsEffect(m_previousOpacity);
    m_nextButton->setGraphicsEffect(m_nextOpacity);
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
    layout->addWidget(createSeparator(this));
    layout->addWidget(m_previousButton);
    layout->addWidget(m_nextButton);
    layout->addWidget(createSeparator(this));
    layout->addWidget(m_zoomOutButton);
    layout->addWidget(m_zoomInButton);
    layout->addWidget(m_fitButton);
    layout->addWidget(m_actualSizeButton);
    layout->addWidget(createSeparator(this));
    layout->addWidget(m_rotateCcwButton);
    layout->addWidget(m_rotateCwButton);
    layout->addWidget(createSeparator(this));
    layout->addWidget(m_deleteButton);
    layout->addWidget(m_moreButton);
    layout->addStretch(1);

    connect(m_openButton, &QToolButton::clicked, this, &ImageToolbar::openRequested);
    connect(m_previousButton, &QToolButton::clicked, this, [this] {
        if (m_hasPrevious) {
            emit previousRequested();
        } else {
            emit navigationUnavailableRequested();
        }
    });
    connect(m_nextButton, &QToolButton::clicked, this, [this] {
        if (m_hasNext) {
            emit nextRequested();
        } else {
            emit navigationUnavailableRequested();
        }
    });
    connect(m_zoomInButton, &QToolButton::clicked, this, &ImageToolbar::zoomInRequested);
    connect(m_zoomOutButton, &QToolButton::clicked, this, &ImageToolbar::zoomOutRequested);
    connect(m_fitButton, &QToolButton::clicked, this, &ImageToolbar::fitRequested);
    connect(m_actualSizeButton, &QToolButton::clicked, this, &ImageToolbar::actualSizeRequested);
    connect(m_rotateCwButton, &QToolButton::clicked, this, &ImageToolbar::rotateClockwiseRequested);
    connect(m_rotateCcwButton, &QToolButton::clicked, this, &ImageToolbar::rotateCounterClockwiseRequested);
    connect(m_deleteButton, &QToolButton::clicked, this, &ImageToolbar::deleteRequested);

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
    auto *associateFormatsAction = menu->addAction(tr("关联图片格式"));
    menu->addSeparator();
    auto *aboutAction = menu->addAction(tr("关于 NGImageViewer"));
    m_moreButton->setMenu(menu);
    m_moreButton->setPopupMode(QToolButton::InstantPopup);

    connect(m_infoAction, &QAction::triggered, this, &ImageToolbar::infoRequested);
    connect(m_copyAction, &QAction::triggered, this, &ImageToolbar::copyImageRequested);
    connect(m_copyPathAction, &QAction::triggered, this, &ImageToolbar::copyPathRequested);
    connect(m_revealAction, &QAction::triggered, this, &ImageToolbar::revealRequested);
    connect(associateFormatsAction, &QAction::triggered, this, &ImageToolbar::associateFormatsRequested);
    connect(aboutAction, &QAction::triggered, this, &ImageToolbar::aboutRequested);
}

void ImageToolbar::setState(bool imageAvailable, bool hasPrevious, bool hasNext, bool fitToWindow)
{
    m_hasPrevious = imageAvailable && hasPrevious;
    m_hasNext = imageAvailable && hasNext;

    m_openButton->setEnabled(true);
    m_zoomInButton->setEnabled(imageAvailable);
    m_zoomOutButton->setEnabled(imageAvailable);
    m_fitButton->setEnabled(imageAvailable);
    m_actualSizeButton->setEnabled(imageAvailable);
    m_rotateCwButton->setEnabled(imageAvailable);
    m_rotateCcwButton->setEnabled(imageAvailable);
    m_deleteButton->setEnabled(imageAvailable);
    m_previousButton->setEnabled(imageAvailable);
    m_nextButton->setEnabled(imageAvailable);
    m_previousOpacity->setOpacity(m_hasPrevious ? 1.0 : 0.35);
    m_nextOpacity->setOpacity(m_hasNext ? 1.0 : 0.35);
    m_moreButton->setEnabled(true);
    m_copyAction->setEnabled(imageAvailable);
    m_infoAction->setEnabled(imageAvailable);
    m_copyPathAction->setEnabled(imageAvailable);
    m_revealAction->setEnabled(imageAvailable);
    m_fitButton->setToolTip(fitToWindow ? tr("切换到原始比例") : tr("适配窗口"));
}

QToolButton *ImageToolbar::createToolButton(const QString &iconPath, const QString &tooltip)
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
