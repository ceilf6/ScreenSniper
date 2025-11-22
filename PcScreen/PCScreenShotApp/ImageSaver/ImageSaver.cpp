#include"ImageSaver.h"
#include<QFileInfo>
#include<QClipboard>
#include<QGuiApplication>
#include<QFile>
#include <QDir>

ImageSaver::ImageSaver(QObject *parent):QObject(parent){

}

//保存到本地
bool ImageSaver::saveToLocal(const QPixmap &pixmap, const QString &savePath, const QString &format){
    if(pixmap.isNull()){
        emit saveFail("无效图片可以保存");
        return false;
    }
    QFileInfo fileInfo(savePath);
    if(!fileInfo.dir().exists()){
        emit saveFail("保存路径不存在");
        return false;
    }
    bool saveOk=pixmap.save(savePath,format.toUtf8().data(),100);
    if(saveOk){
        emit saveSuccess("已保存到："+savePath);
    }else{
        emit saveFail("保存失败，请检查路径权限");
    }
    return saveOk;
}

//保存到系统剪切板
void ImageSaver::saveToClipboard(const QPixmap &pixmap){
    if(pixmap.isNull()){
        emit saveFail("无有效图片可复制");
        return;
    }
    QClipboard *clipboard = QGuiApplication::clipboard();
    clipboard->setPixmap(pixmap);
    emit saveSuccess("已复制到剪切板");

}
