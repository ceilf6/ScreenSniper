#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include<QPixmap>
#include<QLabel>
#include"ScreenCapture/SCreenCapture.h"
#include"ScreenshotEditor/ScreenshotEditor.h"
#include"ImageSaver/ImageSaver.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    //初始化布局
    void initStyle();
    void enableEditButtons(bool enabled);


private slots:
    //按钮交互槽函数
    //清除
    void on_btnClear_clicked();
    //绘制
    void on_btnDraw_clicked();
    //全屏截图
    void on_btnFullscreen_clicked();
    //矩形截图
    void on_btnRect_clicked();
    //保存
    void on_btnSave_clicked();
    //保存到剪贴板
    void on_btnCopy_clicked();

    //编辑参数区槽函数
    void on_cmbLineColor_currentIndexChanged(int index);
    void on_spinLineWidth_valueChanged(int value);

    //接收功能类信号的槽函数
    //截屏成功
    void onCaptureSuccess(QPixmap pixmap);
    //截屏失败
    void onCaptureFail(const QString &errorMsg);
    //保存成功
    void onSaveSucess(const QString &msg);
    //保存失败
    void onSaveFail(const QString &errorMsg);

protected:
    void mousePressEvent(QMouseEvent* e)override;
    void mouseMoveEvent(QMouseEvent *e)override;
    void mouseReleaseEvent(QMouseEvent *e)override;
    void paintEvent(QPaintEvent *e)override;

private:
    Ui::MainWindow *ui;
    ScreenCapture *m_capture;
    ScreenshotEditor *m_editor;
    ImageSaver *m_saver;
    QPixmap m_originalPix;

    bool m_isDrawing = false;
    QPoint m_startPos;
    QPoint m_endPos;
    bool m_canDraw = false;
    QColor m_drawColor = Qt::red;
    QColor m_currentLineColor;
    int m_lineWidth = 2;

    QLabel *m_previewLabel;


};


#endif // MAINWINDOW_H
