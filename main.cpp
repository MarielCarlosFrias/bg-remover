#include <QImage>
#include <iostream>
#include "backgroundremover.h"

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Uso: " << argv[0]
                  << " <modelo.onnx> <imagem> [saida.png]"
                  << std::endl;
        return 1;
    }

    const char* model_path  = argv[1];
    const char* input_path  = argv[2];
    const char* output_path = (argc >= 4) ? argv[3]
                                          : "resultado.png";

    QImage input(input_path);
    if (input.isNull()) {
        std::cerr << "Erro ao carregar: " << input_path
                  << std::endl;
        return 1;
    }

    BackgroundRemover remover(model_path);
    QImage result = remover.removeBackground(input);

    int fg = 0;
    for (int y = 0; y < result.height(); ++y) {
        for (int x = 0; x < result.width(); ++x) {
            if (qAlpha(result.pixel(x, y)) > 128) fg++;
        }
    }
    double fg_pct = 100.0 * fg /
                    (result.width() * result.height());
    std::cout << "[M1] Pixel foreground: "
              << fg_pct << "%" << std::endl;

    if (!result.save(output_path, "PNG")) {
        std::cerr << "Erro ao salvar: " << output_path
                  << std::endl;
        return 1;
    }
    std::cout << "[M1] Salvo: " << output_path << std::endl;

    return 0;
}
