#ifndef MODELPATHRESOLVER_H
#define MODELPATHRESOLVER_H

#include <QString>
#include <QStringList>

class ModelPathResolver {
public:
    static QString resolve(const QString& modelName);
    static QStringList candidates(const QString& modelName);

    // Diretorio de modelos instalados pelo utilizador (aba "Modelos");
    // a GUI descarrega para aqui e o CLI tambem procura aqui.
    static QString userModelsDir();
};

#endif
