#include "batchprocessor.h"
#include "backgroundremover.h"
#include "batchplan.h"
#include "imagepostprocessor.h"
#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <iostream>
#include <utility>

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

    // Filtragem de imagens e atribuicao de nomes de saida agora
    // compartilhados com a GUI via BatchPlan (antes duplicado aqui
    // e em MainWindow::processBatch). A filtragem e case-insensitive,
    // portanto "foto.JPG" tambem e processado.
    const QList<BatchPlan::Entry> plan =
        BatchPlan::build(dir, outDir);
    if (plan.isEmpty()) {
        std::cout << "Nenhuma imagem encontrada em: "
                  << inputDir.toStdString() << std::endl;
        return result;
    }

    result.total = plan.size();
    std::cout << "Processando " << plan.size()
              << " imagens..." << std::endl;

    try {
        BackgroundRemover remover(options_.modelPath);

        for (int i = 0; i < plan.size(); ++i) {
            const QString file =
                QFileInfo(plan[i].inputPath).fileName();
            const QString outName =
                QFileInfo(plan[i].outputPath).fileName();

            QImage img(plan[i].inputPath);
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
            if (!processed.save(plan[i].outputPath, "PNG")) {
                result.errors++;
                result.errorMessages.append(
                    "[ERRO] Falha ao salvar: " + plan[i].outputPath);
                std::cerr << "[ERRO] Falha ao salvar: "
                          << plan[i].outputPath.toStdString() << std::endl;
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
