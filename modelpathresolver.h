#ifndef MODELPATHRESOLVER_H
#define MODELPATHRESOLVER_H

#include <QString>
#include <QStringList>

class ModelPathResolver {
public:
    static QString resolve(const QString& modelName);
    static QStringList candidates(const QString& modelName);
};

#endif
