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
    bool hasEdits() const;

signals:
    void editingDone();
    // Emitido sempre que a disponibilidade de desfazer/refazer muda.
    void availabilityChanged(bool canUndo, bool canRedo);

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
};

#endif
