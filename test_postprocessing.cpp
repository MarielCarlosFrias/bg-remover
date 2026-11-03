#include <QImage>
#include <iostream>

#include "imagepostprocessor.h"

int main()
{
    QImage alphaImage(3, 3, QImage::Format_RGBA8888);
    alphaImage.fill(qRgba(10, 20, 30, 20));
    alphaImage.setPixel(1, 1, qRgba(10, 20, 30, 255));

    const QImage thresholded = ImagePostProcessor::applyAlphaThreshold(
        alphaImage, 30.0f);
    if (qAlpha(thresholded.pixel(0, 0)) != 0
        || qAlpha(thresholded.pixel(1, 1)) != 255) {
        std::cerr << "FAIL: threshold" << std::endl;
        return 1;
    }

    const QImage refined = ImagePostProcessor::applyEdgeRefinement(
        thresholded, 0.5f);
    if (qAlpha(refined.pixel(0, 1)) == 0) {
        std::cerr << "FAIL: refinement" << std::endl;
        return 1;
    }

    QImage translucent(1, 1, QImage::Format_RGBA8888);
    translucent.setPixel(0, 0, qRgba(255, 0, 0, 128));
    const QImage composited = ImagePostProcessor::compositeBackground(
        translucent, Qt::white);
    const QRgb pixel = composited.pixel(0, 0);
    if (qRed(pixel) != 255 || qGreen(pixel) < 126 || qGreen(pixel) > 128
        || qBlue(pixel) < 126 || qBlue(pixel) > 128 || qAlpha(pixel) != 255) {
        std::cerr << "FAIL: composite" << std::endl;
        return 1;
    }

    // Teste de Auto-Crop
    QImage cropSource(30, 30, QImage::Format_RGBA8888);
    cropSource.fill(Qt::transparent);
    for (int y = 10; y < 20; ++y) {
        for (int x = 10; x < 20; ++x) {
            cropSource.setPixel(x, y, qRgba(255, 0, 0, 255));
        }
    }
    const QImage cropped = ImagePostProcessor::autoCrop(cropSource, 0);
    if (cropped.width() != 10 || cropped.height() != 10) {
        std::cerr << "FAIL: autoCrop dimensions expected 10x10, got "
                  << cropped.width() << "x" << cropped.height() << std::endl;
        return 1;
    }

    // Teste de Sticker Outline
    const QImage sticker = ImagePostProcessor::applyStickerOutline(
        cropSource, 2, Qt::white);
    if (sticker.size() != cropSource.size() || qAlpha(sticker.pixel(8, 10)) == 0) {
        std::cerr << "FAIL: sticker outline" << std::endl;
        return 1;
    }

    // Teste de Drop Shadow
    const QImage shadowed = ImagePostProcessor::applyDropShadow(
        cropSource, 2, 2, 4, 0.5f);
    if (shadowed.size() != cropSource.size() || qAlpha(shadowed.pixel(21, 21)) == 0) {
        std::cerr << "FAIL: drop shadow" << std::endl;
        return 1;
    }

    // Teste de Blur
    const QImage blurred = ImagePostProcessor::blurImage(cropSource, 3);
    if (blurred.size() != cropSource.size()) {
        std::cerr << "FAIL: blur" << std::endl;
        return 1;
    }

    std::cout << "PASS" << std::endl;
    return 0;
}
