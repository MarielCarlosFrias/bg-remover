#include "modelmanager.h"
#include "i18n.h"
#include "modelpathresolver.h"

#include <QCryptographicHash>
#include <QDir>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProgressBar>
#include <QPushButton>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

namespace {

QString formatSize(qint64 bytes)
{
    if (bytes >= 1024 * 1024)
        return QString::number(bytes / (1024.0 * 1024.0), 'f', 0) + " MB";
    if (bytes >= 1024)
        return QString::number(bytes / 1024.0, 'f', 0) + " kB";
    return QString::number(bytes) + " B";
}

} // namespace

ModelManagerWidget::ModelManagerWidget(QWidget* parent)
    : QWidget(parent),
      tree_(new QTreeWidget),
      detailLabel_(new QLabel),
      btnDownload_(new QPushButton(TR("Descarregar", "Download"))),
      btnCancel_(new QPushButton(TR("Cancelar", "Cancel"))),
      btnDelete_(new QPushButton(TR("Apagar do disco", "Delete from disk"))),
      progress_(new QProgressBar),
      statusLabel_(new QLabel(TR(
          "Os modelos ficam guardados em: ",
          "Models are stored in: ")
          + ModelPathResolver::userModelsDir())),
      nam_(new QNetworkAccessManager(this))
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(10);

    auto* title = new QLabel(TR("Catálogo de modelos",
                                "Model catalog"));
    title->setObjectName("inspectorTitle");
    layout->addWidget(title);

    auto* hint = new QLabel(TR(
        "Modelos maiores geram recortes melhores (sobretudo cabelo e "
        "contornos finos), mas demoram mais a processar. Escolha, "
        "baixe e o modelo passa a estar disponível na aba Editor "
        "e também no CLI. Tudo em HTTPS, com verificação de "
        "integridade quando disponível.",
        "Larger models produce better cutouts (especially hair and "
        "fine edges), but take longer to process. Choose, download "
        "and the model becomes available in the Editor tab and in "
        "the CLI. All over HTTPS, with integrity checks when "
        "available."));
    hint->setObjectName("inspectorHint");
    hint->setWordWrap(true);
    layout->addWidget(hint);

    tree_->setRootIsDecorated(false);
    tree_->setAlternatingRowColors(false);
    tree_->setSelectionMode(QAbstractItemView::SingleSelection);
    tree_->setColumnCount(3);
    tree_->setHeaderLabels({TR("Modelo", "Model"),
                            TR("Tamanho", "Size"),
                            TR("Estado", "Status")});
    tree_->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    tree_->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    tree_->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    layout->addWidget(tree_, 1);

    detailLabel_->setObjectName("inspectorHint");
    detailLabel_->setWordWrap(true);
    layout->addWidget(detailLabel_);

    auto* buttons = new QHBoxLayout;
    buttons->addWidget(btnDownload_);
    buttons->addWidget(btnCancel_);
    buttons->addWidget(btnRefresh_ = new QPushButton("Atualizar"));
    buttons->addStretch();
    buttons->addWidget(btnDelete_);
    layout->addLayout(buttons);

    progress_->setRange(0, 100);
    progress_->setValue(0);
    progress_->setVisible(false);
    layout->addWidget(progress_);

    statusLabel_->setObjectName("inspectorHint");
    statusLabel_->setWordWrap(true);
    layout->addWidget(statusLabel_);

    connect(tree_, &QTreeWidget::itemSelectionChanged,
            this,  [this]() {
        const CatalogEntry e = selectedEntry();
        detailLabel_->setText(e.description.isEmpty()
            ? QString() : e.description);
        updateButtonsState();
    });
    connect(btnDownload_, &QPushButton::clicked,
            this,         &ModelManagerWidget::downloadSelected);
    connect(btnCancel_,   &QPushButton::clicked,
            this,         &ModelManagerWidget::cancelDownload);
    connect(btnDelete_,   &QPushButton::clicked,
            this,         &ModelManagerWidget::deleteSelected);
    connect(btnRefresh_,  &QPushButton::clicked,
            this,         &ModelManagerWidget::refreshList);

    refreshList();
    setDownloading(false);
}

QList<ModelManagerWidget::CatalogEntry>
ModelManagerWidget::catalog()
{
    static const QString base =
        "https://github.com/danielgatis/rembg/releases/download/v0.0.0";

    return {
        // U2-Netp: unico com SHA256 publicado pelo projeto no CI.
        { "u2netp.onnx",
          TR("U²-Netp — rápido", "U²-Netp — fast"),
          TR("Modelo leve (~4.6 MB), ideal para testes e máquinas fracas. "
             "É o modelo padrão do programa.",
             "Lightweight model (~4.6 MB), ideal for testing and weaker "
             "machines. It is the program's default model."),
          4574861, base + "/u2netp.onnx",
          "309c8469258dda742793dce0ebea8e6dd393174f89934733ecc8b14c76f4ddd8" },
        { "silueta.onnx",
          TR("Silueta — equilíbrio", "Silueta — balanced"),
          TR("U²-Net comprimido (~43 MB): qualidade próxima do modelo "
             "completo com download 4x menor.",
             "Compressed U²-Net (~43 MB): quality close to the full "
             "model with a 4x smaller download."),
          44173029, base + "/silueta.onnx",
          "75da6c8d2f8096ec743d071951be73b4a8bc7b3e51d9a6625d63644f90ffeedb" },
        { "u2net.onnx",
          TR("U²-Net — máxima qualidade", "U²-Net — maximum quality"),
          TR("Modelo completo (~176 MB). Melhor segmentação geral; "
             "mais lento em CPU.",
             "Full model (~176 MB). Best general segmentation; "
             "slower on CPU."),
          175997641, base + "/u2net.onnx", QString() },
        { "u2net_human_seg.onnx",
          TR("U²-Net Human Seg — pessoas",
             "U²-Net Human Seg — people"),
          TR("Especializado em pessoas (~176 MB): retratos, fotos de "
             "grupo e vídeos com humanos.",
             "Specialized in people (~176 MB): portraits, group "
             "photos and footage with humans."),
          175997641, base + "/u2net_human_seg.onnx", QString() },
        { "isnet-general-use.onnx",
          TR("ISNet General Use — bordas melhores",
             "ISNet General Use — better edges"),
          TR("Geração ISNet (~178 MB), tipicamente com bordas mais limpas "
             "e menos resíduos que o U²-Net.",
             "ISNet generation (~178 MB), typically with cleaner edges "
             "and less residue than U²-Net."),
          178648008, base + "/isnet-general-use.onnx", QString() },
        { "isnet-anime.onnx",
          TR("ISNet Anime — ilustração", "ISNet Anime — illustration"),
          TR("Otimizado para anime/ilustração (~176 MB): mantém linhas e "
             "áreas planas intactas.",
             "Optimized for anime/illustration (~176 MB): keeps lines "
             "and flat areas intact."),
          176069933, base + "/isnet-anime.onnx", QString() },
    };
}

ModelManagerWidget::CatalogEntry
ModelManagerWidget::selectedEntry() const
{
    const QList<QTreeWidgetItem*> sel = tree_->selectedItems();
    if (sel.isEmpty()) return CatalogEntry();
    const int idx = tree_->indexOfTopLevelItem(sel.first());
    const QList<CatalogEntry> entries = catalog();
    if (idx < 0 || idx >= entries.size()) return CatalogEntry();
    return entries[idx];
}

void ModelManagerWidget::refreshList()
{
    const CatalogEntry selected = selectedEntry();
    const QString selectedFile = selected.fileName;

    tree_->clear();
    const QDir dir(ModelPathResolver::userModelsDir());
    const QList<CatalogEntry> entries = catalog();
    for (const CatalogEntry& e : entries) {
        auto* item = new QTreeWidgetItem(tree_);
        item->setText(0, e.title);
        item->setText(1, formatSize(e.sizeBytes));
        item->setText(2, QFile::exists(dir.absoluteFilePath(e.fileName))
                             ? TR("Instalado ✓", "Installed ✓") : TR("Disponível", "Available"));
        item->setToolTip(0, e.description + "\n" + e.fileName);
        item->setData(0, Qt::UserRole, e.fileName);
    }

    // Repõe a seleção anterior, se possível.
    for (int i = 0; i < tree_->topLevelItemCount(); ++i) {
        if (tree_->topLevelItem(i)->data(0, Qt::UserRole).toString()
                == selectedFile) {
            tree_->setCurrentItem(tree_->topLevelItem(i));
            break;
        }
    }
    updateButtonsState();
}

void ModelManagerWidget::updateButtonsState()
{
    const bool hasSelection = !tree_->selectedItems().isEmpty();
    btnDownload_->setEnabled(hasSelection && !reply_);
    btnDelete_->setEnabled(hasSelection && !reply_);
    btnCancel_->setEnabled(reply_ != nullptr);
}

void ModelManagerWidget::setDownloading(bool downloading)
{
    progress_->setVisible(downloading);
    tree_->setEnabled(!downloading);
    btnRefresh_->setEnabled(!downloading);
    updateButtonsState();
    if (!downloading && progress_->maximum() == 0)
        progress_->setRange(0, 100);
}

void ModelManagerWidget::downloadSelected()
{
    if (reply_) return;
    const CatalogEntry e = selectedEntry();
    if (e.fileName.isEmpty()) return;

    QDir().mkpath(ModelPathResolver::userModelsDir());
    pendingFileName_ = e.fileName;
    pendingSha256Hex_ = e.sha256Hex;

    outFile_.setFileName(ModelPathResolver::userModelsDir()
                         + "/" + e.fileName + ".part");
    if (!outFile_.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        statusLabel_->setText(
            TR("Erro: não foi possível escrever em ",
               "Error: could not write to ")
            + ModelPathResolver::userModelsDir());
        return;
    }

    hasher_.reset(e.sha256Hex.isEmpty() ? nullptr
        : new QCryptographicHash(QCryptographicHash::Sha256));

    QNetworkRequest request(QUrl(e.url));
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    reply_ = nam_->get(request);
    connect(reply_, &QNetworkReply::downloadProgress,
            this,   &ModelManagerWidget::onDownloadProgress);
    connect(reply_, &QNetworkReply::readyRead,
            this,   &ModelManagerWidget::onReadyRead);
    connect(reply_, &QNetworkReply::finished,
            this,   &ModelManagerWidget::onDownloadFinished);

    progress_->setRange(0, 100);
    progress_->setValue(0);
    statusLabel_->setText(TR("A descarregar ", "Downloading ")
                          + e.title + "…");
    setDownloading(true);
}

void ModelManagerWidget::onDownloadProgress(
    qint64 received, qint64 total)
{
    if (total > 0) {
        progress_->setRange(0, 100);
        progress_->setValue(static_cast<int>(received * 100 / total));
        statusLabel_->setText(QString(TR(
                "A descarregar… %1 / %2 (%3%)",
                "Downloading… %1 / %2 (%3%)"))
            .arg(formatSize(received), formatSize(total))
            .arg(received * 100 / total));
    } else {
        progress_->setRange(0, 0);
        statusLabel_->setText(TR("A descarregar… ", "Downloading… ")
            + formatSize(received));
    }
}

void ModelManagerWidget::onReadyRead()
{
    if (!reply_) return;
    const QByteArray data = reply_->readAll();
    outFile_.write(data);
    if (hasher_) hasher_->addData(data);
}

void ModelManagerWidget::onDownloadFinished()
{
    if (!reply_) return;
    outFile_.close();

    const QNetworkReply::NetworkError err = reply_->error();
    const QString errorString = reply_->errorString();
    QByteArray actualHash;
    if (hasher_) actualHash = hasher_->result().toHex();
    const QString fileName = pendingFileName_;
    const QString expectedHash = pendingSha256Hex_;

    reply_->deleteLater();
    reply_ = nullptr;
    hasher_.reset();
    setDownloading(false);

    const QString partPath = ModelPathResolver::userModelsDir()
        + "/" + fileName + ".part";
    const QString finalPath = ModelPathResolver::userModelsDir()
        + "/" + fileName;

    if (err != QNetworkReply::NoError) {
        QFile::remove(partPath);
        statusLabel_->setText(
            err == QNetworkReply::OperationCanceledError
                ? TR("Descarga cancelada.", "Download cancelled.")
                : TR("Erro na descarga: ", "Download error: ")
                      + errorString);
        refreshList();
        return;
    }

    if (!expectedHash.isEmpty() && actualHash != expectedHash.toLatin1()) {
        QFile::remove(partPath);
        statusLabel_->setText(
            TR("Erro: verificação SHA256 falhou (ficheiro corrompido ou "
               "alterado no servidor). Descarga rejeitada e apagada.",
               "Error: SHA256 verification failed (file corrupted or "
               "tampered with on the server). Download rejected and "
               "deleted."));
        refreshList();
        return;
    }

    QFile::remove(finalPath);
    if (QFile::rename(partPath, finalPath)) {
        statusLabel_->setText(TR("Modelo instalado: ", "Model installed: ")
                              + finalPath);
        refreshList();
        emit modelInstalled(fileName);
    } else {
        statusLabel_->setText(
            TR("Erro: não foi possível finalizar ",
               "Error: could not finalize ") + finalPath);
    }
    refreshList();
}

void ModelManagerWidget::cancelDownload()
{
    if (reply_) reply_->abort();
}

void ModelManagerWidget::deleteSelected()
{
    const CatalogEntry e = selectedEntry();
    if (e.fileName.isEmpty()) return;
    const QString path = ModelPathResolver::userModelsDir()
        + "/" + e.fileName;
    if (QFile::remove(path)) {
        statusLabel_->setText(TR("Removido: ", "Removed: ") + path);
        refreshList();
        emit modelInstalled(e.fileName);
    } else {
        statusLabel_->setText(
            TR("Não foi possível remover: ", "Could not remove: ") + path);
    }
}
