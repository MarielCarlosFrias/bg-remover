#include "imagepostprocessor.h"

#include <algorithm>

QImage ImagePostProcessor::apply(const QImage& input,
                                 const ProcessingOptions& options)
{
    QImage result = input;
    if (options.cleanNoise) {
        result = applyAlphaThreshold(result, options.alphaThreshold);
    }
    if (options.refineEdges) {
        result = applyEdgeRefinement(result, options.edgeRefinementIntensity);
    }
    if (options.backgroundColor.isValid()) {
        result = compositeBackground(result, options.backgroundColor);
    }
    return result;
}

QImage ImagePostProcessor::applyAlphaThreshold(
    const QImage& input, float threshold)
{
    if (input.isNull() || threshold <= 0.0f) return input.copy();
    QImage result = input.convertToFormat(QImage::Format_RGBA8888);
    for (int y = 0; y < result.height(); ++y) {
        for (int x = 0; x < result.width(); ++x) {
            QRgb px = result.pixel(x, y);
            if (qAlpha(px) < static_cast<int>(threshold)) {
                result.setPixel(x, y, qRgba(
                    qRed(px), qGreen(px), qBlue(px), 0));
            }
        }
    }
    return result;
}

QImage ImagePostProcessor::applyEdgeRefinement(
    const QImage& input, float intensity)
{
    if (input.isNull() || intensity <= 0.0f) return input.copy();
    QImage result = input.convertToFormat(QImage::Format_RGBA8888);
    QImage refined = result.copy();
    const int width = result.width();
    const int height = result.height();
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            float alphaSum = 0.0f;
            for (int dy = -1; dy <= 1; ++dy) {
                for (int dx = -1; dx <= 1; ++dx) {
                    alphaSum += qAlpha(result.pixel(
                        qBound(0, x + dx, width - 1),
                        qBound(0, y + dy, height - 1)));
                }
            }
            const QRgb px = result.pixel(x, y);
            const float alpha = qAlpha(px);
            const int refinedAlpha = static_cast<int>(std::clamp(
                alpha + (alphaSum / 9.0f - alpha) * intensity,
                0.0f, 255.0f));
            refined.setPixel(x, y, qRgba(
                qRed(px), qGreen(px), qBlue(px), refinedAlpha));
        }
    }
    return refined;
}

QImage ImagePostProcessor::compositeBackground(
    const QImage& input, const QColor& backgroundColor)
{
    if (input.isNull()) return QImage();
    QImage result(input.size(), QImage::Format_RGBA8888);
    result.fill(backgroundColor);
    for (int y = 0; y < input.height(); ++y) {
        for (int x = 0; x < input.width(); ++x) {
            const QRgb src = input.pixel(x, y);
            const int alpha = qAlpha(src);
            if (alpha == 255) {
                result.setPixel(x, y, src);
            } else if (alpha > 0) {
                const QRgb dst = result.pixel(x, y);
                const float opacity = alpha / 255.0f;
                result.setPixel(x, y, qRgba(
                    static_cast<int>(qRed(src) * opacity + qRed(dst) * (1 - opacity)),
                    static_cast<int>(qGreen(src) * opacity + qGreen(dst) * (1 - opacity)),
                    static_cast<int>(qBlue(src) * opacity + qBlue(dst) * (1 - opacity)), 255));
            }
        }
    }
    return result;
}
