#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QImage>
#include <QStringList>
#include <memory>
#include "backgroundremover.h"
#include "processingoptions.h"

class QLabel;
class QScrollArea;
class QPushButton;
class QLineEdit;
class QProgressBar;
class QPlainTextEdit;
class QComboBox;
class QCheckBox;
class QSpinBox;
class QResizeEvent;
class MaskEditCanvas;

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
    void editingDone();

private:
    void resizeEvent(QResizeEvent* event) override;
    void setPreviewZoom(int percent);
    void updatePreviewZoom();
    void setOriginal(const QImage& img);
    void setResult(const QImage& img);
    ProcessingOptions currentProcessingOptions() const;
    QColor currentBackgroundColor() const;

    QLabel* originalLabel_;
    QLabel* imageNameLabel_;
    QLabel* zoomLabel_;
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
    QPushButton* btnUndo_;
    QComboBox* bgColorCombo_;
    QComboBox* brushModeCombo_;
    QSpinBox* brushRadiusSpin_;
    QCheckBox* refineCheckBox_;
    QCheckBox* noiseCheckBox_;
    QProgressBar* progressBar_;
    QPlainTextEdit* logEdit_;
    MaskEditCanvas* maskEditCanvas_;

    QString batchFolder_;
    QString batchOutputDir_;
    QStringList batchFiles_;
    QStringList batchOutputPaths_;
    ProcessingOptions batchOptions_;
    int batchIndex_;
    int batchOk_;
    int batchErrors_;
    bool batchRunning_;
    QColor customBgColor_;
    int previewZoomPercent_;

    QImage originalImage_;
    QImage resultImage_;
    std::unique_ptr<BackgroundRemover> remover_;
};

#endif
