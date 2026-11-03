// Teste de fumo da interface: constroi a janela real com o backend
// offscreen do Qt, abre imagens, verifica controlos/atalhos e corre uma
// inferencia completa quando u2netp.onnx esta disponivel.
//
// Corre com ctest (gui_test_pt/gui_test_en). O modelo e opcional: sem
// ele, o caso de inferencia valida a recuperacao da UI depois do erro.
#include <QApplication>
#include <QComboBox>
#include <QDir>
#include <QEventLoop>
#include <QFileInfo>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPixmap>
#include <QProgressBar>
#include <QPushButton>
#include <QSettings>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTimer>
#include <QWidget>
#include <cstdio>

#include "mainwindow.h"
#include "maskeditcanvas.h"

static int failures = 0;

#define CHECK(cond, msg) do { \
    if (!(cond)) { std::printf("FALHOU: %s\n", msg); ++failures; } \
    else { std::printf("OK: %s\n", msg); } \
} while (0)

static void spin()
{
    QCoreApplication::processEvents(QEventLoop::AllEvents, 100);
}

static QPushButton* zoomButton(MainWindow& window, const QString& text)
{
    const QList<QPushButton*> buttons =
        window.findChildren<QPushButton*>("zoomButton");
    for (QPushButton* button : buttons) {
        if (button->text() == text) return button;
    }
    return nullptr;
}

int main(int argc, char* argv[])
{
    // Preferencias isoladas num diretorio temporario: o teste nao pode
    // alterar a geometria/modelo/idioma da instalacao do utilizador.
    // XDG_CONFIG_HOME cobre o Linux; setPath cobre os outros sistemas.
    QTemporaryDir tmp;
    if (!tmp.isValid()) {
        std::printf("FALHOU: nao foi possivel criar o diretorio temporario\n");
        return 1;
    }
    qputenv("XDG_CONFIG_HOME", tmp.path().toUtf8());
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope,
                       tmp.path());

    QApplication app(argc, argv);
    app.setOrganizationName("bg-remover");
    app.setApplicationName("bg-remover-gui-test");

    // Nunca consultar a rede durante os testes.
    qputenv("BG_REMOVER_NO_UPDATE_CHECK", "1");

    const bool english = qgetenv("BG_REMOVER_LANG") == "en";
    const QString modelArg =
        argc > 1 ? QString::fromLocal8Bit(argv[1]) : QString();
    const bool hasModel =
        !modelArg.isEmpty() && QFileInfo::exists(modelArg);

    const QString firstPath = tmp.filePath("gui_test_first.png");
    const QString secondPath = tmp.filePath("gui_test_second.png");
    QImage first(160, 100, QImage::Format_ARGB32_Premultiplied);
    first.fill(QColor(180, 40, 40));
    QImage second(80, 60, QImage::Format_ARGB32_Premultiplied);
    second.fill(QColor(30, 120, 200));
    CHECK(first.save(firstPath), "imagem de teste 1 criada");
    CHECK(second.save(secondPath), "imagem de teste 2 criada");

    MainWindow window(nullptr, hasModel ? modelArg : QString());
    window.show();
    spin();

    // ---- 1. Estrutura principal ----
    auto* tabs = window.findChild<QTabWidget*>("mainTabs");
    CHECK(tabs != nullptr, "aba principal existe");
    CHECK(tabs && tabs->count() == 3, "tres abas: Editor, Modelos, Ajuda");
    CHECK(window.windowTitle().startsWith("BG Remover"),
          "titulo da janela contem BG Remover");

    // ---- 2. Idioma e seletor ----
    auto* lang = window.findChild<QComboBox*>("langCombo");
    CHECK(lang && lang->count() == 2, "seletor de idioma tem PT e EN");
    CHECK(lang && lang->currentIndex() == (english ? 1 : 0),
          "idioma inicial segue BG_REMOVER_LANG");
    CHECK(tabs && tabs->tabText(2) == (english ? "Help" : "Ajuda"),
          "aba Ajuda traduzida");

    // ---- 3. Formato de saida ----
    auto* format = window.findChild<QComboBox*>("saveFormatCombo");
    CHECK(format && format->count() == 3, "formatos PNG, JPEG e WebP");
    CHECK(format && format->currentIndex() == 0, "PNG e o formato inicial");
    if (format) {
        format->setCurrentIndex(1);
        spin();
        CHECK(format->currentText().contains("JPEG"),
              "selecao de JPEG atualiza a combo");
        format->setCurrentIndex(0);
    }

    // ---- 4. Abrir imagem ----
    auto* imageName = window.findChild<QLabel*>("imageName");
    auto* original = window.findChild<QLabel*>("originalPreview");
    auto* canvas = window.findChild<MaskEditCanvas*>("resultCanvas");
    CHECK(imageName && original && canvas,
          "preview original e canvas do resultado existem");
    window.openPath(firstPath);
    spin();
    CHECK(imageName
              && imageName->text() == QFileInfo(firstPath).fileName(),
          "nome da imagem aberta aparece na barra");
    if (original) {
        const QPixmap preview = original->pixmap();
        CHECK(!preview.isNull(), "preview original recebeu a imagem");
    }
    CHECK(canvas && canvas->getEditedImage().isNull(),
          "resultado antigo e limpo ao abrir imagem");

    // ---- 5. Trocar de imagem nao reutiliza o resultado anterior ----
    window.openPath(secondPath);
    spin();
    CHECK(imageName
              && imageName->text() == QFileInfo(secondPath).fileName(),
          "segunda imagem substitui a primeira");
    CHECK(canvas && canvas->getEditedImage().isNull(),
          "canvas continua vazio depois da troca");

    // ---- 6. Comparacao antes/depois ----
    auto* compare = window.findChild<QPushButton*>("viewModeActive");
    CHECK(compare != nullptr, "botao Comparar existe");
    if (compare && canvas) {
        compare->click();
        spin();
        CHECK(canvas->compareEnabled(), "modo Comparar ativa no canvas");
        compare->click();
        spin();
        CHECK(!canvas->compareEnabled(), "modo Comparar desativa no canvas");
    }

    // ---- 7. Zoom ----
    auto* zoomLabel = window.findChild<QLabel*>("zoomLabel");
    auto* fit = window.findChild<QPushButton*>("fitButton");
    QPushButton* zoomIn = zoomButton(window, "+");
    QPushButton* zoomOut = zoomButton(window, QString::fromUtf8("\u2212"));
    CHECK(zoomLabel && fit && zoomIn && zoomOut,
          "controlos de zoom existem");
    if (zoomLabel && fit && zoomIn && zoomOut) {
        zoomIn->click();
        spin();
        CHECK(zoomLabel->text() == "125%", "zoom + chega a 125%");
        fit->click();
        spin();
        CHECK(zoomLabel->text() == "100%", "Ajustar repoe 100%");
        zoomOut->click();
        spin();
        CHECK(zoomLabel->text() == "75%", "zoom - desce para 75%");
        fit->click();
    }

    // ---- 8. Modelo selecionado ----
    auto* modelCombo = window.findChild<QComboBox*>("modelCombo");
    auto* modelPath = window.findChild<QLineEdit*>("modelPath");
    CHECK(modelCombo && modelCombo->count() >= 1,
          "combo de modelos tem pelo menos a opcao personalizada");
    if (hasModel) {
        CHECK(modelPath && modelPath->text() == modelArg,
              "modelo indicado por argumento fica selecionado");
    } else {
        std::printf("SKIP: u2netp.onnx ausente; inferencia valida so o erro\n");
    }

    // ---- 9. Inferencia e estado ocupado ----
    auto* open = window.findChild<QPushButton*>("openButton");
    auto* remove = window.findChild<QPushButton*>("primaryAction");
    auto* progress = window.findChild<QProgressBar*>("progressBar");
    auto* batch = window.findChild<QPushButton*>("processBatchButton");
    auto* log = window.findChild<QPlainTextEdit*>("logEdit");
    CHECK(open && remove && progress && batch && log,
          "controlos de processamento existem");
    if (open && remove && progress && batch && log && canvas) {
        window.openPath(firstPath);
        spin();
        window.removeBackground();
        spin();
        CHECK(!open->isEnabled(), "inferencia desativa Abrir");
        CHECK(!batch->isEnabled(), "inferencia desativa Processar lote");
        CHECK(progress->isVisible(), "barra de progresso aparece");
        CHECK(!canvas->isEnabled(), "canvas bloqueado durante inferencia");

        bool finished = false;
        QEventLoop loop;
        QObject::connect(&window, &MainWindow::jobFinished, &loop,
                         [&finished, &loop]() {
            finished = true;
            loop.quit();
        });
        // Em caso de erro, a GUI mostra uma caixa modal; fecha-a para
        // o teste nao ficar bloqueado.
        QTimer modalGuard;
        QObject::connect(&modalGuard, &QTimer::timeout, []() {
            if (QWidget* modal = QApplication::activeModalWidget())
                modal->close();
        });
        modalGuard.start(100);
        QTimer::singleShot(180000, &loop, &QEventLoop::quit);
        loop.exec();
        modalGuard.stop();
        spin();

        CHECK(finished, "sinal jobFinished chega ao fim da inferencia");
        CHECK(open->isEnabled(), "Abrir volta a estar ativo");
        CHECK(!progress->isVisible(), "barra de progresso desaparece");
        CHECK(canvas->isEnabled(), "canvas volta a aceitar edicao");
        if (hasModel) {
            CHECK(!canvas->getEditedImage().isNull(),
                  "resultado da inferencia chega ao canvas");
            CHECK(log->toPlainText().contains(
                      english ? "Background removed." : "Fundo removido."),
                  "log confirma a remocao do fundo");
        } else {
            CHECK(log->toPlainText().contains("[ERROR]"),
                  "sem modelo, o erro fica registado no log");
        }
    }

    // ---- 10. Lote e salvamento ----
    auto* input = window.findChild<QLineEdit*>("batchInput");
    auto* output = window.findChild<QLineEdit*>("batchOutput");
    auto* save = window.findChild<QPushButton*>("saveButton");
    auto* copy = window.findChild<QPushButton*>("copyButton");
    CHECK(input && output, "campos do lote existem");
    CHECK(output && output->text().contains("_rembg"),
          "pasta de saida padrao do lote e _rembg");
    CHECK(save && copy, "botoes Salvar e Copiar existem");

    window.close();
    spin();

    std::printf(failures == 0
        ? "\nTODOS OS TESTES GUI PASSARAM\n"
        : "\n%d TESTE(S) GUI FALHARAM\n", failures);
    return failures == 0 ? 0 : 1;
}
