#include "pinwidget.h"
#include <QPainter>
#include <QMouseEvent>
#include <QMenu>
#include <QApplication>
#include <QClipboard>
#include <QFileDialog>
#include <QDateTime>

PinWidget::PinWidget(const QPixmap &pixmap, QWidget *parent)
    : QWidget(parent), m_sourcePixmap(pixmap), m_scale(1.0)
{
    // 【核心设置】
    // FramelessWindowHint: 去掉标题栏和边框
    // WindowStaysOnTopHint: 永远置顶
    // Tool: 不在任务栏显示图标（像贴纸一样）
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);

    // 设置窗口在这个属性下，关闭时自动释放内存
    setAttribute(Qt::WA_DeleteOnClose);

    // 设置窗口初始大小为图片大小
    resize(m_sourcePixmap.size());
}

void PinWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);

    // 开启平滑抗锯齿，防止缩放时图片变糊
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    // 将图片绘制在整个窗口区域
    painter.drawPixmap(rect(), m_sourcePixmap);

    // 可选：画一个极细的灰色边框，防止白色图片在白色背景下看不清
    painter.setPen(QPen(QColor(200, 200, 200), 1));
    painter.drawRect(rect().adjusted(0, 0, -1, -1));
}

void PinWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        // 记录鼠标按下时，光标相对于窗口左上角的距离
        // 注意：Qt6 使用 globalPosition()，Qt5 使用 globalPos()
        m_dragPosition = event->globalPosition().toPoint() - frameGeometry().topLeft();
        event->accept();
    }
}

void PinWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (event->buttons() & Qt::LeftButton) {
        // 移动窗口：当前鼠标绝对位置 - 之前记录的相对距离
        move(event->globalPosition().toPoint() - m_dragPosition);
        event->accept();
    }
}

void PinWidget::wheelEvent(QWheelEvent *event)
{
    // 获取滚轮滚动的角度 delta
    int numDegrees = event->angleDelta().y();

    // 计算缩放因子：向上滚放大，向下滚缩小
    if (numDegrees > 0) {
        m_scale *= 1.1; // 放大10%
    } else {
        m_scale /= 1.1; // 缩小
    }

    // 限制最小尺寸，防止缩没见了
    if (m_scale < 0.1) m_scale = 0.1;

    // 计算新的尺寸
    QSize newSize = m_scale * m_sourcePixmap.size();

    // 调整窗口大小，paintEvent 会自动把图片拉伸填充
    resize(newSize);

    event->accept();
}

void PinWidget::contextMenuEvent(QContextMenuEvent *event)
{
    QMenu menu(this);

    // 添加菜单项
    QAction *copyAction = menu.addAction("复制到剪切板");
    QAction *saveAction = menu.addAction("另存为...");
    menu.addSeparator(); // 分隔线
    QAction *closeAction = menu.addAction("关闭贴图");

    // 显示菜单并等待用户点击
    QAction *selectedItem = menu.exec(event->globalPos());

    if (selectedItem == closeAction) {
        close(); // 关闭窗口
    }
    else if (selectedItem == copyAction) {
        // 复制到剪切板
        QApplication::clipboard()->setPixmap(m_sourcePixmap);
    }
    else if (selectedItem == saveAction) {
        // 打开保存对话框
        QString fileName = QFileDialog::getSaveFileName(this, "保存图片",
                                                        "screenshot_" + QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss") + ".png",
                                                        "Images (*.png *.jpg)");
        if (!fileName.isEmpty()) {
            m_sourcePixmap.save(fileName);
        }
    }
}
