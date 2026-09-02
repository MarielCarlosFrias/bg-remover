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
class QComboBox;
class QCheckBox;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr,
                        const QString& modelPath = "");

private slots:
    void openImage();
    void removeBackground();
    void saveResult();
    void loadModel();
    void selectFolder();
    void selectOutputFolder();
    void processBatch();
    void processBatchStep();
    void modelIndexChanged(int index);
    void bgColorChanged(int index);

private:
    void setOriginal(const QImage& img);
    void setResult(const QImage& img);
    static QImage addAlphaBackground(const QImage& img,
                                         const QColor& bgColor = QColor());
    QImage applyEdgeRefinement(const QImage& input,
                                   float intensity);
    QImage applyAlphaThreshold(const QImage& input,
                                   float threshold);
    static QString resolveModelPath(const QString& name);
    QColor currentBackgroundColor() const;

    QLabel* originalLabel_;
    QLabel* resultLabel_;
    QScrollArea* originalScroll_;
    QScrollArea* resultScroll_;
    QComboBox* modelCombo_;
    QLineEdit* modelPathEdit_;
    QLineEdit* folderEdit_;
    QLineEdit* outputDirEdit_;
    QPushButton* btnOpen_;
    QPushButton* btnRemove_;
    QPushButton* btnSave_;
    QPushButton* btnLoadModel_;
    QPushButton* btnSelectFolder_;
    QPushButton* btnSelectOutput_;
    QPushButton* btnProcessBatch_;
    QComboBox* bgColorCombo_;
    QCheckBox* refineCheckBox_;
    QCheckBox* noiseCheckBox_;
    QProgressBar* progressBar_;
    QPlainTextEdit* logEdit_;

    QString batchFolder_;
    QString batchOutputDir_;
    QStringList batchFiles_;
    int batchIndex_;
    bool batchRunning_;
    QColor customBgColor_;

    QImage originalImage_;
    QImage resultImage_;
    std::unique_ptr<BackgroundRemover> remover_;
};

#endif
