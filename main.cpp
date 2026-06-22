#include "mainwindow.h"

#include <QApplication>
#include <QCoreApplication>
#include <QIcon>
#include <QLocale>
#include <QTranslator>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    QApplication::setWindowIcon(QIcon(QStringLiteral(":/icons/resources/icons/app-icon.svg")));

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
    w.show();

    const QStringList args = QCoreApplication::arguments();
    if (args.size() > 1) {
        w.openImage(args.at(1), true);
    }

    return QApplication::exec();
}
