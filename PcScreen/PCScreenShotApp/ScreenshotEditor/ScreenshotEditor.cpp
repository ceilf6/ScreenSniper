#include "screenshoteditor.h"
#include <QPainter>

ScreenshotEditor::ScreenshotEditor(QObject *parent) : QObject(parent) {}

// 设置原始截图（必须调用，否则编辑区无图可画）
void ScreenshotEditor::setOriginalPixmap(const QPixmap &pixmap)
{
    m_originalPix = pixmap;
    m_editedPix = pixmap.copy(); // 初始化编辑图为原始图
}

// 绘制矩形核心逻辑（传入原始截图的实际坐标）
QPixmap ScreenshotEditor::drawRect(const QRect &pixRect, const QColor &color, int lineWidth)
{
    if (m_originalPix.isNull()) return QPixmap(); // 无原始图直接返回

    // 基于编辑图绘制（避免每次覆盖原始图）
    m_editedPix = m_originalPix.copy();
    QPainter painter(&m_editedPix);
    QPen pen(color, lineWidth);
    pen.setStyle(Qt::SolidLine); // 确保线条是实线
    painter.setPen(pen);
    painter.drawRect(pixRect); // 绘制用户选择的矩形

    return m_editedPix;
}

// 获取编辑后的图片（供保存使用）
QPixmap ScreenshotEditor::getEditedPixmap() const
{
    return m_editedPix;
}
