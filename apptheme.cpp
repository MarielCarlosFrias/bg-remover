#include "apptheme.h"

#include <QApplication>
#include <QPalette>
#include <QSettings>
#include <QStyleFactory>

namespace AppTheme {

Palette palette(bool dark)
{
    Palette p;
    if (dark) {
        p.bg                 = QColor(0x17, 0x1a, 0x1d);
        p.surface            = QColor(0x20, 0x25, 0x2a);
        p.surfaceHover       = QColor(0x38, 0x41, 0x49);
        p.border             = QColor(0x38, 0x40, 0x47);
        p.text               = QColor(0xed, 0xf2, 0xf7);
        p.textMuted          = QColor(0x9b, 0xa6, 0xaf);
        p.inputBg            = QColor(0x2a, 0x31, 0x37);
        p.accent             = QColor(0x36, 0xa8, 0x5c);
        p.accentHover        = QColor(0x42, 0xcf, 0x74);
        p.accentPressed      = QColor(0x2f, 0x9c, 0x52);
        p.accentText         = QColor(0x10, 0x22, 0x17);
        p.scrollHandle       = QColor(0x34, 0x3a, 0x40);
        p.scrollHandleHover  = QColor(0x46, 0x50, 0x58);
        p.logBg              = QColor(0x1b, 0x20, 0x24);
    } else {
        p.bg                 = QColor(0xf3, 0xf5, 0xf7);
        p.surface            = QColor(0xff, 0xff, 0xff);
        p.surfaceHover       = QColor(0xe9, 0xed, 0xf1);
        p.border             = QColor(0xd1, 0xd7, 0xdd);
        p.text               = QColor(0x1f, 0x24, 0x29);
        p.textMuted          = QColor(0x5c, 0x66, 0x70);
        p.inputBg            = QColor(0xff, 0xff, 0xff);
        p.accent             = QColor(0x2f, 0x9e, 0x52);
        p.accentHover        = QColor(0x37, 0xb8, 0x62);
        p.accentPressed      = QColor(0x2a, 0x8a, 0x48);
        p.accentText         = QColor(0xff, 0xff, 0xff);
        p.scrollHandle       = QColor(0xc6, 0xcd, 0xd4);
        p.scrollHandleHover  = QColor(0xaa, 0xb2, 0xba);
        p.logBg              = QColor(0xf7, 0xf9, 0xfa);
    }
    return p;
}

QString stylesheet(bool dark)
{
    const Palette p = palette(dark);
    const QString bg        = p.bg.name();
    const QString surface   = p.surface.name();
    const QString hover     = p.surfaceHover.name();
    const QString border    = p.border.name();
    const QString text      = p.text.name();
    const QString muted     = p.textMuted.name();
    const QString inputBg   = p.inputBg.name();
    const QString accent    = p.accent.name();
    const QString accentHov = p.accentHover.name();
    const QString accentTxt = p.accentText.name();
    const QString logBg     = p.logBg.name();

    QString q;
    q += QString("#studioRoot { background: %1; }\n").arg(bg);
    q += QString("QFrame#titleBar { min-height: 50px; max-height: 50px;"
                 " background: %1; border-bottom: 1px solid %2; }\n")
             .arg(surface, border);
    q += QString("QFrame#inspector { background: %1;"
                 " border: 1px solid %2; border-radius: 10px; }\n")
             .arg(surface, border);
    q += QString("QFrame#batchBar { background: %1;"
                 " border: 1px solid %2; border-radius: 10px; }\n")
             .arg(logBg, border);
    q += QString("QFrame#previewPanel { background: %1;"
                 " border: 1px solid %2; border-radius: 10px; }\n")
             .arg(inputBg, border);
    q += QString("QLabel#brandMark { min-width: 24px; max-width: 24px;"
                 " min-height: 24px; max-height: 24px;"
                 " background: #36c66a; color: #102217; border-radius: 7px;"
                 " font-weight: 800; qproperty-alignment: AlignCenter; }\n");
    q += QString("QLabel#appTitle { color: %1; font-size: 15px;"
                 " font-weight: 600; }\n").arg(text);
    q += QString("QLabel#appSubtitle, QLabel#imageName,"
                 " QLabel#workspaceHint, QLabel#inspectorHint"
                 " { color: %1; }\n").arg(muted);
    q += "QLabel#appSubtitle { font-size: 12px; }\n";
    q += "QLabel#imageName { font-weight: 600; }\n";
    q += "QLabel#workspaceHint { font-size: 12px; }\n";
    q += QString("QLabel#viewModeActive { background: %1;"
                 " border: 1px solid %2; border-radius: 7px;"
                 " padding: 6px 10px; color: %3; font-size: 12px;"
                 " font-weight: 600; }\n").arg(hover, border, text);
    q += QString("QLabel#zoomLabel { min-width: 42px; color: %1;"
                 " font-size: 12px; qproperty-alignment: AlignCenter; }\n")
             .arg(muted);
    q += QString("QLabel#previewTitle { padding: 11px 13px;"
                 " font-weight: 600; color: %1; background: %2;"
                 " border-bottom: 1px solid %3; }\n").arg(text, surface, border);
    q += QString("QLabel#inspectorTitle { font-size: 14px;"
                 " font-weight: 600; color: %1; }\n").arg(text);
    q += "QLabel#inspectorHint { font-size: 12px; line-height: 1.35; }\n";
    q += QString("QLabel#batchTitle { font-weight: 700; color: %1; }\n")
             .arg(text);
    q += QString("QScrollArea { background: %1; border: 0; }\n").arg(inputBg);
    q += "QScrollArea#inspectorScroll { background: transparent;"
         " border: 0; }\n";
    q += QString("QLineEdit, QComboBox, QSpinBox { background: %1;"
                 " border: 1px solid %2; border-radius: 7px;"
                 " padding: 7px 9px; color: %3; }\n").arg(inputBg, border, text);
    q += QString("QPushButton { background: %1; border: 1px solid %2;"
                 " border-radius: 7px; padding: 8px 11px; color: %3; }\n")
             .arg(surface, border, text);
    q += QString("QPushButton:hover { background: %1; }\n").arg(hover);
    q += QString("QPushButton:disabled { background: %1; color: %2;"
                 " border-color: %3; }\n").arg(bg, muted, border);
    q += QString("QPushButton#primaryAction { background: %1;"
                 " border-color: %1; color: %2; font-weight: 700; }\n")
             .arg(accent, accentTxt);
    q += QString("QPushButton#primaryAction:hover { background: %1; }\n")
             .arg(accentHov);
    q += QString("QPushButton#saveButton { background: %1;"
                 " border-color: %1; color: %2; font-weight: 700; }\n")
             .arg(accent, accentTxt);
    q += QString("QPushButton#saveButton:hover { background: %1; }\n")
             .arg(accentHov);
    q += QString("QPushButton#logToggle { background: transparent; border: 0;"
                 " color: %1; text-align: left; }\n").arg(muted);
    q += "QPushButton#zoomButton { min-width: 30px; max-width: 30px;"
         " padding: 6px; font-size: 16px; }\n";
    q += "QPushButton#fitButton { padding: 6px 9px; font-size: 12px; }\n";
    q += "QPushButton#themeToggle { padding: 6px 10px; font-size: 12px; }\n";
    q += QString("QFrame#inspectorSeparator { color: %1; }\n").arg(border);
    q += QString("QSplitter::handle { background: %1; }\n").arg(bg);

    // Abas principais (Editor / Modelos)
    q += QString("QTabWidget#mainTabs::pane { border: 1px solid %1;"
                 " border-radius: 10px; top: -1px; background: %2; }\n")
             .arg(border, bg);
    q += QString("QTabBar::tab { background: %1; color: %2;"
                 " padding: 8px 18px; margin-right: 4px;"
                 " border: 1px solid %3; border-bottom: none;"
                 " border-top-left-radius: 8px;"
                 " border-top-right-radius: 8px; }\n")
             .arg(surface, muted, border);
    q += QString("QTabBar::tab:selected { color: %1; background: %2;"
                 " border-color: %3; }\n").arg(text, bg, border);
    q += QString("QTabBar::tab:hover:!selected { background: %1;"
                 " color: %2; }\n").arg(hover, text);

    // Log
    q += QString("QPlainTextEdit { background: %1; border: 1px solid %2;"
                 " border-radius: 7px; color: %3; }\n").arg(logBg, border, text);

    // Aba de ajuda
    q += QString("QTextBrowser { background: %1; border: 1px solid %2;"
                 " border-radius: 10px; padding: 12px; color: %3; }\n")
             .arg(inputBg, border, text);
    q += QString("QTextBrowser a { color: %1; }\n").arg(accent);

    // Barra de progresso
    q += QString("QProgressBar { background: %1; border: 1px solid %2;"
                 " border-radius: 7px; text-align: center; color: %3;"
                 " min-height: 16px; }\n").arg(inputBg, border, text);
    q += QString("QProgressBar::chunk { background: %1;"
                 " border-radius: 6px; }\n").arg(accent);

    // Scrollbars discretas
    q += QString("QScrollBar:vertical { background: transparent; width: 10px;"
                 " margin: 0; }\n");
    q += QString("QScrollBar::handle:vertical { background: %1;"
                 " border-radius: 5px; min-height: 24px; }\n")
             .arg(p.scrollHandle.name());
    q += QString("QScrollBar::handle:vertical:hover { background: %1; }\n")
             .arg(p.scrollHandleHover.name());
    q += QString("QScrollBar:horizontal { background: transparent; height: 10px;"
                 " margin: 0; }\n");
    q += QString("QScrollBar::handle:horizontal { background: %1;"
                 " border-radius: 5px; min-width: 24px; }\n")
             .arg(p.scrollHandle.name());
    q += QString("QScrollBar::handle:horizontal:hover { background: %1; }\n")
             .arg(p.scrollHandleHover.name());
    q += "QScrollBar::add-line, QScrollBar::sub-line { height: 0; width: 0; }\n";
    q += "QScrollBar::add-page, QScrollBar::sub-page { background: transparent; }\n";

    // Dropdown do QComboBox legivel no escuro
    q += QString("QComboBox QAbstractItemView { background: %1; color: %2;"
                 " selection-background-color: %3; selection-color: %4;"
                 " border: 1px solid %5; }\n")
             .arg(inputBg, text, accent, accentTxt, border);

    // Menus e checkboxes herdam a paleta
    q += QString("QCheckBox { color: %1; }\n").arg(text);
    q += QString("QMenu { background: %1; color: %2;"
                 " border: 1px solid %3; }\n").arg(surface, text, border);
    q += QString("QMenu::item:selected { background: %1; color: %2; }\n")
             .arg(accent, accentTxt);

    return q;
}

void apply(bool dark)
{
    if (!QApplication::instance()) return;

    const Palette p = palette(dark);

    QApplication::setStyle(QStyleFactory::create("Fusion"));
    QPalette pal;
    pal.setColor(QPalette::Window, p.bg);
    pal.setColor(QPalette::WindowText, p.text);
    pal.setColor(QPalette::Base, p.inputBg);
    pal.setColor(QPalette::AlternateBase, p.surface);
    pal.setColor(QPalette::Text, p.text);
    pal.setColor(QPalette::Button, p.surface);
    pal.setColor(QPalette::ButtonText, p.text);
    pal.setColor(QPalette::Highlight, p.accent);
    pal.setColor(QPalette::HighlightedText, p.accentText);
    pal.setColor(QPalette::ToolTipBase, p.surface);
    pal.setColor(QPalette::ToolTipText, p.text);
    pal.setColor(QPalette::PlaceholderText, p.textMuted);
    pal.setColor(QPalette::Disabled, QPalette::Text, p.textMuted);
    pal.setColor(QPalette::Disabled, QPalette::ButtonText, p.textMuted);
    pal.setColor(QPalette::Disabled, QPalette::Window, p.surface);
    QApplication::setPalette(pal);
    qApp->setStyleSheet(stylesheet(dark));
}

bool isDark()
{
    const QString env = qEnvironmentVariable("BG_REMOVER_THEME")
                            .toLower();
    if (env == "dark") return true;
    if (env == "light") return false;

    QSettings settings("bg-remover", "bg-remover");
    return settings.value("theme/dark", true).toBool();
}

bool toggle()
{
    const bool dark = !isDark();
    apply(dark);
    QSettings settings("bg-remover", "bg-remover");
    settings.setValue("theme/dark", dark);
    return dark;
}

} // namespace AppTheme
