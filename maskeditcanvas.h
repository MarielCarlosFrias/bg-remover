#ifndef MASKEDITCANVAS_H
#define MASKEDITCANVAS_H

#include <QWidget>
#include <QImage>
#include <QColor>
#include <QPoint>
#include <QStack>

class MaskEditCanvas : public QWidget {
    Q_OBJECT
public:
    enum Mode { Restore, Erase };

    explicit MaskEditCanvas(QWidget* parent = nullptr);

    void setImage(const QImage& image,
                  const QColor& bgColor = QColor());
    void setBackgroundColor(const QColor& bgColor);
    void setBrushRadius(int radius);
    void setDisplaySize(const QSize& size);
    QImage getEditedImage() const;
    void undo();
    void redo();
    void clearEdits();
    bool canUndo() const;
    bool canRedo() const;

    // Comparacao "antes/depois": com o modo ativo, a area a esquerda
    // do divisor mostra a imagem original e a da lado direito o
    // resultado editado. Arrastar o divisor muda a proporcao; enquanto
    // o modo esta ativo o pincel fica suspenso.
    void setCompareEnabled(bool enabled);
    bool compareEnabled() const { return compareEnabled_; }
    void setCompareImage(const QImage& image);
    qreal compareRatio() const { return compareRatio_; }
    void setCompareRatio(qreal ratio);

signals:
    void editingDone();
    // Emitido sempre que a disponibilidade de desfazer/refazer muda.
    void availabilityChanged(bool canUndo, bool canRedo);
    // Emitido quando o utilizador arrasta o divisor de comparacao.
    void compareRatioChanged(qreal ratio);

public slots:
    void setMode(Mode mode);

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void leaveEvent(QEvent*) override;

private:
    QPoint widgetToImage(const QPoint& wgt) const;
    QRect getPixmapRect() const;
    float scaleFactor() const;
    void applyBrush(const QPoint& imgPos);
    static QImage renderWithBackground(const QImage& img,
                                        const QColor& bgColor);
    int dividerX() const;
    void invalidateScaledCaches();

    QImage originalImage_;
    QImage originalRgba_;   // originalImage_ normalizado em RGBA8888
    QImage editBuffer_;
    QColor bgColor_;
    Mode mode_;
    int brushRadius_;
    bool painting_;
    QPoint lastPoint_;
    QPoint cursorPos_;
    QStack<QImage> undoStack_;
    QStack<QImage> redoStack_;
    static constexpr int kMaxUndoLevels = 10;
    QImage renderCache_;        // composicao xadrez/fundo + alpha em cache
    bool renderCacheValid_ = false;
    QSize renderCacheSize_;     // tamanho para o qual o cache e valido

    bool compareEnabled_ = false;
    qreal compareRatio_ = 0.5;
    bool draggingDivider_ = false;
    QImage compareImage_;       // original, para o lado esquerdo
    QImage compareScaled_;      // compareImage_ escalado ao destino
    QSize compareScaledSize_;
};

#endif
