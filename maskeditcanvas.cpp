#include "maskeditcanvas.h"
#include "i18n.h"
#include <QPainter>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QColorDialog>
#include <QLineF>
#include <algorithm>
#include <cmath>

MaskEditCanvas::MaskEditCanvas(QWidget* parent)
    : QWidget(parent),
      mode_(Restore),
      brushRadius_(10),
      painting_(false)
{
    setAttribute(Qt::WA_StaticContents, true);
    setMouseTracking(true);
    setFocusPolicy(Qt::ClickFocus);
    cursorPos_ = QPoint(-1, -1);
}

void MaskEditCanvas::setImage(
    const QImage& image, const QColor& bgColor)
{
    originalImage_ = image;
    originalRgba_ = image.convertToFormat(QImage::Format_RGBA8888);
    bgColor_ = bgColor;
    editBuffer_ = originalRgba_.copy();
    undoStack_.clear();
    redoStack_.clear();
    renderCacheValid_ = false;
    painting_ = false;
    cursorPos_ = QPoint(-1, -1);
    emit availabilityChanged(canUndo(), canRedo());
    update();
}

void MaskEditCanvas::setBackgroundColor(const QColor& bgColor)
{
    bgColor_ = bgColor;
    renderCacheValid_ = false;
    update();
}

void MaskEditCanvas::setMode(Mode mode)
{
    mode_ = mode;
    update();
}

void MaskEditCanvas::setBrushRadius(int radius)
{
    brushRadius_ = qBound(1, radius, 200);
}

void MaskEditCanvas::setDisplaySize(const QSize& size)
{
    if (size.isEmpty()) return;
    setMinimumSize(size);
    setMaximumSize(size);
    resize(size);
}

QImage MaskEditCanvas::getEditedImage() const
{
    return editBuffer_.isNull()
               ? QImage()
               : editBuffer_.copy();
}

bool MaskEditCanvas::canUndo() const
{
    return !undoStack_.isEmpty();
}

bool MaskEditCanvas::canRedo() const
{
    return !redoStack_.isEmpty();
}

void MaskEditCanvas::undo()
{
    if (undoStack_.isEmpty()) return;
    redoStack_.push(editBuffer_.copy());
    editBuffer_ = undoStack_.pop();
    renderCacheValid_ = false;
    update();
    emit editingDone();
    emit availabilityChanged(canUndo(), canRedo());
}

void MaskEditCanvas::redo()
{
    if (redoStack_.isEmpty()) return;
    undoStack_.push(editBuffer_.copy());
    editBuffer_ = redoStack_.pop();
    renderCacheValid_ = false;
    update();
    emit editingDone();
    emit availabilityChanged(canUndo(), canRedo());
}

void MaskEditCanvas::clearEdits()
{
    if (originalImage_.isNull()) return;
    editBuffer_ = originalRgba_.copy();
    undoStack_.clear();
    redoStack_.clear();
    renderCacheValid_ = false;
    painting_ = false;
    cursorPos_ = QPoint(-1, -1);
    emit availabilityChanged(canUndo(), canRedo());
    update();
}

bool MaskEditCanvas::hasEdits() const
{
    return !editBuffer_.isNull()
           && editBuffer_ != originalRgba_;
}

float MaskEditCanvas::scaleFactor() const
{
    if (editBuffer_.isNull()
        || width() == 0 || height() == 0) {
        return 1.0f;
    }
    float scaleW =
        (float)width() / (float)editBuffer_.width();
    float scaleH =
        (float)height() / (float)editBuffer_.height();
    return std::min(scaleW, scaleH);
}

QRect MaskEditCanvas::getPixmapRect() const
{
    if (editBuffer_.isNull()) return rect();
    float scale = scaleFactor();
    int pw = (int)(editBuffer_.width() * scale);
    int ph = (int)(editBuffer_.height() * scale);
    int ox = (width() - pw) / 2;
    int oy = (height() - ph) / 2;
    return QRect(ox, oy, pw, ph);
}

QPoint MaskEditCanvas::widgetToImage(
    const QPoint& wgt) const
{
    QRect pr = getPixmapRect();
    if (!pr.contains(wgt)) return QPoint(-1, -1);
    float scale = scaleFactor();
    int imgX = (int)((wgt.x() - pr.x()) / scale);
    int imgY = (int)((wgt.y() - pr.y()) / scale);
    imgX = qBound(0, imgX, editBuffer_.width() - 1);
    imgY = qBound(0, imgY, editBuffer_.height() - 1);
    return QPoint(imgX, imgY);
}

void MaskEditCanvas::applyBrush(const QPoint& imgPos)
{
    if (editBuffer_.isNull() || originalRgba_.isNull()) return;
    renderCacheValid_ = false;
    const int r = brushRadius_;
    const int w = editBuffer_.width();
    const int h = editBuffer_.height();
    const int cx = imgPos.x();
    const int cy = imgPos.y();

    for (int dy = -r; dy <= r; ++dy) {
        const int py = cy + dy;
        if (py < 0 || py >= h) continue;
        // Cada linha do circulo e um segmento continuo [x0, x1]:
        // evita o teste pixel a pixel e o setPixel() (lento).
        const int spanX = static_cast<int>(
            std::sqrt(static_cast<float>(r * r - dy * dy)));
        const int x0 = qMax(0, cx - spanX);
        const int x1 = qMin(w - 1, cx + spanX);
        QRgb* dst = reinterpret_cast<QRgb*>(
            editBuffer_.scanLine(py));
        if (mode_ == Restore) {
            const QRgb* src = reinterpret_cast<const QRgb*>(
                originalRgba_.constScanLine(py));
            for (int x = x0; x <= x1; ++x) dst[x] = src[x];
        } else {
            for (int x = x0; x <= x1; ++x) {
                dst[x] = qRgba(qRed(dst[x]), qGreen(dst[x]),
                               qBlue(dst[x]), 0);
            }
        }
    }
}

QImage MaskEditCanvas::renderWithBackground(
    const QImage& img, const QColor& bgColor)
{
    if (img.isNull()) return QImage();
    QImage result(img.size(), QImage::Format_RGBA8888);

    if (!bgColor.isValid()) {
        // Xadrez 8x8 preenchido por blocos (rapido, sem loop por pixel).
        const QRgb lightC = 0xFFCCCCCC;
        const QRgb darkC  = 0xFFAAAAAA;
        for (int by = 0; by < result.height(); by += 8) {
            const int bh = qMin(8, result.height() - by);
            for (int bx = 0; bx < result.width(); bx += 8) {
                const bool light = ((bx / 8) + (by / 8)) % 2 == 0;
                const QRgb c = light ? lightC : darkC;
                const int bw = qMin(8, result.width() - bx);
                for (int y = by; y < by + bh; ++y) {
                    QRgb* row = reinterpret_cast<QRgb*>(
                        result.scanLine(y));
                    for (int x = bx; x < bx + bw; ++x) row[x] = c;
                }
            }
        }
    } else {
        result.fill(bgColor);
    }

    // Mistura alpha com acesso direto por scanline (muito mais rapido
    // que pixel()/setPixel()); a leitura e a escrita usam a mesma
    // convensao de bytes, pelo que os canais RGB mantem-se corretos.
    const QImage src = img.format() == QImage::Format_RGBA8888
        ? img : img.convertToFormat(QImage::Format_RGBA8888);
    for (int y = 0; y < src.height(); ++y) {
        const QRgb* s = reinterpret_cast<const QRgb*>(
            src.constScanLine(y));
        QRgb* d = reinterpret_cast<QRgb*>(result.scanLine(y));
        for (int x = 0; x < src.width(); ++x) {
            const QRgb p = s[x];
            const int alpha = qAlpha(p);
            if (alpha == 255) {
                d[x] = p;
            } else if (alpha > 0) {
                const float t = alpha / 255.0f;
                const float u = 1.0f - t;
                d[x] = qRgba(
                    static_cast<int>(qRed(p) * t + qRed(d[x]) * u),
                    static_cast<int>(qGreen(p) * t + qGreen(d[x]) * u),
                    static_cast<int>(qBlue(p) * t + qBlue(d[x]) * u),
                    255);
            }
        }
    }
    return result;
}

void MaskEditCanvas::paintEvent(QPaintEvent*)
{
    if (editBuffer_.isNull()) {
        QPainter painter(this);
        painter.fillRect(rect(), QColor(40, 40, 40));
        painter.setPen(Qt::white);
        painter.drawText(
            rect(), Qt::AlignCenter,
            TR("Clique em 'Remover fundo' para gerar\n"
               "uma máscara e edite-a manualmente",
               "Click 'Remove background' to generate\n"
               "a mask and edit it manually"));
        return;
    }

    QPainter painter(this);
    const QRect destRect = getPixmapRect();

    // So recompoem quando ha edicoes novas/mudanca de fundo; movimentos
    // do rato (cursor do pincel) reutilizam a imagem em cache.
    if (!renderCacheValid_) {
        renderCache_ = renderWithBackground(editBuffer_, bgColor_);
        renderCacheValid_ = true;
    }
    painter.drawImage(destRect, renderCache_);

    if (cursorPos_.x() >= 0 && cursorPos_.y() >= 0) {
        painter.setPen(QPen(Qt::red, 1, Qt::SolidLine));
        painter.setBrush(Qt::NoBrush);
        const int cursorRadius = qMax(1, static_cast<int>(
            brushRadius_ * scaleFactor()));
        painter.drawEllipse(cursorPos_,
                            cursorRadius, cursorRadius);
    }
}

void MaskEditCanvas::mousePressEvent(
    QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton) return;
    QPoint imgPos = widgetToImage(event->pos());
    if (imgPos.x() < 0) return;

    if (!painting_) {
        // Estado anterior a este traco entra na pilha de undo; o redo
        // e invalidado (comportamento padrao de editores).
        undoStack_.push(editBuffer_.copy());
        if (undoStack_.size() > kMaxUndoLevels)
            undoStack_.removeFirst();
        redoStack_.clear();
        painting_ = true;
    }
    lastPoint_ = event->pos();
    applyBrush(imgPos);
    update();
}

void MaskEditCanvas::mouseMoveEvent(
    QMouseEvent* event)
{
    cursorPos_ = event->pos();
    update();

    if (!painting_) return;
    QPoint imgPos = widgetToImage(event->pos());
    if (imgPos.x() < 0) return;

    QPoint prev = widgetToImage(lastPoint_);
    if (prev.x() < 0) prev = imgPos;
    int steps = qMax(1,
        (int)(QLineF(prev, imgPos).length() / brushRadius_));
    for (int i = 0; i <= steps; ++i) {
        float t = (float)i / steps;
        int x = (int)(prev.x() + t * (imgPos.x() - prev.x()));
        int y = (int)(prev.y() + t * (imgPos.y() - prev.y()));
        applyBrush(QPoint(x, y));
    }
    lastPoint_ = event->pos();
    update();
}

void MaskEditCanvas::mouseReleaseEvent(
    QMouseEvent*)
{
    if (painting_) {
        painting_ = false;
        emit availabilityChanged(canUndo(), canRedo());
        update();
        emit editingDone();
    }
}

void MaskEditCanvas::leaveEvent(QEvent*)
{
    if (painting_) {
        painting_ = false;
        emit availabilityChanged(canUndo(), canRedo());
        update();
        emit editingDone();
    }
    cursorPos_ = QPoint(-1, -1);
    update();
}
