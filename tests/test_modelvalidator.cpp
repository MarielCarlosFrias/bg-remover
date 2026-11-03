// Teste unitario do ModelValidator: estados Missing/Incomplete/
// Corrupt/Installed com verificacao de tamanho e SHA256. Corre com
// ctest (alvo modelvalidator_test).
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QFile>
#include <QTemporaryDir>
#include <cstdio>

#include "modelvalidator.h"

static int failures = 0;

#define CHECK(cond, msg) do { \
    if (!(cond)) { std::printf("FALHOU: %s\n", msg); ++failures; } \
    else { std::printf("OK: %s\n", msg); } \
} while (0)

static bool writeFile(const QString& path, const QByteArray& data)
{
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;
    return f.write(data) == data.size();
}

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    app.setApplicationName("bg_remover_test");

    QTemporaryDir tmp;
    if (!tmp.isValid()) {
        std::printf("FALHOU: nao foi possivel criar diretorio temporario\n");
        return 1;
    }

    const QByteArray content("modelo onnx de teste");
    const QString path = tmp.path() + "/modelo.onnx";
    CHECK(writeFile(path, content), "caso1: ficheiro de teste criado");

    const QString sha = QString::fromLatin1(
        QCryptographicHash::hash(
            content, QCryptographicHash::Sha256).toHex());

    // ---- Caso 1: ficheiro inexistente ----
    {
        const ModelFileValidation v = validateModelFile(
            tmp.path() + "/nao_existe.onnx", 10, QString());
        CHECK(v.state == ModelFileState::Missing,
              "caso1: inexistente -> Missing");
    }

    // ---- Caso 2: menor que o esperado ----
    {
        const ModelFileValidation v = validateModelFile(
            path, content.size() + 1, QString());
        CHECK(v.state == ModelFileState::Incomplete,
              "caso2: menor -> Incomplete");
        CHECK(v.actualSize == content.size(),
              "caso2: actualSize correto");
    }

    // ---- Caso 3: maior que o esperado ----
    {
        const ModelFileValidation v = validateModelFile(
            path, content.size() - 1, QString());
        CHECK(v.state == ModelFileState::Corrupt,
              "caso3: maior -> Corrupt");
    }

    // ---- Caso 4: tamanho certo, SHA errado ----
    {
        const ModelFileValidation v = validateModelFile(
            path, content.size(), QString(64, '0'));
        CHECK(v.state == ModelFileState::Corrupt,
              "caso4: SHA errado -> Corrupt");
    }

    // ---- Caso 5: tamanho certo, SHA certo ----
    {
        const ModelFileValidation v = validateModelFile(
            path, content.size(), sha);
        CHECK(v.state == ModelFileState::Installed,
              "caso5: tamanho e SHA certos -> Installed");
    }

    // ---- Caso 6: sem SHA, so tamanho ----
    {
        const ModelFileValidation v = validateModelFile(
            path, content.size(), QString());
        CHECK(v.state == ModelFileState::Installed,
              "caso6: sem SHA -> Installed pelo tamanho");
    }

    // ---- Caso 7: tamanho desconhecido (0) e sem SHA ----
    {
        const ModelFileValidation v = validateModelFile(
            path, 0, QString());
        CHECK(v.state == ModelFileState::Installed,
              "caso7: sem expectativas -> Installed");
    }

    if (failures == 0)
        std::printf("Todos os testes do ModelValidator passaram.\n");
    return failures == 0 ? 0 : 1;
}
