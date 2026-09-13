#include "batchprocessor.h"
#include "backgroundremover.h"
#include "imagepostprocessor.h"
#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <QRegularExpression>
#include <QSet>
#include <iostream>
#include <utility>

static const QSet<QString> kImageExts = {
    "png", "jpg", "jpeg", "bmp", "webp", "tiff", "tif"
};
static const QRegularExpression kGeneratedOutputName(
    "_rembg(?:_[0-9]+)?$");

BatchProcessor::BatchProcessor(ProcessingOptions options)
    : options_(std::move(options))
{}

BatchProcessor::Result BatchProcessor::processDirectory(
    const QString& inputDir,
    const QString& outputDir)
{
    Result result;
    QDir dir(inputDir);

    if (!dir.exists()) {
        std::cerr << "Diretorio de entrada nao existe: "
                  << inputDir.toStdString() << std::endl;
        return result;
    }

    QDir outDir(outputDir);
    if (!outDir.exists() && !outDir.mkpath(".")) {
        std::cerr << "Nao foi possivel criar diretorio de saida: "
                  << outputDir.toStdString() << std::endl;
        return result;
    }

    // BUGFIX: entryList(filters, ...) e case-sensitive no Linux
    // (mesmo bug ja corrigido antes em mainwindow.cpp processBatch()) —
    // "foto.JPG" seria ignorado. Filtro manual case-insensitive.
    QStringList files;
    const bool outputIsInputDirectory =
        QDir::cleanPath(dir.absolutePath())
        == QDir::cleanPath(outDir.absolutePath());
    QStringList allFiles = dir.entryList(QDir::Files, QDir::Name);
    for (const QString& f : allFiles) {
        const QFileInfo fileInfo(f);
        const bool isGeneratedOutput = outputIsInputDirectory
            && fileInfo.suffix().compare("png", Qt::CaseInsensitive) == 0
            && kGeneratedOutputName.match(
                fileInfo.completeBaseName()).hasMatch();
        if (kImageExts.contains(fileInfo.suffix().toLower())
            && !isGeneratedOutput) {
            files << f;
        }
    }

    if (files.isEmpty()) {
        std::cout << "Nenhuma imagem encontrada em: "
                  << inputDir.toStdString() << std::endl;
        return result;
    }

    result.total = files.size();
    std::cout << "Processando " << files.size()
              << " imagens..." << std::endl;

    try {
        BackgroundRemover remover(options_.modelPath);
        QSet<QString> reservedOutputPaths;

        for (int i = 0; i < files.size(); ++i) {
            const QString& file = files[i];
            QString inputPath = dir.absoluteFilePath(file);
            QFileInfo fi(file);
            const QString outputBase = fi.completeBaseName() + "_rembg";
            QString outName = outputBase + ".png";
            QString outputPath = outDir.absoluteFilePath(outName);
            for (int suffix = 2;
                 reservedOutputPaths.contains(outputPath)
                 || QFileInfo::exists(outputPath);
                 ++suffix) {
                outName = outputBase + "_" + QString::number(suffix)
                    + ".png";
                outputPath = outDir.absoluteFilePath(outName);
            }
            reservedOutputPaths.insert(outputPath);

            QImage img(inputPath);
            if (img.isNull()) {
                result.errors++;
                result.errorMessages.append(
                    "[ERRO] Falha ao carregar: " + file);
                std::cerr << "[ERRO] Falha ao carregar: "
                          << file.toStdString() << std::endl;
                continue;
            }

            QImage processed = ImagePostProcessor::apply(
                remover.removeBackground(img), options_);
            if (!processed.save(outputPath, "PNG")) {
                result.errors++;
                result.errorMessages.append(
                    "[ERRO] Falha ao salvar: " + outputPath);
                std::cerr << "[ERRO] Falha ao salvar: "
                          << outputPath.toStdString() << std::endl;
                continue;
            }
            result.ok++;

            std::cout << "[" << (i + 1) << "/"
                      << result.total << "] " << file.toStdString()
                      << " -> " << outName.toStdString()
                      << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "Falha no modelo: "
                  << e.what() << std::endl;
        result.errors = result.total;
    }

    return result;
}
