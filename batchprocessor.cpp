#include "batchprocessor.h"
#include "backgroundremover.h"
#include "batchplan.h"
#include "i18n.h"
#include "imagepostprocessor.h"
#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <QImageReader>
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
        std::cerr << I18n::tr("Diretorio de entrada nao existe: ",
                              "Input directory does not exist: ").toStdString()
                  << inputDir.toStdString() << std::endl;
        result.fatal = true;
        return result;
    }

    QDir outDir(outputDir);
    if (!outDir.exists() && !outDir.mkpath(".")) {
        std::cerr << I18n::tr("Nao foi possivel criar diretorio de saida: ",
                              "Could not create output directory: ").toStdString()
                  << outputDir.toStdString() << std::endl;
        result.fatal = true;
        return result;
    }

    // Filtragem de imagens e atribuicao de nomes de saida agora
    // compartilhados com a GUI via BatchPlan (antes duplicado aqui
    // e em MainWindow::processBatch). A filtragem e case-insensitive,
    // portanto "foto.JPG" tambem e processado.
    const QList<BatchPlan::Entry> plan =
        BatchPlan::build(dir, outDir);
    if (plan.isEmpty()) {
        std::cout << I18n::tr("Nenhuma imagem encontrada em: ",
                              "No images found in: ").toStdString()
                  << inputDir.toStdString() << std::endl;
        return result;
    }

    result.total = plan.size();
    std::cout << I18n::tr("Processando ", "Processing ").toStdString()
              << plan.size()
              << I18n::tr(" imagens...", " images...").toStdString() << std::endl;

    try {
        BackgroundRemover remover(options_.modelPath);

        const QString format = options_.outputFormat.isEmpty()
            ? QStringLiteral("png")
            : options_.outputFormat;

        for (int i = 0; i < plan.size(); ++i) {
            const QString file =
                QFileInfo(plan[i].inputPath).fileName();
            const QString outName =
                QFileInfo(plan[i].outputPath).fileName();

            const QImage img = ImagePostProcessor::loadImage(plan[i].inputPath);
            if (img.isNull()) {
                ++result.errors;
                const QString err = I18n::tr("Falha ao carregar a imagem",
                                             "Failed to load the image");
                result.errorMessages.append(
                    "[ERRO] " + file + ": " + err);
                std::cerr << "[ERRO] " << file.toStdString()
                          << ": " << err.toStdString()
                          << std::endl;
                continue;
            }

            QImage processed = ImagePostProcessor::apply(
                remover.removeBackground(img), options_);

            if (!processed.save(plan[i].outputPath, format.toUtf8().constData())) {
                ++result.errors;
                const QString err = I18n::tr("Falha ao salvar a imagem",
                                             "Failed to save the image");
                result.errorMessages.append(
                    "[ERRO] " + file + ": " + err);
                std::cerr << "[ERRO] " << file.toStdString()
                          << ": " << err.toStdString()
                          << std::endl;
                continue;
            }
            ++result.ok;

            std::cout << "[" << (i + 1) << "/"
                      << result.total << "] " << file.toStdString()
                      << " -> " << outName.toStdString()
                      << std::endl;
        }
    } catch (const std::exception& e) {
        result.fatal = true;
        const QString msg = I18n::tr(
            "Falha no modelo: %1", "Model failure: %1")
            .arg(QString::fromUtf8(e.what()));
        result.errorMessages.append(msg);
        std::cerr << msg.toStdString() << std::endl;
        // As imagens ainda nao processadas tambem contam como erro,
        // sem apagar o que ja foi feito com sucesso.
        const int processed = result.ok + result.errors;
        if (processed < result.total)
            result.errors += result.total - processed;
    } catch (...) {
        result.fatal = true;
        const QString msg = I18n::tr(
            "Falha desconhecida no modelo.",
            "Unknown model failure.");
        result.errorMessages.append(msg);
        std::cerr << msg.toStdString() << std::endl;
        const int processed = result.ok + result.errors;
        if (processed < result.total)
            result.errors += result.total - processed;
    }

    return result;
}