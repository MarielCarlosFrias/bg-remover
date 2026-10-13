#ifndef BATCHPLAN_H
#define BATCHPLAN_H

#include <QDir>
#include <QFileInfo>
#include <QList>
#include <QRegularExpression>
#include <QSet>
#include <QString>

// Logica compartilhada entre a GUI (MainWindow::processBatch) e o CLI
// (BatchProcessor::processDirectory): filtragem de imagens do diretorio
// de entrada e atribuicao de nomes de saida que nao sobrescrevem
// ficheiros existentes nem outros caminhos ja reservados no mesmo lote.

namespace BatchPlan {

inline const QSet<QString>& imageExts()
{
    static const QSet<QString> exts = {
        "png", "jpg", "jpeg", "bmp", "webp", "tiff", "tif"
    };
    return exts;
}

inline const QRegularExpression& generatedOutputName()
{
    static const QRegularExpression re("_rembg(?:_[0-9]+)?$");
    return re;
}

struct Entry {
    QString inputPath;   // caminho absoluto da imagem de entrada
    QString outputPath;  // caminho absoluto unico de saida (PNG)
};

inline bool isGeneratedOutputName(const QFileInfo& fileInfo)
{
    return fileInfo.suffix().compare("png", Qt::CaseInsensitive) == 0
        && generatedOutputName().match(
               fileInfo.completeBaseName()).hasMatch();
}

// Lista as imagens de inputDir e atribui a cada uma um caminho de saida
// em outputDir com sufixo _rembg. Se inputDir == outputDir, os
// resultados gerados anteriormente (_rembg, _rembg_2, ...) sao
// ignorados como entrada para nao reprocessar as proprias saidas.
inline QList<Entry> build(const QDir& inputDir, const QDir& outputDir)
{
    const bool outputIsInputDirectory =
        QDir::cleanPath(inputDir.absolutePath())
        == QDir::cleanPath(outputDir.absolutePath());

    QList<Entry> entries;
    QSet<QString> reserved;

    const QStringList allFiles = inputDir.entryList(
        QDir::Files | QDir::NoDotAndDotDot, QDir::Name);
    for (const QString& f : allFiles) {
        const QFileInfo fileInfo(f);
        if (!imageExts().contains(fileInfo.suffix().toLower()))
            continue;
        if (outputIsInputDirectory && isGeneratedOutputName(fileInfo))
            continue;

        const QString outputBase = fileInfo.completeBaseName() + "_rembg";
        QString outputPath = outputDir.absoluteFilePath(outputBase + ".png");
        for (int suffix = 2;
             reserved.contains(outputPath)
                 || QFileInfo::exists(outputPath);
             ++suffix) {
            outputPath = outputDir.absoluteFilePath(
                outputBase + "_" + QString::number(suffix) + ".png");
        }
        reserved.insert(outputPath);

        Entry entry;
        entry.inputPath = inputDir.absoluteFilePath(f);
        entry.outputPath = outputPath;
        entries.append(entry);
    }
    return entries;
}

} // namespace BatchPlan

#endif
