#include "helpwidget.h"
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
        "\"><b><span style=\"font-size:14px; color:%1\">"
        "\U0001F512 100%% local e privado \u2014 as suas imagens nunca "
        "saem do computador.</span></b></td></tr></table><br>")
        .arg(onGreen);

    html += QStringLiteral(
        "<table width=\"100%\" cellpadding=\"12\" cellspacing=\"8\">"
        "<tr>%1%2</tr>"
        "<tr>%3%4</tr>"
        "</table><br>")
        .arg(
            cardCell("\U0001F5BC", "1.", "Abra a imagem",
                     "Ctrl+O ou arraste para a janela"),
            cardCell("\u2702", "2.", "Remova o fundo",
                     "um clique no bot\u00e3o verde"),
            cardCell("\U0001F58C", "3.", "Ajuste detalhes",
                     "pincel Restaurar/Apagar + Ctrl+Z"),
            cardCell("\U0001F4BE", "4.", "Salve o PNG",
                     "fundo transparente ou colorido"));

    html += QStringLiteral(
        "<h3>\u2328 Atalhos</h3>"
        "<table cellpadding=\"7\" cellspacing=\"6\">"
        "%1%2%3%4"
        "</table><br>")
        .arg(
            shortcutRow("Ctrl+O", "abrir imagem",
                        "Ctrl+S", "salvar PNG"),
            shortcutRow("Ctrl+Z / Y", "desfazer / refazer",
                        "Ctrl + roda", "zoom no cursor"),
            shortcutRow("Ctrl + / \u2212", "zoom",
                        "Ctrl+0", "ajustar \u00e0 janela"),
            shortcutRow("Ctrl+Q", "sair", "", ""));

    html += QString(
                "<table width=\"100%\" cellpadding=\"12\" cellspacing=\"8\">"
                "<tr>"
                "<td width=\"50%\" bgcolor=\"%1\">"
                "<b>\u2b07 Modelos melhores</b><br>"
                "<span style=\"color:%2\">Silueta, ISNet, U\u00b2-Net na "
                "aba <b>Modelos</b> \u2014 um clique e ficam instalados "
                "para GUI e CLI.</span></td>"
                "<td width=\"50%\" bgcolor=\"%1\">"
                "<b>\U0001F4E6 Lote de pastas</b><br>"
                "<span style=\"color:%2\">Barra inferior processa pastas "
                "inteiras (com cancelar). CLI: "
                "<code>bg_remover_cli entrada saida modelo.onnx</code>"
                "</span></td>"
                "</tr></table><br>")
                .arg(card, muted);

    html += QString(
                "<table cellpadding=\"10\" cellspacing=\"8\"><tr>"
                "<td bgcolor=\"%1\"><a href=\"%2\" "
                "style=\"color:%3; text-decoration:none\">"
                "<b>\u2b50 Reposit\u00f3rio no GitHub</b></a></td>"
                "<td bgcolor=\"%4\"><a href=\"%5\" "
                "style=\"color:%3; text-decoration:none\">"
                "<b>\U0001F41E Reportar problema</b></a></td>"
                "<td><span style=\"color:%6\">por "
                "<a href=\"%7\" style=\"color:%6\">Mariel Carlos "
                "Frias</a></span></td>"
                "</tr></table>"
                "<p><span style=\"color:%6; font-size:small\">"
                "U\u00b2-Net (Xuebin Qin et al.) \u00b7 ONNX do "
                "<a href=\"https://github.com/danielgatis/rembg\" "
                "style=\"color:%6\">rembg</a> \u00b7 "
                "ONNX Runtime</span></p>")
                .arg(green, repo, onGreen, greenDark, issues,
                     muted, profile);

    setHtml(html);
}
