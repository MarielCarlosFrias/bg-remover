// Verifica que o Qt consegue gravar/ler WebP quando o plugin esta
// instalado. Em instalacoes sem qt6-image-formats-plugins o teste e
// ignorado (SKIP), para o ctest continuar verde em ambientes minimos.
#include <QColor>
#include <QCoreApplication>
#include <QImage>
#include <QImageWriter>
#include <QTemporaryDir>
#include <cstdio>

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);

    if (!QImageWriter::supportedImageFormats().contains("webp")) {
        std::printf("SKIP: plugin WebP nao instalado "
                    "(qt6-image-formats-plugins)\n");
        return 0;
    }

    QImage image(4, 4, QImage::Format_RGBA8888);
    image.fill(QColor(10, 20, 30, 128));

    QTemporaryDir tmp;
    if (!tmp.isValid()) {
        std::printf("FALHOU: nao foi possivel criar diretorio temporario\n");
        return 1;
    }

    const QString path = tmp.path() + "/test.webp";
    if (!image.save(path, "webp")) {
        std::printf("FALHOU: nao foi possivel gravar WebP\n");
        return 1;
    }

    const QImage read(path);
    if (read.isNull() || read.size() != image.size()) {
        std::printf("FALHOU: nao foi possivel ler o WebP gravado\n");
        return 1;
    }
    if (qAlpha(read.pixel(0, 0)) == 0) {
        std::printf("FALHOU: WebP perdeu o canal alfa\n");
        return 1;
    }

    std::printf("PASS: WebP com alfa\n");
    return 0;
}
