#ifndef BATCHPROCESSOR_H
#define BATCHPROCESSOR_H

#include <QString>
#include <QStringList>

class BatchProcessor {
public:
    struct Result {
        int total = 0;
        int ok = 0;
        int errors = 0;
        QStringList errorMessages;
    };

    explicit BatchProcessor(const QString& modelPath);

    Result processDirectory(
        const QString& inputDir,
        const QString& outputDir);

private:
    QString modelPath_;
};

#endif
