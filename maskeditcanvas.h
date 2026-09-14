#ifndef MASKEDITCANVAS_H
#define MASKEDITCANVAS_H

#include <QWidget>
#include <QImage>
#include <QColor>
#include <QPoint>

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
    void clearEdits();
    bool hasEdits() const;

signals:
    void editingDone();

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
    QImage editBuffer_;
    QColor bgColor_;
    Mode mode_;
    int brushRadius_;
    bool painting_;
    QPoint lastPoint_;
    QPoint cursorPos_;
    QImage undoBuffer_;
    bool hasUndo_;
};

#endif
