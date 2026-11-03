#ifndef MODELPATHRESOLVER_H
#define MODELPATHRESOLVER_H

#include <QString>
#include <QStringList>

class ModelPathResolver {
public:
    static QString resolve(const QString& modelName);
    static QStringList candidates(const QString& modelName);

    // Diretorio canonico de modelos instalados pelo utilizador (aba
    // "Modelos"); a GUI descarrega para aqui e o CLI tambem procura aqui.
    static QString userModelsDir();

    // Caminhos usados por versoes anteriores (GUI e CLI tinham nomes de
    // aplicacao diferentes). Servem para continuar a encontrar modelos
    // ja instalados sem obrigar a descarregar tudo outra vez.
    static QStringList legacyUserModelsDirs();

    // Devolve o caminho de um modelo instalado, canonico ou antigo.
    // String vazia se nao existir.
    static QString findUserModel(const QString& modelName);

    // Todas as copias instaladas (canonica + diretorios antigos).
    // Usado para remover o modelo por completo, sem deixar copias
    // antigas a reaparecer na lista.
    static QStringList findAllUserModels(const QString& modelName);
};

#endif
