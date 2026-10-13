#ifndef MODELMANAGER_H
#define MODELMANAGER_H

#include <QWidget>
#include <QCryptographicHash>
#include <QFile>
#include <QList>
#include <QString>
#include <memory>

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
    void modelInstalled(const QString& fileName);

private slots:
    void refreshList();
    void downloadSelected();
    void cancelDownload();
    void deleteSelected();
    void onDownloadProgress(qint64 received, qint64 total);
    void onReadyRead();
    void onDownloadFinished();

private:
    struct CatalogEntry {
        QString fileName;     // ex.: "isnet-general-use.onnx"
        QString title;        // nome legivel
        QString description;  // para que serve
        qint64 sizeBytes = 0; // aproximado, para display
        QString url;
        QString sha256Hex;    // vazio = sem verificacao (so HTTPS+tamanho)
    };

    static QList<CatalogEntry> catalog();
    CatalogEntry selectedEntry() const;
    void setDownloading(bool downloading);
    void updateButtonsState();

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
};

#endif
