#ifndef I18N_H
#define I18N_H

#include <QLocale>
#include <QSettings>
#include <QString>
#include <QtGlobal>

// Internacionalizacao leve, sem ferramentas externas: cada chamada
// traz o texto em PT-BR e em EN. O idioma ativo vem, nesta ordem, de
//   1. variavel de ambiente BG_REMOVER_LANG=pt|en (tests/CI)
//   2. QSettings (seletor de idioma na interface)
//   3. localidade do sistema (portugues -> PT-BR, senao EN)
namespace I18n {

enum class Lang { PtBr, En };

inline Lang lang()
{
    const QString env = qEnvironmentVariable("BG_REMOVER_LANG").toLower();
    if (env == "en") return Lang::En;
    if (env == "pt") return Lang::PtBr;

    QSettings settings("bg-remover", "bg-remover");
    const QString saved = settings.value("lang").toString();
    if (saved == "en") return Lang::En;
    if (saved == "pt") return Lang::PtBr;

    return QLocale::system().language() == QLocale::Portuguese
               ? Lang::PtBr : Lang::En;
}

inline QString tr(const char* ptBr, const char* en)
{
    return lang() == Lang::En ? QString::fromUtf8(en)
                              : QString::fromUtf8(ptBr);
}

} // namespace I18n

// Atalho para os pontos de chamada.
#define TR(ptBr, en) I18n::tr(ptBr, en)

#endif
