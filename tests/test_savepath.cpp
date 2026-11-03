// Teste unitario de savePathWithExtension: a extensao gravada tem de
// ser coerente com o formato escolhido. Corre com ctest (alvo
// savepath_test).
#include <QCoreApplication>
#include <QDir>
#include <cstdio>

#include "savepath.h"

static int failures = 0;

#define CHECK(cond, msg) do { \
    if (!(cond)) { std::printf("FALHOU: %s\n", msg); ++failures; } \
    else { std::printf("OK: %s\n", msg); } \
} while (0)

static QString p(const QString& name)
{
    return QDir::tempPath() + "/" + name;
}

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    app.setApplicationName("bg_remover_test");

    CHECK(savePathWithExtension(p("foto"), "png") == p("foto.png"),
          "sem extensao -> acrescenta .png");
    CHECK(savePathWithExtension(p("foto.PNG"), "png") == p("foto.PNG"),
          "extensao igual (case-insensitive) -> mantem");
    CHECK(savePathWithExtension(p("foto.jpg"), "png") == p("foto.png"),
          "extensao de outro formato -> substitui por .png");
    CHECK(savePathWithExtension(p("foto.jpeg"), "jpg",
                                QStringList() << "jpeg")
              == p("foto.jpeg"),
          "JPEG aceita .jpeg");
    CHECK(savePathWithExtension(p("foto.png"), "jpg")
              == p("foto.jpg"),
          ".png com formato JPEG -> substitui por .jpg");
    CHECK(savePathWithExtension(p("foto.webp"), "jpg")
              == p("foto.jpg"),
          ".webp com formato JPEG -> substitui por .jpg");
    CHECK(savePathWithExtension(p("a.b/foto"), "webp")
              == p("a.b/foto.webp"),
          "diretorio com ponto nao e confundido com extensao");
    CHECK(savePathWithExtension(p("foto.tar.gz"), "png")
              == p("foto.tar.png"),
          "nome com varios pontos -> substitui so a ultima extensao");
    CHECK(savePathWithExtension(p("foto."), "png") == p("foto.png"),
          "nome terminado em ponto -> acrescenta .png");
    CHECK(savePathWithExtension(QString(), "png").isEmpty(),
          "caminho vazio -> vazio");
    CHECK(savePathWithExtension(p("foto"), QString()) == p("foto"),
          "formato vazio -> mantem o caminho");

    if (failures == 0)
        std::printf("Todos os testes de SavePath passaram.\n");
    return failures == 0 ? 0 : 1;
}
