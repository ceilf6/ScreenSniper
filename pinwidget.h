#ifndef PINWIDGET_H
#define PINWIDGET_H

#include <QWidget>
#include <QPixmap>
#include <QPoint>

class PinWidget : public QWidget
{
    Q_OBJECT

public:
    // 构造函数：接收一张截图图片
    explicit PinWidget(const QPixmap &pixmap, QWidget *parent = nullptr);

protected:
    // 核心绘制：把图片画出来
    void paintEvent(QPaintEvent *event) override;

    // 鼠标交互：实现拖拽移动
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;

    // 滚轮交互：实现缩放
    void wheelEvent(QWheelEvent *event) override;

    // 右键菜单：保存、复制、关闭
    void contextMenuEvent(QContextMenuEvent *event) override;

private:
    QPixmap m_sourcePixmap; // 原始图片数据
    double m_scale;         // 当前缩放比例
    QPoint m_dragPosition;  // 拖拽时的相对坐标记录
};

#endif // PINWIDGET_H
