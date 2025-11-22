#include "ScreenCaptureSelectWindow.h"
#include <QScreen>
#include <QGuiApplication>
#include <QPainter>
#include <QMouseEvent>
#include <QFileDialog>
#include <QMessageBox>

ScreenCaptureSelectWindow::ScreenCaptureSelectWindow(QWidget *parent) : QWidget(parent)
{
    // 1. 窗口属性设置
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
    setAttribute(Qt::WA_TranslucentBackground);
    showFullScreen();

    // 2. 缓存全屏截图
    QScreen *screen = QGuiApplication::primaryScreen();
    m_screenPix = screen->grabWindow(0);

    // 3. 初始化底部按钮栏
    m_btnBar = new QWidget(this);
    m_btnBar->setStyleSheet("background-color: white;");
    m_btnBar->setFixedHeight(40);

    m_btnFull = new QPushButton("获取全屏", m_btnBar);
    m_btnSave = new QPushButton("保存本地", m_btnBar);
    m_btnCancel = new QPushButton("取消", m_btnBar);
    m_btnConfirm = new QPushButton("确认", m_btnBar);

    QHBoxLayout *barLayout = new QHBoxLayout(m_btnBar);
    barLayout->addWidget(m_btnFull);
    barLayout->addWidget(m_btnSave);
    barLayout->addWidget(m_btnCancel);
    barLayout->addWidget(m_btnConfirm);
    barLayout->setMargin(0);
    barLayout->setSpacing(10);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->addStretch();
    mainLayout->addWidget(m_btnBar);
    mainLayout->setMargin(0);

    // 4. 连接按钮信号
    connect(m_btnFull, &QPushButton::clicked, this, &ScreenCaptureSelectWindow::on_btnFull_clicked);
    connect(m_btnSave, &QPushButton::clicked, this, &ScreenCaptureSelectWindow::on_btnSave_clicked);
    connect(m_btnCancel, &QPushButton::clicked, this, &ScreenCaptureSelectWindow::on_btnCancel_clicked);
    connect(m_btnConfirm, &QPushButton::clicked, this, &ScreenCaptureSelectWindow::on_btnConfirm_clicked);
}

void ScreenCaptureSelectWindow::mousePressEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton && !m_btnBar->geometry().contains(e->pos())) {
        m_startPos = e->pos();
        m_isSelecting = true;
    }
}

void ScreenCaptureSelectWindow::mouseMoveEvent(QMouseEvent *e)
{
    if (m_isSelecting) {
        m_endPos = e->pos();
        update();
    }
}

void ScreenCaptureSelectWindow::mouseReleaseEvent(QMouseEvent *e)
{
    if (m_isSelecting && e->button() == Qt::LeftButton) {
        m_isSelecting = false;
        update();
    }
}

void ScreenCaptureSelectWindow::paintEvent(QPaintEvent *e)
{
    Q_UNUSED(e);
    QPainter painter(this);

    // 绘制半透明遮罩
    painter.fillRect(rect(), QColor(0, 0, 0, 100));

    // 绘制选择区域
    if (m_isSelecting) {
        QRect selectRect = QRect(m_startPos, m_endPos).normalized();
        painter.setCompositionMode(QPainter::CompositionMode_Clear);
        painter.fillRect(selectRect, Qt::transparent);
        painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
        painter.setPen(QPen(Qt::red, 2));
        painter.drawRect(selectRect);
    }
}

void ScreenCaptureSelectWindow::on_btnFull_clicked()
{
    emit fullScreenCaptured(m_screenPix);
    close();
}

void ScreenCaptureSelectWindow::on_btnSave_clicked()
{
    QRect selectRect = QRect(m_startPos, m_endPos).normalized();
    QPixmap savePix = m_screenPix.copy(selectRect.isEmpty() ? rect() : selectRect);
    QString path = QFileDialog::getSaveFileName(this, "保存截图", "", "PNG (*.png);;JPG (*.jpg)");
    if (!path.isEmpty() && savePix.save(path)) {
        QMessageBox::information(this, "成功", "保存完成");
    }
}

void ScreenCaptureSelectWindow::on_btnCancel_clicked()
{
    emit canceled();
    close();
}

void ScreenCaptureSelectWindow::on_btnConfirm_clicked()
{
    QRect selectRect = QRect(m_startPos, m_endPos).normalized();
        if (selectRect.isEmpty()) {
            QMessageBox::warning(this, "提示", "请先选择截图区域");
            return;
        }
        QPixmap rectPix = m_screenPix.copy(selectRect);
        emit rectCaptured(rectPix, selectRect);

        hide();
}
