#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QImage>
#include <QStringList>
#include <memory>
#include "backgroundremover.h"

class QLabel;
class QScrollArea;
class QPushButton;
class QLineEdit;
class QProgressBar;
class QPlainTextEdit;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);

private slots:
    void openImage();
    void removeBackground();
    void saveResult();
    void loadModel();
    void selectFolder();
    void selectOutputFolder();
    void processBatch();
    void processBatchStep();

private:
    void setOriginal(const QImage& img);
    void setResult(const QImage& img);
    static QImage addAlphaBackground(const QImage& img);

    // GUI widgets
    QLabel* originalLabel_;
    QLabel* resultLabel_;
    QScrollArea* originalScroll_;
    QScrollArea* resultScroll_;
    QLineEdit* modelPathEdit_;
    QLineEdit* folderEdit_;
    QLineEdit* outputDirEdit_;
    QPushButton* btnOpen_;
    QPushButton* btnRemove_;
    QPushButton* btnSave_;
    QPushButton* btnLoadModel_;
    QPushButton* btnSelectFolder_;
    QPushButton* btnSelectOutputFolder_;
    QPushButton* btnProcessBatch_;
    QProgressBar* progressBar_;
    QPlainTextEdit* logEdit_;

    // Batch state
    QString batchFolder_;
    QString batchOutputDir_;
    QStringList batchFiles_;
    int batchIndex_;
    bool batchRunning_;

    // Dados de imagens
    QImage originalImage_;
    QImage resultImage_;
    std::unique_ptr<BackgroundRemover> remover_;
};

#endif
