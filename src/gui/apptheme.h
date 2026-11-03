#ifndef APPTHEME_H
#define APPTHEME_H

#include <QColor>
#include <QString>

class QApplication;

// Central de temas do app: paletas Fusion + folha de estilo (QSS)
// para os modos escuro e claro, com persistencia via QSettings.
namespace AppTheme {

struct Palette {
    QColor bg;             // fundo da janela
    QColor surface;        // cards / paineis
    QColor surfaceHover;   // hover de cards e botoes
    QColor border;
    QColor text;
    QColor textMuted;
    QColor inputBg;
    QColor accent;
    QColor accentHover;
    QColor accentPressed;
    QColor accentText;
    QColor scrollHandle;
    QColor scrollHandleHover;
    QColor logBg;
};

Palette palette(bool dark);
QString stylesheet(bool dark);

// Aplica o tema (estilo Fusion + paleta + QSS) e re-polish de todos
// os widgets. Persiste a escolha em QSettings.
void apply(bool dark);

// Tema atual: env BG_REMOVER_THEME=dark|light tem prioridade
// (util para testes/snapshots); caso contrario, QSettings.
bool isDark();

// Inverte o tema, aplica e persiste. Retorna o novo estado (true = escuro).
bool toggle();

} // namespace AppTheme

#endif
