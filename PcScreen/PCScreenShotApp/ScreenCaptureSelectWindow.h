#ifndef SCREENCAPTURESELECTWINDOW_H
#define SCREENCAPTURESELECTWINDOW_H

#include <QWidget>
#include <QPushButton>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPixmap>

class ScreenCaptureSelectWindow : public QWidget
{
    Q_OBJECT
public:
    explicit ScreenCaptureSelectWindow(QWidget *parent = nullptr);

signals:
    void fullScreenCaptured(const QPixmap &pix); // 全屏截图信号
    void rectCaptured(const QPixmap &pix, const QRect &rect); // 矩形截图信号
    void canceled(); // 取消信号

protected:
    void mousePressEvent(QMouseEvent *e) override;
    void mouseMoveEvent(QMouseEvent *e) override;
    void mouseReleaseEvent(QMouseEvent *e) override;
    void paintEvent(QPaintEvent *e) override;

private slots:
    void on_btnFull_clicked();
    void on_btnSave_clicked();
    void on_btnCancel_clicked();
    void on_btnConfirm_clicked();

private:
    QWidget *m_btnBar; // 底部按钮栏
    QPushButton *m_btnFull;
    QPushButton *m_btnSave;
    QPushButton *m_btnCancel;
    QPushButton *m_btnConfirm;

    QPoint m_startPos;
    QPoint m_endPos;
    bool m_isSelecting = false;
    QPixmap m_screenPix; // 全屏截图缓存
};

#endif // SCREENCAPTURESELECTWINDOW_H
