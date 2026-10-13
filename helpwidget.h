#ifndef HELPWIDGET_H
#define HELPWIDGET_H

#include <QTextBrowser>

// Aba "Ajuda": cartoes visuais com o resumo do programa, atalhos e
// links do projeto. O HTML e reconstruido com as cores da paleta
// ativa, por isso acompanha o tema escuro/claro automaticamente.
class HelpWidget : public QTextBrowser {
    Q_OBJECT
public:
    explicit HelpWidget(QWidget* parent = nullptr);

protected:
    bool event(QEvent* event) override;

private:
    void rebuildHtml();
};

#endif
