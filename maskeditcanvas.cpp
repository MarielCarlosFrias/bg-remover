#include "maskeditcanvas.h"
#include <QPainter>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QColorDialog>
#include <QLineF>
#include <algorithm>

MaskEditCanvas::MaskEditCanvas(QWidget* parent)
    : QWidget(parent),
      mode_(Restore),
      brushRadius_(10),
      painting_(false),
      hasUndo_(false)
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
    bgColor_ = bgColor;
    editBuffer_ = image.copy();
    undoBuffer_ = QImage();
    hasUndo_ = false;
    painting_ = false;
    cursorPos_ = QPoint(-1, -1);
    update();
}

void MaskEditCanvas::setBackgroundColor(const QColor& bgColor)
{
    bgColor_ = bgColor;
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

QImage MaskEditCanvas::getEditedImage() const
{
    return editBuffer_.isNull()
               ? QImage()
               : editBuffer_.copy();
}

void MaskEditCanvas::undo()
{
    if (!hasUndo_) return;
    editBuffer_ = undoBuffer_.copy();
    hasUndo_ = false;
    update();
    emit editingDone();
}

void MaskEditCanvas::clearEdits()
{
    if (originalImage_.isNull()) return;
    editBuffer_ = originalImage_.copy();
    undoBuffer_ = QImage();
    hasUndo_ = false;
    painting_ = false;
    cursorPos_ = QPoint(-1, -1);
    update();
}

bool MaskEditCanvas::hasEdits() const
{
    return !editBuffer_.isNull()
           && editBuffer_ != originalImage_;
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
    if (editBuffer_.isNull()) return;
    int r = brushRadius_;
    int w = editBuffer_.width();
    int h = editBuffer_.height();
    for (int dy = -r; dy <= r; ++dy) {
        for (int dx = -r; dx <= r; ++dx) {
            if (dx * dx + dy * dy > r * r) continue;
            int px = imgPos.x() + dx;
            int py = imgPos.y() + dy;
            if (px < 0 || px >= w
                || py < 0 || py >= h) {
                continue;
            }
            if (mode_ == Restore) {
                editBuffer_.setPixel(
                    px, py,
                    originalImage_.pixel(px, py));
            } else {
                QRgb pxl = editBuffer_.pixel(px, py);
                editBuffer_.setPixel(
                    px, py,
                    qRgba(qRed(pxl), qGreen(pxl),
                          qBlue(pxl), 0));
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
        for (int y = 0; y < img.height(); ++y) {
            for (int x = 0; x < img.width(); ++x) {
                bool light = ((x / 8) + (y / 8)) % 2 == 0;
                result.setPixel(x, y, qRgba(
                    light ? 0xCC : 0xAA,
                    light ? 0xCC : 0xAA,
                    light ? 0xCC : 0xAA, 0xFF));
            }
        }
    } else {
        result.fill(bgColor);
    }

    for (int y = 0; y < img.height(); ++y) {
        for (int x = 0; x < img.width(); ++x) {
            QRgb src = img.pixel(x, y);
            int alpha = qAlpha(src);
            if (alpha == 255) {
                result.setPixel(x, y, src);
            } else if (alpha > 0) {
                QRgb dst = result.pixel(x, y);
                float a = alpha / 255.0f;
                int r = static_cast<int>(qRed(src) * a + qRed(dst) * (1 - a));
                int g = static_cast<int>(qGreen(src) * a + qGreen(dst) * (1 - a));
                int b = static_cast<int>(qBlue(src) * a + qBlue(dst) * (1 - a));
                result.setPixel(x, y, qRgba(r, g, b, 255));
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
            "Clique em 'Remover fundo' para gerar\n"
            "uma máscara e edite-a manualmente");
        return;
    }

    QPainter painter(this);
    QRect destRect = getPixmapRect();

    QImage displayed = renderWithBackground(editBuffer_, bgColor_);
    painter.drawImage(destRect, displayed);

    if (cursorPos_.x() >= 0 && cursorPos_.y() >= 0) {
        painter.setPen(QPen(Qt::red, 1, Qt::SolidLine));
        painter.setBrush(Qt::NoBrush);
        painter.drawEllipse(cursorPos_,
                            brushRadius_, brushRadius_);
    }
}

void MaskEditCanvas::mousePressEvent(
    QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton) return;
    QPoint imgPos = widgetToImage(event->pos());
    if (imgPos.x() < 0) return;

    if (!painting_) {
        undoBuffer_ = editBuffer_.copy();
        hasUndo_ = true;
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
        update();
        emit editingDone();
    }
}

void MaskEditCanvas::leaveEvent(QEvent*)
{
    if (painting_) {
        painting_ = false;
        update();
        emit editingDone();
    }
    cursorPos_ = QPoint(-1, -1);
    update();
}
