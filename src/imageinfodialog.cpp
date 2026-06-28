#include "imageinfodialog.h"

#include <QDir>
#include <QFileInfo>
#include <QLocale>
#include <QMessageBox>
#include <QObject>

namespace ImageInfoDialog {

void show(QWidget *parent, const Details &details)
{
    const QFileInfo info(details.filePath);
    const QString sizeText = details.imageSize.isEmpty()
                                 ? QObject::tr("未知")
                                 : QObject::tr("%1 x %2")
                                       .arg(details.imageSize.width())
                                       .arg(details.imageSize.height());
    const QString fileSize = QLocale().formattedDataSize(info.size());
    const QString modified = QLocale().toString(info.lastModified(), QLocale::ShortFormat);

    QStringList extraRows;
    if (details.isRaw) {
        if (!details.rawDisplaySource.isEmpty()) {
            extraRows << QObject::tr("RAW 显示来源：%1").arg(details.rawDisplaySource.toHtmlEscaped());
        }
        if (!details.rawDecoderInfo.isEmpty()) {
            extraRows << QObject::tr("RAW 解码器：%1").arg(details.rawDecoderInfo.toHtmlEscaped());
        }
        if (!details.rawCameraInfo.isEmpty()) {
            extraRows << QObject::tr("相机：%1").arg(details.rawCameraInfo.toHtmlEscaped());
        }
        if (!details.rawSourceSize.isEmpty()) {
            extraRows << QObject::tr("RAW 标称尺寸：%1 x %2")
                             .arg(details.rawSourceSize.width())
                             .arg(details.rawSourceSize.height());
        }
        if (!details.rawEmbeddedPreviewSize.isEmpty()) {
            extraRows << QObject::tr("内嵌预览尺寸：%1 x %2")
                             .arg(details.rawEmbeddedPreviewSize.width())
                             .arg(details.rawEmbeddedPreviewSize.height());
        }
    } else if (details.isHeif) {
        if (!details.heifDecoderInfo.isEmpty()) {
            extraRows << QObject::tr("HEIF 解码器：%1").arg(details.heifDecoderInfo.toHtmlEscaped());
        }
        if (!details.heifSourceSize.isEmpty()) {
            extraRows << QObject::tr("HEIF 标称尺寸：%1 x %2")
                             .arg(details.heifSourceSize.width())
                             .arg(details.heifSourceSize.height());
        }
        extraRows << QObject::tr("Alpha 通道：%1")
                         .arg(details.heifHasAlpha ? QObject::tr("有") : QObject::tr("无"));
    }

    QMessageBox box(parent);
    box.setIcon(QMessageBox::Information);
    box.setWindowTitle(QObject::tr("更多信息"));
    box.setTextFormat(Qt::RichText);
    box.setText(QObject::tr("<b>%1</b>").arg(info.fileName().toHtmlEscaped()));
    QString informativeText =
        QObject::tr("格式：%1<br/>尺寸：%2<br/>文件大小：%3<br/>修改时间：%4<br/>当前缩放：%5")
            .arg(info.suffix().toUpper().toHtmlEscaped(),
                 sizeText.toHtmlEscaped(),
                 fileSize.toHtmlEscaped(),
                 modified.toHtmlEscaped(),
                 details.zoomText.toHtmlEscaped());
    if (!extraRows.isEmpty()) {
        informativeText += QStringLiteral("<br/>") + extraRows.join(QStringLiteral("<br/>"));
    }
    informativeText += QObject::tr("<br/>路径：%1")
                           .arg(QDir::toNativeSeparators(details.filePath).toHtmlEscaped());
    box.setInformativeText(informativeText);
    box.exec();
}

} // namespace ImageInfoDialog
