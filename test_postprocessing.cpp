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

    std::cout << "PASS" << std::endl;
    return 0;
}
