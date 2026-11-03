#ifndef BATCHPROCESSOR_H
#define BATCHPROCESSOR_H

#include <QString>
#include <QStringList>
#include "processingoptions.h"

class BatchProcessor {
public:
    struct Result {
        int total = 0;
        int ok = 0;
        int errors = 0;
        QStringList errorMessages;
        // Erro que impede o lote de continuar (entrada/saida invalida,
        // falha ao carregar o modelo). O CLI usa isto para o exit code.
        bool fatal = false;
    };

    explicit BatchProcessor(ProcessingOptions options);

    Result processDirectory(
        const QString& inputDir,
        const QString& outputDir);

private:
    ProcessingOptions options_;
};

#endif
