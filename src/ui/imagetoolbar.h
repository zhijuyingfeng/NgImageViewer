#ifndef IMAGETOOLBAR_H
#define IMAGETOOLBAR_H

#include <QString>
#include <QWidget>

class QAction;
class QGraphicsOpacityEffect;
class QToolButton;

class ImageToolbar : public QWidget
{
    Q_OBJECT

public:
    explicit ImageToolbar(QWidget *parent = nullptr);

    void setState(bool imageAvailable, bool hasPrevious, bool hasNext, bool fitToWindow);

signals:
    void openRequested();
    void previousRequested();
    void nextRequested();
    void navigationUnavailableRequested();
    void zoomInRequested();
    void zoomOutRequested();
    void fitRequested();
    void actualSizeRequested();
    void rotateClockwiseRequested();
    void rotateCounterClockwiseRequested();
    void deleteRequested();
    void infoRequested();
    void copyImageRequested();
    void copyPathRequested();
    void revealRequested();
    void associateFormatsRequested();
    void aboutRequested();

private:
    QToolButton *createToolButton(const QString &iconPath, const QString &tooltip);

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
    QGraphicsOpacityEffect *m_previousOpacity = nullptr;
    QGraphicsOpacityEffect *m_nextOpacity = nullptr;
    bool m_hasPrevious = false;
    bool m_hasNext = false;
};

#endif // IMAGETOOLBAR_H
