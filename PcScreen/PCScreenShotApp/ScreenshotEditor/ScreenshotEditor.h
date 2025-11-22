#pragma once

#include<QObject>
#include<QPixmap>
#include<QRect>
#include<QColor>

//截图编辑类，负责在截图上绘制图形
class ScreenshotEditor:public QObject{
    Q_OBJECT
public:
    explicit ScreenshotEditor(QObject *parent = nullptr);

    //设置原始截图
    void setOriginalPixmap(const QPixmap &pixmap);

    //绘制矩形
    QPixmap drawRect(const QRect &rect,const QColor & color= Qt::red,int lineWidth=2);

    //获取编辑后的图形
    QPixmap getEditedPixmap() const;

private:
    //原始截图
    QPixmap m_originalPix;
    //编辑后的截图
    QPixmap m_editedPix;

};
