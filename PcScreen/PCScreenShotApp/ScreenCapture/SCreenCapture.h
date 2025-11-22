#pragma once

#include<QObject>
#include<QPixmap>
#include<QRect>

//屏幕捕捉类，负责获取屏幕图像
class ScreenCapture:public QObject{
    Q_OBJECT
public:
    explicit ScreenCapture(QObject *parent = nullptr);

    //截取全屏
    QPixmap captureFullscreen();

    //截取指定矩形区域
    QPixmap captureRect(const QRect &rect);


signals:
    //截图成功信号
    void captureSuccess(QPixmap pixmap);
    //截图失败信号
    void captureFail(const QString &errorMsg);

};
