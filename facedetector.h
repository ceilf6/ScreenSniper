#ifndef FACEDETECTOR_H
#define FACEDETECTOR_H

#include <QPixmap>
#include <QRect>
#include <QList>
#include <QString>
#include <QStringList>
#include <QFileInfo>
#include <QApplication>
#include <QDebug>
#include <QImage>
#include <QVector>

#ifndef NO_OPENCV
// OpenCV头文件
#include <opencv2/opencv.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/objdetect.hpp>
#include <opencv2/dnn.hpp>
#endif

/**
 * @brief 人脸检测器类
 * 封装了人脸检测相关的功能，支持 Haar Cascade 和 DNN 两种检测模型
 */
class FaceDetector
{
public:
    /**
     * @brief 构造函数
     * @param preferDNN 是否优先使用 DNN 模型（默认 true）
     */
    explicit FaceDetector(bool preferDNN = true);
    
    /**
     * @brief 析构函数
     */
    ~FaceDetector();
    
    /**
     * @brief 检测图像中的人脸位置
     * @param pixmap 要检测的图像
     * @return 检测到的人脸矩形列表
     */
    QList<QRect> detectFaces(const QPixmap& pixmap);
    
    /**
     * @brief 加载人脸检测模型
     * @return 是否加载成功
     */
    bool initialize();
    
    /**
     * @brief 检查模型是否已加载
     * @return 模型是否可用
     */
    bool isReady() const;

private:
#ifndef NO_OPENCV
    /**
     * @brief 使用 DNN 模型检测人脸位置
     * @param pixmap 要检测的图像
     * @return 检测到的人脸矩形列表
     */
    QList<QRect> detectFacesWithDNN(const QPixmap& pixmap);
    
    /**
     * @brief 使用 Haar Cascade 模型检测人脸位置
     * @param pixmap 要检测的图像
     * @return 检测到的人脸矩形列表
     */
    QList<QRect> detectFacesWithCascade(const QPixmap& pixmap);
    
    /**
     * @brief 加载 Haar Cascade 人脸检测模型
     * @return 是否加载成功
     */
    bool loadFaceCascade();
    
    /**
     * @brief 加载 DNN 深度学习模型
     * @return 是否加载成功
     */
    bool loadDNNModel();
    
    /**
     * @brief 合并重叠的人脸检测框，去除重复检测
     * @param faces 原始检测框列表
     * @return 合并后的检测框列表
     */
    QList<QRect> mergeOverlappingFaces(const QList<QRect>& faces);
    
    /**
     * @brief 将 QPixmap 转换为 OpenCV Mat 格式
     * @param pixmap Qt 图像
     * @return OpenCV Mat 图像
     */
    cv::Mat pixmapToMat(const QPixmap& pixmap);
#endif

private:
#ifndef NO_OPENCV
    QList<cv::CascadeClassifier> faceCascades;  // 多个人脸检测分类器（使用多个模型提高精度）
    cv::dnn::Net faceDNNNet;  // DNN 深度学习模型网络
#else
    void* faceCascades;  // 占位符
    void* faceDNNNet;  // 占位符
#endif
    bool dnnModelLoaded;  // DNN 模型是否已加载
    bool useDNNModel;  // 是否优先使用 DNN 模型（如果加载成功）
    bool initialized;
};

#endif // FACEDETECTOR_H
