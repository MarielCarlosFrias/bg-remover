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
                                 const ProcessingOptions& options,
                                 const QImage& originalImage)
{
    QImage result = input;
    if (options.cleanNoise) {
        result = applyAlphaThreshold(result, options.alphaThreshold);
    }
    if (options.refineEdges) {
        result = applyEdgeRefinement(result, options.edgeRefinementIntensity);
    }
    if (options.stickerOutline) {
        result = applyStickerOutline(result, options.stickerWidth, options.stickerColor);
    }
    if (options.dropShadow) {
        result = applyDropShadow(result, options.shadowOffsetX, options.shadowOffsetY,
                                 options.shadowBlur, options.shadowOpacity);
    }

    if (options.backgroundStyle == ProcessingOptions::BgBlurredOriginal && !originalImage.isNull()) {
        result = compositeBlurredBackground(result, originalImage, options.blurRadius);
    } else if (options.backgroundStyle == ProcessingOptions::BgCustomImage && !options.backgroundImagePath.isEmpty()) {
        QImage bgImg = loadImage(options.backgroundImagePath);
        if (!bgImg.isNull()) {
            result = compositeBackgroundImage(result, bgImg);
        }
    } else if (options.backgroundStyle == ProcessingOptions::BgSolidColor || options.backgroundColor.isValid()) {
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

QRect ImagePostProcessor::contentBounds(const QImage& img, int alphaThreshold)
{
    if (img.isNull()) return QRect();
    const QImage rgba = img.format() == QImage::Format_RGBA8888
        ? img
        : img.convertToFormat(QImage::Format_RGBA8888);

    const int width = rgba.width();
    const int height = rgba.height();
    const int cutoff = std::clamp(alphaThreshold, 1, 255);

    int minX = width;
    int maxX = -1;
    int minY = height;
    int maxY = -1;

    for (int y = 0; y < height; ++y) {
        const uchar* row = rgba.constScanLine(y);
        for (int x = 0; x < width; ++x) {
            if (row[x * 4 + 3] >= cutoff) {
                if (x < minX) minX = x;
                if (x > maxX) maxX = x;
                if (y < minY) minY = y;
                if (y > maxY) maxY = y;
            }
        }
    }

    if (maxX < minX || maxY < minY) {
        return QRect(0, 0, width, height);
    }

    return QRect(minX, minY, maxX - minX + 1, maxY - minY + 1);
}

QImage ImagePostProcessor::autoCrop(const QImage& img, int paddingPercent)
{
    if (img.isNull()) return img;
    QRect bounds = contentBounds(img);
    if (bounds.isEmpty() || bounds == img.rect()) return img;

    if (paddingPercent > 0) {
        const int padX = std::max(1, bounds.width() * paddingPercent / 100);
        const int padY = std::max(1, bounds.height() * paddingPercent / 100);
        bounds.adjust(-padX, -padY, padX, padY);
        bounds = bounds.intersected(img.rect());
    }

    return img.copy(bounds);
}

QImage ImagePostProcessor::blurImage(const QImage& input, int radius)
{
    if (input.isNull() || radius <= 0) return input.copy();
    const int r = std::clamp(radius, 1, 80);
    QImage src = input.format() == QImage::Format_RGBA8888
        ? input
        : input.convertToFormat(QImage::Format_RGBA8888);

    const int width = src.width();
    const int height = src.height();
    if (width == 0 || height == 0) return src;

    QImage intermediate(width, height, QImage::Format_RGBA8888);
    QImage dst(width, height, QImage::Format_RGBA8888);

    // Passagem 1: Horizontal deslizante
    for (int y = 0; y < height; ++y) {
        const uchar* inRow = src.constScanLine(y);
        uchar* outRow = intermediate.scanLine(y);

        int sumR = 0, sumG = 0, sumB = 0, sumA = 0;
        for (int i = -r; i <= r; ++i) {
            const int px = std::clamp(i, 0, width - 1);
            sumR += inRow[px * 4];
            sumG += inRow[px * 4 + 1];
            sumB += inRow[px * 4 + 2];
            sumA += inRow[px * 4 + 3];
        }

        const int div = 2 * r + 1;
        for (int x = 0; x < width; ++x) {
            outRow[x * 4]     = static_cast<uchar>(sumR / div);
            outRow[x * 4 + 1] = static_cast<uchar>(sumG / div);
            outRow[x * 4 + 2] = static_cast<uchar>(sumB / div);
            outRow[x * 4 + 3] = static_cast<uchar>(sumA / div);

            const int nextX = std::clamp(x + r + 1, 0, width - 1);
            const int prevX = std::clamp(x - r, 0, width - 1);

            sumR += inRow[nextX * 4]     - inRow[prevX * 4];
            sumG += inRow[nextX * 4 + 1] - inRow[prevX * 4 + 1];
            sumB += inRow[nextX * 4 + 2] - inRow[prevX * 4 + 2];
            sumA += inRow[nextX * 4 + 3] - inRow[prevX * 4 + 3];
        }
    }

    // Passagem 2: Vertical deslizante
    const int div = 2 * r + 1;
    for (int x = 0; x < width; ++x) {
        int sumR = 0, sumG = 0, sumB = 0, sumA = 0;
        for (int i = -r; i <= r; ++i) {
            const int py = std::clamp(i, 0, height - 1);
            const uchar* row = intermediate.constScanLine(py);
            sumR += row[x * 4];
            sumG += row[x * 4 + 1];
            sumB += row[x * 4 + 2];
            sumA += row[x * 4 + 3];
        }

        for (int y = 0; y < height; ++y) {
            uchar* outRow = dst.scanLine(y);
            outRow[x * 4]     = static_cast<uchar>(sumR / div);
            outRow[x * 4 + 1] = static_cast<uchar>(sumG / div);
            outRow[x * 4 + 2] = static_cast<uchar>(sumB / div);
            outRow[x * 4 + 3] = static_cast<uchar>(sumA / div);

            const int nextY = std::clamp(y + r + 1, 0, height - 1);
            const int prevY = std::clamp(y - r, 0, height - 1);

            const uchar* nextRow = intermediate.constScanLine(nextY);
            const uchar* prevRow = intermediate.constScanLine(prevY);

            sumR += nextRow[x * 4]     - prevRow[x * 4];
            sumG += nextRow[x * 4 + 1] - prevRow[x * 4 + 1];
            sumB += nextRow[x * 4 + 2] - prevRow[x * 4 + 2];
            sumA += nextRow[x * 4 + 3] - prevRow[x * 4 + 3];
        }
    }

    return dst;
}

QImage ImagePostProcessor::applyStickerOutline(const QImage& input, int width,
                                              const QColor& color)
{
    if (input.isNull() || width <= 0) return input.copy();
    const int r = std::clamp(width, 1, 50);

    const QImage rgba = input.format() == QImage::Format_RGBA8888
        ? input
        : input.convertToFormat(QImage::Format_RGBA8888);

    const int w = rgba.width();
    const int h = rgba.height();

    // 1. Extrair mascara binaria de opacidade
    std::vector<uint8_t> mask(static_cast<size_t>(w) * static_cast<size_t>(h), 0);
    for (int y = 0; y < h; ++y) {
        const uchar* row = rgba.constScanLine(y);
        for (int x = 0; x < w; ++x) {
            if (row[x * 4 + 3] > 15) {
                mask[static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x)] = 255;
            }
        }
    }

    // 2. Dilatação morfológica com kernel circular
    std::vector<uint8_t> dilated = mask;
    std::vector<std::pair<int, int>> offsets;
    const int r2 = r * r;
    for (int dy = -r; dy <= r; ++dy) {
        for (int dx = -r; dx <= r; ++dx) {
            if (dx * dx + dy * dy <= r2) {
                offsets.emplace_back(dx, dy);
            }
        }
    }

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            if (mask[static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x)] == 255) {
                for (const auto& [dx, dy] : offsets) {
                    const int nx = x + dx;
                    const int ny = y + dy;
                    if (nx >= 0 && nx < w && ny >= 0 && ny < h) {
                        dilated[static_cast<size_t>(ny) * static_cast<size_t>(w) + static_cast<size_t>(nx)] = 255;
                    }
                }
            }
        }
    }

    // 3. Montar camada de contorno
    QImage result(w, h, QImage::Format_RGBA8888);
    result.fill(Qt::transparent);

    const uchar red = static_cast<uchar>(color.red());
    const uchar green = static_cast<uchar>(color.green());
    const uchar blue = static_cast<uchar>(color.blue());
    const uchar alpha = static_cast<uchar>(color.alpha());

    for (int y = 0; y < h; ++y) {
        uchar* row = result.scanLine(y);
        for (int x = 0; x < w; ++x) {
            if (dilated[static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x)] > 0) {
                row[x * 4]     = red;
                row[x * 4 + 1] = green;
                row[x * 4 + 2] = blue;
                row[x * 4 + 3] = alpha;
            }
        }
    }

    // 4. Desenhar a imagem recortada por cima
    QPainter p(&result);
    p.drawImage(0, 0, rgba);
    p.end();

    return result;
}

QImage ImagePostProcessor::applyDropShadow(const QImage& input, int offsetX,
                                          int offsetY, int blurRadius,
                                          float opacity)
{
    if (input.isNull()) return input.copy();
    const QImage rgba = input.format() == QImage::Format_RGBA8888
        ? input
        : input.convertToFormat(QImage::Format_RGBA8888);

    const int w = rgba.width();
    const int h = rgba.height();
    const float op = std::clamp(opacity, 0.0f, 1.0f);

    // Cria a camada de silhueta preta com alfa proporcional
    QImage shadow(w, h, QImage::Format_RGBA8888);
    shadow.fill(Qt::transparent);

    for (int y = 0; y < h; ++y) {
        const uchar* srcRow = rgba.constScanLine(y);
        uchar* shadowRow = shadow.scanLine(y);
        for (int x = 0; x < w; ++x) {
            const uchar srcA = srcRow[x * 4 + 3];
            if (srcA > 0) {
                shadowRow[x * 4]     = 0;
                shadowRow[x * 4 + 1] = 0;
                shadowRow[x * 4 + 2] = 0;
                shadowRow[x * 4 + 3] = static_cast<uchar>(srcA * op);
            }
        }
    }

    if (blurRadius > 0) {
        shadow = blurImage(shadow, blurRadius);
    }

    QImage result(w, h, QImage::Format_RGBA8888);
    result.fill(Qt::transparent);

    QPainter p(&result);
    p.drawImage(offsetX, offsetY, shadow);
    p.drawImage(0, 0, rgba);
    p.end();

    return result;
}

QImage ImagePostProcessor::compositeBlurredBackground(const QImage& foreground,
                                                      const QImage& original,
                                                      int blurRadius)
{
    if (foreground.isNull()) return original;
    const QImage base = original.isNull() ? foreground : original;

    QImage bg = base;
    if (bg.size() != foreground.size()) {
        bg = bg.scaled(foreground.size(), Qt::KeepAspectRatioByExpanding,
                       Qt::SmoothTransformation);
        const int cropX = (bg.width() - foreground.width()) / 2;
        const int cropY = (bg.height() - foreground.height()) / 2;
        bg = bg.copy(cropX, cropY, foreground.width(), foreground.height());
    }

    bg = blurImage(bg, blurRadius);
    QImage result = bg.convertToFormat(QImage::Format_RGBA8888);

    QPainter p(&result);
    p.drawImage(0, 0, foreground);
    p.end();

    return result;
}

QImage ImagePostProcessor::compositeBackgroundImage(const QImage& foreground,
                                                    const QImage& backgroundImage)
{
    if (foreground.isNull()) return backgroundImage;
    if (backgroundImage.isNull()) return foreground;

    QImage bg = backgroundImage.scaled(foreground.size(),
                                       Qt::KeepAspectRatioByExpanding,
                                       Qt::SmoothTransformation);
    const int cropX = (bg.width() - foreground.width()) / 2;
    const int cropY = (bg.height() - foreground.height()) / 2;
    bg = bg.copy(cropX, cropY, foreground.width(), foreground.height());

    QImage result = bg.convertToFormat(QImage::Format_RGBA8888);
    QPainter p(&result);
    p.drawImage(0, 0, foreground);
    p.end();

    return result;
}