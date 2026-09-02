#include "mainwindow.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QScrollArea>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QProgressBar>
#include <QPlainTextEdit>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QApplication>
#include <QPixmap>
#include <QPainter>
#include <QSizePolicy>
#include <QDir>
#include <QFileInfo>
#include <QTimer>
#include <iostream>

// -- helper: fundo xadrez + alpha (so pra preview) --
QImage MainWindow::addAlphaBackground(const QImage& img) {
    if (img.isNull()) return QImage();
    QImage result(img.size(), QImage::Format_RGBA8888);

    for (int y = 0; y < img.height(); ++y) {
        for (int x = 0; x < img.width(); ++x) {
            bool light = ((x / 8) + (y / 8)) % 2 == 0;
            result.setPixel(x, y, qRgba(
                light ? 0xCC : 0xAA,
                light ? 0xCC : 0xAA,
                light ? 0xCC : 0xAA, 0xFF));
        }
    }

    for (int y = 0; y < img.height(); ++y) {
        for (int x = 0; x < img.width(); ++x) {
            QRgb src = img.pixel(x, y);
            int alpha = qAlpha(src);
            if (alpha == 255) {
                result.setPixel(x, y, src);
            } else if (alpha > 0) {
                QRgb dst = result.pixel(x, y);
                float a = alpha / 255.0f;
                int r = static_cast<int>(qRed(src) * a + qRed(dst) * (1 - a));
                int g = static_cast<int>(qGreen(src) * a + qGreen(dst) * (1 - a));
                int b = static_cast<int>(qBlue(src) * a + qBlue(dst) * (1 - a));
                result.setPixel(x, y, qRgb(r, g, b));
            }
        }
    }
    return result;
}

// -- construtor --
MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent),
      originalLabel_(new QLabel("Nenhuma imagem")),
      resultLabel_(new QLabel("Sem resultado")),
      originalScroll_(new QScrollArea),
      resultScroll_(new QScrollArea),
      modelPathEdit_(new QLineEdit("./u2netp.onnx")),
      folderEdit_(new QLineEdit),
      outputDirEdit_(new QLineEdit("./_rembg")),
      btnOpen_(new QPushButton("Abrir imagem")),
      btnRemove_(new QPushButton("Remover fundo")),
      btnSave_(new QPushButton("Salvar como PNG")),
      btnLoadModel_(new QPushButton("Carregar modelo")),
      btnSelectFolder_(new QPushButton("...")),
      btnSelectOutputFolder_(new QPushButton("...")),
      btnProcessBatch_(new QPushButton("Processar lote")),
      progressBar_(new QProgressBar),
      logEdit_(new QPlainTextEdit),
      batchIndex_(0),
      batchRunning_(false)
{
    originalLabel_->setAlignment(Qt::AlignCenter);
    resultLabel_->setAlignment(Qt::AlignCenter);

    originalScroll_->setWidget(originalLabel_);
    originalScroll_->setWidgetResizable(true);
    originalScroll_->setAlignment(Qt::AlignCenter);
    resultScroll_->setWidget(resultLabel_);
    resultScroll_->setWidgetResizable(true);
    resultScroll_->setAlignment(Qt::AlignCenter);

    modelPathEdit_->setReadOnly(true);
    modelPathEdit_->setMinimumWidth(250);
    folderEdit_->setReadOnly(true);
    outputDirEdit_->setReadOnly(true);

    progressBar_->setRange(0, 100);
    progressBar_->setValue(0);
    progressBar_->setVisible(false);

    logEdit_->setReadOnly(true);
    logEdit_->setMaximumHeight(150);
    logEdit_->setPlaceholderText("Log do processamento...");

    // ---- layout superior: modelo ----
    auto* topLayout = new QHBoxLayout();
    topLayout->addWidget(new QLabel("Modelo:"));
    topLayout->addWidget(modelPathEdit_);
    topLayout->addWidget(btnLoadModel_);

    // ---- regioes de preview ----
    auto* previewLayout = new QHBoxLayout();
    previewLayout->addWidget(originalScroll_);
    previewLayout->addWidget(resultScroll_);

    // ---- botoes single image ----
    auto* btnLayout = new QHBoxLayout();
    btnLayout->addWidget(btnOpen_);
    btnLayout->addWidget(btnRemove_);
    btnLayout->addWidget(btnSave_);

    // ---- seletor de pasta ----
    auto* folderLayout = new QHBoxLayout();
    folderLayout->addWidget(new QLabel("Pasta:"));
    folderLayout->addWidget(folderEdit_);
    folderLayout->addWidget(btnSelectFolder_);

    // ---- diretorio saida + processar ----
    auto* batchLayout = new QHBoxLayout();
    batchLayout->addWidget(new QLabel("Saida:"));
    batchLayout->addWidget(outputDirEdit_);
    batchLayout->addWidget(btnSelectOutputFolder_);
    batchLayout->addWidget(btnProcessBatch_);

    // ---- layout principal ----
    auto* mainLayout = new QVBoxLayout();
    mainLayout->addLayout(topLayout);
    mainLayout->addLayout(previewLayout);
    mainLayout->addLayout(btnLayout);
    mainLayout->addLayout(folderLayout);
    mainLayout->addLayout(batchLayout);
    mainLayout->addWidget(progressBar_);
    mainLayout->addWidget(logEdit_);

    auto* central = new QWidget(this);
    central->setLayout(mainLayout);
    setCentralWidget(central);

    originalScroll_->setMaximumSize(640, 640);
    resultScroll_->setMaximumSize(640, 640);

    setWindowTitle("BG Remover — U\u00b2-Netp");
    resize(1200, 800);

    // conexoes
    connect(btnOpen_,               &QPushButton::clicked, this, &MainWindow::openImage);
    connect(btnRemove_,             &QPushButton::clicked, this, &MainWindow::removeBackground);
    connect(btnSave_,               &QPushButton::clicked, this, &MainWindow::saveResult);
    connect(btnLoadModel_,          &QPushButton::clicked, this, &MainWindow::loadModel);
    connect(btnSelectFolder_,       &QPushButton::clicked, this, &MainWindow::selectFolder);
    connect(btnSelectOutputFolder_, &QPushButton::clicked, this, &MainWindow::selectOutputFolder);
    connect(btnProcessBatch_,       &QPushButton::clicked, this, &MainWindow::processBatch);
}

// ===== SLOTS =====

void MainWindow::openImage() {
    QString path = QFileDialog::getOpenFileName(
        this, "Abrir imagem", QDir::homePath(),
        "Imagens (*.png *.jpg *.jpeg *.bmp *.webp *.tiff)");
    if (path.isEmpty()) return;

    QImage img(path);
    if (img.isNull()) {
        QMessageBox::warning(this, "Erro",
            "Nao foi possivel carregar:\n" + path);
        return;
    }
    originalImage_ = img;
    remover_.reset();
    setOriginal(img);
    logEdit_->appendPlainText("[INFO] Carregada: " + path);
}

void MainWindow::loadModel() {
    QString path = QFileDialog::getOpenFileName(
        this, "Carregar modelo ONNX", QDir::homePath(),
        "ONNX (*.onnx)");
    if (path.isEmpty()) return;
    modelPathEdit_->setText(path);
    remover_.reset();
    logEdit_->appendPlainText("[INFO] Modelo definido: " + path);
}

void MainWindow::removeBackground() {
    if (originalImage_.isNull()) {
        QMessageBox::warning(this, "Aviso",
            "Abra uma imagem primeiro.");
        return;
    }

    try {
        remover_ = std::make_unique<BackgroundRemover>(
            modelPathEdit_->text().toStdString());
        resultImage_ = remover_->removeBackground(
            originalImage_);
        setResult(resultImage_);
        logEdit_->appendPlainText("[OK] Fundo removido.");
    } catch (const Ort::Exception& e) {
        QMessageBox::critical(this, "Erro ONNX",
            QString("Falha na inferência:\n") + e.what());
    } catch (const std::exception& e) {
        QMessageBox::critical(this, "Erro",
            QString("Falha:\n") + e.what());
    }
}

void MainWindow::saveResult() {
    if (resultImage_.isNull()) {
        QMessageBox::warning(this, "Aviso",
            "Remova o fundo primeiro.");
        return;
    }

    QString path = QFileDialog::getSaveFileName(
        this, "Salvar PNG", QDir::homePath(),
        "PNG (*.png)");
    if (path.isEmpty()) return;

    QFileInfo fi(path);
    if (fi.suffix().toLower() != "png") {
        path += ".png";
    }

    // alpha REAL — sem addAlphaBackground!
    if (!resultImage_.save(path, "PNG")) {
        QMessageBox::critical(this, "Erro",
            "Nao foi possivel salvar:\n" + path);
    } else {
        QMessageBox::information(this, "OK",
            "Salvo com alpha real:\n" + path);
        logEdit_->appendPlainText("[OK] PNG salvo: " + path);
    }
}

void MainWindow::selectFolder() {
    QString folder = QFileDialog::getExistingDirectory(
        this, "Selecionar pasta com imagens",
        QDir::homePath());
    if (folder.isEmpty()) return;
    folderEdit_->setText(folder);
    logEdit_->appendPlainText("[INFO] Pasta selecionada: " + folder);
}

void MainWindow::selectOutputFolder() {
    QString folder = QFileDialog::getExistingDirectory(
        this, "Selecionar pasta de saida",
        outputDirEdit_->text().isEmpty()
            ? QDir::homePath()
            : outputDirEdit_->text());
    if (folder.isEmpty()) return;
    outputDirEdit_->setText(folder);
    logEdit_->appendPlainText("[INFO] Pasta de saida definida: " + folder);
}

void MainWindow::processBatch() {
    // BUGFIX: a checagem original testava batchFolder_ (so setado
    // MAIS ABAIXO nesta mesma funcao), entao era sempre vazio na
    // primeira chamada e o batch nunca rodava. A fonte real do dado
    // e folderEdit_, entao checamos ele primeiro.
    if (folderEdit_->text().isEmpty()) {
        QMessageBox::warning(this, "Aviso",
            "Selecione uma pasta primeiro.");
        return;
    }

    batchFolder_ = folderEdit_->text();
    batchOutputDir_ = outputDirEdit_->text();

    // listar imagens na pasta (filtro manual, case-insensitive:
    // o QDir::entryList com nameFilters e case-sensitive no Linux,
    // entao "foto.JPG" seria ignorado com o filtro original)
    static const QStringList kExts = {
        "png", "jpg", "jpeg", "bmp", "webp", "tiff"};

    QDir dir(batchFolder_);
    QStringList allFiles = dir.entryList(QDir::Files, QDir::Name);
    batchFiles_.clear();
    for (const QString& f : allFiles) {
        QString ext = QFileInfo(f).suffix().toLower();
        if (kExts.contains(ext)) {
            batchFiles_.append(f);
        }
    }

    if (batchFiles_.isEmpty()) {
        QMessageBox::warning(this, "Aviso",
            "Nenhuma imagem encontrada na pasta.");
        return;
    }

    // criar diretorio de saida
    QDir outDir(batchOutputDir_);
    if (!outDir.exists()) {
        if (!outDir.mkpath(".")) {
            QMessageBox::critical(this, "Erro",
                "Nao foi possivel criar diretorio de saida:\n"
                + batchOutputDir_);
            return;
        }
    }

    // desabilitar botoes
    btnOpen_->setEnabled(false);
    btnRemove_->setEnabled(false);
    btnSave_->setEnabled(false);
    btnProcessBatch_->setEnabled(false);
    btnSelectFolder_->setEnabled(false);
    btnSelectOutputFolder_->setEnabled(false);
    btnLoadModel_->setEnabled(false);
    progressBar_->setRange(0, batchFiles_.size());
    progressBar_->setValue(0);
    progressBar_->setVisible(true);
    logEdit_->clear();
    logEdit_->appendPlainText(
        QString("[BATCH] Processando %1 arquivos "
                "para %2...")
            .arg(batchFiles_.size())
            .arg(batchOutputDir_));

    batchIndex_ = 0;
    batchRunning_ = true;

    // garantir que o modelo esta carregado
    try {
        remover_ = std::make_unique<BackgroundRemover>(
            modelPathEdit_->text().toStdString());
    } catch (const Ort::Exception& e) {
        QMessageBox::critical(this, "Erro de modelo",
            QString("Nao foi possivel carregar o modelo:\n")
                + e.what());
        batchRunning_ = false;
        btnOpen_->setEnabled(true);
        btnRemove_->setEnabled(true);
        btnSave_->setEnabled(true);
        btnProcessBatch_->setEnabled(true);
        btnSelectFolder_->setEnabled(true);
        btnSelectOutputFolder_->setEnabled(true);
        btnLoadModel_->setEnabled(true);
        progressBar_->setVisible(false);
        return;
    }

    // processar primeira imagem (adiar para liberar GUI)
    QTimer::singleShot(0, this, &MainWindow::processBatchStep);
}

void MainWindow::processBatchStep() {
    if (!batchRunning_) return;

    if (batchIndex_ >= batchFiles_.size()) {
        batchRunning_ = false;
        progressBar_->setValue(batchFiles_.size());
        logEdit_->appendPlainText(
            "[BATCH] Concluido! "
            + QString::number(batchFiles_.size())
            + " arquivo(s) processado(s).");

        // reabilitar botoes
        btnOpen_->setEnabled(true);
        btnRemove_->setEnabled(true);
        btnSave_->setEnabled(true);
        btnProcessBatch_->setEnabled(true);
        btnSelectFolder_->setEnabled(true);
        btnSelectOutputFolder_->setEnabled(true);
        btnLoadModel_->setEnabled(true);
        progressBar_->setVisible(false);
        return;
    }

    QString file = batchFiles_[batchIndex_];
    QString inputPath = QDir(batchFolder_).absoluteFilePath(file);

    // nome de saida: foto.jpg -> foto_rembg.png
    QFileInfo fi(file);
    QString outName = fi.baseName() + "_rembg.png";
    QString outPath = QDir(batchOutputDir_).absoluteFilePath(outName);

    try {
        QImage img(inputPath);
        if (img.isNull()) {
            logEdit_->appendPlainText(
                "[ERRO] Falha ao carregar: " + file);
        } else {
            QImage result = remover_->removeBackground(img);
            result.save(outPath, "PNG");
            logEdit_->appendPlainText(
                "[OK] " + file + " -> " + outName);
        }
    } catch (const Ort::Exception& e) {
        logEdit_->appendPlainText(
            "[ERRO] " + file + ": " + QString(e.what()));
    } catch (const std::exception& e) {
        logEdit_->appendPlainText(
            "[ERRO] " + file + ": " + QString::fromStdString(
                e.what()));
    }

    batchIndex_++;
    progressBar_->setValue(batchIndex_);

    // processar proxima (sem bloquear GUI)
    QTimer::singleShot(0, this, &MainWindow::processBatchStep);
}

// ===== helpers =====

void MainWindow::setOriginal(const QImage& img) {
    originalLabel_->setPixmap(
        QPixmap::fromImage(img).scaled(
            originalScroll_->size(),
            Qt::KeepAspectRatio,
            Qt::SmoothTransformation));
}

void MainWindow::setResult(const QImage& img) {
    QImage shown = addAlphaBackground(img);
    resultLabel_->setPixmap(
        QPixmap::fromImage(shown).scaled(
            resultScroll_->size(),
            Qt::KeepAspectRatio,
            Qt::SmoothTransformation));
}

