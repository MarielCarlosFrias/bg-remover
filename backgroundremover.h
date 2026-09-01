#ifndef BACKGROUNDREMOVER_H
#define BACKGROUNDREMOVER_H

#include <QImage>
#include <onnxruntime_cxx_api.h>
#include <string>
#include <vector>
#include <cstdint>

class BackgroundRemover {
public:
    explicit BackgroundRemover(const std::string& model_path);

    QImage removeBackground(const QImage& input);

    BackgroundRemover(const BackgroundRemover&) = delete;
    BackgroundRemover& operator=(const BackgroundRemover&) = delete;

private:
    std::vector<float> buildInputBlob(const QImage& img) const;
    QImage buildAlphaMask(const float* mask, int orig_w, int orig_h) const;
    QImage composite(const QImage& img, const QImage& alpha) const;

    Ort::Env env_;
    Ort::Session session_;
    std::string input_name_;
    std::string output_name_;

    static constexpr int INPUT_SIZE = 320;
};

#endif
