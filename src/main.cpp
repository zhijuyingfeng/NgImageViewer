#include "mainwindow.h"

#include <QApplication>
#include <QCoreApplication>
#include <QEvent>
#include <QFileOpenEvent>
#include <QGuiApplication>
#include <QIcon>
#include <QLocale>
#include <QTranslator>

namespace {

class ImageViewerApplication : public QApplication
{
public:
    ImageViewerApplication(int &argc, char **argv)
        : QApplication(argc, argv)
    {
    }

    void setMainWindow(MainWindow *window)
    {
        m_window = window;
        if (m_window && !m_pendingFilePath.isEmpty()) {
            m_window->openImage(m_pendingFilePath, true);
            m_pendingFilePath.clear();
        }
    }

protected:
    bool event(QEvent *event) override
    {
        if (event->type() == QEvent::FileOpen) {
            auto *fileOpenEvent = static_cast<QFileOpenEvent *>(event);
            const QString filePath = fileOpenEvent->file();
            if (!filePath.isEmpty()) {
                if (m_window) {
                    m_window->openImage(filePath, true);
                } else {
                    m_pendingFilePath = filePath;
                }
                return true;
            }
        }
        return QApplication::event(event);
    }

private:
    MainWindow *m_window = nullptr;
    QString m_pendingFilePath;
};

} // namespace

int main(int argc, char *argv[])
{
    ImageViewerApplication a(argc, argv);
    const QIcon appIcon(QStringLiteral(":/icons/resources/icons/app-icon.svg"));
    QApplication::setWindowIcon(appIcon);
#ifdef Q_OS_LINUX
    QGuiApplication::setDesktopFileName(QStringLiteral("ngimageviewer"));
#endif

    QTranslator translator;
    const QStringList uiLanguages = QLocale::system().uiLanguages();
    for (const QString &locale : uiLanguages) {
        const QString baseName = "NgImageViewer_" + QLocale(locale).name();
        if (translator.load(":/i18n/" + baseName)) {
            a.installTranslator(&translator);
            break;
        }
    }
    MainWindow w;
    a.setMainWindow(&w);
    w.setWindowIcon(appIcon);
    w.show();

    const QStringList args = QCoreApplication::arguments();
    if (args.size() > 1) {
        w.openImage(args.at(1), true);
    }

    return QApplication::exec();
}
