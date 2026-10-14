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

    // Cartao de passo: emoji + titulo + descricao curta.
    auto cardCell = [&](const QString& emoji, const QString& num,
                        const QString& title, const QString& desc) {
        return QString(
                   "<td width=\"50%\" bgcolor=\"%1\">"
                   "<span style=\"font-size:20px\">%2</span> "
                   "<b>%3 %4</b><br>"
                   "<span style=\"color:%5\">%6</span></td>")
            .arg(card, emoji, num, title, muted, desc);
    };

    // Atalho: tecla em destaque + acao.
    auto shortcutRow = [&](const QString& k1, const QString& a1,
                           const QString& k2, const QString& a2) {
        return QString(
                   "<tr>"
                   "<td bgcolor=\"%1\"><b>%2</b></td>"
                   "<td><span style=\"color:%3\">%4</span></td>"
                   "<td bgcolor=\"%1\"><b>%5</b></td>"
                   "<td><span style=\"color:%3\">%6</span></td>"
                   "</tr>")
            .arg(chip, k1, muted, a1, k2, a2);
    };

    QString html;
    html += QStringLiteral(
        "<h1>BG Remover %1</h1>"
        "<table width=\"100%\" cellpadding=\"12\" cellspacing=\"0\">"
        "<tr><td bgcolor=\"")
        .arg(version) + green + QStringLiteral(
        "\"><b><span style=\"font-size:14px; color:%1\">")
        .arg(onGreen)
        + TR(
        "\U0001F512 100% local e privado \u2014 as suas imagens nunca "
        "saem do computador.",
        "\U0001F512 100% local and private \u2014 your images never "
        "leave your computer.")
        + QStringLiteral("</span></b></td></tr></table><br>");

    html += QStringLiteral(
        "<table width=\"100%\" cellpadding=\"12\" cellspacing=\"8\">"
        "<tr>%1%2</tr>"
        "<tr>%3%4</tr>"
        "</table><br>")
        .arg(
            cardCell("\U0001F5BC", "1.",
                     TR("Abra a imagem", "Open the image"),
                     TR("Ctrl+O ou arraste para a janela",
                        "Ctrl+O or drag it to the window")),
            cardCell("\u2702", "2.",
                     TR("Remova o fundo", "Remove the background"),
                     TR("um clique no bot\u00e3o verde",
                        "one click on the green button")),
            cardCell("\U0001F58C", "3.",
                     TR("Ajuste detalhes", "Touch up details"),
                     TR("pincel Restaurar/Apagar + Ctrl+Z",
                        "Restore/Erase brush + Ctrl+Z")),
            cardCell("\U0001F4BE", "4.",
                     TR("Salve o PNG", "Save the PNG"),
                     TR("fundo transparente ou colorido",
                        "transparent or colored background")));

    html += QStringLiteral("<h3>\u2328 %1</h3>"
        "<table cellpadding=\"7\" cellspacing=\"6\">"
        "%2%3%4%5"
        "</table><br>")
        .arg(TR("Atalhos", "Shortcuts"))
        .arg(
            shortcutRow("Ctrl+O",
                        TR("abrir imagem", "open image"),
                        "Ctrl+S", TR("salvar PNG", "save PNG")),
            shortcutRow("Ctrl+Z / Y",
                        TR("desfazer / refazer", "undo / redo"),
                        "Ctrl + " + TR("roda", "wheel"),
                        TR("zoom no cursor", "zoom at cursor")),
            shortcutRow("Ctrl + / \u2212", TR("zoom", "zoom"),
                        "Ctrl+0",
                        TR("ajustar \u00e0 janela", "fit to window")),
            shortcutRow("Ctrl+Q", TR("sair", "quit"), "", ""));

    html += QString(
                "<table width=\"100%\" cellpadding=\"12\" cellspacing=\"8\">"
                "<tr>"
                "<td width=\"50%\" bgcolor=\"%1\">"
                "<b>\u2b07 %2</b><br>"
                "<span style=\"color:%3\">%4</span></td>"
                "<td width=\"50%\" bgcolor=\"%1\">"
                "<b>\U0001F4E6 %5</b><br>"
                "<span style=\"color:%3\">%6"
                "</span></td>"
                "</tr></table><br>")
                .arg(card)
                .arg(TR("Modelos melhores", "Better models"))
                .arg(muted)
                .arg(TR(
                    "Silueta, ISNet, U\u00b2-Net na aba <b>Modelos</b> "
                    "\u2014 um clique e ficam instalados para GUI e CLI.",
                    "Silueta, ISNet, U\u00b2-Net in the <b>Models</b> "
                    "tab \u2014 one click installs them for both the "
                    "GUI and the CLI."))
                .arg(TR("Lote de pastas", "Folder batch"))
                .arg(TR(
                    "Barra inferior processa pastas inteiras (com "
                    "cancelar). CLI: "
                    "<code>bg_remover_cli entrada saida modelo.onnx</code>",
                    "The bottom bar processes entire folders (with "
                    "cancel). CLI: "
                    "<code>bg_remover_cli input output model.onnx</code>"));

    html += QString(
                "<table cellpadding=\"10\" cellspacing=\"8\"><tr>"
                "<td bgcolor=\"%1\"><a href=\"%2\" "
                "style=\"color:%3; text-decoration:none\">"
                "<b>\u2b50 %4</b></a></td>"
                "<td bgcolor=\"%5\"><a href=\"%6\" "
                "style=\"color:%3; text-decoration:none\">"
                "<b>\U0001F41E %7</b></a></td>"
                "<td><span style=\"color:%8\">%9 "
                "<a href=\"%10\" style=\"color:%8\">Mariel Carlos "
                "Frias</a></span></td>"
                "</tr></table>"
                "<p><span style=\"color:%8; font-size:small\">"
                "%11 <a href=\"https://github.com/danielgatis/rembg\" "
                "style=\"color:%8\">rembg</a> \u00b7 "
                "ONNX Runtime</span></p>")
                .arg(green, repo, onGreen)
                .arg(TR("Reposit\u00f3rio no GitHub", "GitHub repository"))
                .arg(greenDark, issues)
                .arg(TR("Reportar problema", "Report an issue"))
                .arg(muted)
                .arg(TR("por", "by"))
                .arg(TR(
                    "U\u00b2-Net (Xuebin Qin et al.) \u00b7 ONNX do ",
                    "U\u00b2-Net (Xuebin Qin et al.) \u00b7 ONNX files by "));

    setHtml(html);
}
