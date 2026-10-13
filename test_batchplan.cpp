// Teste unitario do BatchPlan: filtragem de imagens e atribuicao de
// nomes de saida sem sobrescrita — a logica partilhada entre a GUI e
// o CLI. Corre com ctest (alvo batchplan_test).
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <cstdio>
#include "batchplan.h"

static int failures = 0;

#define CHECK(cond, msg) do { \
    if (!(cond)) { std::printf("FALHOU: %s\n", msg); ++failures; } \
    else { std::printf("OK: %s\n", msg); } \
} while (0)

static bool touch(const QString& path)
{
    QFile f(path);
    return f.open(QIODevice::WriteOnly | QIODevice::Truncate);
}

static QStringList outputNames(const QList<BatchPlan::Entry>& plan)
{
    QStringList names;
    for (const BatchPlan::Entry& e : plan)
        names << QFileInfo(e.outputPath).fileName();
    return names;
}

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);

    // ---- Caso 1: entrada == saida ----
    // Ignora ficheiros nao-imagem e saidas geradas (_rembg, _rembg_N),
    // aceita extensoes em maiusculas (FOTO.JPG).
    {
        QTemporaryDir tmp;
        const QString dir = tmp.path();
        touch(dir + "/foto.jpg");
        touch(dir + "/foto.png");
        touch(dir + "/FOTO.JPG");
        touch(dir + "/nota.txt");
        touch(dir + "/foto_rembg.png");     // saida anterior
        touch(dir + "/outro_rembg_2.png");  // saida anterior numerada

        const QList<BatchPlan::Entry> plan =
            BatchPlan::build(QDir(dir), QDir(dir));

        CHECK(plan.size() == 3,
              "caso1: so 3 imagens entram no lote (ignora txt e _rembg)");
        const QStringList names = outputNames(plan);
        CHECK(names.contains("foto_rembg_3.png") || names.contains("foto_rembg_2.png"),
              "caso1: colisao com foto_rembg.png existente gera _N");
        CHECK(!names.contains("foto_rembg.png"),
              "caso1: nao sobrescreve foto_rembg.png existente");
        bool sawUpper = false;
        for (const BatchPlan::Entry& e : plan)
            if (QFileInfo(e.inputPath).fileName() == "FOTO.JPG") sawUpper = true;
        CHECK(sawUpper, "caso1: FOTO.JPG incluido (filtro case-insensitive)");
    }

    // ---- Caso 2: saida separada, colisoes em cadeia ----
    // a.jpg e a.png partilham base "a"; a_rembg.png ja existe.
    {
        QTemporaryDir tmp;
        const QDir in(tmp.path() + "/in");
        const QDir out(tmp.path() + "/out");
        in.mkpath(".");
        out.mkpath(".");
        touch(in.absoluteFilePath("a.jpg"));
        touch(in.absoluteFilePath("a.png"));
        touch(out.absoluteFilePath("a_rembg.png"));

        const QList<BatchPlan::Entry> plan =
            BatchPlan::build(in, out);

        CHECK(plan.size() == 2, "caso2: duas entradas planeadas");
        const QStringList names = outputNames(plan);
        CHECK(names.contains("a_rembg_2.png"),
              "caso2: a_rembg.png existente empurra para a_rembg_2.png");
        CHECK(names.contains("a_rembg_3.png"),
              "caso2: segundo a.* recebe a_rembg_3.png (sem colisao)");
        CHECK(names.count(names.first()) == 1,
              "caso2: nenhum caminho de saida duplicado");
    }

    // ---- Caso 3: diretorio vazio ----
    {
        QTemporaryDir tmp;
        const QList<BatchPlan::Entry> plan =
            BatchPlan::build(QDir(tmp.path()), QDir(tmp.path() + "/out"));
        CHECK(plan.isEmpty(), "caso3: pasta vazia gera plano vazio");
    }

    std::printf(failures == 0
        ? "\nTODOS OS TESTES PASSARAM\n"
        : "\n%d TESTE(S) FALHARAM\n", failures);
    return failures == 0 ? 0 : 1;
}
