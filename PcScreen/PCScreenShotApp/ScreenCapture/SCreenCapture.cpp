#include"SCreenCapture.h"
#include<QScreen>
#include<QGuiApplication>


ScreenCapture::ScreenCapture(QObject *parent):QObject(parent){
}

//截取全屏
QPixmap ScreenCapture::captureFullscreen(){
    QScreen *screen = QGuiApplication::primaryScreen();
    if(!screen){
        emit captureFail("无法获取屏幕信息");
        return QPixmap();
    }
    QPixmap pix = screen->grabWindow(0);
    if(pix.isNull()){
        emit captureFail("全屏截图失败");
        return QPixmap();
    }
    emit captureSuccess(pix);
    return pix;
}

//截取指定矩形区域
QPixmap ScreenCapture::captureRect(const QRect &rect){
    if(rect.isEmpty()){
        emit captureFail("选取的区域无效");
        return QPixmap();
    }
    QScreen *screen = QGuiApplication::primaryScreen();
    if(!screen){
        emit captureFail("无法获取屏幕信息");
        return QPixmap();
    }
    QPixmap pix = screen->grabWindow(0,rect.x(),rect.y(),rect.width(),rect.height());
    if(pix.isNull()){
        emit captureFail("矩形截图失败");
        return QPixmap();
    }
    emit captureSuccess(pix);
    return pix;
}



