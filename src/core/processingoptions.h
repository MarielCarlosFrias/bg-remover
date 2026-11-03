#ifndef PROCESSINGOPTIONS_H
#define PROCESSINGOPTIONS_H

#include <QColor>
#include <QString>

struct ProcessingOptions {
    QString modelPath;
    bool cleanNoise = false;
    float alphaThreshold = 30.0f;
    bool refineEdges = false;
    float edgeRefinementIntensity = 0.5f;
    QColor backgroundColor;
    QString outputFormat; // "png", "jpg", "webp" - formato de saida

    enum BackgroundStyle {
        BgTransparent,
        BgSolidColor,
        BgBlurredOriginal,
        BgCustomImage
    };
    BackgroundStyle backgroundStyle = BgTransparent;
    QString backgroundImagePath;
    int blurRadius = 15;

    bool stickerOutline = false;
    int stickerWidth = 6;
    QColor stickerColor = Qt::white;

    bool dropShadow = false;
    int shadowBlur = 10;
    int shadowOffsetX = 4;
    int shadowOffsetY = 6;
    float shadowOpacity = 0.5f;
};

#endif
