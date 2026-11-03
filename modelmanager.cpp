#include "modelmanager.h"
#include "i18n.h"
#include "modelpathresolver.h"
#include "modelvalidator.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProgressBar>
#include <QPushButton>
#include <QStringList>
#include <QTreeWidgetItem>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrent>

namespace {

QString formatSize(qint64 bytes)
{
    if (bytes >= 1024 * 1024)
        return QString::number(bytes / (1024.0 * 1024.0), 'f', 0) + " MB";
    if (bytes >= 1024)
        return QString::number(bytes / 1024.0, 'f', 0) + " kB";
    return QString::number(bytes) + " B";
}

QString modelStateText(ModelFileState state)
{
    switch (state) {
    case ModelFileState::Installed:
        return TR("Instalado ✓", "Installed ✓");
    case ModelFileState::Incomplete:
        return TR("Incompleto", "Incomplete");
    case ModelFileState::Corrupt:
        return TR("Corrompido", "Corrupt");
    case ModelFileState::Missing:
        break;
    }
    return TR("Disponível", "Available");
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
    buttons->addWidget(btnRefresh_ = new QPushButton(
        TR("Atualizar", "Refresh")));
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
    connect(&validationWatcher_,
            &QFutureWatcher<ModelFileValidation>::finished,
            this, &ModelManagerWidget::onValidationFinished);

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

    // A fila e reconstruida; a verificacao em curso (se houver)
    // termina e so atualiza a linha se o caminho ainda for o mesmo.
    validationQueue_.clear();

    tree_->clear();
    const QList<CatalogEntry> entries = catalog();
    for (const CatalogEntry& e : entries) {
        auto* item = new QTreeWidgetItem(tree_);
        item->setText(0, e.title);
        item->setText(1, formatSize(e.sizeBytes));
        const QString installedPath =
            ModelPathResolver::findUserModel(e.fileName);

        // Verificacao rapida (tamanho) para a lista abrir sem
        // bloquear: o SHA256 de modelos de ~178 MB corre depois,
        // em segundo plano.
        const ModelFileValidation quick = validateModelFile(
            installedPath, e.sizeBytes, QString());
        item->setText(2, modelStateText(quick.state));

        QString tip = e.description + "\n"
            + (installedPath.isEmpty() ? e.fileName : installedPath);
        if (quick.state == ModelFileState::Incomplete) {
            tip += "\n" + TR("Tamanho: %1 de %2",
                             "Size: %1 of %2")
                       .arg(formatSize(quick.actualSize),
                            formatSize(e.sizeBytes));
        } else if (quick.state == ModelFileState::Corrupt) {
            tip += "\n" + TR("Tamanho inesperado: %1 (esperado %2)",
                             "Unexpected size: %1 (expected %2)")
                       .arg(formatSize(quick.actualSize),
                            formatSize(e.sizeBytes));
        } else if (quick.state == ModelFileState::Installed
                   && !e.sha256Hex.isEmpty()) {
            tip += "\n" + TR("A verificar integridade (SHA256)…",
                             "Verifying integrity (SHA256)…");
        }
        item->setToolTip(0, tip);
        item->setToolTip(2, tip);

        item->setData(0, Qt::UserRole, e.fileName);
        item->setData(0, Qt::UserRole + 1, installedPath);
        item->setData(0, Qt::UserRole + 2, e.sizeBytes);

        if (quick.state == ModelFileState::Installed
            && !e.sha256Hex.isEmpty()) {
            const bool alreadyRunning =
                validationRunning_
                && currentValidation_.path == installedPath
                && currentValidation_.expectedSize == e.sizeBytes;
            if (!alreadyRunning) {
                ValidationJob job;
                job.fileName = e.fileName;
                job.path = installedPath;
                job.expectedSize = e.sizeBytes;
                job.sha256Hex = e.sha256Hex;
                const QFileInfo info(installedPath);
                job.fileSize = info.size();
                job.lastModifiedMs =
                    info.lastModified().toMSecsSinceEpoch();
                validationQueue_.append(job);
            }
        }
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
    startNextValidation();
}

void ModelManagerWidget::updateButtonsState()
{
    const bool hasSelection = !tree_->selectedItems().isEmpty();
    btnDownload_->setEnabled(hasSelection && !reply_);
    btnDelete_->setEnabled(hasSelection && !reply_);
    btnCancel_->setEnabled(reply_ != nullptr);
}

void ModelManagerWidget::startNextValidation()
{
    if (validationRunning_ || validationQueue_.isEmpty()) return;

    currentValidation_ = validationQueue_.takeFirst();
    validationRunning_ = true;
    const ValidationJob job = currentValidation_;
    validationWatcher_.setFuture(QtConcurrent::run([job]() {
        return validateModelFile(job.path, job.expectedSize,
                                 job.sha256Hex);
    }));
}

void ModelManagerWidget::onValidationFinished()
{
    validationRunning_ = false;
    ModelFileValidation validation = validationWatcher_.result();

    // O ficheiro pode ter sido apagado ou substituido enquanto o
    // hash corria. Comparar tamanho E mtime evita aplicar um hash
    // antigo a um ficheiro novo do mesmo tamanho; nesse caso o
    // estado atual (so tamanho) manda.
    const QFileInfo info(currentValidation_.path);
    const bool unchanged = info.exists()
        && info.size() == currentValidation_.fileSize
        && info.lastModified().toMSecsSinceEpoch()
               == currentValidation_.lastModifiedMs;
    const ModelFileValidation current = validateModelFile(
        currentValidation_.path, currentValidation_.expectedSize,
        QString());
    if (!unchanged || current.state != ModelFileState::Installed)
        validation = current;

    updateRowStatus(currentValidation_, validation);
    startNextValidation();
}

void ModelManagerWidget::updateRowStatus(
    const ValidationJob& job,
    const ModelFileValidation& validation)
{
    for (int i = 0; i < tree_->topLevelItemCount(); ++i) {
        QTreeWidgetItem* item = tree_->topLevelItem(i);
        if (item->data(0, Qt::UserRole).toString() != job.fileName)
            continue;

        // A lista pode ter sido reconstruida com outro caminho (ex.:
        // modelo canonico criado entretanto); o resultado antigo nao
        // se aplica a esse caminho.
        if (item->data(0, Qt::UserRole + 1).toString() != job.path)
            return;

        item->setText(2, modelStateText(validation.state));
        QString tip;
        switch (validation.state) {
        case ModelFileState::Installed:
            tip = TR("Integridade verificada (SHA256).",
                     "Integrity verified (SHA256).");
            break;
        case ModelFileState::Incomplete:
            tip = TR("Ficheiro incompleto: %1 de %2.",
                     "Incomplete file: %1 of %2.")
                      .arg(formatSize(validation.actualSize),
                           formatSize(job.expectedSize));
            break;
        case ModelFileState::Corrupt:
            tip = TR("Ficheiro corrompido: o SHA256 ou o tamanho não "
                     "corresponde ao esperado. Descarregue novamente.",
                     "Corrupt file: SHA256 or size does not match the "
                     "expected value. Download it again.");
            break;
        case ModelFileState::Missing:
            tip = TR("Modelo não instalado.",
                     "Model not installed.");
            break;
        }
        item->setToolTip(2, tip);
        return;
    }
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
    pendingSizeBytes_ = e.sizeBytes;

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
    request.setTransferTimeout(30000);
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

    // Sem SHA256 publicado, o tamanho exato e a unica verificacao
    // disponivel (os valores do catalogo vem dos assets do rembg).
    const qint64 actualSize = QFileInfo(partPath).size();
    if (pendingSizeBytes_ > 0 && actualSize != pendingSizeBytes_) {
        QFile::remove(partPath);
        statusLabel_->setText(
            TR("Erro: tamanho inesperado (%1 bytes, esperado %2). "
               "Descarga rejeitada e apagada.",
               "Error: unexpected size (%1 bytes, expected %2). "
               "Download rejected and deleted.")
                .arg(actualSize).arg(pendingSizeBytes_));
        refreshList();
        return;
    }

    // Nao apagar o modelo antigo antes de o novo estar no lugar: se
    // o rename falhar, o utilizador ficaria sem nenhuma versao.
    const QString backupPath = finalPath + ".old";
    QFile::remove(backupPath);
    const bool hadOld = QFile::exists(finalPath);
    if (hadOld && !QFile::rename(finalPath, backupPath)) {
        QFile::remove(partPath);
        statusLabel_->setText(
            TR("Erro: não foi possível substituir o modelo existente.",
               "Error: could not replace the existing model."));
        refreshList();
        return;
    }

    if (QFile::rename(partPath, finalPath)) {
        if (hadOld) QFile::remove(backupPath);
        statusLabel_->setText(TR("Modelo instalado: ", "Model installed: ")
                              + finalPath);
        refreshList();
        emit modelsChanged(fileName);
    } else {
        // Restaurar a versao anterior, se existia.
        if (hadOld) QFile::rename(backupPath, finalPath);
        statusLabel_->setText(
            TR("Erro: não foi possível finalizar ",
               "Error: could not finalize ") + finalPath);
        refreshList();
    }
}

void ModelManagerWidget::cancelDownload()
{
    if (reply_) reply_->abort();
}

void ModelManagerWidget::deleteSelected()
{
    const CatalogEntry e = selectedEntry();
    if (e.fileName.isEmpty()) return;

    const auto answer = QMessageBox::question(
        this, TR("Apagar modelo", "Delete model"),
        TR("Apagar \"%1\" do disco? Esta acao nao pode ser desfeita.",
           "Delete \"%1\" from disk? This cannot be undone.")
            .arg(e.fileName),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (answer != QMessageBox::Yes) return;

    const QStringList paths =
        ModelPathResolver::findAllUserModels(e.fileName);
    if (paths.isEmpty()) {
        statusLabel_->setText(
            TR("Modelo não instalado.", "Model not installed."));
        return;
    }

    QStringList removed;
    QStringList failed;
    for (const QString& path : paths) {
        if (QFile::remove(path))
            removed << path;
        else
            failed << path;
    }

    QString message;
    if (!removed.isEmpty()) {
        message = TR("Removido: ", "Removed: ") + removed.join(", ");
    }
    if (!failed.isEmpty()) {
        if (!message.isEmpty()) message += "\n";
        message += TR("Não foi possível remover: ",
                      "Could not remove: ") + failed.join(", ");
    }
    statusLabel_->setText(message);

    if (!removed.isEmpty()) {
        refreshList();
        emit modelsChanged(e.fileName);
    }
}
