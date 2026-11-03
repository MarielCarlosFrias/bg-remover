// Teste de regressao do lote do CLI: uma imagem invalida nao pode
// abortar as imagens seguintes; uma falha ao carregar o modelo e
// fatal. Corre com ctest (alvo batchprocessor_test) e recebe o
// caminho de u2netp.onnx como argumento.
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QTemporaryDir>
#include <cstdio>

#include "batchprocessor.h"

static int failures = 0;

#define CHECK(cond, msg) do { \
    if (!(cond)) { std::printf("FALHOU: %s\n", msg); ++failures; } \
    else { std::printf("OK: %s\n", msg); } \
} while (0)

static bool writeGarbage(const QString& path)
{
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;
    return f.write("isto nao e uma imagem") > 0;
}

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    app.setApplicationName("bg_remover_test");

    if (argc < 2) {
        std::printf("FALHOU: falta o caminho do modelo ONNX\n");
        return 1;
    }
    const QString modelPath = QString::fromLocal8Bit(argv[1]);

    QTemporaryDir tmp;
    if (!tmp.isValid()) {
        std::printf("FALHOU: nao foi possivel criar diretorio temporario\n");
        return 1;
    }

    const QString inDir = tmp.path() + "/in";
    const QString outDir = tmp.path() + "/out";
    QDir().mkpath(inDir);
    QDir().mkpath(outDir);

    // "a_broken.png" vem antes de "b_valid.png" na ordenacao do
    // QDir::Name; o lote tem de continuar depois do erro.
    CHECK(writeGarbage(inDir + "/a_broken.png"),
          "caso1: imagem invalida criada");
    QImage valid(40, 30, QImage::Format_RGB32);
    valid.fill(Qt::red);
    CHECK(valid.save(inDir + "/b_valid.png", "PNG"),
          "caso1: imagem valida criada");

    ProcessingOptions options;
    options.modelPath = modelPath;
    BatchProcessor processor(options);
    const BatchProcessor::Result r =
        processor.processDirectory(inDir, outDir);

    CHECK(!r.fatal, "caso1: erro numa imagem nao e fatal");
    CHECK(r.total == 2, "caso1: total = 2");
    CHECK(r.ok == 1, "caso1: imagem valida processada");
    CHECK(r.errors == 1, "caso1: imagem invalida contada como erro");

    const QString outFile = outDir + "/b_valid_rembg.png";
    CHECK(QFile::exists(outFile),
          "caso1: saida da imagem valida existe");
    const QImage out(outFile);
    CHECK(out.size() == QSize(40, 30),
          "caso1: saida mantem o tamanho da imagem valida");

    // ---- Caso 2: falha ao carregar o modelo e fatal ----
    const QString emptyModel = tmp.path() + "/modelo_vazio.onnx";
    {
        QFile f(emptyModel);
        CHECK(f.open(QIODevice::WriteOnly | QIODevice::Truncate),
              "caso2: modelo vazio criado");
    }
    ProcessingOptions badOptions;
    badOptions.modelPath = emptyModel;
    BatchProcessor badProcessor(badOptions);
    const BatchProcessor::Result bad =
        badProcessor.processDirectory(inDir, outDir);

    CHECK(bad.fatal, "caso2: modelo invalido -> fatal");
    CHECK(bad.total == 2, "caso2: total = 2");
    CHECK(bad.errors == bad.total,
          "caso2: todas as imagens contam como erro");
    CHECK(!bad.errorMessages.isEmpty(),
          "caso2: mensagem de erro do modelo presente");

    if (failures == 0)
        std::printf("Todos os testes do BatchProcessor passaram.\n");
    return failures == 0 ? 0 : 1;
}
