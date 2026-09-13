#include <QCoreApplication>
#include <QStringList>
#include <QDir>
#include <QFile>
#include <iostream>
#include "batchprocessor.h"

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    app.setApplicationName("bg_remover_cli");

    if (argc < 2) {
        std::cerr
            << "Uso: bg_remover_cli <pasta_entrada> [pasta_saida] [modelo.onnx]\n"
            << "  pasta_entrada: diretorio com imagens\n"
            << "  pasta_saida:   diretorio de saida (padrao: _rembg_output)\n"
            << "  modelo.onnx:   caminho do modelo ONNX (padrao: ./u2netp.onnx)\n";
        return 1;
    }

    QString inputDir = argv[1];
    QString outputDir = "_rembg_output";
    QString modelPath =
        QDir(QCoreApplication::applicationDirPath()).filePath("u2netp.onnx");
    if (!QFile::exists(modelPath)) {
#ifdef INSTALLED
        modelPath = "/usr/share/bg-remover/u2netp.onnx";
#endif
    }
    if (!QFile::exists(modelPath)) {
        modelPath = "./u2netp.onnx";
    }

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
        std::cerr << "Modelo nao encontrado: "
                  << modelPath.toStdString() << std::endl;
        return 1;
    }

    if (!QDir(inputDir).exists()) {
        std::cerr << "Diretorio de entrada nao existe: "
                  << inputDir.toStdString() << std::endl;
        return 1;
    }

    std::cout << "Modelo: " << modelPath.toStdString()
              << "\nEntrada: " << inputDir.toStdString()
              << "\nSaida: " << outputDir.toStdString()
              << "\n---\n" << std::flush;

    BatchProcessor processor(modelPath);
    BatchProcessor::Result r = processor.processDirectory(
        inputDir, outputDir);

    std::cout << "---\n"
              << "Concluido: " << r.ok << "/"
              << r.total << " OK, "
              << r.errors << " erros\n";

    if (!r.errorMessages.isEmpty()) {
        std::cout << "Erros:\n";
        for (const QString& msg : r.errorMessages) {
            std::cout << "  " << msg.toStdString() << "\n";
        }
    }

    return r.errors > 0 ? 1 : 0;
}
