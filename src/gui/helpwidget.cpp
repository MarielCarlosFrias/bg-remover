#include "helpwidget.h"
#include "i18n.h"
#include <QApplication>
#include <QPalette>

namespace {

// Mistura a cor a em direcao a b no fator t (0..1).
QString blend(const QColor& a, const QColor& b, float t)
{
    const QColor c(
        int(a.red()   * (1 - t) + b.red()   * t),
        int(a.green() * (1 - t) + b.green() * t),
        int(a.blue()  * (1 - t) + b.blue()  * t));
    return c.name();
}

} // namespace

HelpWidget::HelpWidget(QWidget* parent)
    : QTextBrowser(parent)
{
    setOpenExternalLinks(true);
    setObjectName("helpView");
    setFrameShape(QFrame::NoFrame);

#ifdef BG_REMOVER_VERSION
    const QString version = BG_REMOVER_VERSION;
#else
    const QString version = QStringLiteral("1.0.0");
#endif
    Q_UNUSED(version);

    // Acompanha o tema escuro/claro em tempo real: mudancas de
    // paleta da aplicacao chegam como QEvent::ApplicationPaletteChange.
    rebuildHtml();
}

bool HelpWidget::event(QEvent* event)
{
    if (event->type() == QEvent::ApplicationPaletteChange) {
        rebuildHtml();
    }
    return QTextBrowser::event(event);
}

void HelpWidget::rebuildHtml()
{
#ifdef BG_REMOVER_VERSION
    const QString version = BG_REMOVER_VERSION;
#else
    const QString version = QStringLiteral("1.0.0");
#endif

    // Repositorio do projeto (ajuste aqui se o repositorio mudar).
    const QString repo =
        QStringLiteral("https://github.com/MarielCarlosFrias/bg-remover");
    const QString issues = repo + QStringLiteral("/issues");
    const QString profile =
        QStringLiteral("https://github.com/MarielCarlosFrias");

    // Cores derivadas da paleta ativa (tema escuro ou claro).
    const QPalette pal = palette();
    const QColor base = pal.color(QPalette::Base);
    const QColor text = pal.color(QPalette::WindowText);
    const bool dark = base.lightness() < 128;
    const QString card = dark
        ? blend(base, QColor(255, 255, 255), 0.07f)
        : blend(base, QColor(0, 0, 0), 0.045f);
    const QString muted = blend(base, text, 0.60f);
    const QString chip = card;
    const QString green = "#36c66a";
    const QString greenDark = "#2f9e52";
    const QString onGreen = "#102217";

    auto cardCell = [&](const QString& emoji, const QString& num,
                        const QString& title, const QString& desc) {
        return QString(
                   "<td width=\"50%\" bgcolor=\"%1\" style=\"border-radius:6px; padding:12px;\">"
                   "<span style=\"font-size:22px\">%2</span> "
                   "<b>%3 %4</b><br>"
                   "<span style=\"color:%5\">%6</span></td>")
            .arg(card, emoji, num, title, muted, desc);
    };

    auto shortcutRow = [&](const QString& k1, const QString& a1,
                           const QString& k2, const QString& a2) {
        return QString(
                   "<tr>"
                   "<td bgcolor=\"%1\" style=\"padding:6px 10px;\"><b><font color=\"%2\">%3</font></b></td>"
                   "<td style=\"padding:6px 10px;\"><span style=\"color:%4\">%5</span></td>"
                   "<td bgcolor=\"%1\" style=\"padding:6px 10px;\"><b><font color=\"%2\">%6</font></b></td>"
                   "<td style=\"padding:6px 10px;\"><span style=\"color:%4\">%7</span></td>"
                   "</tr>")
            .arg(chip, green, k1, muted, a1, k2, a2);
    };

    QString html;

    // Cabeçalho e garantia de privacidade
    html += QString(
        "<h1>Aletheia %1</h1>"
        "<table width=\"100%\" cellpadding=\"12\" cellspacing=\"0\" style=\"border-radius:6px;\">"
        "<tr><td bgcolor=\"%2\" style=\"border-radius:6px;\"><b><span style=\"font-size:14px; color:%3\">"
        "\U0001F512 %4"
        "</span></b></td></tr></table><br>")
        .arg(version, green, onGreen,
             TR("100% Local e Privado \u2014 suas imagens são processadas no seu computador e nunca vão para a nuvem.",
                "100% Local and Private \u2014 your images are processed on your computer and never sent to the cloud."));

    // Guia Rápido de 4 Passos
    html += QString("<h3>\U0001F680 %1</h3>").arg(TR("Fluxo de Trabalho em 4 Passos", "Quick 4-Step Workflow"));
    html += QString(
        "<table width=\"100%\" cellpadding=\"10\" cellspacing=\"8\">"
        "<tr>%1%2</tr>"
        "<tr>%3%4</tr>"
        "</table><br>")
        .arg(
            cardCell("\U0001F5BC", "1.",
                     TR("Abrir ou Colar", "Open or Paste"),
                     TR("Arraste uma foto, use <b>Ctrl+O</b> ou cole direto com <b>Ctrl+V</b>.",
                        "Drag an image, press <b>Ctrl+O</b> or paste directly with <b>Ctrl+V</b>.")),
            cardCell("\u2699", "2.",
                     TR("Escolha o Modo", "Choose the Mode"),
                     TR("<b>IA</b> para fotos e pessoas; <b>Cor (Chroma Key)</b> para Pixel Art e Sprites.",
                        "<b>AI</b> for photos and people; <b>Color (Chroma Key)</b> for Pixel Art and Sprites.")),
            cardCell("\u2702", "3.",
                     TR("Ajuste e Efeitos", "Touch Up & Effects"),
                     TR("Use o Pincel, Auto-Recorte, Contorno de Adesivo ou Sombra Projetada.",
                        "Use the Brush, Auto-Crop, Sticker Outline or Drop Shadow.")),
            cardCell("\U0001F4BE", "4.",
                     TR("Salvar ou Arrastar", "Save or Drag-Out"),
                     TR("Salve em PNG/WebP, copie ou <b>arraste o resultado</b> direto para o Photoshop/Discord.",
                        "Save to PNG/WebP, copy or <b>drag the result</b> directly to Photoshop/Discord.")));

    // Dicas de Modos: IA vs Chroma Key (Pixel Art)
    html += QString("<h3>\U0001F4A1 %1</h3>").arg(TR("Qual Modo de Remoção Escolher?", "Which Removal Mode to Choose?"));
    html += QString(
        "<table width=\"100%\" cellpadding=\"12\" cellspacing=\"8\">"
        "<tr>"
        "<td width=\"50%\" bgcolor=\"%1\" style=\"border-radius:6px;\">"
        "<b>\U0001F9E0 %2</b><br>"
        "<span style=\"color:%3\">%4</span>"
        "</td>"
        "<td width=\"50%\" bgcolor=\"%1\" style=\"border-radius:6px;\">"
        "<b>\U0001F3A8 %5</b><br>"
        "<span style=\"color:%3\">%6</span>"
        "</td>"
        "</tr>"
        "</table><br>")
        .arg(card)
        .arg(TR("Modo IA (Rede Neural)", "AI Mode (Neural Network)"))
        .arg(muted)
        .arg(TR("Ideal para fotos reais, produtos, pessoas e animais. O modelo analisa formas complexas e cabelos com suavização inteligente. Modelos de alta precisão (como <i>BRIA RMBG</i> e <i>BiRefNet</i>) estão disponíveis para download na aba <b>Modelos</b>.",
                "Ideal for real photos, products, people and pets. The neural network analyzes complex shapes and hair with smart smoothing. High-precision models (such as <i>BRIA RMBG</i> and <i>BiRefNet</i>) are available in the <b>Models</b> tab."))
        .arg(TR("Modo Cor / Chroma Key (Pixel Art)", "Color Mode / Chroma Key (Pixel Art)"))
        .arg(TR("Essencial para <b>Pixel Art</b>, sprites de jogos (ex: fundo magenta #FF00FF), logos e ícones com fundo liso. Detecta a cor dos cantos automaticamente, preserva bordas duras 100% nítidas pixel a pixel e executa em <b>menos de 2ms</b>.",
                "Essential for <b>Pixel Art</b>, game sprites (e.g. magenta #FF00FF backgrounds), logos and flat backgrounds. Automatically detects corner colors, preserves 100% crisp pixel edges and runs in <b>under 2ms</b>."));

    // Recursos Especiais
    html += QString("<h3>\u2728 %1</h3>").arg(TR("Recursos e Ferramentas Especiais", "Special Tools & Features"));
    html += QString(
        "<table width=\"100%\" cellpadding=\"12\" cellspacing=\"8\">"
        "<tr>"
        "<td width=\"33%\" bgcolor=\"%1\" style=\"border-radius:6px;\">"
        "<b>\u2704 %2</b><br>"
        "<span style=\"color:%3\">%4</span>"
        "</td>"
        "<td width=\"33%\" bgcolor=\"%1\" style=\"border-radius:6px;\">"
        "<b>\U0001F4CF %5</b><br>"
        "<span style=\"color:%3\">%6</span>"
        "</td>"
        "<td width=\"33%\" bgcolor=\"%1\" style=\"border-radius:6px;\">"
        "<b>\U0001F504 %7</b><br>"
        "<span style=\"color:%3\">%8</span>"
        "</td>"
        "</tr>"
        "</table><br>")
        .arg(card)
        .arg(TR("Auto-Recorte", "Auto-Crop"))
        .arg(muted)
        .arg(TR("Remove as margens transparentes vazias ao redor do objeto, ajustando a imagem exatamente ao conteúdo.",
                "Crops empty transparent margins around the subject, fitting the canvas perfectly."))
        .arg(TR("Arrastar Resultado", "Drag Result"))
        .arg(TR("Clique e segure no botão para soltar o arquivo PNG diretamente em mensageiros, navegadores ou pastas.",
                "Click and hold the button to drag-and-drop the PNG file directly into apps or folders."))
        .arg(TR("Modo Comparar", "Compare Mode"))
        .arg(TR("Ative a barra 'Comparar' para arrastar o divisor interativo e inspecionar o antes e depois em tempo real.",
                "Toggle the 'Compare' mode to slide the interactive divider and see before/after side by side."));

    // Tabela Completa de Atalhos de Teclado
    html += QString("<h3>\u2328 %1</h3>").arg(TR("Atalhos de Teclado", "Keyboard Shortcuts"));
    html += QString("<table cellpadding=\"7\" cellspacing=\"6\">%1%2%3%4%5</table><br>")
        .arg(
            shortcutRow("Ctrl+V", TR("colar imagem do clipboard", "paste image from clipboard"),
                        "Ctrl+O", TR("abrir arquivo de imagem", "open image file")),
            shortcutRow("Ctrl+S", TR("salvar imagem resultante", "save result image"),
                        "Ctrl+Shift+C", TR("copiar resultado", "copy result to clipboard")),
            shortcutRow("Ctrl+Z / Ctrl+Y", TR("desfazer / refazer pincel", "undo / redo brush edit"),
                        "Ctrl + " + TR("roda", "wheel"), TR("zoom no ponto do cursor", "zoom centered at cursor")),
            shortcutRow("Ctrl + / Ctrl \u2212", TR("aumentar / diminuir zoom", "zoom in / zoom out"),
                        "Ctrl+0", TR("ajustar zoom à tela (100%)", "fit zoom to screen (100%)")),
            shortcutRow("Ctrl+Q", TR("sair do aplicativo", "quit application"), "", ""));

    // Lote e CLI
    html += QString("<h3>\U0001F4E6 %1</h3>").arg(TR("Processamento em Lote & Terminal (CLI)", "Batch Processing & CLI"));
    html += QString(
        "<table width=\"100%\" cellpadding=\"12\" cellspacing=\"8\">"
        "<tr>"
        "<td width=\"100%\" bgcolor=\"%1\" style=\"border-radius:6px;\">"
        "<span style=\"color:%2\">"
        "%3<br><br>"
        "<b>%4:</b><br>"
        "<code>aletheia_cli ~/imagens -o ~/saida -m u2net.onnx -f png</code><br>"
        "<code>aletheia_cli --help</code>"
        "</span></td>"
        "</tr></table><br>")
        .arg(card, muted,
             TR("A barra inferior do aplicativo permite converter pastas inteiras com centenas de fotos de uma só vez. Para automação em servidores ou scripts, o utilitário de linha de comando oferece o mesmo motor sem interface gráfica.",
                "The bottom bar allows converting entire folders with hundreds of images at once. For automation in servers or scripts, the CLI utility provides the exact same engine without a GUI."),
             TR("Exemplo de comando CLI", "CLI command example"));

    // Rodapé de Links & Créditos
    html += QString(
        "<table cellpadding=\"10\" cellspacing=\"8\"><tr>"
        "<td bgcolor=\"%1\" style=\"border-radius:6px;\"><a href=\"%2\" style=\"color:%3; text-decoration:none;\">"
        "<b>\u2b50 %4</b></a></td>"
        "<td bgcolor=\"%5\" style=\"border-radius:6px;\"><a href=\"%6\" style=\"color:%3; text-decoration:none;\">"
        "<b>\U0001F41E %7</b></a></td>"
        "<td><span style=\"color:%8\">%9 <a href=\"%10\" style=\"color:%8; font-weight:bold;\">Mariel Carlos Frias</a></span></td>"
        "</tr></table>"
        "<p><span style=\"color:%8; font-size:small;\">"
        "%11 <a href=\"https://github.com/danielgatis/rembg\" style=\"color:%8;\">rembg</a> \u00b7 "
        "ONNX Runtime \u00b7 Qt 6</span></p>")
        .arg(green, repo, onGreen)
        .arg(TR("Reposit\u00f3rio no GitHub", "GitHub Repository"))
        .arg(greenDark, issues)
        .arg(TR("Reportar Problema", "Report an Issue"))
        .arg(muted)
        .arg(TR("por", "by"))
        .arg(profile)
        .arg(TR("Modelos U\u00b2-Net / IS-Net / BiRefNet \u00b7 Arquivos ONNX por ",
                "U\u00b2-Net / IS-Net / BiRefNet models \u00b7 ONNX files by "));

    setHtml(html);
}
