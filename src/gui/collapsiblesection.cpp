#include "collapsiblesection.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QStyle>

CollapsibleSection::CollapsibleSection(const QString& title,
                                       bool expanded,
                                       QWidget* parent)
    : QWidget(parent),
      content_(new QWidget(this)),
      expanded_(expanded)
{
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    // Cabecalho: um botao transparente que ocupa a linha toda.
    header_ = new QPushButton(this);
    header_->setObjectName("collapsibleHeader");
    header_->setCheckable(true);
    header_->setChecked(expanded);
    header_->setText(QString::fromUtf8("\u25be ") + title);
    header_->setStyleSheet(
        "QPushButton#collapsibleHeader {"
        "  background: transparent; border: 0; text-align: left;"
        "  padding: 7px 2px; font-weight: 600;"
        "}");
    outer->addWidget(header_);

    contentLayout_ = new QVBoxLayout(content_);
    contentLayout_->setContentsMargins(2, 4, 2, 8);
    contentLayout_->setSpacing(7);
    outer->addWidget(content_);

    content_->setVisible(expanded);

    connect(header_, &QPushButton::clicked, this,
            [this](bool checked) {
        setExpanded(checked);
    });

    if (!expanded) {
        header_->setText(QString::fromUtf8("\u25b8 ") + title);
    }
}

void CollapsibleSection::setExpanded(bool expanded)
{
    if (expanded_ == expanded) return;
    expanded_ = expanded;
    content_->setVisible(expanded);
    header_->setChecked(expanded);
    header_->blockSignals(true);
    header_->setChecked(expanded);
    header_->blockSignals(false);

    // O titulo guarda a seta; reescrever sem ela para poder trocar.
    const QString base = header_->text().mid(2);
    header_->setText((expanded ? QString::fromUtf8("\u25be ")
                                : QString::fromUtf8("\u25b8 ")) + base);

    emit expandedChanged(expanded);
}
