#include "mainwindow.h"
#include "ui_mainwindow.h"
#include<QMessageBox>
#include<QFileDialog>
#include<QScreen>
#include<QGuiApplication>
#include<QPainter>
#include<QMouseEvent>
#include<QPaintEvent>
#include<QDateTime>
#include"ScreenCaptureSelectWindow.h"

//矩形选择窗口（用于矩形截图）
class RectSelectWidget:public QWidget{
public:
    explicit RectSelectWidget(QWidget *parent=nullptr):QWidget(parent){
        setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnBottomHint | Qt::Tool);
        setAttribute(Qt::WA_TranslucentBackground);
        setCursor(Qt::CrossCursor);
    }
    QRect selectedRect;
protected:
    void mousePressEvent(QMouseEvent *e)override{
       m_start = e->globalPos();
    }
    void mouseMoveEvent(QMouseEvent *e)override{
        if(e->buttons() & Qt::LeftButton){
            m_end = e->globalPos();
            update();
        }
    }
    void mouseReleaseEvent(QMouseEvent *e)override{
        m_end = e->globalPos();
        selectedRect = QRect(m_start,m_end).normalized();
        close();
    }
    void paintEvent(QPaintEvent *)override{
        QPainter p(this);
        p.fillRect(rect(),QColor(0,0,0,120));
        if(m_start != m_end){
            QRect r = QRect(m_start,m_end).normalized();
            p.setCompositionMode(QPainter::CompositionMode_Clear);
            p.fillRect(r,Qt::transparent);
            p.setCompositionMode(QPainter::CompositionMode_SourceOver);
            p.setPen(QPen(QColor(0,16,255),2));
            p.drawRect(r);
        }
    }
private:
    QPoint m_start,m_end;

    //初始化风格Ui样式
    void initStyle();
};

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    ,m_currentLineColor(QColor(0,136,255))
{
    ui->setupUi(this);

    ui->labelDisplay->setAlignment(Qt::AlignCenter);
    ui->labelDisplay->show();

    //初始化初始类
    m_capture = new ScreenCapture(this);
    m_editor = new ScreenshotEditor(this);
    m_saver = new ImageSaver(this);

    //关联功能类信号与主窗口槽函数
    connect(m_capture,&ScreenCapture::captureSuccess,
            this,&MainWindow::onCaptureSuccess);
    connect(m_capture, &ScreenCapture::captureSuccess, this, [=](QPixmap pix) {
        m_originalPix = pix;
        m_editor->setOriginalPixmap(pix);
        onCaptureSuccess(pix);
        enableEditButtons(true);
    });


    //初始化UI样式

    //初始化编辑参数区（线条颜色，宽度）
    ui->cmbLineColor->addItem("蓝色",QColor(0,136,255));
    ui->cmbLineColor->addItem("红色",QColor(245,108,208));
    ui->cmbLineColor->addItem("绿色",QColor(46,203,112));
    ui->cmbLineColor->addItem("黄色",QColor(247,186,30));
    ui->spinLineWidth->setValue(m_lineWidth);

    //初始禁用按钮功能
    ui->btnDraw->setEnabled(false);
    ui->btnSave->setEnabled(false);
    ui->btnClear->setEnabled(false);
    ui->widgetEdit->setEnabled(false);
    ui->btnCopy->setEnabled(false);

    ui->labelDisplay->hide();
    ui->widgetEdit->hide();
}

MainWindow::~MainWindow()
{
    delete ui;
}


//清除按钮
void MainWindow::on_btnClear_clicked()
{
    m_originalPix=QPixmap();
    m_editor->setOriginalPixmap(QPixmap());
    ui->labelDisplay->clear();
    ui->btnDraw->setEnabled(false);
    ui->btnSave->setEnabled(false);
    ui->btnClear->setEnabled(false);
    ui->statusbar->showMessage("已清空截图",2000);
}
//绘制按钮（绘制矩形）
void MainWindow::on_btnDraw_clicked()
{
    if(m_originalPix.isNull()){
        QMessageBox::warning(this,"提示","请先截取一张截图");
        return;
    }
    m_canDraw=true;
    this->setCursor(Qt::CrossCursor);
    ui->statusbar->showMessage("请在图上拖动鼠标绘制矩形",2000);

}

void MainWindow::mousePressEvent(QMouseEvent* e){
    if(m_canDraw && e->button()==Qt::LeftButton &&
            ui->labelDisplay->geometry().contains(e->pos())){
        m_isDrawing=true;
        m_startPos=e->pos() - ui->labelDisplay->pos();
        m_endPos = m_startPos;
    }
}
void MainWindow::mouseMoveEvent(QMouseEvent *e){
    if(m_isDrawing && ui->labelDisplay->geometry().contains(e->pos())){
            m_endPos = e->pos() - ui->labelDisplay->pos();
            update();
        }
}

void MainWindow::mouseReleaseEvent(QMouseEvent *e){
    if(m_isDrawing && e->button() == Qt::LeftButton){
        m_isDrawing = false;
        m_canDraw =false;
        this->setCursor(Qt::ArrowCursor);

        // 计算绘制的矩形（转换为原始截图的实际坐标）
        QRect drawRect = QRect(m_startPos, m_endPos).normalized();
        QRect pixRect = QRect(
            drawRect.x() * m_originalPix.width() / ui->labelDisplay->width(),
            drawRect.y() * m_originalPix.height() / ui->labelDisplay->height(),
            drawRect.width() * m_originalPix.width() / ui->labelDisplay->width(),
            drawRect.height() * m_originalPix.height() / ui->labelDisplay->height()
        );

        // 调用 ScreenshotEditor 绘制矩形
        QPixmap editedPix = m_editor->drawRect(pixRect, m_drawColor, m_lineWidth);
        ui->labelDisplay->setPixmap(editedPix.scaled(ui->labelDisplay->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
        ui->statusbar->showMessage("矩形绘制完成", 2000);
    }
}

void MainWindow::paintEvent(QPaintEvent *e){
    QMainWindow::paintEvent(e); // 先绘制原有内容
        if (m_isDrawing) {
            QPainter painter(this);
            painter.setPen(QPen(m_drawColor, m_lineWidth));
            // 转换为窗口的全局坐标
            QRect drawRect = QRect(
                ui->labelDisplay->pos() + m_startPos,
                ui->labelDisplay->pos() + m_endPos
            ).normalized();
            painter.drawRect(drawRect);
        }
}

//全屏截图按钮
void MainWindow::on_btnFullscreen_clicked()
{
    m_capture->captureFullscreen();
}

//矩形截图按钮
void MainWindow::on_btnRect_clicked()
{
    ScreenCaptureSelectWindow *selectWin = new ScreenCaptureSelectWindow(this);

       connect(selectWin, &ScreenCaptureSelectWindow::fullScreenCaptured, this, [=](const QPixmap &pix) {
           m_originalPix = pix;
           m_editor->setOriginalPixmap(pix);
           onCaptureSuccess(pix);
           enableEditButtons(true); // 启用编辑按钮
           selectWin->deleteLater();
       });

       connect(selectWin, &ScreenCaptureSelectWindow::rectCaptured, this, [=](const QPixmap &pix, const QRect &rect) {
           Q_UNUSED(rect);
           m_originalPix = pix;
           m_editor->setOriginalPixmap(pix);
           onCaptureSuccess(pix);
           enableEditButtons(true); // 启用编辑按钮
           selectWin->deleteLater();
       });

       connect(selectWin, &ScreenCaptureSelectWindow::canceled, this, [=]() {
           selectWin->deleteLater();
       });
}

//保存到本地按钮
void MainWindow::on_btnSave_clicked()
{
    QString defaultName = QString("截图_%1").arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh-mm-ss"));
    QString savePath = QFileDialog::getSaveFileName(
            this,
            "保存截图",
            defaultName,
            "PNG 图片 (*.png);;JPEG 图片 (*.jpg);;BMP 图片 (*.bmp)"
        );
    if(!savePath.isEmpty()){
        m_saver->saveToLocal(m_editor->getEditedPixmap(),savePath);
    }
}

//保存到剪切板
void MainWindow::on_btnCopy_clicked(){
    m_saver->saveToClipboard(m_editor->getEditedPixmap());
}


//编辑参数区槽函数
//线条颜色下拉框切换
void MainWindow::on_cmbLineColor_currentIndexChanged(int index){
    m_currentLineColor = ui->cmbLineColor->itemData(index).value<QColor>();
        m_drawColor = m_currentLineColor;
}
void MainWindow::on_spinLineWidth_valueChanged(int value){
    m_lineWidth = value;
}

//接收功能类信号的槽函数
//截屏成功信号
void MainWindow::onCaptureSuccess(QPixmap pixmap){
    m_originalPix = pixmap;
        m_editor->setOriginalPixmap(pixmap);

        // 显示到UI中的labelDisplay
        ui->labelDisplay->setPixmap(pixmap.scaled(ui->labelDisplay->size(),
                                Qt::KeepAspectRatio, Qt::SmoothTransformation));
        ui->labelDisplay->show();

        enableEditButtons(true);
        ui->statusbar->showMessage("截图成功", 2000);
}
//截屏失败信号
void MainWindow::onCaptureFail(const QString &errorMsg){
    QMessageBox::warning(this,"截图失败",errorMsg,QMessageBox::Ok,QMessageBox::Ok);
    ui->statusbar->showMessage("截图失败："+errorMsg,3000);
}
//保存成功信号
void MainWindow::onSaveSucess(const QString &msg){
    QMessageBox::information(this,"操作成功",msg,QMessageBox::Ok,QMessageBox::Ok);
    ui->statusbar->showMessage(msg,3000);
}
//保存失败信号
void MainWindow::onSaveFail(const QString &errorMsg){
    QMessageBox::critical(this, "操作失败", errorMsg, QMessageBox::Ok, QMessageBox::Ok);
    ui->statusbar->showMessage("失败：" + errorMsg, 3000);
}

void MainWindow::enableEditButtons(bool enabled)
{
    ui->btnDraw->setEnabled(enabled);
    ui->btnSave->setEnabled(enabled);
    ui->btnClear->setEnabled(enabled);
    ui->widgetEdit->setEnabled(enabled);
    ui->btnCopy->setEnabled(enabled);
}


