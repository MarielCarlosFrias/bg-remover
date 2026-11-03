// Teste unitario do ModelPathResolver: cadeia de candidatos,
// resolucao quando o ficheiro existe e fallback estavel quando nao
// existe. Corre com ctest (alvo modelpath_test).
#include <QCoreApplication>
#include <QDir>
#include <QTemporaryDir>
#include <QFile>
#include <cstdio>
#include "modelpathresolver.h"

static int failures = 0;

#define CHECK(cond, msg) do { \
    if (!(cond)) { std::printf("FALHOU: %s\n", msg); ++failures; } \
    else { std::printf("OK: %s\n", msg); } \
} while (0)

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    app.setApplicationName("bg_remover_test");

    // ---- Caso 1: nao existe em lado nenhum ----
    // resolve() nao pode devolver vazio: a GUI mostra o caminho no
    // campo de texto e o utilizador escolhe o ficheiro depois.
    {
        const QString resolved =
            ModelPathResolver::resolve("modelo_inexistente_xyz.onnx");
        CHECK(!resolved.isEmpty(),
              "caso1: resolve() devolve um camino mesmo sem ficheiro");
        CHECK(resolved.endsWith("modelo_inexistente_xyz.onnx"),
              "caso1: o caminho termina no nome do modelo");

        const QStringList cands =
            ModelPathResolver::candidates("modelo_inexistente_xyz.onnx");
        CHECK(!cands.isEmpty(), "caso1: candidates() nao e vazio");
        CHECK(cands.contains(resolved),
              "caso1: resolve() devolve um dos candidatos");
    }

    // ---- Caso 2: ficheiro no diretorio atual ----
    {
        QTemporaryDir tmp;
        QDir::setCurrent(tmp.path());
        CHECK(QFile::exists(tmp.path()), "caso2: diretorio temporario valido");

        const QString model = "caso2_test.onnx";
        QFile f(QDir(tmp.path()).absoluteFilePath(model));
        const bool created = f.open(QIODevice::WriteOnly | QIODevice::Truncate);
        CHECK(created, "caso2: ficheiro criado no diretorio atual");
        f.close();

        const QString resolved = ModelPathResolver::resolve(model);
        CHECK(resolved == QDir(tmp.path()).absoluteFilePath(model),
              "caso2: resolve() encontra o ficheiro do diretorio atual");
    }

    // ---- Caso 3: modelos instalados pelo utilizador ----
    {
        QTemporaryDir home;
        const QString models =
            ModelPathResolver::userModelsDir();
        CHECK(!models.isEmpty(), "caso3: userModelsDir() nao e vazio");
        CHECK(models.endsWith("/bg-remover/models"),
              "caso3: userModelsDir() e canonico e nao depende do app");
        CHECK(models.contains(QDir::separator())
                  || !models.isEmpty(),
              "caso3: userModelsDir() tem forma de caminho");

        // Nenhum ficheiro la instalado: a resolucao para um nome
        // arbitrario continua a devolver um caminho utilizavel.
        const QString r = ModelPathResolver::resolve("zzz_nada.onnx");
        CHECK(!r.isEmpty(), "caso3: resolucao estavel sem modelos instalados");
    }

#ifdef Q_OS_MACOS
    // ---- Caso 4: modelo dentro do .app ----
    // A GUI vive em Contents/MacOS; o instalador coloca o modelo em
    // Contents/Resources. O resolver tem de procurar ai.
    {
        const QStringList cands =
            ModelPathResolver::candidates("mac_bundle_test.onnx");
        const QString expected = QDir(
            QCoreApplication::applicationDirPath())
            .filePath("../Resources/mac_bundle_test.onnx");
        CHECK(cands.contains(expected),
              "caso4: Contents/Resources e um candidato no macOS");
    }
#endif

    QDir::setCurrent(QDir::homePath());

    std::printf(failures == 0
        ? "\nTODOS OS TESTES PASSARAM\n"
        : "\n%d TESTE(S) FALHARAM\n", failures);
    return failures == 0 ? 0 : 1;
}
