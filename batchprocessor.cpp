#include "batchprocessor.h"
#include "backgroundremover.h"
#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <QSet>
#include <iostream>

static const QSet<QString> kImageExts = {
    "png", "jpg", "jpeg", "bmp", "webp", "tiff", "tif"
};

BatchProcessor::BatchProcessor(const QString& modelPath)
    : modelPath_(modelPath)
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

    QStringList files;
    QStringList filters;
    for (const QString& ext : kImageExts) {
        filters << QString("*.") + ext;
    }
    QStringList allFiles = dir.entryList(filters, QDir::Files, QDir::Name);
    files = allFiles; // já vem sem duplicatas do entryList com filtros múltiplos

    if (files.isEmpty()) {
        std::cout << "Nenhuma imagem encontrada em: "
                  << inputDir.toStdString() << std::endl;
        return result;
    }

    result.total = files.size();
    std::cout << "Processando " << files.size()
              << " imagens..." << std::endl;

    try {
        BackgroundRemover remover(modelPath_.toStdString());

        for (int i = 0; i < files.size(); ++i) {
            const QString& file = files[i];
            QString inputPath = dir.absoluteFilePath(file);
            QFileInfo fi(file);
            QString outName = fi.baseName() + "_rembg.png";
            QString outputPath = QDir(outputDir).absoluteFilePath(outName);

            QImage img(inputPath);
            if (img.isNull()) {
                result.errors++;
                result.errorMessages.append(
                    "[ERRO] Falha ao carregar: " + file);
                std::cerr << "[ERRO] Falha ao carregar: "
                          << file.toStdString() << std::endl;
                continue;
            }

            QImage processed = remover.removeBackground(img);
            processed.save(outputPath, "PNG");
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
