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
    QString outputPath;  // caminho absoluto unico de saida (formato escolhido)
};

// Extensao normalizada: sem ponto inicial, em minusculas, com fallback
// para PNG quando o valor recebido esta vazio.
inline QString normalizedExtension(const QString& extension)
{
    QString ext = extension.trimmed().toLower();
    while (ext.startsWith('.')) ext.remove(0, 1);
    return ext.isEmpty() ? QStringLiteral("png") : ext;
}

inline QString pathKey(const QString& path, bool caseInsensitive)
{
    return caseInsensitive ? path.toCaseFolded() : path;
}

inline bool defaultCaseInsensitivePaths()
{
#if defined(Q_OS_WIN) || defined(Q_OS_MACOS)
    // NTFS e APFS sao case-insensitive por omissao. Em Linux o mesmo
    // nome com caixa diferente sao ficheiros distintos.
    return true;
#else
    return false;
#endif
}

inline bool isGeneratedOutputName(const QFileInfo& fileInfo)
{
    // Qualquer imagem gerada pelo lote (PNG, JPEG ou WebP) tem o
    // sufixo _rembg. Ignorar todas evita reprocessar saidas antigas
    // quando entrada e saida sao a mesma pasta.
    return imageExts().contains(fileInfo.suffix().toLower())
        && generatedOutputName().match(
               fileInfo.completeBaseName()).hasMatch();
}

// Lista as imagens de inputDir e atribui a cada uma um caminho de saida
// em outputDir com sufixo _rembg. Se inputDir == outputDir, os
// resultados gerados anteriormente (_rembg, _rembg_2, ...) sao
// ignorados como entrada para nao reprocessar as proprias saidas.
//
// outputExtension define a extensao dos resultados (png, jpg, webp...).
// caseInsensitivePaths deve ser true em sistemas de ficheiros que nao
// distinguem maiusculas de minusculas; nesses sistemas dois nomes-base
// que so diferem na caixa colidiriam no mesmo ficheiro de saida.
inline QList<Entry> build(
    const QDir& inputDir, const QDir& outputDir,
    const QString& outputExtension = QStringLiteral("png"),
    bool caseInsensitivePaths = defaultCaseInsensitivePaths())
{
    const QString extension = normalizedExtension(outputExtension);
    const auto comparablePath = [caseInsensitivePaths](const QString& path) {
        const QFileInfo info(path);
        const QString canonical = info.canonicalFilePath();
        return pathKey(canonical.isEmpty() ? info.absoluteFilePath()
                                           : canonical,
                       caseInsensitivePaths);
    };
    const bool outputIsInputDirectory =
        comparablePath(inputDir.absolutePath())
        == comparablePath(outputDir.absolutePath());

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
        QString outputPath = outputDir.absoluteFilePath(
            outputBase + "." + extension);
        for (int suffix = 2;
             reserved.contains(pathKey(outputPath, caseInsensitivePaths))
                 || QFileInfo::exists(outputPath);
             ++suffix) {
            outputPath = outputDir.absoluteFilePath(
                outputBase + "_" + QString::number(suffix)
                + "." + extension);
        }
        reserved.insert(pathKey(outputPath, caseInsensitivePaths));

        Entry entry;
        entry.inputPath = inputDir.absoluteFilePath(f);
        entry.outputPath = outputPath;
        entries.append(entry);
    }
    return entries;
}

} // namespace BatchPlan

#endif
