#ifndef MODELVALIDATOR_H
#define MODELVALIDATOR_H

#include <QString>
#include <QtGlobal>

// Estado de um modelo ONNX instalado, verificado sem o carregar no
// ONNX Runtime (rapido e seguro para a UI).
enum class ModelFileState {
    Missing,     // ficheiro nao existe
    Installed,   // tamanho (e SHA256, quando conhecido) corretos
    Incomplete,  // ficheiro menor que o tamanho esperado
    Corrupt      // tamanho inesperado ou SHA256 diferente
};

struct ModelFileValidation {
    ModelFileState state = ModelFileState::Missing;
    qint64 actualSize = 0;
};

// Valida um modelo instalado:
// - expectedSize > 0: tamanho menor -> Incomplete; diferente -> Corrupt
// - expectedSha256Hex nao vazio: hash diferente -> Corrupt
// expectedSize <= 0 e expectedSha256Hex vazio desativam as verificacoes
// correspondentes (util para uma primeira verificacao so por tamanho).
ModelFileValidation validateModelFile(const QString& path,
                                      qint64 expectedSize,
                                      const QString& expectedSha256Hex);

#endif
