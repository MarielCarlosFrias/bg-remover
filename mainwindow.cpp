#include "mainwindow.h"
#include "i18n.h"
#include "maskeditcanvas.h"
#include "helpwidget.h"
#include "imagepostprocessor.h"
#include "modelmanager.h"
#include "modelpathresolver.h"
#include <QCloseEvent>
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
#include <QColorDialog>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QCloseEvent>
#include <QKeySequence>
#include <QMimeData>
#include <QResizeEvent>
#include <QScrollBar>
#include <QSettings>
#include <QShortcut>
#include <QSignalBlocker>
#include <QTabWidget>
#include <QtConcurrent/QtConcurrent>
#include <QWheelEvent>
#include <algorithm>
#include <iostream>

#include "apptheme.h"

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
    options.alphaThreshold = noiseThresholdSpin_->value();
    options.refineEdges = refineCheckBox_->isChecked();
    options.edgeRefinementIntensity = 0.5f;
    options.backgroundColor = currentBackgroundColor();
    return options;
}

MainWindow::MainWindow(QWidget* parent, const QString& modelPath)
    : QMainWindow(parent),
      originalLabel_(new QLabel(TR("Nenhuma imagem", "No image"))),
      imageNameLabel_(new QLabel(TR("Nenhuma imagem selecionada",
                                    "No image selected"))),
      zoomLabel_(new QLabel("100%")),
      originalScroll_(new QScrollArea),
      resultScroll_(new QScrollArea),
      modelCombo_(new QComboBox),
      modelPathEdit_(new QLineEdit),
      folderEdit_(new QLineEdit),
      outputDirEdit_(new QLineEdit("./_rembg")),
      btnOpen_(new QPushButton(TR("Abrir imagem", "Open image"))),
      btnThemeToggle_(new QPushButton),
      btnRemove_(new QPushButton(TR("Remover fundo", "Remove background"))),
      btnSave_(new QPushButton(TR("Salvar como PNG", "Save as PNG"))),
      btnLoadModel_(new QPushButton(TR("Buscar...", "Browse..."))),
      btnSelectFolder_(new QPushButton("...")),
      btnSelectOutput_(new QPushButton("...")),
      btnProcessBatch_(new QPushButton(TR("Processar lote", "Process batch"))),
      btnUndo_(new QPushButton(TR("Desfazer", "Undo"))),
      btnRedo_(new QPushButton(TR("Refazer", "Redo"))),
      bgColorCombo_(new QComboBox),
      brushModeCombo_(new QComboBox),
      brushRadiusSpin_(new QSpinBox),
      noiseThresholdSpin_(new QSpinBox),
      refineCheckBox_(new QCheckBox(TR("Refinar bordas", "Refine edges"))),
      noiseCheckBox_(new QCheckBox(TR("Limpar ruído", "Clean noise"))),
      progressBar_(new QProgressBar),
      logEdit_(new QPlainTextEdit),
      maskEditCanvas_(new MaskEditCanvas(this)),
      zoomDebounce_(new QTimer(this)),
      modelCache_(std::make_shared<ModelCache>()),
      batchIndex_(0),
      batchOk_(0),
      batchErrors_(0),
      batchRunning_(false),
      batchCancelRequested_(false),
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

    // A lista de modelos inclui os instalados na aba "Modelos";
    // a selecao preferida (CLI ou QSettings) e aplicada no fim do
    // construtor, depois de restaurar as preferencias.
    populateModelCombo();
    modelPathEdit_->setMinimumWidth(0);

    bgColorCombo_->addItem(TR("Transparente", "Transparent"), QVariant());
    bgColorCombo_->addItem(
        TR("Branco", "White"), QVariant::fromValue(QColor(Qt::white)));
    bgColorCombo_->addItem(
        TR("Preto", "Black"), QVariant::fromValue(QColor(Qt::black)));
    bgColorCombo_->addItem(TR("Personalizado...", "Custom..."), QVariant());
    bgColorCombo_->setCurrentIndex(0);
    bgColorCombo_->setMaximumWidth(160);

    brushModeCombo_->addItem(TR("Restaurar", "Restore"));
    brushModeCombo_->addItem(TR("Apagar", "Erase"));
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

    // Reescalar os previews em cada evento de resize e caro: acumula
    // 40 ms e recalcula uma unica vez quando o utilizador para de
    // arrastar a janela.
    zoomDebounce_->setSingleShot(true);
    zoomDebounce_->setInterval(40);
    connect(zoomDebounce_, &QTimer::timeout,
            this,          &MainWindow::updatePreviewZoom);

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
    langCombo_ = new QComboBox;
    langCombo_->setObjectName("langCombo");
    // Nomes de idioma sempre na propria lingua (pratica padrao).
    langCombo_->addItem("\U0001F310 PT-BR", "pt");
    langCombo_->addItem("\U0001F310 English", "en");
    langCombo_->setToolTip(
        TR("Idioma (aplicado ao reiniciar o aplicativo)",
           "Language (applies after restarting the app)"));
    langCombo_->setCurrentIndex(
        I18n::lang() == I18n::Lang::En ? 1 : 0);
    headerLayout->addWidget(langCombo_);
    btnThemeToggle_->setObjectName("themeToggle");
    btnThemeToggle_->setToolTip(TR("Alternar tema claro/escuro",
                                   "Toggle light/dark theme"));
    btnThemeToggle_->setText(AppTheme::isDark()
        ? TR("\u2600 Tema claro", "\u2600 Light theme")
        : TR("\u263E Tema escuro", "\u263E Dark theme"));
    headerLayout->addWidget(btnThemeToggle_);
    btnOpen_->setObjectName("openButton");
    headerLayout->addWidget(btnOpen_);

    connect(langCombo_, QOverload<int>::of(
                &QComboBox::currentIndexChanged),
            this, [this](int idx) {
        QSettings settings("bg-remover", "bg-remover");
        settings.setValue("lang", idx == 1 ? "en" : "pt");
        QMessageBox::information(this, TR("Idioma", "Language"),
            TR("O idioma sera aplicado ao reiniciar o BG Remover.",
               "The language will be applied after restarting BG Remover."));
    });

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
    auto* compareLabel = new QLabel(TR("Comparar", "Compare"));
    compareLabel->setObjectName("viewModeActive");
    workspaceToolbar->addWidget(compareLabel);
    auto* editHint = new QLabel("Edite a máscara no resultado");
    editHint->setObjectName("workspaceHint");
    workspaceToolbar->addWidget(editHint);
    auto* zoomOutButton = new QPushButton("−");
    zoomOutButton->setObjectName("zoomButton");
    zoomOutButton->setToolTip(
        TR("Diminuir zoom do resultado (Ctrl + roda do mouse)",
           "Zoom out result (Ctrl + mouse wheel)"));
    auto* zoomInButton = new QPushButton("+");
    zoomInButton->setObjectName("zoomButton");
    zoomInButton->setToolTip(
        TR("Aumentar zoom do resultado (Ctrl + roda do mouse)",
           "Zoom in result (Ctrl + mouse wheel)"));
    auto* fitButton = new QPushButton(TR("Ajustar", "Fit"));
    fitButton->setObjectName("fitButton");
    fitButton->setToolTip(
        TR("Ajustar resultado à área de trabalho (Ctrl+0)",
           "Fit result to the workspace (Ctrl+0)"));
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
    auto* inspectorLayout = new QVBoxLayout(inspector);
    inspectorLayout->setContentsMargins(12, 12, 12, 12);
    inspectorLayout->setSpacing(7);
    auto sectionTitle = [](const QString& text) {
        auto* label = new QLabel(text);
        label->setObjectName("inspectorTitle");
        return label;
    };
    inspectorLayout->addWidget(sectionTitle(
        TR("Remover fundo", "Remove background")));
    auto* modelForm = new QFormLayout;
    modelForm->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    modelForm->setRowWrapPolicy(QFormLayout::WrapAllRows);
    modelForm->addRow(TR("Modelo", "Model"), modelCombo_);
    auto* modelPathLayout = new QHBoxLayout;
    modelPathLayout->addWidget(modelPathEdit_);
    modelPathLayout->addWidget(btnLoadModel_);
    modelForm->addRow(TR("Arquivo", "File"), modelPathLayout);
    inspectorLayout->addLayout(modelForm);
    btnRemove_->setObjectName("primaryAction");
    btnRemove_->setMinimumHeight(38);
    inspectorLayout->addWidget(btnRemove_);

    auto* separatorOne = new QFrame;
    separatorOne->setObjectName("inspectorSeparator");
    separatorOne->setFrameShape(QFrame::HLine);
    inspectorLayout->addWidget(separatorOne);
    inspectorLayout->addWidget(sectionTitle(
        TR("Ajustes de saída", "Output adjustments")));
    auto* outputForm = new QFormLayout;
    outputForm->setRowWrapPolicy(QFormLayout::WrapAllRows);
    outputForm->addRow(TR("Fundo ao salvar", "Background on save"),
                       bgColorCombo_);
    inspectorLayout->addLayout(outputForm);
    inspectorLayout->addWidget(refineCheckBox_);
    inspectorLayout->addWidget(noiseCheckBox_);
    auto* noiseRow = new QHBoxLayout;
    noiseRow->setContentsMargins(22, 0, 0, 0);
    noiseThresholdSpin_->setRange(0, 255);
    noiseThresholdSpin_->setValue(kAlphaThreshold);
    noiseThresholdSpin_->setSuffix(" /255");
    noiseThresholdSpin_->setToolTip(
        TR("Alfa abaixo deste valor fica totalmente transparente",
           "Alpha below this value becomes fully transparent"));
    noiseRow->addWidget(new QLabel(TR("Limiar", "Threshold")));
    noiseRow->addWidget(noiseThresholdSpin_, 1);
    inspectorLayout->addLayout(noiseRow);

    auto* separatorTwo = new QFrame;
    separatorTwo->setObjectName("inspectorSeparator");
    separatorTwo->setFrameShape(QFrame::HLine);
    inspectorLayout->addWidget(separatorTwo);
    inspectorLayout->addWidget(sectionTitle(
        TR("Correção manual", "Manual touch-up")));
    auto* brushForm = new QFormLayout;
    brushForm->setRowWrapPolicy(QFormLayout::WrapAllRows);
    brushForm->addRow(TR("Pincel", "Brush"), brushModeCombo_);
    brushForm->addRow(TR("Tamanho", "Size"), brushRadiusSpin_);
    inspectorLayout->addLayout(brushForm);
    auto* undoRow = new QHBoxLayout;
    undoRow->addWidget(btnUndo_);
    undoRow->addWidget(btnRedo_);
    inspectorLayout->addLayout(undoRow);
    auto* brushHint = new QLabel(TR(
        "Após remover o fundo, use o pincel no painel Resultado para restaurar ou apagar detalhes.",
        "After removing the background, use the brush on the Result panel to restore or erase details."));
    brushHint->setObjectName("inspectorHint");
    brushHint->setWordWrap(true);
    inspectorLayout->addWidget(brushHint);
    inspectorLayout->addStretch();

    // O inspector vive dentro de uma area rolavel: com janelas baixas
    // ele roda em vez de cortar os campos dos formularios.
    auto* inspectorScroll = new QScrollArea;
    inspectorScroll->setObjectName("inspectorScroll");
    inspectorScroll->setWidget(inspector);
    inspectorScroll->setWidgetResizable(true);
    inspectorScroll->setFrameShape(QFrame::NoFrame);
    inspectorScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    inspectorScroll->setFixedWidth(280);

    auto* contentSplitter = new QSplitter(Qt::Horizontal);
    contentSplitter->setChildrenCollapsible(false);
    contentSplitter->addWidget(workspace);
    contentSplitter->addWidget(inspectorScroll);
    contentSplitter->setStretchFactor(0, 1);

    // Abas principais: Editor (area de trabalho) e Modelos (catalogo
    // com download). O cabecalho, o lote e o log ficam sempre visiveis.
    mainTabs_ = new QTabWidget;
    mainTabs_->setObjectName("mainTabs");
    mainTabs_->addTab(contentSplitter, TR("Editor", "Editor"));
    modelManager_ = new ModelManagerWidget(this);
    mainTabs_->addTab(modelManager_, TR("Modelos", "Models"));
    mainTabs_->addTab(new HelpWidget(this), TR("Ajuda", "Help"));
    connect(modelManager_, &ModelManagerWidget::modelInstalled,
            this, [this](const QString&) {
        populateModelCombo();
    });

    auto* batchBar = new QFrame;
    batchBar->setObjectName("batchBar");
    auto* batchLayout = new QHBoxLayout(batchBar);
    batchLayout->setContentsMargins(16, 12, 16, 12);
    batchLayout->setSpacing(10);
    auto* batchTitle = new QLabel(TR("Processamento em lote",
                                     "Batch processing"));
    batchTitle->setObjectName("batchTitle");
    batchLayout->addWidget(batchTitle);
    batchLayout->addSpacing(8);
    folderEdit_->setPlaceholderText(
        TR("Nenhuma pasta selecionada", "No folder selected"));
    folderEdit_->setMinimumWidth(190);
    batchLayout->addWidget(folderEdit_, 1);
    btnSelectFolder_->setText(TR("Escolher pasta", "Choose folder"));
    batchLayout->addWidget(btnSelectFolder_);
    outputDirEdit_->setMinimumWidth(120);
    outputDirEdit_->setMaximumWidth(180);
    batchLayout->addWidget(outputDirEdit_, 1);
    btnSelectOutput_->setText(TR("Salvar em…", "Save to…"));
    batchLayout->addWidget(btnSelectOutput_);
    batchLayout->addWidget(btnProcessBatch_);
    btnSave_->setObjectName("saveButton");
    batchLayout->addWidget(btnSave_);

    auto* mainLayout = new QVBoxLayout();
    mainLayout->setContentsMargins(16, 0, 16, 16);
    mainLayout->setSpacing(16);
    mainLayout->addWidget(header);
    mainLayout->addWidget(mainTabs_, 1);
    mainLayout->addWidget(batchBar);
    mainLayout->addWidget(progressBar_);
    auto* btnToggleLog = new QPushButton(TR("Mostrar log \u25be",
                                            "Show log \u25be"));
    btnToggleLog->setObjectName("logToggle");
    mainLayout->addWidget(btnToggleLog);
    mainLayout->addWidget(logEdit_);

    connect(btnToggleLog, &QPushButton::clicked, this, [this, btnToggleLog]() {
        bool nowVisible = !logEdit_->isVisible();
        logEdit_->setVisible(nowVisible);
        btnToggleLog->setText(nowVisible
            ? TR("Esconder log \u25b4", "Hide log \u25b4")
            : TR("Mostrar log \u25be", "Show log \u25be"));
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

    // O tema (paleta Fusion + QSS global) e aplicado por
    // AppTheme::apply() em main.cpp; o botao de tema alterna
    // em tempo de execucao e persiste em QSettings.

    setWindowTitle("BG Remover " BG_REMOVER_VERSION);
    setMinimumSize(980, 680);
    resize(1320, 860);

    connect(btnThemeToggle_,   &QPushButton::clicked,
            this,              [this]() {
        const bool dark = AppTheme::toggle();
        btnThemeToggle_->setText(dark
            ? TR("\u2600 Tema claro", "\u2600 Light theme")
            : TR("\u263E Tema escuro", "\u263E Dark theme"));
    });
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
            this,              [this]() { maskEditCanvas_->undo(); });
    connect(btnRedo_,          &QPushButton::clicked,
            this,              [this]() { maskEditCanvas_->redo(); });
    connect(maskEditCanvas_, &MaskEditCanvas::availabilityChanged,
            this, [this](bool canUndo, bool canRedo) {
        btnUndo_->setEnabled(canUndo);
        btnRedo_->setEnabled(canRedo);
    });
    btnUndo_->setEnabled(false);
    btnRedo_->setEnabled(false);

    // A inferencia corre numa thread; o resultado volta pelo watcher.
    connect(&removeWatcher_, &QFutureWatcher<SingleResult>::finished,
            this,            &MainWindow::onRemoveFinished);
    connect(&batchWatcher_,  &QFutureWatcher<QString>::finished,
            this,            &MainWindow::onBatchStepFinished);

    // Atalhos de teclado.
    auto* scOpen = new QShortcut(QKeySequence::Open, this);
    connect(scOpen, &QShortcut::activated, this, &MainWindow::openImage);
    auto* scSave = new QShortcut(QKeySequence::Save, this);
    connect(scSave, &QShortcut::activated, this, &MainWindow::saveResult);
    auto* scUndo = new QShortcut(QKeySequence::Undo, this);
    connect(scUndo, &QShortcut::activated, this,
            [this]() { maskEditCanvas_->undo(); });
    auto* scRedo = new QShortcut(QKeySequence::Redo, this);
    connect(scRedo, &QShortcut::activated, this,
            [this]() { maskEditCanvas_->redo(); });
    auto* scRedoY = new QShortcut(QKeySequence("Ctrl+Y"), this);
    connect(scRedoY, &QShortcut::activated, this,
            [this]() { maskEditCanvas_->redo(); });
    auto* scZoomIn = new QShortcut(QKeySequence("Ctrl++"), this);
    connect(scZoomIn, &QShortcut::activated, this,
            [this]() { setPreviewZoom(previewZoomPercent_ + 25); });
    auto* scZoomInEq = new QShortcut(QKeySequence("Ctrl+="), this);
    connect(scZoomInEq, &QShortcut::activated, this,
            [this]() { setPreviewZoom(previewZoomPercent_ + 25); });
    auto* scZoomOut = new QShortcut(QKeySequence("Ctrl+-"), this);
    connect(scZoomOut, &QShortcut::activated, this,
            [this]() { setPreviewZoom(previewZoomPercent_ - 25); });
    auto* scFit = new QShortcut(QKeySequence("Ctrl+0"), this);
    connect(scFit, &QShortcut::activated, this,
            [this]() { setPreviewZoom(100); });
    auto* scQuit = new QShortcut(QKeySequence::Quit, this);
    connect(scQuit, &QShortcut::activated, this, &QWidget::close);

    // Abrir imagens e pastas arrastando para a janela.
    setAcceptDrops(true);

    // Ctrl + roda do rato sobre qualquer preview faz zoom do resultado.
    originalScroll_->viewport()->installEventFilter(this);
    resultScroll_->viewport()->installEventFilter(this);
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

    restoreSettings();

    // Modelo preferido: o argumento da linha de comandos tem
    // prioridade; sem argumento, usa o ultimo guardado em QSettings.
    QString preferred = modelPath;
    if (preferred.isEmpty()) {
        preferred = QSettings("bg-remover", "bg-remover")
                        .value("model/path").toString();
    }
    if (!preferred.isEmpty()) {
        const int idx = modelCombo_->findData(preferred);
        if (idx >= 0) {
            modelCombo_->setCurrentIndex(idx);
        } else {
            modelPathEdit_->setText(preferred);
            modelCombo_->setCurrentIndex(modelCombo_->count() - 1);
        }
    }
}

void MainWindow::editingDone()
{
    resultImage_ = maskEditCanvas_->getEditedImage();
}

void MainWindow::resizeEvent(QResizeEvent* event)
{
    QMainWindow::resizeEvent(event);
    zoomDebounce_->start();
}

void MainWindow::setPreviewZoom(int percent)
{
    previewZoomPercent_ = qBound(50, percent, 400);
    zoomLabel_->setText(QString::number(previewZoomPercent_) + "%");
    updatePreviewZoom();
}

void MainWindow::zoomAt(int percent, const QPoint& globalPos)
{
    if (resultImage_.isNull()) return;

    const QWidget* canvas = maskEditCanvas_;
    const QPoint canvasPos = canvas->mapFromGlobal(globalPos);
    const bool overCanvas =
        canvasPos.x() >= 0 && canvasPos.y() >= 0
        && canvasPos.x() < canvas->width()
        && canvasPos.y() < canvas->height();
    if (!overCanvas) {
        // Cursor fora do resultado: zoom sem ancoragem.
        setPreviewZoom(percent);
        return;
    }

    const QSize imageSize = resultImage_.size();
    const double oldScale =
        double(canvas->width()) / imageSize.width();
    setPreviewZoom(percent);
    const double newScale =
        double(maskEditCanvas_->width()) / imageSize.width();
    if (qFuzzyCompare(oldScale, newScale)) return;

    // Mantem o mesmo pixel da imagem sob o cursor: ajusta as barras
    // de rolagem para a nova escala.
    const QPointF imgPt(canvasPos.x() / oldScale,
                        canvasPos.y() / oldScale);
    const QPoint viewportPos =
        resultScroll_->viewport()->mapFromGlobal(globalPos);
    const QPoint target(
        static_cast<int>(imgPt.x() * newScale) - viewportPos.x(),
        static_cast<int>(imgPt.y() * newScale) - viewportPos.y());

    // Os limites das barras atualizam-se apos o resize; agenda o
    // ajuste para o proximo ciclo de eventos.
    QScrollBar* h = resultScroll_->horizontalScrollBar();
    QScrollBar* v = resultScroll_->verticalScrollBar();
    QTimer::singleShot(0, this, [h, v, target]() {
        h->setValue(target.x());
        v->setValue(target.y());
    });
}

void MainWindow::updatePreviewZoom()
{
    if (originalImage_.isNull()) return;

    // O painel Original e referencia fixa: fica sempre ajustado ao
    // espaco disponivel, independentemente do zoom.
    const QSize originalViewport = originalScroll_->viewport()->size();
    if (originalViewport.isEmpty()) return;
    const QSize fittedOriginal = originalImage_.size().scaled(
        originalViewport, Qt::KeepAspectRatio);
    originalLabel_->setPixmap(QPixmap::fromImage(originalImage_).scaled(
        fittedOriginal, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    originalLabel_->resize(fittedOriginal);

    // O zoom (botoes, Ctrl+/- e Ctrl+roda) aplica-se so ao Resultado,
    // que e a area de trabalho.
    if (resultImage_.isNull()) return;
    const QSize resultViewport = resultScroll_->viewport()->size();
    if (resultViewport.isEmpty()) return;
    const QSize fittedResult = resultImage_.size().scaled(
        resultViewport, Qt::KeepAspectRatio);
    const QSize resultDisplay = fittedResult * previewZoomPercent_ / 100;
    maskEditCanvas_->setDisplaySize(resultDisplay);
}

void MainWindow::openImage() {
    const QString path = QFileDialog::getOpenFileName(
        this, TR("Abrir imagem", "Open image"),
        !folderEdit_->text().isEmpty()
            ? folderEdit_->text() : QDir::homePath(),
        TR("Imagens (*.png *.jpg *.jpeg *.bmp *.webp *.tiff)",
           "Images (*.png *.jpg *.jpeg *.bmp *.webp *.tiff)"));
    if (path.isEmpty()) return;
    openPath(path);
}

void MainWindow::openPath(const QString& path) {
    QImage img(path);
    if (img.isNull()) {
        QMessageBox::warning(this, TR("Erro", "Error"),
            TR("Nao foi possivel carregar:\n", "Could not load:\n") + path);
        return;
    }
    originalImage_ = img;
    imageNameLabel_->setText(QFileInfo(path).fileName());
    resetModel();
    maskEditCanvas_->clearEdits();
    setOriginal(img);
    logEdit_->appendPlainText("[INFO] " + TR("Carregada: ", "Loaded: ") + path);
}

void MainWindow::loadModel() {
    QString path = QFileDialog::getOpenFileName(
        this, TR("Carregar modelo ONNX", "Load ONNX model"),
        QDir::homePath(), "ONNX (*.onnx)");
    if (path.isEmpty()) return;
    modelPathEdit_->setText(path);
    modelCombo_->setCurrentIndex(2);
    resetModel();
    logEdit_->appendPlainText(
        "[INFO] " + TR("Modelo definido: ", "Model set: ") + path);
}

void MainWindow::modelIndexChanged(int index) {
    if (index < 0 || index >= modelCombo_->count()) return;
    QString path = modelCombo_->itemData(index).toString();
    modelPathEdit_->setText(path);
    resetModel();
}

void MainWindow::bgColorChanged(int index) {
    if (index == 3) {
        QColor color = QColorDialog::getColor(
            customBgColor_.isValid() ? customBgColor_ : Qt::white,
            this, TR("Escolher cor de fundo", "Choose background color"));
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
        QMessageBox::warning(this, TR("Aviso", "Warning"),
            TR("Abra uma imagem primeiro.", "Open an image first."));
        return;
    }
    if (removeWatcher_.isRunning() || batchWatcher_.isRunning()) return;

    // A inferencia corre numa thread de trabalho: a janela continua
    // reativa e o modelo e carregado la dentro (pode demorar segundos).
    setUiBusy(true);
    progressBar_->setRange(0, 0);
    progressBar_->setValue(0);
    progressBar_->setVisible(true);
    logEdit_->appendPlainText(
        "[INFO] " + TR("A remover fundo...", "Removing background..."));

    ProcessingOptions options = currentProcessingOptions();
    // A cor de fundo e apenas uma pre-visualizacao na imagem unica;
    // manter a mascara transparente permite continuar editando-a.
    options.backgroundColor = QColor();

    removeWatcher_.setFuture(QtConcurrent::run(
        [cache = modelCache_, img = originalImage_, options]() -> SingleResult {
            try {
                const std::shared_ptr<BackgroundRemover> r =
                    obtainRemover(cache, options.modelPath);
                if (!r)
                    return { QImage(), "modelo indisponivel" };
                return { ImagePostProcessor::apply(
                             r->removeBackground(img), options),
                         QString() };
            } catch (const Ort::Exception& e) {
                return { QImage(),
                         QString("Erro ONNX: %1").arg(e.what()) };
            } catch (const std::exception& e) {
                return { QImage(), QString::fromUtf8(e.what()) };
            }
        }));
}

void MainWindow::onRemoveFinished()
{
    progressBar_->setVisible(false);
    progressBar_->setRange(0, 100);
    progressBar_->setValue(0);
    setUiBusy(false);

    const SingleResult res = removeWatcher_.result();
    if (!res.error.isEmpty()) {
        logEdit_->appendPlainText("[ERRO] " + res.error);
        // Sinal antes do dialogo: consumidores automaticos (ex.: a
        // ferramenta de screenshots em modo offscreen) nao ficam
        // bloqueados por uma caixa modal.
        emit jobFinished();
        QMessageBox::critical(this, TR("Erro", "Error"),
            TR("Falha ao remover o fundo:\n",
               "Failed to remove the background:\n") + res.error);
        return;
    }
    resultImage_ = res.image;
    maskEditCanvas_->setImage(resultImage_, currentBackgroundColor());
    setResult(resultImage_);
    logEdit_->appendPlainText("[OK] " + TR("Fundo removido.",
                                           "Background removed."));
    emit jobFinished();
}

void MainWindow::saveResult() {
    if (resultImage_.isNull()) {
        QMessageBox::warning(this, TR("Aviso", "Warning"),
            TR("Remova o fundo primeiro.", "Remove the background first."));
        return;
    }
    QString path = QFileDialog::getSaveFileName(
        this, TR("Salvar PNG", "Save PNG"), QDir::homePath(),
        "PNG (*.png)");
    if (path.isEmpty()) return;
    QFileInfo fi(path);
    if (fi.suffix().toLower() != "png") path += ".png";

    ProcessingOptions saveOptions;
    saveOptions.backgroundColor = currentBackgroundColor();
    const QImage toSave = ImagePostProcessor::apply(
        resultImage_, saveOptions);

    if (!toSave.save(path, "PNG")) {
        QMessageBox::critical(this, TR("Erro", "Error"),
            TR("Nao foi possivel salvar:\n", "Could not save:\n") + path);
    } else {
        QMessageBox::information(this, TR("OK", "OK"),
            TR("Salvo:\n", "Saved:\n") + path);
        logEdit_->appendPlainText(
            "[OK] " + TR("PNG salvo: ", "PNG saved: ") + path);
    }
}

void MainWindow::selectFolder() {
    QString folder = QFileDialog::getExistingDirectory(
        this, TR("Selecionar pasta com imagens",
                 "Select folder with images"),
        folderEdit_->text().isEmpty()
            ? QDir::homePath() : folderEdit_->text());
    if (folder.isEmpty()) return;
    folderEdit_->setText(folder);
    logEdit_->appendPlainText(
        "[INFO] " + TR("Pasta selecionada: ", "Folder selected: ") + folder);
}

void MainWindow::selectOutputFolder() {
    QString folder = QFileDialog::getExistingDirectory(
        this, TR("Selecionar diretorio de saida",
                 "Select output directory"),
        outputDirEdit_->text().isEmpty()
            ? QDir::homePath() : outputDirEdit_->text());
    if (folder.isEmpty()) return;
    outputDirEdit_->setText(folder);
    logEdit_->appendPlainText(
        "[INFO] " + TR("Dir. de saida definido: ",
                       "Output dir. set: ") + folder);
}

void MainWindow::setUiBusy(bool busy)
{
    btnOpen_->setEnabled(!busy);
    btnRemove_->setEnabled(!busy);
    btnSave_->setEnabled(!busy);
    btnSelectFolder_->setEnabled(!busy);
    btnSelectOutput_->setEnabled(!busy);
    btnLoadModel_->setEnabled(!busy);
    btnProcessBatch_->setEnabled(!busy);
    modelCombo_->setEnabled(!busy);
    modelPathEdit_->setEnabled(!busy);
    folderEdit_->setEnabled(!busy);
    outputDirEdit_->setEnabled(!busy);
}

void MainWindow::resetModel()
{
    // O modelo pode estar em uso numa thread de trabalho; o mutex
    // protege e a copia shared_ptr do worker mantem a sessao viva
    // ate o passo terminar.
    QMutexLocker lock(&modelCache_->mutex);
    modelCache_->remover.reset();
    modelCache_->loadedPath.clear();
}

std::shared_ptr<BackgroundRemover> MainWindow::obtainRemover(
    const std::shared_ptr<ModelCache>& cache,
    const QString& modelPath)
{
    if (!cache) return nullptr;
    QMutexLocker lock(&cache->mutex);
    if (!cache->remover || cache->loadedPath != modelPath) {
        // Carrega dentro da thread de trabalho: a GUI nunca bloqueia.
        cache->remover = std::make_shared<BackgroundRemover>(modelPath);
        cache->loadedPath = modelPath;
    }
    return cache->remover;
}

void MainWindow::populateModelCombo()
{
    const QString current = modelPathEdit_->text();
    const QSignalBlocker blocker(modelCombo_);
    modelCombo_->clear();
    modelCombo_->addItem(
        TR("U\u00b2-Netp \u2014 r\u00e1pido (~5 MB)",
           "U\u00b2-Netp \u2014 fast (~5 MB)"),
        ModelPathResolver::resolve("u2netp.onnx"));
    modelCombo_->addItem(
        TR("U\u00b2-Net  \u2014 qualidade (~176 MB)",
           "U\u00b2-Net  \u2014 quality (~176 MB)"),
        ModelPathResolver::resolve("u2net.onnx"));

    // Modelos descarregados na aba "Modelos".
    const QDir modelsDir(ModelPathResolver::userModelsDir());
    const QStringList onnx = modelsDir.entryList(
        QStringList() << "*.onnx", QDir::Files, QDir::Name);
    for (const QString& f : onnx) {
        if (f.compare("u2netp.onnx", Qt::CaseInsensitive) == 0
            || f.compare("u2net.onnx", Qt::CaseInsensitive) == 0) {
            continue; // ja listados acima
        }
        modelCombo_->addItem(f, modelsDir.absoluteFilePath(f));
    }

    modelCombo_->addItem(TR("Caminho customizado", "Custom path"), "");
    int idx = modelCombo_->findData(current);
    if (idx < 0 && !current.isEmpty()) {
        modelPathEdit_->setText(current);
        idx = modelCombo_->count() - 1;
    }
    modelCombo_->setCurrentIndex(qMax(0, idx));
}

void MainWindow::restoreSettings()
{
    QSettings settings("bg-remover", "bg-remover");
    const QByteArray geometry =
        settings.value("ui/geometry").toByteArray();
    if (!geometry.isEmpty()) restoreGeometry(geometry);

    folderEdit_->setText(settings.value("batch/inputDir").toString());
    outputDirEdit_->setText(
        settings.value("batch/outputDir", "./_rembg").toString());
    brushRadiusSpin_->setValue(
        settings.value("brush/radius", 10).toInt());
    refineCheckBox_->setChecked(
        settings.value("options/refineEdges", false).toBool());
    noiseCheckBox_->setChecked(
        settings.value("options/cleanNoise", false).toBool());
    noiseThresholdSpin_->setValue(
        settings.value("options/noiseThreshold", kAlphaThreshold).toInt());

    const QVariant colorVar = settings.value("output/customColor");
    if (colorVar.isValid()) {
        const QColor color = colorVar.value<QColor>();
        if (color.isValid()) {
            customBgColor_ = color;
            bgColorCombo_->setItemData(3, QVariant::fromValue(color));
            bgColorCombo_->setItemText(3, color.name());
        }
    }
    {
        // Sem o QSignalBlocker, o indice 3 dispararia o dialogo de cor.
        const QSignalBlocker blocker(bgColorCombo_);
        const int idx =
            settings.value("output/bgIndex", 0).toInt();
        if (idx >= 0 && idx < bgColorCombo_->count())
            bgColorCombo_->setCurrentIndex(idx);
    }
}

void MainWindow::saveSettings()
{
    QSettings settings("bg-remover", "bg-remover");
    settings.setValue("ui/geometry", saveGeometry());
    settings.setValue("batch/inputDir", folderEdit_->text());
    settings.setValue("batch/outputDir", outputDirEdit_->text());
    settings.setValue("brush/radius", brushRadiusSpin_->value());
    settings.setValue("options/refineEdges",
                      refineCheckBox_->isChecked());
    settings.setValue("options/cleanNoise",
                      noiseCheckBox_->isChecked());
    settings.setValue("options/noiseThreshold",
                      noiseThresholdSpin_->value());
    settings.setValue("output/bgIndex", bgColorCombo_->currentIndex());
    settings.setValue("output/customColor", customBgColor_);
    if (!modelPathEdit_->text().isEmpty())
        settings.setValue("model/path", modelPathEdit_->text());
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    saveSettings();
    if (batchRunning_) {
        batchRunning_ = false;
        batchCancelRequested_ = true;
    }
    QMainWindow::closeEvent(event);
}

void MainWindow::dragEnterEvent(QDragEnterEvent* event)
{
    if (event->mimeData()->urls().isEmpty()) return;
    const QFileInfo fi(
        event->mimeData()->urls().first().toLocalFile());
    if (fi.isDir()
        || BatchPlan::imageExts().contains(fi.suffix().toLower())) {
        event->acceptProposedAction();
    }
}

void MainWindow::dropEvent(QDropEvent* event)
{
    if (event->mimeData()->urls().isEmpty()) return;
    const QFileInfo fi(
        event->mimeData()->urls().first().toLocalFile());
    if (fi.isDir()) {
        folderEdit_->setText(fi.absoluteFilePath());
        logEdit_->appendPlainText(
            "[INFO] " + TR("Pasta de lote: ", "Batch folder: ")
            + fi.absoluteFilePath());
        event->acceptProposedAction();
        return;
    }
    if (BatchPlan::imageExts().contains(fi.suffix().toLower())) {
        openPath(fi.absoluteFilePath());
        event->acceptProposedAction();
    }
}

bool MainWindow::eventFilter(QObject* watched, QEvent* event)
{
    if (event->type() == QEvent::Wheel) {
        auto* wheel = static_cast<QWheelEvent*>(event);
        const bool overPreview =
            watched == originalScroll_->viewport()
            || watched == resultScroll_->viewport();
        if (overPreview
            && wheel->modifiers().testFlag(Qt::ControlModifier)) {
            // Zoom ancorado no ponto sob o cursor.
            zoomAt(previewZoomPercent_
                       + (wheel->angleDelta().y() > 0 ? 10 : -10),
                   wheel->globalPosition().toPoint());
            return true;
        }
    }
    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::processBatch() {
    // Em execucao: o botao funciona como "Cancelar lote".
    if (batchRunning_) {
        batchCancelRequested_ = true;
        btnProcessBatch_->setEnabled(false);
        logEdit_->appendPlainText("[BATCH] " + TR(
            "Cancelamento solicitado; a terminar a imagem em curso...",
            "Cancellation requested; finishing the current image..."));
        return;
    }
    if (removeWatcher_.isRunning() || batchWatcher_.isRunning()) return;

    if (folderEdit_->text().isEmpty()) {
        QMessageBox::warning(this, TR("Aviso", "Warning"),
            TR("Selecione uma pasta primeiro.",
               "Select a folder first."));
        return;
    }
    batchFolder_ = folderEdit_->text();
    batchOutputDir_ = outputDirEdit_->text();

    // Planejamento compartilhado com o CLI (BatchPlan): filtragem
    // case-insensitive e saidas que nao sobrescrevem ficheiros.
    batchPlan_ = BatchPlan::build(
        QDir(batchFolder_), QDir(batchOutputDir_));
    if (batchPlan_.isEmpty()) {
        QMessageBox::warning(this, TR("Aviso", "Warning"),
            TR("Nenhuma imagem encontrada na pasta.",
               "No images found in the folder."));
        return;
    }

    QDir outDir(batchOutputDir_);
    if (!outDir.exists() && !outDir.mkpath(".")) {
        QMessageBox::critical(this, TR("Erro", "Error"),
            TR("Nao foi possivel criar diretorio de saida:\n",
               "Could not create output directory:\n")
                + batchOutputDir_);
        return;
    }

    setUiBusy(true);
    // O botao continua ativo, agora como Cancelar.
    btnProcessBatch_->setEnabled(true);
    btnProcessBatch_->setText(TR("Cancelar lote", "Cancel batch"));
    progressBar_->setRange(0, batchPlan_.size());
    progressBar_->setValue(0);
    progressBar_->setVisible(true);
    logEdit_->clear();
    logEdit_->appendPlainText(
        QString("[BATCH] %1 -> %2")
            .arg(TR("Processando %1 arquivo(s)",
                    "Processing %1 file(s)").arg(batchPlan_.size()))
            .arg(batchOutputDir_));

    batchIndex_ = 0;
    batchOk_ = 0;
    batchErrors_ = 0;
    batchCancelRequested_ = false;
    batchRunning_ = true;
    batchOptions_ = currentProcessingOptions();

    // O modelo e carregado dentro da thread de trabalho (obtainRemover
    // no primeiro passo), por isso esta funcao nunca bloqueia a GUI.
    QTimer::singleShot(0, this,
                       &MainWindow::processBatchStep);
}

void MainWindow::processBatchStep() {
    if (!batchRunning_) return;
    if (batchCancelRequested_ || batchIndex_ >= batchPlan_.size()) {
        finishBatch();
        return;
    }

    const BatchPlan::Entry& entry = batchPlan_[batchIndex_];
    const QString file = QFileInfo(entry.inputPath).fileName();
    logEdit_->appendPlainText(
        QString("[BATCH] %1/%2: %3")
            .arg(batchIndex_ + 1).arg(batchPlan_.size()).arg(file));

    // Mensagens de erro traduzidas na GUI antes da thread (I18n usa
    // QSettings, que nao deve ser lido fora da thread principal).
    const QString errLoad = TR("Falha ao carregar a imagem",
                               "Failed to load the image");
    const QString errSave = TR("Falha ao salvar o PNG",
                               "Failed to save the PNG");
    const QString errModel = TR("modelo indisponivel",
                                "model unavailable");

    // Cada imagem corre na thread de trabalho; a GUI so recebe o
    // resultado via watcher e agenda o passo seguinte.
    batchWatcher_.setFuture(QtConcurrent::run(
        [cache = modelCache_, options = batchOptions_,
         in = entry.inputPath, out = entry.outputPath,
         errLoad, errSave, errModel]() -> QString {
            try {
                QImage img(in);
                if (img.isNull())
                    return errLoad;
                const std::shared_ptr<BackgroundRemover> r =
                    obtainRemover(cache, options.modelPath);
                if (!r) return errModel;
                const QImage processed = ImagePostProcessor::apply(
                    r->removeBackground(img), options);
                if (!processed.save(out, "PNG"))
                    return errSave;
                return QString();
            } catch (const Ort::Exception& e) {
                return QString("ONNX: %1").arg(e.what());
            } catch (const std::exception& e) {
                return QString::fromUtf8(e.what());
            }
        }));
}

void MainWindow::onBatchStepFinished()
{
    if (!batchRunning_) return;
    const QString err = batchWatcher_.result();
    const BatchPlan::Entry& entry = batchPlan_[batchIndex_];
    const QString file = QFileInfo(entry.inputPath).fileName();
    const QString outName = QFileInfo(entry.outputPath).fileName();

    if (err.isEmpty()) {
        ++batchOk_;
        logEdit_->appendPlainText("[OK] " + file + " -> " + outName);
    } else {
        ++batchErrors_;
        logEdit_->appendPlainText("[ERRO] " + file + ": " + err);
    }
    ++batchIndex_;
    progressBar_->setValue(batchIndex_);
    QTimer::singleShot(0, this, &MainWindow::processBatchStep);
}

void MainWindow::finishBatch()
{
    batchRunning_ = false;
    progressBar_->setValue(batchPlan_.size());
    logEdit_->appendPlainText(
        QString("[BATCH] %1: %2 OK, %3 erro(s).")
            .arg(batchCancelRequested_
                     ? TR("Cancelado", "Cancelled")
                     : TR("Concluido", "Completed"))
            .arg(batchOk_).arg(batchErrors_));
    btnProcessBatch_->setText(TR("Processar lote", "Process batch"));
    setUiBusy(false);
    batchCancelRequested_ = false;
}

void MainWindow::setOriginal(const QImage& img) {
    Q_UNUSED(img)
    updatePreviewZoom();
}

void MainWindow::setResult(const QImage& img) {
    Q_UNUSED(img)
    updatePreviewZoom();
}
