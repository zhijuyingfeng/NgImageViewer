#include "rawdecoder.h"

#include <QFile>
#include <QIODevice>
#include <QObject>

#if NGIMAGEVIEWER_HAS_LIBRAW
#include <libraw/libraw.h>
#endif

namespace RawDecoder {

bool isAvailable()
{
#if NGIMAGEVIEWER_HAS_LIBRAW
    return true;
#else
    return false;
#endif
}

QString unavailableMessage()
{
    return QObject::tr("当前构建未启用 RAW 支持。请安装 LibRaw 后重新配置并构建项目。");
}

DecodeResult decode(const QString &filePath)
{
#if NGIMAGEVIEWER_HAS_LIBRAW
    LibRaw raw;
    raw.imgdata.params.use_camera_wb = 1;
    raw.imgdata.params.output_bps = 8;
    raw.imgdata.params.output_color = 1;
    raw.imgdata.params.no_auto_bright = 1;

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return {{}, QObject::tr("RAW 文件打开失败：无法读取文件。")};
    }

    QByteArray rawData = file.readAll();
    if (rawData.isEmpty()) {
        return {{}, QObject::tr("RAW 文件打开失败：文件为空或读取失败。")};
    }

    int result = raw.open_buffer(rawData.data(), static_cast<size_t>(rawData.size()));
    if (result != LIBRAW_SUCCESS) {
        return {{}, QObject::tr("RAW 文件打开失败：%1").arg(QString::fromLocal8Bit(libraw_strerror(result)))};
    }

    result = raw.unpack();
    if (result != LIBRAW_SUCCESS) {
        return {{}, QObject::tr("RAW 数据读取失败：%1").arg(QString::fromLocal8Bit(libraw_strerror(result)))};
    }

    result = raw.dcraw_process();
    if (result != LIBRAW_SUCCESS) {
        return {{}, QObject::tr("RAW 解码失败：%1").arg(QString::fromLocal8Bit(libraw_strerror(result)))};
    }

    int imageError = LIBRAW_SUCCESS;
    libraw_processed_image_t *processed = raw.dcraw_make_mem_image(&imageError);
    if (!processed || imageError != LIBRAW_SUCCESS) {
        if (processed) {
            LibRaw::dcraw_clear_mem(processed);
        }
        return {{}, QObject::tr("RAW 图像转换失败：%1").arg(QString::fromLocal8Bit(libraw_strerror(imageError)))};
    }

    QImage image;
    if (processed->type == LIBRAW_IMAGE_BITMAP && processed->bits == 8) {
        if (processed->colors == 3) {
            image = QImage(
                processed->data,
                processed->width,
                processed->height,
                processed->width * 3,
                QImage::Format_RGB888).copy();
        } else if (processed->colors == 4) {
            image = QImage(
                processed->data,
                processed->width,
                processed->height,
                processed->width * 4,
                QImage::Format_RGBA8888).copy();
        }
    }

    LibRaw::dcraw_clear_mem(processed);
    if (image.isNull()) {
        return {{}, QObject::tr("RAW 解码结果格式暂不支持。")};
    }
    return {image, {}};
#else
    Q_UNUSED(filePath);
    return {{}, unavailableMessage()};
#endif
}

} // namespace RawDecoder
