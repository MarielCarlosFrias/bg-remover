#include <QApplication>
#include <QFile>
#include <QFileInfo>
#include "apptheme.h"
#include "mainwindow.h"
#include "modelpathresolver.h"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    QApplication::setOrganizationName("aletheia");
    QApplication::setApplicationName("aletheia");

    // Tema escuro/claro: env BG_REMOVER_THEME=dark|light tem prioridade,
    // caso contrario usa (e grava em) QSettings.
    AppTheme::apply(AppTheme::isDark());

    QString modelPath;
    QString imagePath;
    if (argc > 1) {
        const QString arg = QString::fromLocal8Bit(argv[1]);
        if (arg.endsWith(".onnx", Qt::CaseInsensitive)) {
            if (QFile::exists(arg)) modelPath = arg;
        } else if (QFileInfo(arg).isFile()) {
            // `bg_remover_gui foto.png` abre a imagem; antes o
            // caminho era passado como se fosse um modelo ONNX.
            imagePath = arg;
        }
        // Caminho indicado nao existe (ou e uma pasta): cai para a
        // resolucao padrao, a mesma logica usada pelo CLI.
        if (modelPath.isEmpty() && imagePath.isEmpty())
            modelPath = ModelPathResolver::resolve("u2netp.onnx");
    }
    MainWindow window(nullptr, modelPath);
    window.show();
    if (!imagePath.isEmpty())
        window.openPath(imagePath);
    return app.exec();
}
