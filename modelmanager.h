#ifndef MODELMANAGER_H
#define MODELMANAGER_H

#include <QWidget>
#include <QCryptographicHash>
#include <QFile>
#include <QFutureWatcher>
#include <QList>
#include <QString>
#include <memory>

#include "modelvalidator.h"

class QLabel;
class QProgressBar;
class QPushButton;
class QTreeWidget;
class QTreeWidgetItem;
class QNetworkAccessManager;
class QNetworkReply;

// Aba "Modelos": catalogo de modelos ONNX (familia rembg/U2-Net/ISNet)
// que o utilizador pode descarregar diretamente do programa. Os
// ficheiros ficam no diretorio de dados da aplicacao (models/) e sao
// detetados automaticamente pelo ModelPathResolver pela GUI e pelo CLI.
class ModelManagerWidget : public QWidget {
    Q_OBJECT
public:
    explicit ModelManagerWidget(QWidget* parent = nullptr);

signals:
    // Emitido quando um modelo e instalado ou removido; a MainWindow
    // atualiza a lista de modelos disponiveis.
    void modelsChanged(const QString& fileName);

private slots:
    void refreshList();
    void downloadSelected();
    void cancelDownload();
    void deleteSelected();
    void onDownloadProgress(qint64 received, qint64 total);
    void onReadyRead();
    void onDownloadFinished();
    void onValidationFinished();

private:
    struct CatalogEntry {
        QString fileName;     // ex.: "isnet-general-use.onnx"
        QString title;        // nome legivel
        QString description;  // para que serve
        qint64 sizeBytes = 0; // tamanho esperado (verificacao)
        QString url;
        QString sha256Hex;    // vazio = sem verificacao (so HTTPS+tamanho)
    };

    struct ValidationJob {
        QString fileName;
        QString path;
        qint64 expectedSize = 0;
        QString sha256Hex;
        // Identidade do ficheiro no inicio do hash: se tamanho ou
        // mtime mudarem, o resultado antigo nao pode ser aplicado.
        qint64 fileSize = 0;
        qint64 lastModifiedMs = 0;
    };

    static QList<CatalogEntry> catalog();
    CatalogEntry selectedEntry() const;
    void setDownloading(bool downloading);
    void updateButtonsState();
    void startNextValidation();
    void updateRowStatus(const ValidationJob& job,
                         const ModelFileValidation& validation);

    QTreeWidget* tree_;
    QLabel* detailLabel_;
    QPushButton* btnDownload_;
    QPushButton* btnCancel_;
    QPushButton* btnDelete_;
    QPushButton* btnRefresh_;
    QProgressBar* progress_;
    QLabel* statusLabel_;

    QNetworkAccessManager* nam_;
    QNetworkReply* reply_ = nullptr;
    std::unique_ptr<QCryptographicHash> hasher_;
    QFile outFile_;
    QString pendingFileName_;   // nome final do modelo em descarga
    QString pendingSha256Hex_;  // hash esperado (vazio = sem verificacao)
    qint64 pendingSizeBytes_ = 0; // tamanho esperado (0 = sem verificacao)

    // Verificacao SHA256 em segundo plano: modelos de ~178 MB levam
    // segundos, por isso o refresh da lista so verifica o tamanho e
    // o hash corre numa fila (um modelo de cada vez).
    QList<ValidationJob> validationQueue_;
    ValidationJob currentValidation_;
    QFutureWatcher<ModelFileValidation> validationWatcher_;
    bool validationRunning_ = false;
};

#endif
