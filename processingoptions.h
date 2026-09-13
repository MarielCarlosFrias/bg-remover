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
};

#endif
