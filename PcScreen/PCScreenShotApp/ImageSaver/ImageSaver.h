#pragma once

#include<QObject>
#include<QPixmap>

//图片保存类，负责将图片保存到本地/剪切板
class ImageSaver:public QObject{
    Q_OBJECT
public:
    explicit ImageSaver(QObject *parent = nullptr);

    //保存到本地
    bool saveToLocal(const QPixmap &pixmap, const QString &savePath, const QString &format = "PNG");

    //保存到系统剪切板
    void saveToClipboard(const QPixmap &pixmap);


signals:
    //保存成功信号
    void saveSuccess(const QString &msg);
    //保存失败信号
    void saveFail(const QString &errorMsg);

};
