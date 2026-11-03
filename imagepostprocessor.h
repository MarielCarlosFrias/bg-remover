#ifndef IMAGEPOSTPROCESSOR_H
#define IMAGEPOSTPROCESSOR_H

#include <QImage>
#include <QString>
#include "processingoptions.h"

class ImagePostProcessor {
public:
    static QImage apply(const QImage& input,
                        const ProcessingOptions& options,
                        const QImage& originalImage = QImage());
    static QImage applyAlphaThreshold(const QImage& input,
                                      float threshold);
    static QImage applyEdgeRefinement(const QImage& input,
                                      float intensity);
    static QImage compositeBackground(const QImage& input,
                                     const QColor& backgroundColor);

    // Auto-recorte: remove margens transparentes com padding percentual opcional.
    static QRect contentBounds(const QImage& img, int alphaThreshold = 5);
    static QImage autoCrop(const QImage& img, int paddingPercent = 0);

    // Efeitos visuais: contorno de adesivo (sticker) e sombra projetada (drop shadow).
    static QImage applyStickerOutline(const QImage& input, int width,
                                      const QColor& color = Qt::white);
    static QImage applyDropShadow(const QImage& input, int offsetX = 4,
                                  int offsetY = 6, int blurRadius = 10,
                                  float opacity = 0.5f);

    // Desfoque rápido e composições avançadas de fundo.
    static QImage blurImage(const QImage& input, int radius);
    static QImage compositeBlurredBackground(const QImage& foreground,
                                             const QImage& original,
                                             int blurRadius = 15);
    static QImage compositeBackgroundImage(const QImage& foreground,
                                           const QImage& backgroundImage);

    // Carrega uma imagem respeitando a orientação EXIF (fotos de telemóvel).
    static QImage loadImage(const QString& path);
};

#endif