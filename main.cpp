#include <QApplication>
#include <QFile>
#include "apptheme.h"
#include "mainwindow.h"
#include "modelpathresolver.h"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    QApplication::setOrganizationName("bg-remover");
    QApplication::setApplicationName("bg-remover");

    // Tema escuro/claro: env BG_REMOVER_THEME=dark|light tem prioridade,
    // caso contrario usa (e grava em) QSettings.
    AppTheme::apply(AppTheme::isDark());

    QString modelPath;
    if (argc > 1) {
        modelPath = QString::fromLocal8Bit(argv[1]);
        // Caminho indicado nao existe: cai para a resolucao padrao,
        // a mesma logica usada pelo CLI.
        if (!QFile::exists(modelPath))
            modelPath = ModelPathResolver::resolve("u2netp.onnx");
    }
    MainWindow window(nullptr, modelPath);
    window.show();
    return app.exec();
}
