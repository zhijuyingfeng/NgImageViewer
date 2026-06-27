#include "heifdecoder.h"

#include <QFile>
#include <QObject>

#include <cstring>
#include <memory>

#if NGIMAGEVIEWER_HAS_LIBHEIF
#include <libheif/heif.h>
#endif

namespace {

#if NGIMAGEVIEWER_HAS_LIBHEIF
struct HeifContextDeleter
{
    void operator()(heif_context *context) const
    {
        heif_context_free(context);
    }
};

struct HeifImageHandleDeleter
{
    void operator()(heif_image_handle *handle) const
    {
        heif_image_handle_release(handle);
    }
};

struct HeifImageDeleter
{
    void operator()(heif_image *image) const
    {
        heif_image_release(image);
    }
};

struct HeifLibraryGuard
{
    ~HeifLibraryGuard()
    {
        heif_deinit();
    }
};

QString errorMessage(const QString &prefix, const heif_error &error)
{
    const QString detail = QString::fromUtf8(error.message ? error.message : "");
    if (detail.isEmpty()) {
        return prefix;
    }
    return QObject::tr("%1：%2").arg(prefix, detail);
}

QString decoderInfo()
{
    return QObject::tr("libheif %1").arg(QString::fromLatin1(heif_get_version()));
}
#endif

} // namespace

namespace HeifDecoder {

bool isAvailable()
{
#if NGIMAGEVIEWER_HAS_LIBHEIF
    return heif_have_decoder_for_format(heif_compression_HEVC) != 0;
#else
    return false;
#endif
}

QString unavailableMessage()
{
    return QObject::tr("当前构建未启用 HEIF/HEIC 支持。请初始化 bundled libheif/libde265 后重新配置并构建项目。");
}

DecodeResult decode(const QString &filePath)
{
#if NGIMAGEVIEWER_HAS_LIBHEIF
    DecodeResult result;
    result.decoderInfo = decoderInfo();

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        result.errorMessage = QObject::tr("HEIF/HEIC 文件打开失败：无法读取文件。");
        return result;
    }

    const QByteArray data = file.readAll();
    if (data.isEmpty()) {
        result.errorMessage = QObject::tr("HEIF/HEIC 文件打开失败：文件为空或读取失败。");
        return result;
    }

    const heif_error initError = heif_init(nullptr);
    if (initError.code != heif_error_Ok) {
        result.errorMessage = errorMessage(QObject::tr("HEIF/HEIC 解码器初始化失败"), initError);
        return result;
    }
    HeifLibraryGuard libraryGuard;

    std::unique_ptr<heif_context, HeifContextDeleter> context(heif_context_alloc());
    if (!context) {
        result.errorMessage = QObject::tr("HEIF/HEIC 解码器初始化失败：无法创建上下文。");
        return result;
    }

    heif_error error = heif_context_read_from_memory_without_copy(
        context.get(),
        data.constData(),
        static_cast<size_t>(data.size()),
        nullptr);
    if (error.code != heif_error_Ok) {
        result.errorMessage = errorMessage(QObject::tr("HEIF/HEIC 文件解析失败"), error);
        return result;
    }

    heif_image_handle *rawHandle = nullptr;
    error = heif_context_get_primary_image_handle(context.get(), &rawHandle);
    std::unique_ptr<heif_image_handle, HeifImageHandleDeleter> handle(rawHandle);
    if (error.code != heif_error_Ok || !handle) {
        result.errorMessage = errorMessage(QObject::tr("HEIF/HEIC 主图读取失败"), error);
        return result;
    }

    const int width = heif_image_handle_get_width(handle.get());
    const int height = heif_image_handle_get_height(handle.get());
    if (width <= 0 || height <= 0) {
        result.errorMessage = QObject::tr("HEIF/HEIC 文件打开失败：图片尺寸无效。");
        return result;
    }

    result.sourceSize = QSize(width, height);
    result.hasAlpha = heif_image_handle_has_alpha_channel(handle.get()) != 0;

    heif_decoding_options *options = heif_decoding_options_alloc();
    if (options) {
        options->convert_hdr_to_8bit = 1;
    }

    heif_image *rawImage = nullptr;
    error = heif_decode_image(
        handle.get(),
        &rawImage,
        heif_colorspace_RGB,
        result.hasAlpha ? heif_chroma_interleaved_RGBA : heif_chroma_interleaved_RGB,
        options);
    if (options) {
        heif_decoding_options_free(options);
    }
    std::unique_ptr<heif_image, HeifImageDeleter> image(rawImage);
    if (error.code != heif_error_Ok || !image) {
        result.errorMessage = errorMessage(QObject::tr("HEIF/HEIC 图片解码失败"), error);
        return result;
    }

    size_t sourceStride = 0;
    const uint8_t *source = heif_image_get_plane_readonly2(
        image.get(),
        heif_channel_interleaved,
        &sourceStride);
    if (!source || sourceStride == 0) {
        result.errorMessage = QObject::tr("HEIF/HEIC 图片解码失败：无法读取像素数据。");
        return result;
    }

    const QImage::Format format = result.hasAlpha ? QImage::Format_RGBA8888 : QImage::Format_RGB888;
    QImage decoded(width, height, format);
    if (decoded.isNull()) {
        result.errorMessage = QObject::tr("HEIF/HEIC 图片解码失败：内存不足。");
        return result;
    }

    const qsizetype bytesPerLine = static_cast<qsizetype>(width) * (result.hasAlpha ? 4 : 3);
    if (sourceStride < static_cast<size_t>(bytesPerLine)) {
        result.errorMessage = QObject::tr("HEIF/HEIC 图片解码失败：像素行跨度无效。");
        return result;
    }
    for (int y = 0; y < height; ++y) {
        std::memcpy(decoded.scanLine(y), source + sourceStride * static_cast<size_t>(y), bytesPerLine);
    }

    result.image = decoded;
    return result;
#else
    Q_UNUSED(filePath);
    return {{}, unavailableMessage()};
#endif
}

} // namespace HeifDecoder
