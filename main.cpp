#include <QApplication>
#include "mainwindow.h"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    const QString modelPath = argc > 1
        ? QString::fromLocal8Bit(argv[1]) : QString();
    MainWindow window(nullptr, modelPath);
    window.show();
    return app.exec();
}
