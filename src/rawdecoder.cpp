#include "rawdecoder.h"

#include <QBuffer>
#include <QFile>
#include <QImageReader>
#include <QIODevice>
#include <QObject>

#include <limits>

#if NGIMAGEVIEWER_HAS_LIBRAW
#include <libraw/libraw.h>
#endif

namespace {

#if NGIMAGEVIEWER_HAS_LIBRAW

QString libRawErrorText(int code)
{
    return QString::fromLocal8Bit(libraw_strerror(code));
}

QString decoderInfo()
{
    return QObject::tr("LibRaw %1").arg(QString::fromLocal8Bit(LibRaw::version()));
}

QString cameraInfo(const LibRaw &raw)
{
    const QString make = QString::fromLocal8Bit(raw.imgdata.idata.make).trimmed();
    const QString model = QString::fromLocal8Bit(raw.imgdata.idata.model).trimmed();
    if (make.isEmpty()) {
        return model;
    }
    if (model.isEmpty() || model.startsWith(make, Qt::CaseInsensitive)) {
        return model.isEmpty() ? make : model;
    }
    return QStringLiteral("%1 %2").arg(make, model);
}

QSize rawImageSize(const LibRaw &raw)
{
    const libraw_image_sizes_t &sizes = raw.imgdata.sizes;
    if (sizes.iwidth > 0 && sizes.iheight > 0) {
        return QSize(sizes.iwidth, sizes.iheight);
    }
    if (sizes.width > 0 && sizes.height > 0) {
        return QSize(sizes.width, sizes.height);
    }
    if (sizes.raw_width > 0 && sizes.raw_height > 0) {
        return QSize(sizes.raw_width, sizes.raw_height);
    }
    return {};
}

QImage bitmapImage(const libraw_processed_image_t *processed)
{
    if (!processed || processed->type != LIBRAW_IMAGE_BITMAP || processed->bits != 8) {
        return {};
    }

    if (processed->colors == 3) {
        return QImage(
                   processed->data,
                   processed->width,
                   processed->height,
                   processed->width * 3,
                   QImage::Format_RGB888)
            .copy();
    }

    if (processed->colors == 4) {
        return QImage(
                   processed->data,
                   processed->width,
                   processed->height,
                   processed->width * 4,
                   QImage::Format_RGBA8888)
            .copy();
    }

    return {};
}

QImage decodeJpegData(const char *data, qsizetype size)
{
    if (!data || size <= 0 || size > std::numeric_limits<int>::max()) {
        return {};
    }

    QByteArray jpegData = QByteArray::fromRawData(data, size);
    QBuffer buffer(&jpegData);
    buffer.open(QIODevice::ReadOnly);

    QImageReader reader(&buffer, "jpg");
    reader.setAutoTransform(true);
    QImage image = reader.read();
    if (!image.isNull()) {
        return image;
    }
    return QImage::fromData(jpegData, "JPG");
}

QImage processedImage(const libraw_processed_image_t *processed)
{
    if (!processed) {
        return {};
    }

    if (processed->type == LIBRAW_IMAGE_JPEG) {
        return decodeJpegData(
            reinterpret_cast<const char *>(processed->data),
            static_cast<qsizetype>(processed->data_size));
    }

    return bitmapImage(processed);
}

RawDecoder::DecodeResult decodeScannedJpegPreview(const QByteArray &rawData)
{
    RawDecoder::DecodeResult result;
    result.decoderInfo = decoderInfo();

    static const QByteArray soi("\xff\xd8\xff", 3);
    static const QByteArray eoi("\xff\xd9", 2);
    constexpr int maxCandidates = 64;

    QImage bestImage;
    qsizetype bestArea = 0;
    int candidates = 0;
    qsizetype start = rawData.indexOf(soi);
    while (start >= 0 && candidates < maxCandidates) {
        const qsizetype end = rawData.indexOf(eoi, start + soi.size());
        if (end < 0) {
            break;
        }

        const qsizetype jpegSize = end + eoi.size() - start;
        const QImage image = decodeJpegData(rawData.constData() + start, jpegSize);
        if (!image.isNull()) {
            const qsizetype area = static_cast<qsizetype>(image.width()) * image.height();
            if (area > bestArea) {
                bestImage = image;
                bestArea = area;
            }
        }

        ++candidates;
        start = rawData.indexOf(soi, end + eoi.size());
    }

    if (!bestImage.isNull()) {
        result.image = bestImage;
        result.usedEmbeddedPreview = true;
        result.displaySource = QObject::tr("内嵌 JPEG 预览");
        result.embeddedPreviewSize = bestImage.size();
    } else {
        result.errorMessage = QObject::tr("RAW 内嵌 JPEG 预览扫描失败。");
    }

    return result;
}

RawDecoder::DecodeResult decodeEmbeddedPreview(const QByteArray &rawData)
{
    RawDecoder::DecodeResult result;
    result.decoderInfo = decoderInfo();

    LibRaw raw;
    int code = raw.open_buffer(rawData.constData(), static_cast<size_t>(rawData.size()));
    if (code != LIBRAW_SUCCESS) {
        result.errorMessage = QObject::tr("RAW 文件打开失败：%1").arg(libRawErrorText(code));
        return result;
    }

    result.cameraInfo = cameraInfo(raw);
    result.rawSize = rawImageSize(raw);

    code = raw.unpack_thumb();
    if (code != LIBRAW_SUCCESS) {
        result.errorMessage = QObject::tr("RAW 内嵌预览读取失败：%1").arg(libRawErrorText(code));
        return result;
    }

    int imageError = LIBRAW_SUCCESS;
    libraw_processed_image_t *processed = raw.dcraw_make_mem_thumb(&imageError);
    if (!processed || imageError != LIBRAW_SUCCESS) {
        if (processed) {
            LibRaw::dcraw_clear_mem(processed);
        }
        result.errorMessage = QObject::tr("RAW 内嵌预览转换失败：%1").arg(libRawErrorText(imageError));
        return result;
    }

    result.image = processedImage(processed);
    if (!result.image.isNull()) {
        result.usedEmbeddedPreview = true;
        result.displaySource = processed->type == LIBRAW_IMAGE_JPEG
                                   ? QObject::tr("内嵌 JPEG 预览")
                                   : QObject::tr("内嵌预览图");
        result.embeddedPreviewSize = result.image.size();
    } else {
        result.errorMessage = QObject::tr("RAW 内嵌预览格式暂不支持。");
    }

    LibRaw::dcraw_clear_mem(processed);
    return result;
}

RawDecoder::DecodeResult decodeFullRaw(const QByteArray &rawData)
{
    RawDecoder::DecodeResult result;
    result.decoderInfo = decoderInfo();
    result.fullDecodeAttempted = true;

    LibRaw raw;
    raw.imgdata.params.use_camera_wb = 1;
    raw.imgdata.params.output_bps = 8;
    raw.imgdata.params.output_color = 1;
    raw.imgdata.params.no_auto_bright = 1;

    int code = raw.open_buffer(rawData.constData(), static_cast<size_t>(rawData.size()));
    if (code != LIBRAW_SUCCESS) {
        result.errorMessage = QObject::tr("RAW 文件打开失败：%1").arg(libRawErrorText(code));
        return result;
    }

    result.cameraInfo = cameraInfo(raw);
    result.rawSize = rawImageSize(raw);

    code = raw.unpack();
    if (code != LIBRAW_SUCCESS) {
        result.errorMessage = QObject::tr("RAW 数据读取失败：%1").arg(libRawErrorText(code));
        return result;
    }

    code = raw.dcraw_process();
    if (code != LIBRAW_SUCCESS) {
        result.errorMessage = QObject::tr("RAW 解码失败：%1").arg(libRawErrorText(code));
        return result;
    }

    int imageError = LIBRAW_SUCCESS;
    libraw_processed_image_t *processed = raw.dcraw_make_mem_image(&imageError);
    if (!processed || imageError != LIBRAW_SUCCESS) {
        if (processed) {
            LibRaw::dcraw_clear_mem(processed);
        }
        result.errorMessage = QObject::tr("RAW 图像转换失败：%1").arg(libRawErrorText(imageError));
        return result;
    }

    result.image = bitmapImage(processed);
    LibRaw::dcraw_clear_mem(processed);

    if (result.image.isNull()) {
        result.errorMessage = QObject::tr("RAW 解码结果格式暂不支持。");
        return result;
    }

    result.displaySource = QObject::tr("完整 RAW 解码");
    return result;
}

#endif

} // namespace

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
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return {{}, QObject::tr("RAW 文件打开失败：无法读取文件。")};
    }

    QByteArray rawData = file.readAll();
    if (rawData.isEmpty()) {
        return {{}, QObject::tr("RAW 文件打开失败：文件为空或读取失败。")};
    }

    DecodeResult preview = decodeEmbeddedPreview(rawData);
    if (!preview.image.isNull()) {
        return preview;
    }

    DecodeResult scannedPreview = decodeScannedJpegPreview(rawData);
    if (!scannedPreview.image.isNull()) {
        scannedPreview.cameraInfo = preview.cameraInfo;
        scannedPreview.rawSize = preview.rawSize;
        return scannedPreview;
    }

    DecodeResult full = decodeFullRaw(rawData);
    if (!full.image.isNull()) {
        return full;
    }

    DecodeResult error;
    error.decoderInfo = full.decoderInfo.isEmpty() ? decoderInfo() : full.decoderInfo;
    error.cameraInfo = full.cameraInfo;
    error.rawSize = full.rawSize;
    error.fullDecodeAttempted = true;
    error.errorMessage = QObject::tr("%1\n%2\n%3")
                             .arg(full.errorMessage,
                                  preview.errorMessage,
                                  scannedPreview.errorMessage);
    return error;
#else
    Q_UNUSED(filePath);
    return {{}, unavailableMessage()};
#endif
}

} // namespace RawDecoder
