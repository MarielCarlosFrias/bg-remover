#ifndef COLLAPSIBLESECTION_H
#define COLLAPSIBLESECTION_H

#include <QPushButton>
#include <QWidget>
#include <QString>
#include <QVBoxLayout>

// Secao colapsavel para o inspector: um cabecalho clicavel que
// abre/fecha o conteudo. Reduz a sobrecarga visual do painel quando
// ha muitas opcoes (grupos de ajustes, correcao manual, etc.).
class CollapsibleSection : public QWidget {
    Q_OBJECT
public:
    // expanded=true comeca aberto (usar para a secao principal).
    explicit CollapsibleSection(const QString& title,
                                bool expanded = true,
                                QWidget* parent = nullptr);

    // Layout do conteudo (so visivel quando expandido).
    QVBoxLayout* contentLayout() const { return contentLayout_; }

    bool isExpanded() const { return expanded_; }

public slots:
    void setExpanded(bool expanded);

signals:
    void expandedChanged(bool expanded);

private:
    void updateToggleStyle();

    QPushButton* header_;
    QWidget* content_;
    QVBoxLayout* contentLayout_;
    bool expanded_;
};

#endif