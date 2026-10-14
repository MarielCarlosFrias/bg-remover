#include <QCoreApplication>
#include <QStringList>
#include <QDir>
#include <QFile>
#include <iostream>
#include "batchprocessor.h"
#include "i18n.h"
#include "modelpathresolver.h"

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    app.setApplicationName("bg_remover_cli");

    const auto printHelp = []() {
        std::cout
            << "bg_remover_cli 1.0.0 — "
            << I18n::tr("remoção de fundos de imagens localmente",
                        "offline image background removal").toStdString()
            << "\n\n"
            << I18n::tr(
                "Uso: bg_remover_cli <pasta_entrada> [pasta_saida] [modelo.onnx]\n"
                "  pasta_entrada: diretorio com imagens\n"
                "  pasta_saida:   diretorio de saida (padrao: _rembg_output)\n"
                "  modelo.onnx:   caminho do modelo ONNX (padrao: u2netp.onnx;\n"
                "                 procurado junto ao executavel e em models/)\n\n"
                "Opcoes:\n"
                "  -h, --help     mostra esta ajuda\n"
                "  -v, --version  mostra a versao\n",
                "Usage: bg_remover_cli <input_folder> [output_folder] [model.onnx]\n"
                "  input_folder: directory with images\n"
                "  output_folder: output directory (default: _rembg_output)\n"
                "  model.onnx:   ONNX model path (default: u2netp.onnx;\n"
                "                searched next to the executable and in models/)\n\n"
                "Options:\n"
                "  -h, --help     show this help\n"
                "  -v, --version  show the version").toStdString()
            << std::endl;
    };

    if (argc >= 2) {
        const QString a1 = argv[1];
        if (a1 == "-h" || a1 == "--help") { printHelp(); return 0; }
        if (a1 == "-v" || a1 == "--version") {
            std::cout << "bg_remover_cli 1.0.0" << std::endl;
            return 0;
        }
    }

    if (argc < 2) {
        printHelp();
        return 1;
    }

    QString inputDir = argv[1];
    QString outputDir = "_rembg_output";
    QString modelPath = ModelPathResolver::resolve("u2netp.onnx");

    // Parse argumentos posicionais
    // Formatos aceitos:
    //   bg_remover_cli <input>
    //   bg_remover_cli <input> <output>
    //   bg_remover_cli <input> <output> <model.onnx>
    //   bg_remover_cli <input> <model.onnx>     (se 3º arg termina em .onnx)
    //   bg_remover_cli <input> <output> <model.onnx>

    if (argc >= 3) {
        QString arg2 = argv[2];
        if (arg2.endsWith(".onnx", Qt::CaseInsensitive)) {
            // Formato: <input> <model.onnx> [output]
            modelPath = arg2;
            if (argc >= 4) {
                outputDir = argv[3];
            }
        } else {
            // Formato: <input> <output> [model]
            outputDir = arg2;
            if (argc >= 4) {
                QString arg3 = argv[3];
                if (arg3.endsWith(".onnx", Qt::CaseInsensitive)) {
                    modelPath = arg3;
                }
            }
        }
    }

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
    BatchProcessor processor(options);
    BatchProcessor::Result r = processor.processDirectory(
        inputDir, outputDir);

    std::cout << "---\n"
              << I18n::tr("Concluido: ", "Completed: ").toStdString()
              << r.ok << "/" << r.total << " OK, "
              << r.errors << " "
              << I18n::tr("erros", "errors").toStdString() << "\n";

    if (!r.errorMessages.isEmpty()) {
        std::cout << I18n::tr("Erros:", "Errors:").toStdString() << "\n";
        for (const QString& msg : r.errorMessages) {
            std::cout << "  " << msg.toStdString() << "\n";
        }
    }

    return r.errors > 0 ? 1 : 0;
}
