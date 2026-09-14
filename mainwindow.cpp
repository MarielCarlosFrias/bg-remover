#include "mainwindow.h"
#include "maskeditcanvas.h"
#include "imagepostprocessor.h"
#include "modelpathresolver.h"
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
#include <QSpinBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QFrame>
#include <QSplitter>
#include <QApplication>
#include <QPixmap>
#include <QSizePolicy>
#include <QDir>
#include <QFileInfo>
#include <QTimer>
#include <QFile>
#include <QRegularExpression>
#include <QSet>
#include <QColorDialog>
#include <QResizeEvent>
#include <algorithm>
#include <iostream>

static const QSet<QString> kImageExts = {
    "png", "jpg", "jpeg", "bmp", "webp", "tiff", "tif"
};
static const QRegularExpression kGeneratedOutputName(
    "_rembg(?:_[0-9]+)?$");
static constexpr float kAlphaThreshold = 30.0f;

QColor MainWindow::currentBackgroundColor() const {
    int idx = bgColorCombo_->currentIndex();
    if (idx == 0) return QColor();
    if (idx == 1) return Qt::white;
    if (idx == 2) return Qt::black;
    if (idx == 3) return customBgColor_;
    return QColor();
}

ProcessingOptions MainWindow::currentProcessingOptions() const
{
    ProcessingOptions options;
    options.modelPath = modelPathEdit_->text();
    options.cleanNoise = noiseCheckBox_->isChecked();
    options.alphaThreshold = kAlphaThreshold;
    options.refineEdges = refineCheckBox_->isChecked();
    options.edgeRefinementIntensity = 0.5f;
    options.backgroundColor = currentBackgroundColor();
    return options;
}

MainWindow::MainWindow(QWidget* parent, const QString& modelPath)
    : QMainWindow(parent),
      originalLabel_(new QLabel("Nenhuma imagem")),
      imageNameLabel_(new QLabel("Nenhuma imagem selecionada")),
      zoomLabel_(new QLabel("100%")),
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
      btnUndo_(new QPushButton("Desfazer")),
      bgColorCombo_(new QComboBox),
      brushModeCombo_(new QComboBox),
      brushRadiusSpin_(new QSpinBox),
      refineCheckBox_(new QCheckBox("Refinar bordas")),
      noiseCheckBox_(new QCheckBox("Limpar ruído")),
      progressBar_(new QProgressBar),
      logEdit_(new QPlainTextEdit),
      maskEditCanvas_(new MaskEditCanvas(this)),
      batchIndex_(0),
      batchOk_(0),
      batchErrors_(0),
      batchRunning_(false),
      customBgColor_(QColor()),
      previewZoomPercent_(100)
{
    originalLabel_->setAlignment(Qt::AlignCenter);

    originalScroll_->setWidget(originalLabel_);
    originalScroll_->setWidgetResizable(false);
    originalScroll_->setAlignment(Qt::AlignCenter);

    maskEditCanvas_->setMinimumSize(320, 240);
    maskEditCanvas_->setSizePolicy(
        QSizePolicy::Expanding, QSizePolicy::Expanding);
    resultScroll_->setWidget(maskEditCanvas_);
    resultScroll_->setWidgetResizable(false);
    resultScroll_->setAlignment(Qt::AlignCenter);

    modelCombo_->addItem(
        "U\u00b2-Netp \u2014 r\u00e1pido (~5 MB)",
        ModelPathResolver::resolve("u2netp.onnx"));
    modelCombo_->addItem(
        "U\u00b2-Net  \u2014 qualidade (~176 MB)",
        ModelPathResolver::resolve("u2net.onnx"));
    modelCombo_->addItem("Caminho customizado", "");
    modelCombo_->setCurrentIndex(0);

    if (!modelPath.isEmpty()) {
        modelPathEdit_->setText(modelPath);
        modelCombo_->setCurrentIndex(2);
    } else {
        modelPathEdit_->setText(
            modelCombo_->itemData(0).toString());
    }
    modelPathEdit_->setMinimumWidth(0);

    bgColorCombo_->addItem("Transparente", QVariant());
    bgColorCombo_->addItem(
        "Branco", QVariant::fromValue(QColor(Qt::white)));
    bgColorCombo_->addItem(
        "Preto", QVariant::fromValue(QColor(Qt::black)));
    bgColorCombo_->addItem("Personalizado...", QVariant());
    bgColorCombo_->setCurrentIndex(0);
    bgColorCombo_->setMaximumWidth(160);

    brushModeCombo_->addItem("Restaurar");
    brushModeCombo_->addItem("Apagar");
    brushModeCombo_->setCurrentIndex(0);

    brushRadiusSpin_->setRange(1, 200);
    brushRadiusSpin_->setValue(10);
    brushRadiusSpin_->setSuffix(" px");

    maskEditCanvas_->setMode(MaskEditCanvas::Restore);
    maskEditCanvas_->setBrushRadius(brushRadiusSpin_->value());

    btnUndo_->setEnabled(false);

    outputDirEdit_->setReadOnly(true);

    progressBar_->setRange(0, 100);
    progressBar_->setValue(0);
    progressBar_->setVisible(false);

    logEdit_->setReadOnly(true);
    logEdit_->setMaximumHeight(150);
    logEdit_->setVisible(false);
    logEdit_->setPlaceholderText("Log do processamento...");

    auto* header = new QFrame;
    header->setObjectName("titleBar");
    auto* headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(18, 0, 18, 0);
    headerLayout->setSpacing(10);
    auto* brandMark = new QLabel("B");
    brandMark->setObjectName("brandMark");
    auto* title = new QLabel("BG Remover");
    title->setObjectName("appTitle");
    auto* subtitle = new QLabel("Editor de fundo local");
    subtitle->setObjectName("appSubtitle");
    headerLayout->addWidget(brandMark);
    headerLayout->addWidget(title);
    headerLayout->addWidget(subtitle);
    headerLayout->addStretch();
    btnOpen_->setObjectName("openButton");
    headerLayout->addWidget(btnOpen_);

    auto* originalPanel = new QFrame;
    originalPanel->setObjectName("previewPanel");
    auto* originalLayout = new QVBoxLayout(originalPanel);
    originalLayout->setContentsMargins(0, 0, 0, 0);
    auto* originalTitle = new QLabel("Original");
    originalTitle->setObjectName("previewTitle");
    originalLayout->addWidget(originalTitle);
    originalLayout->addWidget(originalScroll_);

    auto* resultPanel = new QFrame;
    resultPanel->setObjectName("previewPanel");
    auto* resultLayout = new QVBoxLayout(resultPanel);
    resultLayout->setContentsMargins(0, 0, 0, 0);
    auto* resultTitle = new QLabel("Resultado");
    resultTitle->setObjectName("previewTitle");
    resultLayout->addWidget(resultTitle);
    resultLayout->addWidget(resultScroll_);

    auto* previewSplitter = new QSplitter(Qt::Horizontal);
    previewSplitter->setChildrenCollapsible(false);
    previewSplitter->setHandleWidth(12);
    previewSplitter->addWidget(originalPanel);
    previewSplitter->addWidget(resultPanel);
    previewSplitter->setStretchFactor(0, 1);
    previewSplitter->setStretchFactor(1, 1);

    auto* workspace = new QFrame;
    workspace->setObjectName("workspace");
    auto* workspaceLayout = new QVBoxLayout(workspace);
    workspaceLayout->setContentsMargins(0, 0, 0, 0);
    auto* workspaceToolbar = new QHBoxLayout;
    workspaceToolbar->setContentsMargins(0, 0, 0, 0);
    imageNameLabel_->setObjectName("imageName");
    workspaceToolbar->addWidget(imageNameLabel_);
    workspaceToolbar->addStretch();
    auto* compareLabel = new QLabel("Comparar");
    compareLabel->setObjectName("viewModeActive");
    workspaceToolbar->addWidget(compareLabel);
    auto* editHint = new QLabel("Edite a máscara no resultado");
    editHint->setObjectName("workspaceHint");
    workspaceToolbar->addWidget(editHint);
    auto* zoomOutButton = new QPushButton("−");
    zoomOutButton->setObjectName("zoomButton");
    zoomOutButton->setToolTip("Diminuir zoom");
    auto* zoomInButton = new QPushButton("+");
    zoomInButton->setObjectName("zoomButton");
    zoomInButton->setToolTip("Aumentar zoom");
    auto* fitButton = new QPushButton("Ajustar");
    fitButton->setObjectName("fitButton");
    fitButton->setToolTip("Ajustar imagem à área de trabalho");
    zoomLabel_->setObjectName("zoomLabel");
    workspaceToolbar->addSpacing(4);
    workspaceToolbar->addWidget(zoomOutButton);
    workspaceToolbar->addWidget(zoomLabel_);
    workspaceToolbar->addWidget(zoomInButton);
    workspaceToolbar->addWidget(fitButton);
    workspaceLayout->addLayout(workspaceToolbar);
    workspaceLayout->addWidget(previewSplitter, 1);

    auto* inspector = new QFrame;
    inspector->setObjectName("inspector");
    inspector->setFixedWidth(260);
    auto* inspectorLayout = new QVBoxLayout(inspector);
    inspectorLayout->setContentsMargins(14, 14, 14, 14);
    inspectorLayout->setSpacing(9);
    auto sectionTitle = [](const QString& text) {
        auto* label = new QLabel(text);
        label->setObjectName("inspectorTitle");
        return label;
    };
    inspectorLayout->addWidget(sectionTitle("Remover fundo"));
    auto* modelForm = new QFormLayout;
    modelForm->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    modelForm->setRowWrapPolicy(QFormLayout::WrapAllRows);
    modelForm->addRow("Modelo", modelCombo_);
    auto* modelPathLayout = new QHBoxLayout;
    modelPathLayout->addWidget(modelPathEdit_);
    modelPathLayout->addWidget(btnLoadModel_);
    modelForm->addRow("Arquivo", modelPathLayout);
    inspectorLayout->addLayout(modelForm);
    btnRemove_->setObjectName("primaryAction");
    btnRemove_->setMinimumHeight(38);
    inspectorLayout->addWidget(btnRemove_);

    auto* separatorOne = new QFrame;
    separatorOne->setObjectName("inspectorSeparator");
    separatorOne->setFrameShape(QFrame::HLine);
    inspectorLayout->addWidget(separatorOne);
    inspectorLayout->addWidget(sectionTitle("Ajustes de saída"));
    auto* outputForm = new QFormLayout;
    outputForm->setRowWrapPolicy(QFormLayout::WrapAllRows);
    outputForm->addRow("Fundo ao salvar", bgColorCombo_);
    inspectorLayout->addLayout(outputForm);
    inspectorLayout->addWidget(refineCheckBox_);
    inspectorLayout->addWidget(noiseCheckBox_);

    auto* separatorTwo = new QFrame;
    separatorTwo->setObjectName("inspectorSeparator");
    separatorTwo->setFrameShape(QFrame::HLine);
    inspectorLayout->addWidget(separatorTwo);
    inspectorLayout->addWidget(sectionTitle("Correção manual"));
    auto* brushForm = new QFormLayout;
    brushForm->setRowWrapPolicy(QFormLayout::WrapAllRows);
    brushForm->addRow("Pincel", brushModeCombo_);
    brushForm->addRow("Tamanho", brushRadiusSpin_);
    inspectorLayout->addLayout(brushForm);
    inspectorLayout->addWidget(btnUndo_);
    auto* brushHint = new QLabel("Após remover o fundo, use o pincel no painel Resultado para restaurar ou apagar detalhes.");
    brushHint->setObjectName("inspectorHint");
    brushHint->setWordWrap(true);
    inspectorLayout->addWidget(brushHint);
    inspectorLayout->addStretch();

    auto* contentSplitter = new QSplitter(Qt::Horizontal);
    contentSplitter->setChildrenCollapsible(false);
    contentSplitter->addWidget(workspace);
    contentSplitter->addWidget(inspector);
    contentSplitter->setStretchFactor(0, 1);

    auto* batchBar = new QFrame;
    batchBar->setObjectName("batchBar");
    auto* batchLayout = new QHBoxLayout(batchBar);
    batchLayout->setContentsMargins(16, 12, 16, 12);
    batchLayout->setSpacing(10);
    auto* batchTitle = new QLabel("Processamento em lote");
    batchTitle->setObjectName("batchTitle");
    batchLayout->addWidget(batchTitle);
    batchLayout->addSpacing(8);
    folderEdit_->setPlaceholderText("Nenhuma pasta selecionada");
    folderEdit_->setMinimumWidth(190);
    batchLayout->addWidget(folderEdit_, 1);
    btnSelectFolder_->setText("Escolher pasta");
    batchLayout->addWidget(btnSelectFolder_);
    outputDirEdit_->setMinimumWidth(120);
    outputDirEdit_->setMaximumWidth(180);
    batchLayout->addWidget(outputDirEdit_, 1);
    btnSelectOutput_->setText("Salvar em…");
    batchLayout->addWidget(btnSelectOutput_);
    batchLayout->addWidget(btnProcessBatch_);
    btnSave_->setObjectName("saveButton");
    batchLayout->addWidget(btnSave_);

    auto* mainLayout = new QVBoxLayout();
    mainLayout->setContentsMargins(16, 0, 16, 16);
    mainLayout->setSpacing(16);
    mainLayout->addWidget(header);
    mainLayout->addWidget(contentSplitter, 1);
    mainLayout->addWidget(batchBar);
    mainLayout->addWidget(progressBar_);
    auto* btnToggleLog = new QPushButton("Mostrar log \u25be");
    btnToggleLog->setObjectName("logToggle");
    mainLayout->addWidget(btnToggleLog);
    mainLayout->addWidget(logEdit_);

    connect(btnToggleLog, &QPushButton::clicked, this, [this, btnToggleLog]() {
        bool nowVisible = !logEdit_->isVisible();
        logEdit_->setVisible(nowVisible);
        btnToggleLog->setText(nowVisible
            ? QString::fromUtf8("Esconder log \u25b4")
            : QString::fromUtf8("Mostrar log \u25be"));
    });
    connect(zoomOutButton, &QPushButton::clicked, this, [this]() {
        setPreviewZoom(previewZoomPercent_ - 25);
    });
    connect(zoomInButton, &QPushButton::clicked, this, [this]() {
        setPreviewZoom(previewZoomPercent_ + 25);
    });
    connect(fitButton, &QPushButton::clicked, this, [this]() {
        setPreviewZoom(100);
    });

    auto* central = new QWidget(this);
    central->setLayout(mainLayout);
    central->setObjectName("studioRoot");
    setCentralWidget(central);

    central->setStyleSheet(
        "#studioRoot { background: #171a1d; }"
        "QFrame#titleBar { min-height: 50px; max-height: 50px;"
        " background: #20252a; border-bottom: 1px solid #343a40; }"
        "QFrame#inspector { background: #20252a; border: 1px solid #384047; border-radius: 10px; }"
        "QFrame#batchBar { background: #1b2024; border: 1px solid #343a40; border-radius: 10px; }"
        "QFrame#previewPanel { background: #252a2e; border: 1px solid #384047; border-radius: 10px; }"
        "QLabel#brandMark { min-width: 24px; max-width: 24px; min-height: 24px; max-height: 24px;"
        " background: #36c66a; color: #102217; border-radius: 7px; font-weight: 800; qproperty-alignment: AlignCenter; }"
        "QLabel#appTitle { color: #edf2f7; font-size: 15px; font-weight: 600; }"
        "QLabel#appSubtitle, QLabel#imageName, QLabel#workspaceHint, QLabel#inspectorHint { color: #9ba6af; }"
        "QLabel#appSubtitle { font-size: 12px; }"
        "QLabel#imageName { font-weight: 600; }"
        "QLabel#workspaceHint { font-size: 12px; }"
        "QLabel#viewModeActive { background: #384149; border: 1px solid #6b7781; border-radius: 7px;"
        " padding: 6px 10px; color: #edf2f7; font-size: 12px; font-weight: 600; }"
        "QLabel#zoomLabel { min-width: 42px; color: #a9b3ba; font-size: 12px; qproperty-alignment: AlignCenter; }"
        "QLabel#previewTitle { padding: 11px 13px; font-weight: 600; color: #e8edf0;"
        " background: #20252a; border-bottom: 1px solid #384047; }"
        "QLabel#inspectorTitle { font-size: 14px; font-weight: 600; color: #edf2f4; }"
        "QLabel#inspectorHint { font-size: 12px; line-height: 1.35; }"
        "QLabel#batchTitle { font-weight: 700; color: #e8eef1; }"
        "QScrollArea { background: #252a2e; border: 0; }"
        "QLineEdit, QComboBox, QSpinBox { background: #2a3137; border: 1px solid #465058;"
        " border-radius: 7px; padding: 7px 9px; color: #e8edf0; }"
        "QPushButton { background: #2b3238; border: 1px solid #465058; border-radius: 7px;"
        " padding: 8px 11px; color: #e8edf0; }"
        "QPushButton:hover { background: #384149; }"
        "QPushButton:disabled { background: #252b30; color: #78848c; border-color: #384148; }"
        "QPushButton#primaryAction { background: #36a85c; border-color: #36a85c;"
        " color: #102217; font-weight: 700; }"
        "QPushButton#primaryAction:hover { background: #42cf74; }"
        "QPushButton#saveButton { background: #36c66a; border-color: #36c66a; color: #102217; font-weight: 700; }"
        "QPushButton#saveButton:hover { background: #42cf74; }"
        "QPushButton#logToggle { background: transparent; border: 0; color: #a5b1b9; text-align: left; }"
        "QPushButton#zoomButton { min-width: 30px; max-width: 30px; padding: 6px; font-size: 16px; }"
        "QPushButton#fitButton { padding: 6px 9px; font-size: 12px; }"
        "QFrame#inspectorSeparator { color: #384047; }"
        "QSplitter::handle { background: #171a1d; }");

    setWindowTitle("BG Remover");
    setMinimumSize(980, 680);
    resize(1320, 860);

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
    connect(btnUndo_,          &QPushButton::clicked,
            this,              [this]() {
        maskEditCanvas_->undo();
        resultImage_ = maskEditCanvas_->getEditedImage();
        btnUndo_->setEnabled(maskEditCanvas_->hasEdits());
    });
    connect(modelCombo_, QOverload<int>::of(
                &QComboBox::currentIndexChanged),
            this,              &MainWindow::modelIndexChanged);
    connect(bgColorCombo_, QOverload<int>::of(
                &QComboBox::currentIndexChanged),
            this,              &MainWindow::bgColorChanged);
    connect(maskEditCanvas_, &MaskEditCanvas::editingDone,
            this,              &MainWindow::editingDone);
    connect(brushModeCombo_, QOverload<int>::of(
                &QComboBox::currentIndexChanged),
            this, [this](int idx) {
        maskEditCanvas_->setMode(
            idx == 0 ? MaskEditCanvas::Restore
                     : MaskEditCanvas::Erase);
    });
    connect(brushRadiusSpin_, QOverload<int>::of(
                &QSpinBox::valueChanged),
            this, [this](int value) {
        maskEditCanvas_->setBrushRadius(value);
    });
}

void MainWindow::editingDone()
{
    resultImage_ = maskEditCanvas_->getEditedImage();
    btnUndo_->setEnabled(maskEditCanvas_->hasEdits());
}

void MainWindow::resizeEvent(QResizeEvent* event)
{
    QMainWindow::resizeEvent(event);
    updatePreviewZoom();
}

void MainWindow::setPreviewZoom(int percent)
{
    previewZoomPercent_ = qBound(50, percent, 400);
    zoomLabel_->setText(QString::number(previewZoomPercent_) + "%");
    updatePreviewZoom();
}

void MainWindow::updatePreviewZoom()
{
    if (originalImage_.isNull()) return;

    const QSize originalViewport = originalScroll_->viewport()->size();
    if (originalViewport.isEmpty()) return;

    const QSize fittedOriginal = originalImage_.size().scaled(
        originalViewport, Qt::KeepAspectRatio);
    const QSize originalDisplay = fittedOriginal * previewZoomPercent_ / 100;
    originalLabel_->setPixmap(QPixmap::fromImage(originalImage_).scaled(
        originalDisplay, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    originalLabel_->resize(originalDisplay);

    if (resultImage_.isNull()) return;
    const QSize resultViewport = resultScroll_->viewport()->size();
    if (resultViewport.isEmpty()) return;
    const QSize fittedResult = resultImage_.size().scaled(
        resultViewport, Qt::KeepAspectRatio);
    const QSize resultDisplay = fittedResult * previewZoomPercent_ / 100;
    maskEditCanvas_->setDisplaySize(resultDisplay);
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
    imageNameLabel_->setText(QFileInfo(path).fileName());
    remover_.reset();
    maskEditCanvas_->clearEdits();
    btnUndo_->setEnabled(false);
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
        maskEditCanvas_->setBackgroundColor(currentBackgroundColor());
    }
}

void MainWindow::removeBackground() {
    if (originalImage_.isNull()) {
        QMessageBox::warning(this, "Aviso",
            "Abra uma imagem primeiro.");
        return;
    }

    setCursor(Qt::WaitCursor);
    btnRemove_->setEnabled(false);
    QApplication::processEvents();

    try {
        ProcessingOptions options = currentProcessingOptions();
        // A cor de fundo e apenas uma pre-visualizacao na imagem unica;
        // manter a mascara transparente permite continuar editando-a.
        options.backgroundColor = QColor();
        remover_ = std::make_unique<BackgroundRemover>(
            options.modelPath);
        resultImage_ = ImagePostProcessor::apply(
            remover_->removeBackground(originalImage_), options);
        if (options.cleanNoise) {
            logEdit_->appendPlainText(
                "[INFO] Ruído limpo (threshold "
                + QString::number(kAlphaThreshold) + ").");
        }
        if (options.refineEdges) {
            logEdit_->appendPlainText(
                "[INFO] Bordas refinadas.");
        }
        maskEditCanvas_->setImage(
            resultImage_, currentBackgroundColor());
        btnUndo_->setEnabled(false);
        setResult(resultImage_);
        logEdit_->appendPlainText("[OK] Fundo removido.");
    } catch (const Ort::Exception& e) {
        QMessageBox::critical(this, "Erro ONNX",
            QString("Falha na inferiencia:\n") + e.what());
    } catch (const std::exception& e) {
        QMessageBox::critical(this, "Erro",
            QString("Falha:\n") + e.what());
    }

    btnRemove_->setEnabled(true);
    unsetCursor();
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

    ProcessingOptions saveOptions;
    saveOptions.backgroundColor = currentBackgroundColor();
    const QImage toSave = ImagePostProcessor::apply(
        resultImage_, saveOptions);

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

    QDir inputDir(batchFolder_);
    QDir outputDir(batchOutputDir_);
    const bool outputIsInputDirectory =
        QDir::cleanPath(inputDir.absolutePath())
        == QDir::cleanPath(outputDir.absolutePath());
    QStringList allFiles = inputDir.entryList(
        QDir::Files | QDir::NoDotAndDotDot);
    batchFiles_.clear();
    batchOutputPaths_.clear();
    QSet<QString> reservedOutputPaths;
    for (const QString& f : allFiles) {
        const QFileInfo fileInfo(f);
        const bool isGeneratedOutput = outputIsInputDirectory
            && fileInfo.suffix().compare("png", Qt::CaseInsensitive) == 0
            && kGeneratedOutputName.match(
                fileInfo.completeBaseName()).hasMatch();
        if (!kImageExts.contains(fileInfo.suffix().toLower())
            || isGeneratedOutput) continue;

        const QString outputBase = fileInfo.completeBaseName() + "_rembg";
        QString outputPath = outputDir.absoluteFilePath(outputBase + ".png");
        for (int suffix = 2;
             reservedOutputPaths.contains(outputPath)
             || QFileInfo::exists(outputPath);
             ++suffix) {
            outputPath = outputDir.absoluteFilePath(
                outputBase + "_" + QString::number(suffix) + ".png");
        }
        reservedOutputPaths.insert(outputPath);
        batchFiles_ << f;
        batchOutputPaths_ << outputPath;
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
    btnUndo_->setEnabled(false);
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
    batchOk_ = 0;
    batchErrors_ = 0;
    batchRunning_ = true;
    batchOptions_ = currentProcessingOptions();

    try {
        remover_ = std::make_unique<BackgroundRemover>(
            batchOptions_.modelPath);
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
        btnUndo_->setEnabled(false);
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
            QString("[BATCH] Concluido: %1 OK, %2 erro(s).")
                .arg(batchOk_).arg(batchErrors_));
        btnOpen_->setEnabled(true);
        btnRemove_->setEnabled(true);
        btnSave_->setEnabled(true);
        btnProcessBatch_->setEnabled(true);
        btnSelectFolder_->setEnabled(true);
        btnSelectOutput_->setEnabled(true);
        btnLoadModel_->setEnabled(true);
        btnUndo_->setEnabled(false);
        progressBar_->setVisible(false);
        return;
    }

    QString file = batchFiles_[batchIndex_];
    QString inputPath = QDir(batchFolder_)
                            .absoluteFilePath(file);
    QFileInfo fi(file);
    QString outPath = batchOutputPaths_[batchIndex_];
    QString outName = QFileInfo(outPath).fileName();

    try {
        QImage img(inputPath);
        if (img.isNull()) {
            ++batchErrors_;
            logEdit_->appendPlainText(
                "[ERRO] Falha ao carregar: " + file);
        } else {
            QImage result = ImagePostProcessor::apply(
                remover_->removeBackground(img), batchOptions_);
            if (result.save(outPath, "PNG")) {
                ++batchOk_;
                logEdit_->appendPlainText(
                    "[OK] " + file + " -> " + outName);
            } else {
                ++batchErrors_;
                logEdit_->appendPlainText(
                    "[ERRO] Falha ao salvar: " + outPath);
            }
        }
    } catch (const Ort::Exception& e) {
        ++batchErrors_;
        logEdit_->appendPlainText(
            "[ERRO] " + file + ": " + QString(e.what()));
    } catch (const std::exception& e) {
        ++batchErrors_;
        logEdit_->appendPlainText(
            "[ERRO] " + file + ": "
            + QString::fromStdString(e.what()));
    }
    batchIndex_++;
    progressBar_->setValue(batchIndex_);
    QTimer::singleShot(0, this, &MainWindow::processBatchStep);
}

void MainWindow::setOriginal(const QImage& img) {
    Q_UNUSED(img)
    updatePreviewZoom();
}

void MainWindow::setResult(const QImage& img) {
    Q_UNUSED(img)
    updatePreviewZoom();
}
