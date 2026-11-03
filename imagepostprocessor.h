#ifndef IMAGEPOSTPROCESSOR_H
#define IMAGEPOSTPROCESSOR_H

#include <QImage>
#include <QString>
#include "processingoptions.h"

class ImagePostProcessor {
public:
    static QImage apply(const QImage& input,
                        const ProcessingOptions& options);
    static QImage applyAlphaThreshold(const QImage& input,
                                      float threshold);
    static QImage applyEdgeRefinement(const QImage& input,
                                      float intensity);
    static QImage compositeBackground(const QImage& input,
                                     const QColor& backgroundColor);

    // Carrega uma imagem respeitando a orientação EXIF (fotos de telemóvel).
    static QImage loadImage(const QString& path);
};

#endif