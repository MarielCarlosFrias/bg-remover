#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QFutureWatcher>
#include <QMainWindow>
#include <QImage>
#include <QMutex>
#include <QStringList>
#include <QList>
#include <memory>
#include "backgroundremover.h"
#include "batchplan.h"
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
class QTabWidget;
class QResizeEvent;
class QCloseEvent;
class QDragEnterEvent;
class QDropEvent;
class QEvent;
class QTimer;
class MaskEditCanvas;
class ModelManagerWidget;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    // Sessao ONNX partilhada entre a GUI e as threads de trabalho.
    // O acesso e protegido por mutex porque o modelo e carregado
    // dentro da thread de inferencia (para nao bloquear a janela).
    struct ModelCache {
        QMutex mutex;
        std::shared_ptr<BackgroundRemover> remover;
        QString loadedPath;
    };

    explicit MainWindow(QWidget* parent = nullptr,
                        const QString& modelPath = "");

public slots:
    // Abre a imagem indicada (tambem usada por drag & drop e testes).
    void openPath(const QString& path);
    // Remove o fundo da imagem carregada; o resultado chega pelo
    // sinal jobFinished().
    void removeBackground();

signals:
    // Emitido no fim do processamento (sucesso ou erro) e do lote.
    void jobFinished();

protected:
    void resizeEvent(QResizeEvent* event) override;
    void closeEvent(QCloseEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private slots:
    void openImage();
    void onRemoveFinished();
    void saveResult();
    void loadModel();
    void selectFolder();
    void selectOutputFolder();
    void processBatch();
    void processBatchStep();
    void onBatchStepFinished();
    void modelIndexChanged(int index);
    void bgColorChanged(int index);
    void editingDone();

private:
    struct SingleResult {
        QImage image;
        QString error;
    };

    void setPreviewZoom(int percent);
    void zoomAt(int percent, const QPoint& globalPos);
    void updatePreviewZoom();
    void setOriginal(const QImage& img);
    void setResult(const QImage& img);
    void setUiBusy(bool busy);
    void finishBatch();
    void resetModel();
    void populateModelCombo();
    void restoreSettings();
    void saveSettings();
    ProcessingOptions currentProcessingOptions() const;
    QColor currentBackgroundColor() const;

    static std::shared_ptr<BackgroundRemover> obtainRemover(
        const std::shared_ptr<ModelCache>& cache,
        const QString& modelPath);

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
    QPushButton* btnThemeToggle_;
    QComboBox* langCombo_;
    QPushButton* btnRemove_;
    QPushButton* btnSave_;
    QPushButton* btnLoadModel_;
    QPushButton* btnSelectFolder_;
    QPushButton* btnSelectOutput_;
    QPushButton* btnProcessBatch_;
    QPushButton* btnUndo_;
    QPushButton* btnRedo_;
    QComboBox* bgColorCombo_;
    QComboBox* brushModeCombo_;
    QSpinBox* brushRadiusSpin_;
    QSpinBox* noiseThresholdSpin_;
    QCheckBox* refineCheckBox_;
    QCheckBox* noiseCheckBox_;
    QProgressBar* progressBar_;
    QPlainTextEdit* logEdit_;
    MaskEditCanvas* maskEditCanvas_;
    QTimer* zoomDebounce_;
    QTabWidget* mainTabs_;
    ModelManagerWidget* modelManager_;

    QString batchFolder_;
    QString batchOutputDir_;
    QList<BatchPlan::Entry> batchPlan_;
    ProcessingOptions batchOptions_;
    int batchIndex_;
    int batchOk_;
    int batchErrors_;
    bool batchRunning_;
    bool batchCancelRequested_;
    QColor customBgColor_;
    int previewZoomPercent_;

    QImage originalImage_;
    QImage resultImage_;
    std::shared_ptr<ModelCache> modelCache_;
    QFutureWatcher<SingleResult> removeWatcher_;
    QFutureWatcher<QString> batchWatcher_;
};

#endif
