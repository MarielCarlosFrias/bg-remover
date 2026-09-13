#include <QImage>
#include <QPainter>
#include <QFile>
#include <QDebug>
#include <iostream>
#include "backgroundremover.h"

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Uso: " << argv[0]
                  << " <modelo.onnx>" << std::endl;
        return 1;
    }

    QString modelPath = argv[1];
    if (!QFile::exists(modelPath)) {
        std::cerr << "FAIL: Modelo nao encontrado: "
                  << modelPath.toStdString()
                  << std::endl;
        return 1;
    }

    try {
        // Criar imagem de teste: fundo branco + retangulo verde
        QImage img(320, 320, QImage::Format_RGB32);
        img.fill(Qt::white);
        QPainter painter(&img);
        painter.fillRect(80, 80, 160, 160, Qt::darkGreen);
        painter.end();

        // Testar carregamento do modelo
        BackgroundRemover remover(modelPath);

        // Testar inferência
        QImage result = remover.removeBackground(img);

        // Verificar formato com alpha
        if (result.format() != QImage::Format_RGBA8888) {
            std::cerr << "FAIL: Formato invalido" << std::endl;
            return 1;
        }

        // Verificar dimensoes preservadas
        if (result.width() != img.width()
            || result.height() != img.height()) {
            std::cerr << "FAIL: Dimensao invalida"
                      << std::endl;
            return 1;
        }

        // Verificar que existem pixels com alpha > 0
        bool hasAlpha = false;
        for (int y = 0; y < result.height(); y += 10) {
            for (int x = 0; x < result.width(); x += 10) {
                if (qAlpha(result.pixel(x, y)) > 0) {
                    hasAlpha = true;
                    break;
                }
            }
            if (hasAlpha) break;
        }

        if (!hasAlpha) {
            std::cerr << "FAIL: Nenhum pixel com alpha > 0"
                      << std::endl;
            return 1;
        }

        std::cout << "PASS" << std::endl;
        return 0;

    } catch (const std::exception& e) {
        std::cerr << "FAIL: " << e.what() << std::endl;
        return 1;
    }
}
