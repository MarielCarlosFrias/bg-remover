#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QStringList>
#include <iostream>
#include "batchprocessor.h"
#include "i18n.h"
#include "modelpathresolver.h"

namespace {
#ifdef BG_REMOVER_VERSION
const QString kVersion = QStringLiteral(BG_REMOVER_VERSION);
#else
const QString kVersion = QStringLiteral("1.0.0");
#endif
} // namespace

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    app.setApplicationName("bg_remover_cli");
    app.setApplicationVersion(kVersion);

    QCommandLineParser parser;
    parser.setApplicationDescription(
        I18n::tr("Remoção de fundos de imagens localmente.",
                 "Offline image background removal."));

    const QCommandLineOption helpOption(
        QStringList() << "h" << "help",
        I18n::tr("Mostra esta ajuda.", "Show this help."));
    const QCommandLineOption versionOption(
        QStringList() << "v" << "version",
        I18n::tr("Mostra a versão.", "Show the version."));
    const QCommandLineOption outputOption(
        QStringList() << "o" << "output",
        I18n::tr("Diretório de saída (padrão: _rembg_output).",
                 "Output directory (default: _rembg_output)."),
        I18n::tr("pasta", "folder"));
    const QCommandLineOption modelOption(
        QStringList() << "m" << "model",
        I18n::tr("Caminho do modelo ONNX.",
                 "Path to the ONNX model."),
        I18n::tr("modelo.onnx", "model.onnx"));
    const QCommandLineOption formatOption(
        QStringList() << "f" << "format",
        I18n::tr("Formato de saída (png, jpg, webp).",
                 "Output format (png, jpg, webp)."),
        I18n::tr("formato", "format"),
        QStringLiteral("png"));

    parser.addOption(helpOption);
    parser.addOption(versionOption);
    parser.addOption(outputOption);
    parser.addOption(modelOption);
    parser.addOption(formatOption);
    parser.addPositionalArgument(
        I18n::tr("entrada", "input"),
        I18n::tr("Diretório com imagens.", "Directory with images."));
    parser.addPositionalArgument(
        I18n::tr("saída", "output"),
        I18n::tr("Diretório de saída (opcional).",
                 "Output directory (optional)."),
        I18n::tr("[saída]", "[output]"));
    parser.addPositionalArgument(
        I18n::tr("modelo", "model"),
        I18n::tr("Modelo ONNX (opcional).",
                 "ONNX model (optional)."),
        I18n::tr("[modelo.onnx]", "[model.onnx]"));

    const auto printHelp = [&parser]() {
        std::cout << parser.helpText().toStdString()
                  << I18n::tr(
                         "\nExemplos:\n"
                         "  bg_remover_cli ~/imagens ~/saida\n"
                         "  bg_remover_cli ~/imagens --model u2net.onnx\n"
                         "  bg_remover_cli ~/imagens -o ~/saida -m u2net.onnx\n",
                         "\nExamples:\n"
                         "  bg_remover_cli ~/pictures ~/output\n"
                         "  bg_remover_cli ~/pictures --model u2net.onnx\n"
                         "  bg_remover_cli ~/pictures -o ~/output -m u2net.onnx\n")
                         .toStdString()
                  << std::flush;
    };

    if (!parser.parse(app.arguments())) {
        std::cerr << parser.errorText().toStdString() << "\n\n";
        printHelp();
        return 1;
    }

    if (parser.isSet(helpOption)) {
        printHelp();
        return 0;
    }
    if (parser.isSet(versionOption)) {
        std::cout << app.applicationName().toStdString() << " "
                  << app.applicationVersion().toStdString() << "\n";
        return 0;
    }

    const QStringList positional = parser.positionalArguments();
    if (positional.isEmpty()) {
        std::cerr << I18n::tr("Informe a pasta de entrada.",
                              "The input folder is required.")
                         .toStdString()
                  << "\n\n";
        printHelp();
        return 1;
    }
    if (positional.size() > 3) {
        std::cerr << I18n::tr("Argumentos a mais. Use --help para ver o uso.",
                              "Too many arguments. Use --help for usage.")
                         .toStdString()
                  << "\n";
        return 1;
    }

    QString inputDir = positional[0];
    QString outputDir = parser.value(outputOption);
    QString modelPath = parser.value(modelOption);

    // Compatibilidade com os formatos posicionais antigos:
    //   <entrada> <saída> [modelo.onnx]
    //   <entrada> <modelo.onnx> [saída]
    const QString pos2 = positional.size() > 1 ? positional[1] : QString();
    const QString pos3 = positional.size() > 2 ? positional[2] : QString();
    if (!pos2.isEmpty()) {
        if (pos2.endsWith(".onnx", Qt::CaseInsensitive)) {
            if (modelPath.isEmpty()) modelPath = pos2;
            if (!pos3.isEmpty() && outputDir.isEmpty()) outputDir = pos3;
        } else {
            if (outputDir.isEmpty()) outputDir = pos2;
            if (!pos3.isEmpty()) {
                if (!pos3.endsWith(".onnx", Qt::CaseInsensitive)) {
                    std::cerr << I18n::tr(
                        "O modelo deve terminar em .onnx: ",
                        "The model must end in .onnx: ").toStdString()
                              << pos3.toStdString() << "\n";
                    return 1;
                }
                if (modelPath.isEmpty()) modelPath = pos3;
            }
        }
    }

    if (outputDir.isEmpty()) outputDir = "_rembg_output";
    if (modelPath.isEmpty())
        modelPath = ModelPathResolver::resolve("u2netp.onnx");

    if (!QFile::exists(modelPath)) {
        std::cerr << I18n::tr("Modelo nao encontrado: ",
                              "Model not found: ").toStdString()
                  << modelPath.toStdString() << std::endl;
        return 1;
    }

    if (!QDir(inputDir).exists()) {
        std::cerr << I18n::tr("Diretorio de entrada nao existe: ",
                              "Input directory does not exist: ").toStdString()
                  << inputDir.toStdString() << std::endl;
        return 1;
    }

    std::cout << I18n::tr("Modelo: ", "Model: ").toStdString()
              << modelPath.toStdString()
              << "\n" << I18n::tr("Entrada: ", "Input: ").toStdString()
              << inputDir.toStdString()
              << "\n" << I18n::tr("Saida: ", "Output: ").toStdString()
              << outputDir.toStdString()
              << "\n---\n" << std::flush;

    ProcessingOptions options;
    options.modelPath = modelPath;
    options.outputFormat = parser.value(formatOption);
    BatchProcessor processor(options);
    BatchProcessor::Result r = processor.processDirectory(
        inputDir, outputDir);

    std::cout << "---\n"
              << (r.fatal
                      ? I18n::tr("Lote interrompido: ",
                                 "Batch interrupted: ")
                      : I18n::tr("Concluido: ", "Completed: "))
                     .toStdString()
              << r.ok << "/" << r.total << " OK, "
              << r.errors << " "
              << I18n::tr("erros", "errors").toStdString() << "\n";

    if (!r.errorMessages.isEmpty()) {
        std::cout << I18n::tr("Erros:", "Errors:").toStdString() << "\n";
        for (const QString& msg : r.errorMessages) {
            std::cout << "  " << msg.toStdString() << "\n";
        }
    }

    return (r.fatal || r.errors > 0) ? 1 : 0;
}
