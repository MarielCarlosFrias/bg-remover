#include "mainwindow.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QScrollArea>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QProgressBar>
#include <QPlainTextEdit>
#include <QComboBox>
#include <QCheckBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QApplication>
#include <QPixmap>
#include <QPainter>
#include <QSizePolicy>
#include <QDir>
#include <QFileInfo>
#include <QTimer>
#include <QFile>
#include <QSet>
#include <QColorDialog>
#include <algorithm>
#include <iostream>

static const QSet<QString> kImageExts = {
    "png", "jpg", "jpeg", "bmp", "webp", "tiff", "tif"
};
static constexpr float kAlphaThreshold = 30.0f;

QString MainWindow::resolveModelPath(const QString& name) {
    QStringList candidates;
    QString cwd = QDir::currentPath();
    candidates << cwd + "/" + name;
    candidates << cwd + "/../" + name;
    candidates << cwd + "/../../" + name;
    candidates << QDir::homePath() + "/" + name;
    candidates << QDir::homePath() + "/projetos/bg-remover/" + name;
#ifdef INSTALLED
    candidates << "/usr/share/bg-remover/" + name;
#endif
    for (const QString& p : candidates) {
        if (QFile::exists(p)) return p;
    }
    return QDir::homePath() + "/" + name;
}

QColor MainWindow::currentBackgroundColor() const {
    int idx = bgColorCombo_->currentIndex();
    if (idx == 0) return QColor();
    if (idx == 1) return Qt::white;
    if (idx == 2) return Qt::black;
    if (idx == 3) return customBgColor_;
    return QColor();
}

QImage MainWindow::addAlphaBackground(
    const QImage& img, const QColor& bgColor)
{
    if (img.isNull()) return QImage();
    QImage result(img.size(), QImage::Format_RGBA8888);

    if (!bgColor.isValid()) {
        for (int y = 0; y < img.height(); ++y) {
            for (int x = 0; x < img.width(); ++x) {
                bool light = ((x / 8) + (y / 8)) % 2 == 0;
                result.setPixel(x, y, qRgba(
                    light ? 0xCC : 0xAA,
                    light ? 0xCC : 0xAA,
                    light ? 0xCC : 0xAA, 0xFF));
            }
        }
    } else {
        result.fill(bgColor);
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
                result.setPixel(x, y, qRgba(r, g, b, 255));
            }
        }
    }
    return result;
}

MainWindow::MainWindow(QWidget* parent, const QString& modelPath)
    : QMainWindow(parent),
      originalLabel_(new QLabel("Nenhuma imagem")),
      resultLabel_(new QLabel("Sem resultado")),
      originalScroll_(new QScrollArea),
      resultScroll_(new QScrollArea),
      modelCombo_(new QComboBox),
      modelPathEdit_(new QLineEdit),
      folderEdit_(new QLineEdit),
      outputDirEdit_(new QLineEdit("./_rembg")),
      btnOpen_(new QPushButton("Abrir imagem")),
      btnRemove_(new QPushButton("Remover fundo")),
      btnSave_(new QPushButton("Salvar como PNG")),
      btnLoadModel_(new QPushButton("Buscar...")),
      btnSelectFolder_(new QPushButton("...")),
      btnSelectOutput_(new QPushButton("...")),
      btnProcessBatch_(new QPushButton("Processar lote")),
      bgColorCombo_(new QComboBox),
      refineCheckBox_(new QCheckBox("Refinar bordas")),
      noiseCheckBox_(new QCheckBox("Limpar ruído")),
      progressBar_(new QProgressBar),
      logEdit_(new QPlainTextEdit),
      batchIndex_(0),
      batchRunning_(false),
      customBgColor_(QColor())
{
    originalLabel_->setAlignment(Qt::AlignCenter);
    resultLabel_->setAlignment(Qt::AlignCenter);

    originalScroll_->setWidget(originalLabel_);
    originalScroll_->setWidgetResizable(true);
    originalScroll_->setAlignment(Qt::AlignCenter);
    resultScroll_->setWidget(resultLabel_);
    resultScroll_->setWidgetResizable(true);
    resultScroll_->setAlignment(Qt::AlignCenter);

    modelCombo_->addItem(
        "U\u00b2-Netp \u2014 r\u00e1pido (~5 MB)",
        resolveModelPath("u2netp.onnx"));
    modelCombo_->addItem(
        "U\u00b2-Net  \u2014 qualidade (~176 MB)",
        resolveModelPath("u2net.onnx"));
    modelCombo_->addItem("Caminho customizado", "");
    modelCombo_->setCurrentIndex(0);

    if (!modelPath.isEmpty()) {
        modelPathEdit_->setText(modelPath);
        modelCombo_->setCurrentIndex(2);
    } else {
        modelPathEdit_->setText(
            modelCombo_->itemData(0).toString());
    }
    modelPathEdit_->setMinimumWidth(280);

    bgColorCombo_->addItem("Transparente", QVariant());
    bgColorCombo_->addItem(
        "Branco", QVariant::fromValue(QColor(Qt::white)));
    bgColorCombo_->addItem(
        "Preto", QVariant::fromValue(QColor(Qt::black)));
    bgColorCombo_->addItem("Personalizado...", QVariant());
    bgColorCombo_->setCurrentIndex(0);
    bgColorCombo_->setMaximumWidth(160);

    outputDirEdit_->setReadOnly(true);

    progressBar_->setRange(0, 100);
    progressBar_->setValue(0);
    progressBar_->setVisible(false);

    logEdit_->setReadOnly(true);
    logEdit_->setMaximumHeight(150);
    logEdit_->setPlaceholderText("Log do processamento...");

    auto* topLayout = new QHBoxLayout();
    topLayout->addWidget(new QLabel("Modelo:"));
    topLayout->addWidget(modelCombo_);
    topLayout->addWidget(modelPathEdit_);
    topLayout->addWidget(btnLoadModel_);

    auto* previewLayout = new QHBoxLayout();
    previewLayout->addWidget(originalScroll_);
    previewLayout->addWidget(resultScroll_);

    auto* bgLayout = new QHBoxLayout();
    bgLayout->addWidget(new QLabel("Fundo:"));
    bgLayout->addWidget(bgColorCombo_);
    bgLayout->addStretch();

    auto* btnLayout = new QHBoxLayout();
    btnLayout->addWidget(btnOpen_);
    btnLayout->addWidget(btnRemove_);
    btnLayout->addWidget(btnSave_);
    btnLayout->addStretch();
    btnLayout->addWidget(refineCheckBox_);
    btnLayout->addWidget(noiseCheckBox_);

    auto* folderLayout = new QHBoxLayout();
    folderLayout->addWidget(new QLabel("Pasta:"));
    folderLayout->addWidget(folderEdit_);
    folderLayout->addWidget(btnSelectFolder_);

    auto* batchLayout = new QHBoxLayout();
    batchLayout->addWidget(new QLabel("Saida:"));
    batchLayout->addWidget(outputDirEdit_);
    batchLayout->addWidget(btnSelectOutput_);
    batchLayout->addWidget(btnProcessBatch_);

    auto* mainLayout = new QVBoxLayout();
    mainLayout->addLayout(topLayout);
    mainLayout->addLayout(previewLayout);
    mainLayout->addLayout(bgLayout);
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

    setWindowTitle("BG Remover \u2014 U\u00b2-Netp");
    resize(1200, 800);

    connect(btnOpen_,          &QPushButton::clicked,
            this,              &MainWindow::openImage);
    connect(btnRemove_,        &QPushButton::clicked,
            this,              &MainWindow::removeBackground);
    connect(btnSave_,          &QPushButton::clicked,
            this,              &MainWindow::saveResult);
    connect(btnLoadModel_,     &QPushButton::clicked,
            this,              &MainWindow::loadModel);
    connect(btnSelectFolder_,  &QPushButton::clicked,
            this,              &MainWindow::selectFolder);
    connect(btnSelectOutput_,  &QPushButton::clicked,
            this,              &MainWindow::selectOutputFolder);
    connect(btnProcessBatch_,  &QPushButton::clicked,
            this,              &MainWindow::processBatch);
    connect(modelCombo_, QOverload<int>::of(
                &QComboBox::currentIndexChanged),
            this,              &MainWindow::modelIndexChanged);
    connect(bgColorCombo_, QOverload<int>::of(
                &QComboBox::currentIndexChanged),
            this,              &MainWindow::bgColorChanged);
}

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
    modelCombo_->setCurrentIndex(2);
    remover_.reset();
    logEdit_->appendPlainText("[INFO] Modelo definido: " + path);
}

void MainWindow::modelIndexChanged(int index) {
    if (index < 0 || index >= modelCombo_->count()) return;
    QString path = modelCombo_->itemData(index).toString();
    modelPathEdit_->setText(path);
    remover_.reset();
}

void MainWindow::bgColorChanged(int index) {
    if (index == 3) {
        QColor color = QColorDialog::getColor(
            customBgColor_.isValid() ? customBgColor_
                                      : Qt::white,
            this, "Escolher cor de fundo");
        if (color.isValid()) {
            customBgColor_ = color;
            bgColorCombo_->setItemData(3,
                QVariant::fromValue(color));
            bgColorCombo_->setItemText(3, color.name());
            bgColorCombo_->setCurrentIndex(3);
        } else {
            bgColorCombo_->setCurrentIndex(0);
        }
    }
    if (!resultImage_.isNull()) {
        setResult(resultImage_);
    }
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
        if (noiseCheckBox_->isChecked()) {
            resultImage_ = applyAlphaThreshold(
                resultImage_, kAlphaThreshold);
            logEdit_->appendPlainText(
                "[INFO] Ruído limpo (threshold "
                + QString::number(kAlphaThreshold) + ").");
        }
        if (refineCheckBox_->isChecked()) {
            resultImage_ = applyEdgeRefinement(
                resultImage_, 0.5f);
            logEdit_->appendPlainText(
                "[INFO] Bordas refinadas.");
        }
        setResult(resultImage_);
        logEdit_->appendPlainText("[OK] Fundo removido.");
    } catch (const Ort::Exception& e) {
        QMessageBox::critical(this, "Erro ONNX",
            QString("Falha na inferiencia:\n") + e.what());
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
    if (fi.suffix().toLower() != "png") path += ".png";

    QImage toSave;
    QColor bg = currentBackgroundColor();
    if (!bg.isValid()) {
        toSave = resultImage_.copy();
    } else {
        toSave = addAlphaBackground(resultImage_, bg);
    }

    if (!toSave.save(path, "PNG")) {
        QMessageBox::critical(this, "Erro",
            "Nao foi possivel salvar:\n" + path);
    } else {
        QMessageBox::information(this, "OK",
            "Salvo:\n" + path);
        logEdit_->appendPlainText("[OK] PNG salvo: " + path);
    }
}

void MainWindow::selectFolder() {
    QString folder = QFileDialog::getExistingDirectory(
        this, "Selecionar pasta com imagens",
        folderEdit_->text().isEmpty()
            ? QDir::homePath() : folderEdit_->text());
    if (folder.isEmpty()) return;
    folderEdit_->setText(folder);
    logEdit_->appendPlainText(
        "[INFO] Pasta selecionada: " + folder);
}

void MainWindow::selectOutputFolder() {
    QString folder = QFileDialog::getExistingDirectory(
        this, "Selecionar diretorio de saida",
        outputDirEdit_->text().isEmpty()
            ? QDir::homePath() : outputDirEdit_->text());
    if (folder.isEmpty()) return;
    outputDirEdit_->setText(folder);
    logEdit_->appendPlainText(
        "[INFO] Dir. de saida definido: " + folder);
}

void MainWindow::processBatch() {
    if (folderEdit_->text().isEmpty()) {
        QMessageBox::warning(this, "Aviso",
            "Selecione uma pasta primeiro.");
        return;
    }
    batchFolder_ = folderEdit_->text();
    batchOutputDir_ = outputDirEdit_->text();

    QStringList allFiles = QDir(batchFolder_).entryList(
        QDir::Files | QDir::NoDotAndDotDot);
    batchFiles_.clear();
    for (const QString& f : allFiles) {
        if (kImageExts.contains(
                QFileInfo(f).suffix().toLower())) {
            batchFiles_ << f;
        }
    }
    if (batchFiles_.isEmpty()) {
        QMessageBox::warning(this, "Aviso",
            "Nenhuma imagem encontrada na pasta.");
        return;
    }

    QDir outDir(batchOutputDir_);
    if (!outDir.exists() && !outDir.mkpath(".")) {
        QMessageBox::critical(this, "Erro",
            "Nao foi possivel criar diretorio de saida:\n"
            + batchOutputDir_);
        return;
    }

    btnOpen_->setEnabled(false);
    btnRemove_->setEnabled(false);
    btnSave_->setEnabled(false);
    btnProcessBatch_->setEnabled(false);
    btnSelectFolder_->setEnabled(false);
    btnSelectOutput_->setEnabled(false);
    btnLoadModel_->setEnabled(false);
    progressBar_->setRange(0, batchFiles_.size());
    progressBar_->setValue(0);
    progressBar_->setVisible(true);
    logEdit_->clear();
    logEdit_->appendPlainText(
        QString("[BATCH] Processando %1 arquivo(s) "
                "-> %2")
            .arg(batchFiles_.size())
            .arg(batchOutputDir_));

    batchIndex_ = 0;
    batchRunning_ = true;

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
        btnSelectOutput_->setEnabled(true);
        btnLoadModel_->setEnabled(true);
        progressBar_->setVisible(false);
        return;
    }

    QTimer::singleShot(0, this,
                       &MainWindow::processBatchStep);
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
        btnOpen_->setEnabled(true);
        btnRemove_->setEnabled(true);
        btnSave_->setEnabled(true);
        btnProcessBatch_->setEnabled(true);
        btnSelectFolder_->setEnabled(true);
        btnSelectOutput_->setEnabled(true);
        btnLoadModel_->setEnabled(true);
        progressBar_->setVisible(false);
        return;
    }

    QString file = batchFiles_[batchIndex_];
    QString inputPath = QDir(batchFolder_)
                            .absoluteFilePath(file);
    QFileInfo fi(file);
    QString outName = fi.baseName() + "_rembg.png";
    QString outPath = QDir(batchOutputDir_)
                          .absoluteFilePath(outName);

    try {
        QImage img(inputPath);
        if (img.isNull()) {
            logEdit_->appendPlainText(
                "[ERRO] Falha ao carregar: " + file);
        } else {
            QImage result = remover_->removeBackground(img);
            if (noiseCheckBox_->isChecked()) {
                result = applyAlphaThreshold(
                    result, kAlphaThreshold);
            }
            if (refineCheckBox_->isChecked()) {
                result = applyEdgeRefinement(result, 0.5f);
            }
            QColor bg = currentBackgroundColor();
            if (bg.isValid()) {
                result = addAlphaBackground(result, bg);
            }
            result.save(outPath, "PNG");
            logEdit_->appendPlainText(
                "[OK] " + file + " -> " + outName);
        }
    } catch (const Ort::Exception& e) {
        logEdit_->appendPlainText(
            "[ERRO] " + file + ": " + QString(e.what()));
    } catch (const std::exception& e) {
        logEdit_->appendPlainText(
            "[ERRO] " + file + ": "
            + QString::fromStdString(e.what()));
    }
    batchIndex_++;
    progressBar_->setValue(batchIndex_);
    QTimer::singleShot(0, this, &MainWindow::processBatchStep);
}

QImage MainWindow::applyEdgeRefinement(
    const QImage& input, float intensity)
{
    if (input.isNull() || intensity <= 0.0f)
        return input.copy();
    QImage result = input.convertToFormat(
        QImage::Format_RGBA8888);
    QImage blurred = result.copy();
    int w = result.width(), h = result.height();
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            float sum_a = 0.0f; int count = 0;
            for (int dy = -1; dy <= 1; ++dy) {
                for (int dx = -1; dx <= 1; ++dx) {
                    int nx = qBound(0, x + dx, w - 1);
                    int ny = qBound(0, y + dy, h - 1);
                    sum_a += qAlpha(result.pixel(nx, ny));
                    ++count;
                }
            }
            float blurred_a = sum_a / static_cast<float>(count);
            QRgb px = result.pixel(x, y);
            float orig_a = qAlpha(px);
            float new_a = orig_a + (blurred_a - orig_a) * intensity;
            new_a = std::clamp(new_a, 0.0f, 255.0f);
            blurred.setPixel(x, y, qRgba(
                qRed(px), qGreen(px), qBlue(px),
                static_cast<int>(new_a)));
        }
    }
    return blurred;
}

QImage MainWindow::applyAlphaThreshold(
    const QImage& input, float threshold)
{
    if (input.isNull() || threshold <= 0.0f)
        return input.copy();
    QImage result = input.convertToFormat(
        QImage::Format_RGBA8888);
    int w = result.width(), h = result.height();
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            QRgb px = result.pixel(x, y);
            int alpha = qAlpha(px);
            if (alpha < static_cast<int>(threshold)) {
                result.setPixel(x, y, qRgba(
                    qRed(px), qGreen(px), qBlue(px), 0));
            }
        }
    }
    return result;
}

void MainWindow::setOriginal(const QImage& img) {
    originalLabel_->setPixmap(
        QPixmap::fromImage(img).scaled(
            originalScroll_->size(),
            Qt::KeepAspectRatio,
            Qt::SmoothTransformation));
}

void MainWindow::setResult(const QImage& img) {
    QColor bg = currentBackgroundColor();
    QImage shown = addAlphaBackground(img, bg);
    resultLabel_->setPixmap(
        QPixmap::fromImage(shown).scaled(
            resultScroll_->size(),
            Qt::KeepAspectRatio,
            Qt::SmoothTransformation));
}
