#include "backgroundremover.h"
#include <iostream>
#include <cmath>
#include <algorithm>
#include <array>

// Constantes ImageNet
static constexpr float MEAN[3] = {0.485f, 0.456f, 0.406f};
static constexpr float STD[3]  = {0.229f, 0.224f, 0.225f};

BackgroundRemover::BackgroundRemover(const std::string& model_path)
    : env_(ORT_LOGGING_LEVEL_WARNING, "u2netp"),
      session_(nullptr)
{
    Ort::SessionOptions sess_opts;
    sess_opts.SetIntraOpNumThreads(1);
    sess_opts.SetGraphOptimizationLevel(
        GraphOptimizationLevel::ORT_ENABLE_ALL);

#ifdef _WIN32
    std::wstring wpath(model_path.begin(), model_path.end());
    session_ = Ort::Session(env_, wpath.c_str(), sess_opts);
#else
    session_ = Ort::Session(env_, model_path.c_str(), sess_opts);
#endif

    Ort::AllocatorWithDefaultOptions allocator;

    Ort::AllocatedStringPtr in_ptr  = session_.GetInputNameAllocated(0, allocator);
    Ort::AllocatedStringPtr out_ptr = session_.GetOutputNameAllocated(0, allocator);
    input_name_  = std::string(in_ptr.get());
    output_name_ = std::string(out_ptr.get());

    std::cout << "[BackgroundRemover] Modelo carregado: " << model_path
              << "\n  Input: " << input_name_
              << "\n  Output: " << output_name_ << std::endl;
}

QImage BackgroundRemover::removeBackground(const QImage& input) {
    std::vector<float> blob = buildInputBlob(input);

    std::vector<int64_t> input_shape = {1, 3, INPUT_SIZE, INPUT_SIZE};
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

    float* mask_ptr = outputs[0].GetTensorMutableData<float>();
    size_t mask_total = outputs[0].GetTensorTypeAndShapeInfo()
                            .GetElementCount();
    if (mask_total != static_cast<size_t>(INPUT_SIZE * INPUT_SIZE)) {
        std::cerr << "[BackgroundRemover] Aviso: mascara "
                  << "incompativel (" << mask_total
                  << " esperado " << INPUT_SIZE * INPUT_SIZE
                  << ")" << std::endl;
    }

    QImage alpha = buildAlphaMask(
        mask_ptr, input.width(), input.height());

    return composite(input, alpha);
}

std::vector<float> BackgroundRemover::buildInputBlob(
    const QImage& img) const
{
    QImage rgb = img.convertToFormat(QImage::Format_RGB32);
    QImage resized = rgb.scaled(
        INPUT_SIZE, INPUT_SIZE,
        Qt::IgnoreAspectRatio,
        Qt::SmoothTransformation);

    int plane = INPUT_SIZE * INPUT_SIZE;
    std::vector<float> blob(1UL * 3 * plane);

    for (int y = 0; y < INPUT_SIZE; ++y) {
        for (int x = 0; x < INPUT_SIZE; ++x) {
            QRgb px = resized.pixel(x, y);
            float r = qRed(px) / 255.0f;
            float g = qGreen(px) / 255.0f;
            float b = qBlue(px) / 255.0f;

            int idx = y * INPUT_SIZE + x;
            blob[0 * plane + idx] = (r - MEAN[0]) / STD[0];
            blob[1 * plane + idx] = (g - MEAN[1]) / STD[1];
            blob[2 * plane + idx] = (b - MEAN[2]) / STD[2];
        }
    }
    return blob;
}

QImage BackgroundRemover::buildAlphaMask(
    const float* mask, int orig_w, int orig_h) const
{
    float min_v = mask[0];
    float max_v = mask[0];
    for (int i = 1; i < INPUT_SIZE * INPUT_SIZE; ++i) {
        if (mask[i] < min_v) min_v = mask[i];
        if (mask[i] > max_v) max_v = mask[i];
    }
    float range = max_v - min_v;
    if (range < 1e-8f) range = 1.0f;

    QImage mask_img(INPUT_SIZE, INPUT_SIZE,
                    QImage::Format_Grayscale8);
    for (int y = 0; y < INPUT_SIZE; ++y) {
        quint8* row = mask_img.scanLine(y);
        for (int x = 0; x < INPUT_SIZE; ++x) {
            float val = (mask[y * INPUT_SIZE + x] - min_v)
                        / range * 255.0f;
            row[x] = static_cast<quint8>(
                std::clamp(val, 0.0f, 255.0f));
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
