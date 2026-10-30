#include "backgroundremover.h"
#include <QDebug>
#include <QFileInfo>
#include <cmath>
#include <algorithm>
#include <array>
#include <stdexcept>
#include <thread>

// Constantes ImageNet
static constexpr float MEAN[3] = {0.485f, 0.456f, 0.406f};
static constexpr float STD[3]  = {0.229f, 0.224f, 0.225f};

BackgroundRemover::BackgroundRemover(const QString& modelPath)
    : env_(ORT_LOGGING_LEVEL_WARNING, "bg-remover"),
      session_(nullptr)
{
    Ort::SessionOptions sess_opts;
    // Usar todos os nucleos disponiveis: 2-4x mais rapido que o valor
    // fixo de 1 thread em CPU multi-core.
    sess_opts.SetIntraOpNumThreads(
        std::max(1u, std::thread::hardware_concurrency()));
    sess_opts.SetGraphOptimizationLevel(
        GraphOptimizationLevel::ORT_ENABLE_ALL);

#ifdef _WIN32
    const std::wstring wpath = modelPath.toStdWString();
    session_ = Ort::Session(env_, wpath.c_str(), sess_opts);
#else
    const QByteArray utf8Path = modelPath.toUtf8();
    session_ = Ort::Session(env_, utf8Path.constData(), sess_opts);
#endif

    Ort::AllocatorWithDefaultOptions allocator;

    Ort::AllocatedStringPtr in_ptr  = session_.GetInputNameAllocated(0, allocator);
    Ort::AllocatedStringPtr out_ptr = session_.GetOutputNameAllocated(0, allocator);
    input_name_  = std::string(in_ptr.get());
    output_name_ = std::string(out_ptr.get());

    input_size_ = detectInputSize(modelPath);

    const QString sizeStr = QString::number(input_size_)
        + QStringLiteral("x") + QString::number(input_size_);
    qInfo().noquote() << QStringLiteral(
        "[BackgroundRemover] Modelo carregado: %1\n  Input: %2 (%3)\n  Output: %4")
        .arg(modelPath,
             QString::fromStdString(input_name_),
             sizeStr,
             QString::fromStdString(output_name_));
}

// U2-Net/Silueta trabalham a 320x320; a geracao ISNet a 1024x1024.
// O shape declarado pelo modelo e a fonte de verdade; modelos com
// shape dinamico (-1) caem no fallback pelo nome do ficheiro.
int BackgroundRemover::detectInputSize(const QString& modelPath) const
{
    try {
        const std::vector<int64_t> shape =
            session_.GetInputTypeInfo(0)
                .GetTensorTypeAndShapeInfo()
                .GetShape();
        if (shape.size() == 4 && shape[2] > 0 && shape[3] > 0) {
            if (shape[2] == shape[3]) {
                return static_cast<int>(shape[2]);
            }
            qWarning() << "[BackgroundRemover] Entrada nao quadrada ("
                       << shape[2] << "x" << shape[3]
                       << "); a usar" << kDefaultInputSize;
        }
    } catch (const Ort::Exception& e) {
        qWarning() << "[BackgroundRemover] Nao foi possivel ler o shape "
                      "da entrada:" << e.what();
    }

    const QString name = QFileInfo(modelPath).fileName().toLower();
    if (name.contains(QStringLiteral("isnet"))) {
        return 1024;
    }
    return kDefaultInputSize;
}

QImage BackgroundRemover::removeBackground(const QImage& input) {
    if (input.isNull()) {
        throw std::invalid_argument("A imagem de entrada e invalida");
    }

    const int size = input_size_;
    std::vector<float> blob = buildInputBlob(input);

    std::vector<int64_t> input_shape = {1, 3, size, size};
    Ort::MemoryInfo mem_info = Ort::MemoryInfo::CreateCpu(
        OrtArenaAllocator, OrtMemTypeDefault);
    Ort::Value input_tensor = Ort::Value::CreateTensor<float>(
        mem_info, blob.data(), blob.size(),
        input_shape.data(), input_shape.size());

    const char* in_name  = input_name_.c_str();
    const char* out_name = output_name_.c_str();

    auto outputs = session_.Run(
        Ort::RunOptions{nullptr},
        &in_name, &input_tensor, 1,
        &out_name, 1);

    if (outputs.empty() || !outputs[0].IsTensor()) {
        throw std::runtime_error("O modelo nao retornou uma mascara tensorial");
    }

    const auto tensorInfo = outputs[0].GetTensorTypeAndShapeInfo();
    if (tensorInfo.GetElementType()
            != ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT) {
        throw std::runtime_error(
            "Mascara de saida incompativel: esperado tensor float, "
            "recebido tipo "
            + std::to_string(
                  static_cast<int>(tensorInfo.GetElementType())));
    }

    // As dimensoes da mascara vem do proprio tensor de saida: nem todos
    // os modelos devolvem uma mascara do tamanho da entrada.
    int mask_w = size;
    int mask_h = size;
    const std::vector<int64_t> out_shape = tensorInfo.GetShape();
    if (out_shape.size() == 4 && out_shape[2] > 0 && out_shape[3] > 0) {
        mask_h = static_cast<int>(out_shape[2]);
        mask_w = static_cast<int>(out_shape[3]);
    }

    const size_t expectedMaskSize =
        static_cast<size_t>(mask_w) * static_cast<size_t>(mask_h);
    if (tensorInfo.GetElementCount() != expectedMaskSize) {
        throw std::runtime_error(
            "Mascara de saida incompativel: esperado tensor float com "
            + std::to_string(expectedMaskSize) + " elementos, recebido "
            + std::to_string(tensorInfo.GetElementCount()));
    }

    const float* mask_ptr = outputs[0].GetTensorData<float>();

    QImage alpha = buildAlphaMask(
        mask_ptr, mask_w, mask_h, input.width(), input.height());

    return composite(input, alpha);
}

std::vector<float> BackgroundRemover::buildInputBlob(
    const QImage& img) const
{
    const int size = input_size_;
    QImage rgb = img.convertToFormat(QImage::Format_RGB32);
    QImage resized = rgb.scaled(
        size, size,
        Qt::IgnoreAspectRatio,
        Qt::SmoothTransformation);

    const int plane = size * size;
    std::vector<float> blob(static_cast<size_t>(3) * plane);

    // Acesso por scanline (Format_RGB32 garante QRgb alinhado) evita
    // a chamada QImage::pixel() por pixel, que domina o custo.
    for (int y = 0; y < size; ++y) {
        const QRgb* row = reinterpret_cast<const QRgb*>(
            resized.constScanLine(y));
        for (int x = 0; x < size; ++x) {
            const QRgb px = row[x];
            const float r = qRed(px) / 255.0f;
            const float g = qGreen(px) / 255.0f;
            const float b = qBlue(px) / 255.0f;

            const int idx = y * size + x;
            blob[idx]             = (r - MEAN[0]) / STD[0];
            blob[plane + idx]     = (g - MEAN[1]) / STD[1];
            blob[2 * plane + idx] = (b - MEAN[2]) / STD[2];
        }
    }
    return blob;
}

QImage BackgroundRemover::buildAlphaMask(
    const float* mask, int mask_w, int mask_h,
    int orig_w, int orig_h) const
{
    // U2-Net/ISNet ja devolvem probabilidades 0..1 (sigmoid na ultima
    // camada). O clamp direto mantem as bordas fieis; a normalizacao
    // min-max anterior esticava o contraste e saturava o fundo quando
    // o objeto ocupava pouco da imagem.
    QImage mask_img(mask_w, mask_h, QImage::Format_Grayscale8);
    for (int y = 0; y < mask_h; ++y) {
        quint8* row = mask_img.scanLine(y);
        const float* src = mask + static_cast<size_t>(y) * mask_w;
        for (int x = 0; x < mask_w; ++x) {
            const float v = std::clamp(src[x], 0.0f, 1.0f);
            row[x] = static_cast<quint8>(v * 255.0f + 0.5f);
        }
    }
    return mask_img.scaled(
        orig_w, orig_h,
        Qt::IgnoreAspectRatio,
        Qt::SmoothTransformation);
}

QImage BackgroundRemover::composite(
    const QImage& img, const QImage& alpha) const
{
    QImage result = img.convertToFormat(
        QImage::Format_RGBA8888);
    for (int y = 0; y < result.height(); ++y) {
        const QRgb* rgb_row = reinterpret_cast<const QRgb*>(
            result.constScanLine(y));
        const quint8* alpha_row =
            alpha.constScanLine(y);
        QRgb* out_row = reinterpret_cast<QRgb*>(
            result.scanLine(y));
        for (int x = 0; x < result.width(); ++x) {
            QRgb rgb = rgb_row[x];
            quint8 a  = alpha_row[x];
            out_row[x] = qRgba(qRed(rgb), qGreen(rgb),
                                   qBlue(rgb), a);
        }
    }
    return result;
}
