#include "imagepostprocessor.h"

#include <QImageReader>
#include <QImageWriter>
#include <QPainter>
#include <algorithm>
#include <cmath>

// Todas as operacoes trabalham em Format_RGBA8888, cujo canal alfa
// fica no byte 3 de cada pixel. O acesso por scanline evita
// pixel()/setPixel() por pixel (lento em imagens grandes).

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
    const int cutoff = std::clamp(static_cast<int>(threshold), 0, 255);
    for (int y = 0; y < result.height(); ++y) {
        uchar* row = result.scanLine(y);
        for (int x = 0; x < result.width(); ++x) {
            if (row[x * 4 + 3] < cutoff) row[x * 4 + 3] = 0;
        }
    }
    return result;
}

QImage ImagePostProcessor::applyEdgeRefinement(
    const QImage& input, float intensity)
{
    if (input.isNull() || intensity <= 0.0f) return input.copy();
    QImage result = input.convertToFormat(QImage::Format_RGBA8888);
    QImage refined = result;
    const int width = result.width();
    const int height = result.height();
    for (int y = 0; y < height; ++y) {
        const uchar* r0 = result.constScanLine(qMax(0, y - 1));
        const uchar* r1 = result.constScanLine(y);
        const uchar* r2 = result.constScanLine(qMin(height - 1, y + 1));
        uchar* dst = refined.scanLine(y);
        for (int x = 0; x < width; ++x) {
            const int left  = qMax(0, x - 1);
            const int right = qMin(width - 1, x + 1);
            const int sum =
                r0[left * 4 + 3] + r0[x * 4 + 3] + r0[right * 4 + 3] +
                r1[left * 4 + 3] + r1[x * 4 + 3] + r1[right * 4 + 3] +
                r2[left * 4 + 3] + r2[x * 4 + 3] + r2[right * 4 + 3];
            const float avg = sum / 9.0f;
            const float alpha = r1[x * 4 + 3];
            dst[x * 4 + 3] = static_cast<uchar>(std::clamp(
                alpha + (avg - alpha) * intensity, 0.0f, 255.0f));
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
    QPainter painter(&result);
    painter.drawImage(0, 0, input);
    painter.end();
    return result;
}

QImage ImagePostProcessor::loadImage(const QString& path)
{
    QImageReader reader(path);
    reader.setAutoTransform(true);
    return reader.read();
}