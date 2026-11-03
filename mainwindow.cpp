#include "mainwindow.h"
#include "i18n.h"
#include "maskeditcanvas.h"
#include "helpwidget.h"
#include "imagepostprocessor.h"
#include "modelmanager.h"
#include "modelpathresolver.h"
#include <QCloseEvent>
#include <QFile>
#include <QFileDialog>
#include <QImageReader>
#include <QImageWriter>
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
#include <QByteArray>
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
#include <QDrag>
#include <QUrl>
#include <QClipboard>
#include <QDesktopServices>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
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
#include "savepath.h"

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

    const int bgIdx = bgColorCombo_->currentIndex();
    if (bgIdx == 4) {
        options.backgroundStyle = ProcessingOptions::BgBlurredOriginal;
    } else if (bgIdx == 5 && !customBgImagePath_.isEmpty()) {
        options.backgroundStyle = ProcessingOptions::BgCustomImage;
        options.backgroundImagePath = customBgImagePath_;
    } else if (options.backgroundColor.isValid()) {
        options.backgroundStyle = ProcessingOptions::BgSolidColor;
    } else {
        options.backgroundStyle = ProcessingOptions::BgTransparent;
    }

    options.stickerOutline = stickerCheckBox_->isChecked();
    options.stickerWidth = 6;
    options.stickerColor = Qt::white;

    options.dropShadow = shadowCheckBox_->isChecked();
    options.shadowBlur = 10;
    options.shadowOffsetX = 4;
    options.shadowOffsetY = 6;
    options.shadowOpacity = 0.5f;

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
      btnSave_(new QPushButton(TR("Salvar imagem", "Save image"))),
      btnCopy_(new QPushButton),
      btnLoadModel_(new QPushButton(TR("Buscar...", "Browse..."))),
      btnSelectFolder_(new QPushButton("...")),
      btnSelectOutput_(new QPushButton("...")),
      btnProcessBatch_(new QPushButton(TR("Processar lote", "Process batch"))),
      btnUndo_(new QPushButton(TR("Desfazer", "Undo"))),
      btnRedo_(new QPushButton(TR("Refazer", "Redo"))),
      btnAutoCrop_(new QPushButton(TR("Auto-Recorte", "Auto-Crop"))),
      btnDragOut_(new QPushButton(TR("Arrastar resultado", "Drag result"))),
      presetCombo_(new QComboBox),
      bgColorCombo_(new QComboBox),
      brushModeCombo_(new QComboBox),
      brushRadiusSpin_(new QSpinBox),
      noiseThresholdSpin_(new QSpinBox),
      refineCheckBox_(new QCheckBox(TR("Refinar bordas", "Refine edges"))),
      noiseCheckBox_(new QCheckBox(TR("Limpar ruído", "Clean noise"))),
      stickerCheckBox_(new QCheckBox(TR("Contorno (Sticker)", "Sticker outline"))),
      shadowCheckBox_(new QCheckBox(TR("Sombra suave", "Drop shadow"))),
      saveFormatCombo_(new QComboBox),
      updateBanner_(new QLabel),
      progressBar_(new QProgressBar),
      logEdit_(new QPlainTextEdit),
      maskEditCanvas_(new MaskEditCanvas(this)),
      zoomDebounce_(new QTimer(this)),
      batchIndex_(0),
      batchOk_(0),
      batchErrors_(0),
      batchRunning_(false),
      batchCancelRequested_(false),
      customBgColor_(QColor()),
      previewZoomPercent_(100),
      // Ordem igual a da declaracao na classe (evita -Wreorder).
      modelCache_(std::make_shared<ModelCache>())
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

    presetCombo_->addItem(TR("Personalizado", "Custom"));
    presetCombo_->addItem(TR("Padrão (Transparente)", "Standard (Transparent)"));
    presetCombo_->addItem(TR("E-commerce (Branco + Sombra)", "E-commerce (White + Shadow)"));
    presetCombo_->addItem(TR("Sticker (Contorno Branco)", "Sticker (White Outline)"));
    presetCombo_->addItem(TR("Retrato (Fundo Desfocado)", "Portrait (Blurred Background)"));
    presetCombo_->setCurrentIndex(0);

    bgColorCombo_->addItem(TR("Transparente", "Transparent"), QVariant());
    bgColorCombo_->addItem(
        TR("Branco", "White"), QVariant::fromValue(QColor(Qt::white)));
    bgColorCombo_->addItem(
        TR("Preto", "Black"), QVariant::fromValue(QColor(Qt::black)));
    bgColorCombo_->addItem(TR("Personalizado...", "Custom..."), QVariant());
    bgColorCombo_->addItem(TR("Desfoque original (Retrato)", "Original blur (Portrait)"), QVariant());
    bgColorCombo_->addItem(TR("Imagem de fundo...", "Background image..."), QVariant());
    bgColorCombo_->setCurrentIndex(0);
    bgColorCombo_->setMaximumWidth(200);

    brushModeCombo_->addItem(TR("Restaurar", "Restore"));
    brushModeCombo_->addItem(TR("Apagar", "Erase"));
    brushModeCombo_->setCurrentIndex(0);

    brushRadiusSpin_->setRange(1, 200);
    brushRadiusSpin_->setValue(10);
    brushRadiusSpin_->setSuffix(" px");

    maskEditCanvas_->setMode(MaskEditCanvas::Restore);
    maskEditCanvas_->setBrushRadius(brushRadiusSpin_->value());

    btnUndo_->setEnabled(false);
    btnAutoCrop_->setEnabled(false);
    btnDragOut_->setEnabled(false);
    btnAutoCrop_->setToolTip(TR("Recorta o espaço transparente ao redor do objeto",
                                "Crops transparent empty space around the object"));
    btnDragOut_->setToolTip(TR("Clique e arraste para soltar o PNG no Photoshop, Telegram ou pasta",
                               "Click and drag to drop the PNG into Photoshop, Telegram or folder"));

    outputDirEdit_->setReadOnly(true);

    progressBar_->setRange(0, 100);
    progressBar_->setValue(0);
    progressBar_->setVisible(false);

    logEdit_->setReadOnly(true);
    logEdit_->setMaximumHeight(150);
    logEdit_->setVisible(false);
    logEdit_->setPlaceholderText(TR("Log do processamento...",
                                    "Processing log..."));

    // Nomes estaveis para acessibilidade e para o teste GUI (offscreen)
    // encontrar os controlos sem depender da ordem do layout.
    originalLabel_->setObjectName("originalPreview");
    modelCombo_->setObjectName("modelCombo");
    modelPathEdit_->setObjectName("modelPath");
    folderEdit_->setObjectName("batchInput");
    outputDirEdit_->setObjectName("batchOutput");
    saveFormatCombo_->setObjectName("saveFormatCombo");
    presetCombo_->setObjectName("presetCombo");
    bgColorCombo_->setObjectName("bgColorCombo");
    btnAutoCrop_->setObjectName("autoCropButton");
    btnDragOut_->setObjectName("dragOutButton");
    stickerCheckBox_->setObjectName("stickerCheckBox");
    shadowCheckBox_->setObjectName("shadowCheckBox");
    refineCheckBox_->setObjectName("refineEdges");
    noiseCheckBox_->setObjectName("cleanNoise");
    noiseThresholdSpin_->setObjectName("noiseThreshold");
    brushModeCombo_->setObjectName("brushMode");
    brushRadiusSpin_->setObjectName("brushRadius");
    progressBar_->setObjectName("progressBar");
    logEdit_->setObjectName("logEdit");
    maskEditCanvas_->setObjectName("resultCanvas");
    btnProcessBatch_->setObjectName("processBatchButton");
    btnUndo_->setObjectName("undoButton");
    btnRedo_->setObjectName("redoButton");

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
    auto* subtitle = new QLabel(TR("Editor de fundo local",
                                   "Local background editor"));
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
    auto* resultTitle = new QLabel(TR("Resultado", "Result"));
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
    auto* btnCompare = new QPushButton(TR("Comparar", "Compare"));
    btnCompare->setObjectName("viewModeActive");
    btnCompare->setCheckable(true);
    btnCompare->setToolTip(
        TR("Arraste o divisor para ver o antes/depois (desativa o pincel)",
           "Drag the divider to see before/after (disables the brush)"));
    connect(btnCompare, &QPushButton::toggled, this,
            [this](bool on) {
        maskEditCanvas_->setCompareEnabled(on);
        maskEditCanvas_->setCompareImage(originalImage_);
        logEdit_->appendPlainText(on
            ? "[INFO] " + TR("Comparacao antes/depois ativada.",
                             "Before/after comparison enabled.")
            : "[INFO] " + TR("Comparacao desativada.",
                             "Comparison disabled."));
    });
    workspaceToolbar->addWidget(btnCompare);
    auto* editHint = new QLabel(TR("Edite a máscara no resultado",
                                   "Edit the mask on the result"));
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
    outputForm->addRow(TR("Predefinição", "Preset"), presetCombo_);
    outputForm->addRow(TR("Fundo ao salvar", "Background on save"),
                       bgColorCombo_);
    // Formato de saida: PNG e WebP guardam transparencia; JPEG
    // compoe sobre o fundo escolhido (branco por omissao).
    saveFormatCombo_->addItem(TR("PNG (transparente)",
                                 "PNG (transparent)"));
    saveFormatCombo_->addItem(TR("JPEG", "JPEG"));
    saveFormatCombo_->addItem(TR("WebP", "WebP"));
    outputForm->addRow(TR("Formato", "Format"),
                       saveFormatCombo_);
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
    inspectorLayout->addWidget(stickerCheckBox_);
    inspectorLayout->addWidget(shadowCheckBox_);

    auto* separatorTwo = new QFrame;
    separatorTwo->setObjectName("inspectorSeparator");
    separatorTwo->setFrameShape(QFrame::HLine);
    inspectorLayout->addWidget(separatorTwo);
    inspectorLayout->addWidget(sectionTitle(
        TR("Correção manual & Ações", "Manual touch-up & Actions")));
    auto* brushForm = new QFormLayout;
    brushForm->setRowWrapPolicy(QFormLayout::WrapAllRows);
    brushForm->addRow(TR("Pincel", "Brush"), brushModeCombo_);
    brushForm->addRow(TR("Tamanho", "Size"), brushRadiusSpin_);
    inspectorLayout->addLayout(brushForm);
    auto* undoRow = new QHBoxLayout;
    undoRow->addWidget(btnUndo_);
    undoRow->addWidget(btnRedo_);
    inspectorLayout->addLayout(undoRow);
    auto* actionRow = new QHBoxLayout;
    actionRow->addWidget(btnAutoCrop_);
    actionRow->addWidget(btnDragOut_);
    inspectorLayout->addLayout(actionRow);
    auto* brushHint = new QLabel(TR(
        "Após remover o fundo, use o pincel para corrigir detalhes, faça auto-recorte ou arraste o resultado.",
        "After removing the background, use the brush to touch up details, auto-crop or drag the result."));
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
    connect(modelManager_, &ModelManagerWidget::modelsChanged,
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
    btnCopy_->setObjectName("copyButton");
    btnCopy_->setToolTip(
        TR("Copiar resultado para a area de transferencia (Ctrl+Shift+C)",
           "Copy result to the clipboard (Ctrl+Shift+C)"));
    btnCopy_->setText(TR("Copiar", "Copy"));
    batchLayout->addWidget(btnCopy_);
    btnSave_->setObjectName("saveButton");
    batchLayout->addWidget(btnSave_);

    mainLayout_ = new QVBoxLayout();
    mainLayout_->setContentsMargins(16, 0, 16, 16);
    mainLayout_->setSpacing(16);
    mainLayout_->addWidget(header);
    mainLayout_->addWidget(mainTabs_, 1);
    mainLayout_->addWidget(batchBar);
    mainLayout_->addWidget(progressBar_);
    auto* btnToggleLog = new QPushButton(TR("Mostrar log \u25be",
                                            "Show log \u25be"));
    btnToggleLog->setObjectName("logToggle");
    mainLayout_->addWidget(btnToggleLog);
    mainLayout_->addWidget(logEdit_);

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
    central->setLayout(mainLayout_);
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
    connect(btnCopy_,          &QPushButton::clicked,
            this,              &MainWindow::copyResult);
    connect(saveFormatCombo_, QOverload<int>::of(
                &QComboBox::currentIndexChanged),
            this,              &MainWindow::onSaveFormatChanged);
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
        if (!isBusy_) maskEditCanvas_->undo();
    });
    connect(btnRedo_,          &QPushButton::clicked,
            this,              [this]() {
        if (!isBusy_) maskEditCanvas_->redo();
    });
    connect(maskEditCanvas_, &MaskEditCanvas::availabilityChanged,
            this, [this](bool canUndo, bool canRedo) {
        btnUndo_->setEnabled(!isBusy_ && canUndo);
        btnRedo_->setEnabled(!isBusy_ && canRedo);
    });
    btnUndo_->setEnabled(false);
    btnRedo_->setEnabled(false);

    connect(btnAutoCrop_,      &QPushButton::clicked,
            this,              &MainWindow::autoCropResult);
    connect(btnDragOut_,       &QPushButton::clicked,
            this,              &MainWindow::startDragOut);
    connect(presetCombo_, QOverload<int>::of(
                &QComboBox::currentIndexChanged),
            this,              &MainWindow::presetChanged);
    connect(stickerCheckBox_,  &QCheckBox::toggled, this, [this](bool) {
        if (presetCombo_->currentIndex() != 0) presetCombo_->setCurrentIndex(0);
    });
    connect(shadowCheckBox_,   &QCheckBox::toggled, this, [this](bool) {
        if (presetCombo_->currentIndex() != 0) presetCombo_->setCurrentIndex(0);
    });

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
    auto* scCopy = new QShortcut(QKeySequence("Ctrl+Shift+C"), this);
    connect(scCopy, &QShortcut::activated, this, &MainWindow::copyResult);
    auto* scPaste = new QShortcut(QKeySequence::Paste, this);
    connect(scPaste, &QShortcut::activated, this, &MainWindow::pasteImageFromClipboard);
    auto* scUndo = new QShortcut(QKeySequence::Undo, this);
    connect(scUndo, &QShortcut::activated, this, [this]() {
        if (!isBusy_) maskEditCanvas_->undo();
    });
    auto* scRedo = new QShortcut(QKeySequence::Redo, this);
    connect(scRedo, &QShortcut::activated, this, [this]() {
        if (!isBusy_) maskEditCanvas_->redo();
    });
    auto* scRedoY = new QShortcut(QKeySequence("Ctrl+Y"), this);
    connect(scRedoY, &QShortcut::activated, this, [this]() {
        if (!isBusy_) maskEditCanvas_->redo();
    });
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

    // Verificacao de nova versao (assincrona, silenciosa em falha).
    checkForUpdate();
}

void MainWindow::editingDone()
{
    // Durante uma inferencia/lote o canvas fica desativado; ainda
    // assim, ignorar edicoes evita substituir o resultado antigo por
    // engano caso algum evento chegue fora de tempo.
    if (isBusy_) return;
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
    if (isBusy_) return;
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
    // setAutoTransform aplica a orientacao EXIF (fotos de telemovel).
    QImageReader reader(path);
    reader.setAutoTransform(true);
    const QImage img = reader.read();
    if (img.isNull()) {
        QString detail = reader.errorString();
        if (!detail.isEmpty()) detail = "\n" + detail;
        QMessageBox::warning(this, TR("Erro", "Error"),
            TR("Nao foi possivel carregar:\n", "Could not load:\n")
                + path + detail);
        return;
    }
    ++imageGeneration_;
    resultImage_ = QImage();
    btnAutoCrop_->setEnabled(false);
    btnDragOut_->setEnabled(false);
    originalImage_ = img;
    currentImagePath_ = path;
    imageNameLabel_->setText(QFileInfo(path).fileName());
    // O canvas do resultado e limpo por completo: sem isto, Salvar e
    // Copiar continuavam a exportar a imagem anterior.
    maskEditCanvas_->clearImage();
    // Se o modo Comparar estiver ativo, o lado "antes" passa a ser a
    // imagem recem-aberta.
    maskEditCanvas_->setCompareImage(img);
    setOriginal(img);
    logEdit_->appendPlainText("[INFO] " + TR("Carregada: ", "Loaded: ") + path);
}

void MainWindow::loadModel() {
    if (isBusy_) return;
    QString path = QFileDialog::getOpenFileName(
        this, TR("Carregar modelo ONNX", "Load ONNX model"),
        QDir::homePath(), "ONNX (*.onnx)");
    if (path.isEmpty()) return;
    // Seleciona "Caminho customizado" sem deixar o slot limpar o
    // caminho escolhido; o indice fixo (2) apontava para outro modelo
    // quando havia modelos descarregados.
    {
        const QSignalBlocker blocker(modelCombo_);
        const int customIdx = modelCombo_->findData(QString());
        modelCombo_->setCurrentIndex(
            customIdx >= 0 ? customIdx : modelCombo_->count() - 1);
    }
    modelPathEdit_->setText(path);
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
    if (presetCombo_->currentIndex() != 0) {
        const QSignalBlocker blocker(presetCombo_);
        presetCombo_->setCurrentIndex(0);
    }
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
    } else if (index == 5) {
        const QString path = QFileDialog::getOpenFileName(
            this, TR("Escolher imagem de fundo", "Choose background image"),
            QDir::homePath(),
            TR("Imagens (*.png *.jpg *.jpeg *.bmp *.webp *.tiff)",
               "Images (*.png *.jpg *.jpeg *.bmp *.webp *.tiff)"));
        if (!path.isEmpty()) {
            customBgImagePath_ = path;
            bgColorCombo_->setItemText(5, QFileInfo(path).fileName());
        } else if (customBgImagePath_.isEmpty()) {
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

    // Mensagens traduzidas na GUI antes da thread: I18n le QSettings,
    // que nao deve ser usado fora da thread principal.
    const QString errModel = TR("modelo indisponível",
                                "model unavailable");
    const QString errOnnx = TR("Erro ONNX: ", "ONNX error: ");
    const QString errGeneric = TR("Falha: ", "Failure: ");
    const QString errUnknown = TR("Erro desconhecido durante a inferência.",
                                  "Unknown error during inference.");

    // Se outra imagem for aberta durante a inferencia, este numero
    // muda e o resultado antigo e descartado ao chegar.
    const quint64 generation = imageGeneration_;

    removeWatcher_.setFuture(QtConcurrent::run(
        [cache = modelCache_, img = originalImage_, options,
         generation, errModel, errOnnx, errGeneric, errUnknown]()
            -> SingleResult {
            try {
                const std::shared_ptr<BackgroundRemover> r =
                    obtainRemover(cache, options.modelPath);
                if (!r)
                    return { QImage(), errModel, generation };
                return { ImagePostProcessor::apply(
                             r->removeBackground(img), options),
                         QString(), generation };
            } catch (const Ort::Exception& e) {
                return { QImage(),
                         errOnnx + QString::fromUtf8(e.what()),
                         generation };
            } catch (const std::exception& e) {
                return { QImage(),
                         errGeneric + QString::fromUtf8(e.what()),
                         generation };
            } catch (...) {
                // Uma excecao desconhecida nao pode escapar do
                // QtConcurrent: result() voltaria a lancar na GUI.
                return { QImage(), errUnknown, generation };
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
    // A imagem pode ter sido trocada enquanto a inferencia corria.
    // Nunca aplicar o resultado antigo sobre a imagem nova.
    if (res.generation != imageGeneration_) {
        logEdit_->appendPlainText("[INFO] " + TR(
            "Resultado de uma imagem anterior ignorado.",
            "Result from a previous image ignored."));
        emit jobFinished();
        maybeCloseAfterJobs();
        return;
    }
    if (!res.error.isEmpty()) {
        logEdit_->appendPlainText("[ERROR] " + res.error);
        // Sinal antes do dialogo: consumidores automaticos (ex.: a
        // ferramenta de screenshots em modo offscreen) nao ficam
        // bloqueados por uma caixa modal.
        emit jobFinished();
        // Durante o fecho nao bloquear a janela com uma caixa modal.
        if (!closeRequested_) {
            QMessageBox::critical(this, TR("Erro", "Error"),
                TR("Falha ao remover o fundo:\n",
                   "Failed to remove the background:\n") + res.error);
        }
        maybeCloseAfterJobs();
        return;
    }
    resultImage_ = res.image;
    maskEditCanvas_->setImage(resultImage_, currentBackgroundColor());
    setResult(resultImage_);
    btnAutoCrop_->setEnabled(true);
    btnDragOut_->setEnabled(true);
    logEdit_->appendPlainText("[OK] " + TR("Fundo removido.",
                                           "Background removed."));
    emit jobFinished();
    maybeCloseAfterJobs();
}

void MainWindow::saveResult() {
    if (isBusy_) return;
    if (resultImage_.isNull()) {
        QMessageBox::warning(this, TR("Aviso", "Warning"),
            TR("Remova o fundo primeiro.", "Remove the background first."));
        return;
    }

    const QString extension = saveFormatExtension();
    QSettings settings("bg-remover", "bg-remover");
    QString startDir = settings.value("output/lastSaveDir").toString();
    if (startDir.isEmpty() || !QDir(startDir).exists()) {
        startDir = currentImagePath_.isEmpty()
            ? QDir::homePath()
            : QFileInfo(currentImagePath_).absolutePath();
    }
    QString path = QFileDialog::getSaveFileName(
        this, TR("Salvar imagem", "Save image"), startDir,
        saveFormatFilter());
    if (path.isEmpty()) return;
    const QString typedPath = path;
    const QString typedSuffix = QFileInfo(path).suffix();
    path = savePathWithExtension(
        path, extension,
        extension == "jpg" ? QStringList() << "jpeg"
                            : QStringList());
    if (path != typedPath && !typedSuffix.isEmpty()) {
        logEdit_->appendPlainText(
            "[INFO] " + TR(
                "Extensão ajustada para o formato escolhido: ",
                "Extension adjusted to the chosen format: ") + path);
    }

    const QByteArray fmt = extension.toUpper().toUtf8();
    if (!QImageWriter::supportedImageFormats().contains(
            extension.toLatin1())) {
        QMessageBox::warning(this, TR("Aviso", "Warning"),
            TR("O formato %1 não é suportado nesta instalação do Qt "
               "(no Linux instale o pacote "
               "'qt6-image-formats-plugins'). Use PNG.",
               "Format %1 is not supported by this Qt installation "
               "(on Linux install the 'qt6-image-formats-plugins' "
               "package). Use PNG instead.")
                .arg(extension.toUpper()));
        return;
    }

    ProcessingOptions saveOptions = currentProcessingOptions();
    // Formatos sem canal alfa precisam de fundo solido, senao o Qt
    // compoe sobre preto e o resultado surpreende.
    const bool needSolidBg = currentBackgroundColor().isValid()
        || !saveFormatIsAlphaCapable();
    if (needSolidBg && saveOptions.backgroundStyle == ProcessingOptions::BgTransparent) {
        saveOptions.backgroundStyle = ProcessingOptions::BgSolidColor;
        saveOptions.backgroundColor = Qt::white;
    }

    const QImage toSave = ImagePostProcessor::apply(
        resultImage_, saveOptions, originalImage_);

    if (!toSave.save(path, fmt.constData())) {
        QMessageBox::critical(this, TR("Erro", "Error"),
            TR("Não foi possível salvar:\n", "Could not save:\n") + path);
    } else {
        settings.setValue("output/lastSaveDir",
                          QFileInfo(path).absolutePath());
        QMessageBox::information(this, TR("OK", "OK"),
            TR("Salvo:\n", "Saved:\n") + path);
        logEdit_->appendPlainText(
            "[OK] " + TR("Imagem salva: ", "Image saved: ") + path);
    }
}

QString MainWindow::saveFormatFilter() const
{
    switch (saveFormatCombo_->currentIndex()) {
    case 1: return QStringLiteral("JPEG (*.jpg *.jpeg)");
    case 2: return QStringLiteral("WebP (*.webp)");
    default: return QStringLiteral("PNG (*.png)");
    }
}

QString MainWindow::saveFormatExtension() const
{
    switch (saveFormatCombo_->currentIndex()) {
    case 1: return QStringLiteral("jpg");
    case 2: return QStringLiteral("webp");
    default: return QStringLiteral("png");
    }
}

bool MainWindow::saveFormatIsAlphaCapable() const
{
    // PNG e WebP suportam canal alfa; JPEG nao.
    const int index = saveFormatCombo_->currentIndex();
    return index == 0 || index == 2;
}

void MainWindow::copyResult()
{
    if (isBusy_) return;
    if (resultImage_.isNull()) {
        QMessageBox::warning(this, TR("Aviso", "Warning"),
            TR("Remova o fundo primeiro.", "Remove the background first."));
        return;
    }
    ProcessingOptions copyOptions = currentProcessingOptions();
    const QImage toCopy = ImagePostProcessor::apply(
        resultImage_, copyOptions, originalImage_);

    QApplication::clipboard()->setImage(toCopy);
    logEdit_->appendPlainText(
        "[OK] " + TR("Resultado copiado para a area de transferencia.",
                     "Result copied to the clipboard."));
}

void MainWindow::onSaveFormatChanged(int index)
{
    // PNG e WebP guardam transparencia; JPEG funde sobre o fundo
    // escolhido, por isso o rotulo da combo muda de contexto.
    Q_UNUSED(index)
}

void MainWindow::checkForUpdate()
{
    // Uma vez por sessao, assincrono e silencioso: falha de rede nao
    // deve incomodar quem trabalha offline. A verificacao pode ser
    // desativada por variavel de ambiente ou QSettings; nao envia
    // imagens, apenas consulta a API publica de releases.
    if (qEnvironmentVariableIsSet("BG_REMOVER_NO_UPDATE_CHECK")) return;
    if (!QSettings("bg-remover", "bg-remover")
             .value("update/check", true).toBool()) {
        return;
    }
    if (updateNam_) return;
    updateNam_ = new QNetworkAccessManager(this);

    QNetworkRequest request(QUrl(QStringLiteral(
        "https://api.github.com/repos/MarielCarlosFrias/bg-remover"
        "/releases/latest")));
    request.setRawHeader("Accept", "application/vnd.github+json");
    request.setRawHeader("User-Agent",
                         QByteArray("bg-remover/") + BG_REMOVER_VERSION);
    request.setTransferTimeout(5000);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    QNetworkReply* reply = updateNam_->get(request);
    connect(reply, &QNetworkReply::finished, this,
            &MainWindow::onUpdateCheckFinished);
}

void MainWindow::onUpdateCheckFinished()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        // Offline ou rate limit: sigiloso.
        return;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
    if (doc.isObject()) {
        latestRemoteVersion_ =
            doc.object().value("tag_name").toString();
    }
    if (latestRemoteVersion_.isEmpty()) return;

    // Comparacao semantica simples (major.minor.patch), ignorando o "v".
    const QString remote = latestRemoteVersion_.startsWith('v')
        ? latestRemoteVersion_.mid(1) : latestRemoteVersion_;
    const QString local = QStringLiteral(BG_REMOVER_VERSION);
    auto parse = [](const QString& s) {
        const QStringList parts = s.split('.');
        QList<int> nums;
        for (const QString& p : parts) nums << p.toInt();
        while (nums.size() < 3) nums << 0;
        return nums;
    };
    const QList<int> r = parse(remote);
    const QList<int> l = parse(local);

    if (r.at(0) > l.at(0)
        || (r.at(0) == l.at(0) && r.at(1) > l.at(1))
        || (r.at(0) == l.at(0) && r.at(1) == l.at(1) && r.at(2) > l.at(2))) {
        showUpdateBanner(latestRemoteVersion_);
    }
}

void MainWindow::showUpdateBanner(const QString& version)
{
    if (version.isEmpty()) return;

    // Banner clicavel no topo do workspace. O texto tem de ser
    // rich text com <a href>; sem isso linkActivated nunca dispara.
    const QUrl releasesUrl(QStringLiteral(
        "https://github.com/MarielCarlosFrias/bg-remover/releases"));
    updateBanner_->setTextFormat(Qt::RichText);
    updateBanner_->setOpenExternalLinks(false);
    updateBanner_->setTextInteractionFlags(Qt::LinksAccessibleByMouse);
    updateBanner_->setText(
        TR("⬆ <a href=\"%1\" style=\"color:#102217;\">"
           "Nova versão %2 disponível — clique para abrir as releases"
           "</a>",
           "⬆ <a href=\"%1\" style=\"color:#102217;\">"
           "New version %2 available — click to open releases"
           "</a>")
            .arg(releasesUrl.toString(), version));
    updateBanner_->setCursor(Qt::PointingHandCursor);
    updateBanner_->setObjectName("updateBanner");
    updateBanner_->setWordWrap(true);

    // Inserido antes do divisor de preview (posicao 0 do workspace).
    if (auto* wl = workspaceLayout()) {
        if (wl->indexOf(updateBanner_) < 0)
            wl->insertWidget(0, updateBanner_);
    }

    // Evita ligacoes duplicadas se o banner for mostrado mais de uma vez.
    QObject::disconnect(updateBanner_, nullptr, this, nullptr);
    connect(updateBanner_, &QLabel::linkActivated, this,
            [releasesUrl](const QString&) {
        QDesktopServices::openUrl(releasesUrl);
    });
}

QVBoxLayout* MainWindow::workspaceLayout()
{
    // O workspace e o frame pai da barra de ferramentas de zoom.
    QWidget* w = originalScroll_;
    while (w && w->objectName() != QLatin1String("workspace"))
        w = w->parentWidget();
    return w ? qobject_cast<QVBoxLayout*>(w->layout()) : nullptr;
}

void MainWindow::selectFolder() {
    if (isBusy_) return;
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
    if (isBusy_) return;
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
    isBusy_ = busy;
    btnOpen_->setEnabled(!busy);
    btnRemove_->setEnabled(!busy);
    btnSave_->setEnabled(!busy);
    btnCopy_->setEnabled(!busy);
    btnSelectFolder_->setEnabled(!busy);
    btnSelectOutput_->setEnabled(!busy);
    btnLoadModel_->setEnabled(!busy);
    btnProcessBatch_->setEnabled(!busy);
    modelCombo_->setEnabled(!busy);
    modelPathEdit_->setEnabled(!busy);
    folderEdit_->setEnabled(!busy);
    outputDirEdit_->setEnabled(!busy);
    saveFormatCombo_->setEnabled(!busy);
    presetCombo_->setEnabled(!busy);
    bgColorCombo_->setEnabled(!busy);
    refineCheckBox_->setEnabled(!busy);
    noiseCheckBox_->setEnabled(!busy);
    noiseThresholdSpin_->setEnabled(!busy);
    stickerCheckBox_->setEnabled(!busy);
    shadowCheckBox_->setEnabled(!busy);
    brushModeCombo_->setEnabled(!busy);
    brushRadiusSpin_->setEnabled(!busy);
    btnAutoCrop_->setEnabled(!busy && !resultImage_.isNull());
    btnDragOut_->setEnabled(!busy && !resultImage_.isNull());
    // O canvas nao pode aceitar pinceladas enquanto a inferencia
    // decorre: as edicoes seriam substituidas pelo resultado novo.
    maskEditCanvas_->setEnabled(!busy);
    btnUndo_->setEnabled(!busy && maskEditCanvas_->canUndo());
    btnRedo_->setEnabled(!busy && maskEditCanvas_->canRedo());
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
    {
        QMutexLocker lock(&cache->mutex);
        if (cache->remover && cache->loadedPath == modelPath)
            return cache->remover;
    }

    // Carrega fora do lock: a GUI pode chamar resetModel() enquanto o
    // ONNX carrega (segundos) sem ficar bloqueada. Se outra thread
    // carregar o mesmo modelo entretanto, reutiliza-se essa sessao.
    const auto loaded = std::make_shared<BackgroundRemover>(modelPath);
    {
        QMutexLocker lock(&cache->mutex);
        if (cache->remover && cache->loadedPath == modelPath)
            return cache->remover;
        cache->remover = loaded;
        cache->loadedPath = modelPath;
    }
    return loaded;
}

void MainWindow::populateModelCombo()
{
    const QString current = modelPathEdit_->text();
    const QSignalBlocker blocker(modelCombo_);
    modelCombo_->clear();

    // So lista modelos que existem no disco; antes os dois primeiros
    // apareciam sempre e falhavam na inferencia quando em falta.
    const auto addInstalled = [this](const QString& fileName,
                                     const QString& label) {
        const QString path = ModelPathResolver::resolve(fileName);
        if (QFile::exists(path))
            modelCombo_->addItem(label, path);
    };
    addInstalled("u2netp.onnx",
                 TR("U\u00b2-Netp \u2014 r\u00e1pido (~5 MB)",
                    "U\u00b2-Netp \u2014 fast (~5 MB)"));
    addInstalled("u2net.onnx",
                 TR("U\u00b2-Net  \u2014 qualidade (~176 MB)",
                    "U\u00b2-Net  \u2014 quality (~176 MB)"));

    // Modelos descarregados na aba "Modelos". Inclui os caminhos
    // antigos (GUI/CLI anteriores) para nao "desaparecerem" modelos
    // ja instalados; o download novo usa o caminho canonico.
    const auto addModelsFromDir = [this](const QDir& dir) {
        const QStringList onnx = dir.entryList(
            QStringList() << "*.onnx", QDir::Files, QDir::Name);
        for (const QString& f : onnx) {
            bool alreadyListed = false;
            for (int i = 0; i < modelCombo_->count(); ++i) {
                const QString listed =
                    QFileInfo(modelCombo_->itemData(i).toString())
                        .fileName();
                if (listed.compare(f, Qt::CaseInsensitive) == 0) {
                    alreadyListed = true;
                    break;
                }
            }
            if (!alreadyListed)
                modelCombo_->addItem(f, dir.absoluteFilePath(f));
        }
    };
    addModelsFromDir(QDir(ModelPathResolver::userModelsDir()));
    for (const QString& legacy : ModelPathResolver::legacyUserModelsDirs())
        addModelsFromDir(QDir(legacy));

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
    {
        const QSignalBlocker blocker(saveFormatCombo_);
        const int fmt = settings.value("output/saveFormat", 0).toInt();
        if (fmt >= 0 && fmt < saveFormatCombo_->count())
            saveFormatCombo_->setCurrentIndex(fmt);
    }

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
    settings.setValue("output/saveFormat",
                     saveFormatCombo_->currentIndex());
    if (!modelPathEdit_->text().isEmpty())
        settings.setValue("model/path", modelPathEdit_->text());
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    saveSettings();

    // Cancelar o lote, mas deixar o passo em curso terminar: o
    // watcher ainda esta a usar o modelo e a imagem.
    if (batchRunning_)
        batchCancelRequested_ = true;

    if (removeWatcher_.isRunning() || batchWatcher_.isRunning()) {
        if (!closeRequested_) {
            closeRequested_ = true;
            logEdit_->appendPlainText("[INFO] " + TR(
                "A terminar o processamento antes de fechar...",
                "Finishing processing before closing..."));
        }
        event->ignore();
        return;
    }

    closeRequested_ = false;
    QMainWindow::closeEvent(event);
}

void MainWindow::maybeCloseAfterJobs()
{
    if (!closeRequested_) return;
    if (removeWatcher_.isRunning() || batchWatcher_.isRunning()) return;

    // O close() volta a entrar em closeEvent(); sem jobs pendentes
    // a janela fecha normalmente.
    closeRequested_ = false;
    QTimer::singleShot(0, this, [this]() { close(); });
}

void MainWindow::dragEnterEvent(QDragEnterEvent* event)
{
    // Aceitar se qualquer um dos URLs for pasta ou imagem; antes so
    // o primeiro era considerado.
    for (const QUrl& url : event->mimeData()->urls()) {
        const QFileInfo fi(url.toLocalFile());
        if (fi.isDir()
            || BatchPlan::imageExts().contains(fi.suffix().toLower())) {
            event->acceptProposedAction();
            return;
        }
    }
}

void MainWindow::dropEvent(QDropEvent* event)
{
    if (isBusy_) return;
    const QList<QUrl> urls = event->mimeData()->urls();
    if (urls.isEmpty()) return;

    QString folder;
    QString image;
    for (const QUrl& url : urls) {
        const QFileInfo fi(url.toLocalFile());
        if (folder.isEmpty() && fi.isDir())
            folder = fi.absoluteFilePath();
        else if (image.isEmpty()
                 && BatchPlan::imageExts().contains(
                        fi.suffix().toLower()))
            image = fi.absoluteFilePath();
    }

    if (!folder.isEmpty()) {
        folderEdit_->setText(folder);
        logEdit_->appendPlainText(
            "[INFO] " + TR("Pasta de lote: ", "Batch folder: ") + folder);
        event->acceptProposedAction();
        return;
    }
    if (!image.isEmpty()) {
        openPath(image);
        if (urls.size() > 1) {
            logEdit_->appendPlainText(
                "[INFO] " + TR("Vários itens arrastados; a abrir o primeiro.",
                               "Multiple items dropped; opening the first."));
        }
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
    if (isBusy_ || removeWatcher_.isRunning()
        || batchWatcher_.isRunning()) return;

    if (folderEdit_->text().isEmpty()) {
        QMessageBox::warning(this, TR("Aviso", "Warning"),
            TR("Selecione uma pasta primeiro.",
               "Select a folder first."));
        return;
    }
    batchFolder_ = folderEdit_->text();
    batchOutputDir_ = outputDirEdit_->text();

    // O formato escolhido no painel tambem vale para o lote. Se o
    // plugin nao existir (ex.: WebP no Linux), avisar antes de
    // comecar em vez de falhar imagem a imagem.
    batchOutputExtension_ = saveFormatExtension();
    if (!QImageWriter::supportedImageFormats().contains(
            batchOutputExtension_.toLatin1())) {
        QMessageBox::warning(this, TR("Aviso", "Warning"),
            TR("O formato %1 não é suportado nesta instalação do Qt "
               "(no Linux instale o pacote "
               "'qt6-image-formats-plugins').",
               "Format %1 is not supported by this Qt installation "
               "(on Linux install the 'qt6-image-formats-plugins' "
               "package).")
                .arg(batchOutputExtension_.toUpper()));
        return;
    }

    // Planejamento compartilhado com o CLI (BatchPlan): filtragem
    // case-insensitive e saidas que nao sobrescrevem ficheiros.
    batchPlan_ = BatchPlan::build(
        QDir(batchFolder_), QDir(batchOutputDir_),
        batchOutputExtension_);
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
    batchOptions_.outputFormat = saveFormatExtension();
    // JPEG nao tem canal alfa: compor sobre branco quando o fundo
    // escolhido e transparente. PNG/WebP mantem o alfa.
    if (!saveFormatIsAlphaCapable()
        && !batchOptions_.backgroundColor.isValid()) {
        batchOptions_.backgroundColor = Qt::white;
    }

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
    const QString errSave = TR("Falha ao salvar a imagem",
                               "Failed to save the image");
    const QString errModel = TR("modelo indisponível",
                                "model unavailable");
    const QString errOnnx = TR("Erro ONNX: ", "ONNX error: ");
    const QString errUnknown = TR("Erro desconhecido ao processar a imagem",
                                  "Unknown error processing the image");

    // Cada imagem corre na thread de trabalho; a GUI so recebe o
    // resultado via watcher e agenda o passo seguinte.
    batchWatcher_.setFuture(QtConcurrent::run(
        [cache = modelCache_, options = batchOptions_,
         in = entry.inputPath, out = entry.outputPath,
         format = batchOutputExtension_,
         errLoad, errSave, errModel, errOnnx, errUnknown]() -> QString {
            try {
                // setAutoTransform respeita a orientacao EXIF, como no
                // CLI; sem isto fotos de telemovel saiam rodadas no lote.
                QImageReader reader(in);
                reader.setAutoTransform(true);
                const QImage img = reader.read();
                if (img.isNull())
                    return errLoad;
                const std::shared_ptr<BackgroundRemover> r =
                    obtainRemover(cache, options.modelPath);
                if (!r) return errModel;
                const QImage processed = ImagePostProcessor::apply(
                    r->removeBackground(img), options);
                if (!processed.save(out, format.toUtf8().constData()))
                    return errSave;
                return QString();
            } catch (const Ort::Exception& e) {
                return errOnnx + QString::fromUtf8(e.what());
            } catch (const std::exception& e) {
                return QString::fromUtf8(e.what());
            } catch (...) {
                return errUnknown;
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
        logEdit_->appendPlainText("[ERROR] " + file + ": " + err);
    }
    ++batchIndex_;
    progressBar_->setValue(batchIndex_);
    // Durante o fecho/cancelamento nao agendar mais imagens; fecha o
    // lote e deixa maybeCloseAfterJobs() concluir o fecho.
    if (batchCancelRequested_ || closeRequested_)
        finishBatch();
    else
        QTimer::singleShot(0, this, &MainWindow::processBatchStep);
}

void MainWindow::finishBatch()
{
    batchRunning_ = false;
    progressBar_->setValue(batchPlan_.size());
    progressBar_->setVisible(false);
    logEdit_->appendPlainText(
        QString("[BATCH] %1: %2 OK, %3 erro(s).")
            .arg(batchCancelRequested_
                     ? TR("Cancelado", "Cancelled")
                     : TR("Concluido", "Completed"))
            .arg(batchOk_).arg(batchErrors_));
    btnProcessBatch_->setText(TR("Processar lote", "Process batch"));
    setUiBusy(false);
    batchCancelRequested_ = false;
    emit jobFinished();
    maybeCloseAfterJobs();
}

void MainWindow::setOriginal(const QImage& img) {
    Q_UNUSED(img)
    updatePreviewZoom();
}

void MainWindow::setResult(const QImage& img) {
    Q_UNUSED(img)
    updatePreviewZoom();
}

void MainWindow::pasteImageFromClipboard()
{
    if (isBusy_) return;
    const QClipboard* clipboard = QApplication::clipboard();
    const QImage img = clipboard->image();
    if (!img.isNull()) {
        ++imageGeneration_;
        resultImage_ = QImage();
        btnAutoCrop_->setEnabled(false);
        btnDragOut_->setEnabled(false);
        originalImage_ = img;
        currentImagePath_.clear();
        imageNameLabel_->setText(TR("Colada da área de transferência",
                                    "Pasted from clipboard"));
        maskEditCanvas_->clearImage();
        maskEditCanvas_->setCompareImage(img);
        setOriginal(img);
        logEdit_->appendPlainText("[INFO] " + TR(
            "Imagem colada da área de transferência.",
            "Image pasted from clipboard."));
        return;
    }

    const QMimeData* mime = clipboard->mimeData();
    if (mime && mime->hasUrls()) {
        for (const QUrl& url : mime->urls()) {
            const QString localFile = url.toLocalFile();
            const QFileInfo fi(localFile);
            if (BatchPlan::imageExts().contains(fi.suffix().toLower())) {
                openPath(localFile);
                return;
            }
        }
    }

    QMessageBox::information(this, TR("Área de transferência", "Clipboard"),
        TR("Nenhuma imagem encontrada na área de transferência.",
           "No image found in the clipboard."));
}

void MainWindow::autoCropResult()
{
    if (isBusy_ || resultImage_.isNull()) return;

    const QImage cropped = ImagePostProcessor::autoCrop(resultImage_, 2);
    if (cropped.size() == resultImage_.size()) {
        logEdit_->appendPlainText("[INFO] " + TR("Nenhuma borda vazia para recortar.",
                                                "No empty border to crop."));
        return;
    }

    resultImage_ = cropped;
    maskEditCanvas_->setImage(resultImage_, currentBackgroundColor());
    setResult(resultImage_);
    logEdit_->appendPlainText(QString("[OK] %1: %2x%3")
        .arg(TR("Auto-recorte aplicado", "Auto-crop applied"))
        .arg(resultImage_.width())
        .arg(resultImage_.height()));
}

void MainWindow::presetChanged(int index)
{
    const QSignalBlocker bBg(bgColorCombo_);
    const QSignalBlocker bSticker(stickerCheckBox_);
    const QSignalBlocker bShadow(shadowCheckBox_);

    switch (index) {
    case 1: // Padrão (Transparente)
        bgColorCombo_->setCurrentIndex(0);
        stickerCheckBox_->setChecked(false);
        shadowCheckBox_->setChecked(false);
        break;
    case 2: // E-commerce (Branco + Sombra)
        bgColorCombo_->setCurrentIndex(1); // Branco
        stickerCheckBox_->setChecked(false);
        shadowCheckBox_->setChecked(true);
        break;
    case 3: // Sticker (Contorno Branco)
        bgColorCombo_->setCurrentIndex(0); // Transparente
        stickerCheckBox_->setChecked(true);
        shadowCheckBox_->setChecked(false);
        break;
    case 4: // Retrato (Fundo Desfocado)
        bgColorCombo_->setCurrentIndex(4); // Desfoque original
        stickerCheckBox_->setChecked(false);
        shadowCheckBox_->setChecked(false);
        break;
    default:
        break;
    }

    if (!resultImage_.isNull()) {
        maskEditCanvas_->setBackgroundColor(currentBackgroundColor());
    }
}

void MainWindow::startDragOut()
{
    if (resultImage_.isNull()) return;

    ProcessingOptions dragOptions = currentProcessingOptions();
    if (!saveFormatIsAlphaCapable()
        && dragOptions.backgroundStyle == ProcessingOptions::BgTransparent) {
        dragOptions.backgroundStyle = ProcessingOptions::BgSolidColor;
        dragOptions.backgroundColor = Qt::white;
    }
    const QImage finalImage = ImagePostProcessor::apply(
        resultImage_, dragOptions, originalImage_);

    const QString tempDir = QDir::tempPath();
    const QString baseName = currentImagePath_.isEmpty()
        ? QStringLiteral("bg_removed")
        : QFileInfo(currentImagePath_).completeBaseName() + QStringLiteral("_rembg");
    const QString tempPath = QDir(tempDir).filePath(baseName + QStringLiteral(".png"));
    finalImage.save(tempPath, "PNG");

    auto* drag = new QDrag(this);
    auto* mimeData = new QMimeData;
    mimeData->setImageData(finalImage);
    mimeData->setUrls({QUrl::fromLocalFile(tempPath)});
    drag->setMimeData(mimeData);

    const QPixmap preview = QPixmap::fromImage(
        finalImage.scaled(128, 128, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    drag->setPixmap(preview);
    drag->setHotSpot(QPoint(preview.width() / 2, preview.height() / 2));

    drag->exec(Qt::CopyAction);
}
