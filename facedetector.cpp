#include "facedetector.h"
#include <QImage>

FaceDetector::FaceDetector(bool preferDNN)
    : dnnModelLoaded(false),
      useDNNModel(preferDNN),
      initialized(false)
{
#ifndef NO_OPENCV
    // OpenCV 相关初始化（使用默认构造函数）
#else
    // OpenCV 未启用，使用占位符
    faceCascades = nullptr;
    faceDNNNet = nullptr;
#endif
}

FaceDetector::~FaceDetector()
{
#ifndef NO_OPENCV
    // OpenCV 相关清理
#endif
}

bool FaceDetector::initialize()
{
#ifndef NO_OPENCV
    if (initialized) {
        return true;
    }
    
    // 加载人脸检测模型（优先加载 DNN，如果失败则使用 Haar Cascade）
    if (!loadDNNModel()) {
        qDebug() << "DNN 模型加载失败，将使用 Haar Cascade 模型";
        if (loadFaceCascade()) {
            initialized = true;
            return true;
        }
    } else {
        qDebug() << "DNN 模型加载成功，将使用 DNN 进行人脸检测";
        // 仍然加载 Haar Cascade 作为备选
        loadFaceCascade();
        initialized = true;
        return true;
    }
    
    return false;
#else
    qDebug() << "OpenCV未启用，无法初始化人脸检测器";
    return false;
#endif
}

bool FaceDetector::isReady() const
{
#ifndef NO_OPENCV
    return initialized && (dnnModelLoaded || !faceCascades.isEmpty());
#else
    return false;  // OpenCV未启用时始终返回false
#endif
}

QList<QRect> FaceDetector::detectFaces(const QPixmap& pixmap)
{
#ifndef NO_OPENCV
    // 如果未初始化，先尝试初始化
    if (!initialized) {
        if (!initialize()) {
            qDebug() << "人脸检测器未初始化，无法检测";
            return QList<QRect>();
        }
    }
    
    // 优先使用 DNN 模型（如果已加载且启用）
    if (useDNNModel && dnnModelLoaded && !faceDNNNet.empty()) {
        qDebug() << "使用 DNN 模型进行人脸检测";
        return detectFacesWithDNN(pixmap);
    }
    
    // 如果 DNN 不可用，使用 Haar Cascade 模型
    qDebug() << "使用 Haar Cascade 模型进行人脸检测";
    return detectFacesWithCascade(pixmap);
#else
    qDebug() << "OpenCV未启用，无法进行人脸检测";
    return QList<QRect>();
#endif
}

#ifndef NO_OPENCV

cv::Mat FaceDetector::pixmapToMat(const QPixmap& pixmap)
{
    // 将QPixmap转换为OpenCV的Mat格式
    QImage qImage = pixmap.toImage();
    cv::Mat cvImage;
    
    // 转换颜色格式（Qt使用RGB，OpenCV使用BGR）
    if (qImage.format() == QImage::Format_RGB32 || qImage.format() == QImage::Format_ARGB32) {
        // 处理ARGB32格式
        cvImage = cv::Mat(qImage.height(), qImage.width(), CV_8UC4, 
                         (void*)qImage.constBits(), qImage.bytesPerLine());
        cv::cvtColor(cvImage, cvImage, cv::COLOR_BGRA2BGR);
    } else {
        // 转换为RGB888格式
        QImage converted = qImage.convertToFormat(QImage::Format_RGB888);
        cvImage = cv::Mat(converted.height(), converted.width(), CV_8UC3, 
                         (void*)converted.constBits(), converted.bytesPerLine());
        cv::cvtColor(cvImage, cvImage, cv::COLOR_RGB2BGR);
    }
    
    return cvImage;
}

QList<QRect> FaceDetector::detectFacesWithDNN(const QPixmap& pixmap)
{
    QList<QRect> faces;
    
    // 检查 DNN 模型是否已加载
    if (!dnnModelLoaded || faceDNNNet.empty()) {
        qDebug() << "DNN 模型未加载，无法使用 DNN 检测";
        return faces;
    }
    
    // 将QPixmap转换为OpenCV的Mat格式
    cv::Mat cvImage = pixmapToMat(pixmap);
    
    // DNN 模型输入尺寸（OpenCV 人脸检测模型通常使用 300x300）
    int inputWidth = 300;
    int inputHeight = 300;
    
    // 创建 blob（DNN 输入格式）
    // 参数：图像，缩放因子，输入尺寸，均值（RGB），是否交换RB通道，是否裁剪
    cv::Mat blob = cv::dnn::blobFromImage(cvImage, 1.0, 
                                          cv::Size(inputWidth, inputHeight), 
                                          cv::Scalar(104.0, 177.0, 123.0), 
                                          false, false);
    
    // 设置输入
    faceDNNNet.setInput(blob);
    
    // 前向传播，获取检测结果
    cv::Mat detections = faceDNNNet.forward();
    
    // 解析检测结果
    // detections 的形状通常是 [1, 1, N, 7]
    // 其中 N 是检测到的对象数量
    // 每个检测结果包含：[batch_id, class_id, confidence, x1, y1, x2, y2]
    cv::Mat detectionMat(detections.size[2], detections.size[3], CV_32F, 
                        detections.ptr<float>());
    
    // 原始图像尺寸
    int imageWidth = cvImage.cols;
    int imageHeight = cvImage.rows;
    
    // 置信度阈值（提高到0.7，减少误检）
    float confidenceThreshold = 0.7;
    
    for (int i = 0; i < detectionMat.rows; i++) {
        float confidence = detectionMat.at<float>(i, 2);
        
        // 只保留置信度高于阈值的检测结果
        if (confidence > confidenceThreshold) {
            // 获取边界框坐标（归一化坐标，需要转换为像素坐标）
            int x1 = static_cast<int>(detectionMat.at<float>(i, 3) * imageWidth);
            int y1 = static_cast<int>(detectionMat.at<float>(i, 4) * imageHeight);
            int x2 = static_cast<int>(detectionMat.at<float>(i, 5) * imageWidth);
            int y2 = static_cast<int>(detectionMat.at<float>(i, 6) * imageHeight);
            
            // 确保坐标在图像范围内
            x1 = qMax(0, qMin(x1, imageWidth - 1));
            y1 = qMax(0, qMin(y1, imageHeight - 1));
            x2 = qMax(0, qMin(x2, imageWidth - 1));
            y2 = qMax(0, qMin(y2, imageHeight - 1));
            
            // 确保 x2 > x1 且 y2 > y1
            if (x2 > x1 && y2 > y1) {
                int width = x2 - x1;
                int height = y2 - y1;
                
                // 过滤条件：验证检测框是否符合人脸特征
                // 1. 最小尺寸：人脸至少应该有一定大小
                if (width < 10 || height < 10) {
                    continue;  // 太小，可能是误检
                }
                
                // 2. 宽高比：人脸通常是接近正方形的，宽高比应该在合理范围内
                // 正常人脸宽高比通常在 0.6 到 1.5 之间
                double aspectRatio = (double)width / height;
                if (aspectRatio < 0.5 || aspectRatio > 2.0) {
                    qDebug() << "过滤掉宽高比异常的人脸检测:" << aspectRatio 
                             << "置信度:" << confidence;
                    continue;  // 宽高比异常，可能是误检（如文字、矩形框等）
                }
                
                // 3. 面积：人脸应该有一定的最小面积
                int area = width * height;
                if (area < 100) {  // 至少100像素
                    continue;  // 面积太小，可能是误检
                }
                
                QRect faceRect(x1, y1, width, height);
                faces.append(faceRect);
                
                qDebug() << "接受人脸检测: 置信度=" << confidence 
                         << "尺寸=" << width << "x" << height 
                         << "宽高比=" << aspectRatio;
            }
        }
    }
    
    // 后处理：合并重叠的检测框
    faces = mergeOverlappingFaces(faces);
    
    qDebug() << "DNN 检测到" << faces.size() << "个人脸（合并后）";
    return faces;
}

QList<QRect> FaceDetector::detectFacesWithCascade(const QPixmap& pixmap)
{
    QList<QRect> faces;
    
    // 检查模型是否已加载
    if (faceCascades.isEmpty()) {
        if (!loadFaceCascade()) {
            return faces;  // 模型加载失败，返回空列表
        }
    }
    
    if (faceCascades.isEmpty()) {
        return faces;  // 仍然为空，返回空列表
    }
    
    // 将QPixmap转换为OpenCV的Mat格式
    cv::Mat cvImage = pixmapToMat(pixmap);
    
    // 转换为灰度图（人脸检测需要）
    cv::Mat gray;
    cv::cvtColor(cvImage, gray, cv::COLOR_BGR2GRAY);
    
    // 使用CLAHE（对比度受限的自适应直方图均衡化）替代普通直方图均衡化
    // 这能更好地处理不同光照条件，提高检测精度
    cv::Ptr<cv::CLAHE> clahe = cv::createCLAHE(3.0, cv::Size(8, 8));  // 提高对比度限制到3.0
    clahe->apply(gray, gray);
    
    // 使用多个模型进行检测，提高精度和召回率
    // 存储所有模型的检测结果（用于投票机制）
    QList<std::vector<cv::Rect>> allModelDetections;
    
    // 对每个加载的模型进行多尺度检测
    for (int modelIdx = 0; modelIdx < faceCascades.size(); ++modelIdx) {
        cv::CascadeClassifier& cascade = faceCascades[modelIdx];
        
        if (cascade.empty()) {
            continue;
        }
        
        std::vector<cv::Rect> modelFaces;
        
        // 第一次检测：最严格参数（高精度，检测小脸）
        std::vector<cv::Rect> faces1;
        cascade.detectMultiScale(
            gray, 
            faces1,
            1.01,     // 缩放因子（降低到1.01，非常细致的检测）
            1,        // 最小邻居数（降低到1，最大化召回率）
            0,        // 标志
            cv::Size(10, 10),  // 最小人脸大小（降低到10x10，检测更小的人脸）
            cv::Size()         // 最大人脸大小（空表示不限制）
        );
        modelFaces.insert(modelFaces.end(), faces1.begin(), faces1.end());
        
        // 第二次检测：中等参数（补充检测中等大小的人脸）
        std::vector<cv::Rect> faces2;
        cascade.detectMultiScale(
            gray, 
            faces2,
            1.03,     // 稍宽松的缩放因子
            1,        // 最小邻居数
            0,        // 标志
            cv::Size(15, 15),  // 最小人脸大小
            cv::Size()         // 最大人脸大小
        );
        modelFaces.insert(modelFaces.end(), faces2.begin(), faces2.end());
        
        // 第三次检测：宽松参数（检测大脸和模糊人脸）
        std::vector<cv::Rect> faces3;
        cascade.detectMultiScale(
            gray, 
            faces3,
            1.05,     // 稍宽松的缩放因子
            2,        // 最小邻居数（稍严格，减少误检）
            0,        // 标志
            cv::Size(25, 25),  // 最小人脸大小
            cv::Size()         // 最大人脸大小
        );
        modelFaces.insert(modelFaces.end(), faces3.begin(), faces3.end());
        
        // 去重该模型的检测结果
        std::vector<cv::Rect> uniqueModelFaces;
        for (const cv::Rect& face : modelFaces) {
            bool isDuplicate = false;
            for (const cv::Rect& existing : uniqueModelFaces) {
                QRect existingRect(existing.x, existing.y, existing.width, existing.height);
                QRect newRect(face.x, face.y, face.width, face.height);
                QRect intersection = existingRect.intersected(newRect);
                if (!intersection.isEmpty()) {
                    double overlapRatio = (double)(intersection.width() * intersection.height()) / 
                                       qMin(existingRect.width() * existingRect.height(),
                                            newRect.width() * newRect.height());
                    if (overlapRatio > 0.3) {
                        isDuplicate = true;
                        break;
                    }
                }
            }
            if (!isDuplicate) {
                uniqueModelFaces.push_back(face);
            }
        }
        
        allModelDetections.append(uniqueModelFaces);
        qDebug() << "模型" << (modelIdx + 1) << "检测到" << uniqueModelFaces.size() << "个人脸";
    }
    
    // 多模型投票机制：如果一个区域被多个模型检测到，则认为是可靠的人脸
    // 使用更智能的合并策略
    std::vector<cv::Rect> detectedFaces;
    QList<int> voteCounts;  // 记录每个检测框被多少个模型支持
    
    // 收集所有检测结果并计算投票
    for (int modelIdx = 0; modelIdx < allModelDetections.size(); ++modelIdx) {
        const std::vector<cv::Rect>& modelFaces = allModelDetections[modelIdx];
        
        for (const cv::Rect& face : modelFaces) {
            bool foundMatch = false;
            
            // 查找是否有重叠的已有检测框
            for (int i = 0; i < detectedFaces.size(); ++i) {
                QRect existingRect(detectedFaces[i].x, detectedFaces[i].y, 
                                  detectedFaces[i].width, detectedFaces[i].height);
                QRect newRect(face.x, face.y, face.width, face.height);
                QRect intersection = existingRect.intersected(newRect);
                
                if (!intersection.isEmpty()) {
                    // 计算重叠率
                    double overlapRatio = (double)(intersection.width() * intersection.height()) / 
                                       qMin(existingRect.width() * existingRect.height(),
                                            newRect.width() * newRect.height());
                    
                    // 如果重叠率超过25%，认为是同一个脸，增加投票数
                    if (overlapRatio > 0.25) {
                        voteCounts[i]++;
                        // 如果多个模型都检测到，取并集（更大的框）以提高覆盖率
                        if (newRect.width() * newRect.height() > existingRect.width() * existingRect.height()) {
                            detectedFaces[i] = cv::Rect(newRect.x(), newRect.y(), 
                                                       newRect.width(), newRect.height());
                        }
                        foundMatch = true;
                        break;
                    }
                }
            }
            
            // 如果没有匹配的已有框，添加新框（至少被一个模型检测到）
            if (!foundMatch) {
                detectedFaces.push_back(face);
                voteCounts.append(1);
            }
        }
    }
    
    // 过滤：只保留被至少一个模型检测到的框（如果只有一个模型，则保留所有）
    // 如果有多个模型，可以设置更高的投票阈值（例如至少2个模型支持）
    int minVotes = faceCascades.size() > 1 ? 1 : 1;  // 至少1个模型支持（可以改为2以提高精度）
    std::vector<cv::Rect> filteredFaces;
    for (int i = 0; i < detectedFaces.size(); ++i) {
        if (voteCounts[i] >= minVotes) {
            filteredFaces.push_back(detectedFaces[i]);
        }
    }
    detectedFaces = filteredFaces;
    
    qDebug() << "多模型检测完成，最终检测到" << detectedFaces.size() << "个人脸";
    
    // 转换OpenCV的Rect为Qt的QRect
    for (const cv::Rect& face : detectedFaces) {
        // OpenCV使用(x, y, width, height)
        QRect faceRect(face.x, face.y, face.width, face.height);
        faces.append(faceRect);
    }
    
    // 后处理：合并重叠的检测框，去除重复检测
    faces = mergeOverlappingFaces(faces);
    
    qDebug() << "检测到" << faces.size() << "个人脸（合并后）";
    return faces;
}

QList<QRect> FaceDetector::mergeOverlappingFaces(const QList<QRect>& faces)
{
    if (faces.isEmpty()) {
        return faces;
    }
    
    QList<QRect> mergedFaces;
    QList<bool> used(faces.size(), false);
    
    for (int i = 0; i < faces.size(); ++i) {
        if (used[i]) {
            continue;
        }
        
        QRect currentFace = faces[i];
        used[i] = true;
        
        // 查找与当前框重叠的其他框
        bool merged = false;
        for (int j = i + 1; j < faces.size(); ++j) {
            if (used[j]) {
                continue;
            }
            
            QRect otherFace = faces[j];
            
            // 计算重叠区域
            QRect intersection = currentFace.intersected(otherFace);
            if (!intersection.isEmpty()) {
                // 计算重叠率
                double overlapRatio = (double)(intersection.width() * intersection.height()) / 
                                     qMin(currentFace.width() * currentFace.height(),
                                          otherFace.width() * otherFace.height());
                
                // 如果重叠率超过30%，合并两个框
                if (overlapRatio > 0.3) {
                    // 合并：取两个框的并集
                    currentFace = currentFace.united(otherFace);
                    used[j] = true;
                    merged = true;
                }
            }
        }
        
        mergedFaces.append(currentFace);
    }
    
    qDebug() << "合并前:" << faces.size() << "个检测框，合并后:" << mergedFaces.size() << "个";
    return mergedFaces;
}

bool FaceDetector::loadFaceCascade()
{
    // 清空现有模型
    faceCascades.clear();
    
    // 要加载的模型列表（按优先级排序，alt2通常精度最高）
    QStringList modelNames = {
        "haarcascade_frontalface_alt2.xml",  // 通常精度最高
        "haarcascade_frontalface_alt.xml",   // 备选模型
        "haarcascade_frontalface_default.xml" // 默认模型
    };
    
    // 尝试多个可能的路径
    QStringList basePaths = {
        // 项目根目录（开发时）
        "models/",
        "",
        // 可执行文件所在目录（运行时）
        QApplication::applicationDirPath() + "/models/",
        QApplication::applicationDirPath() + "/",
        // 可执行文件上级目录
        QApplication::applicationDirPath() + "/../models/",
        QApplication::applicationDirPath() + "/../../models/",
        // 项目根目录（从build目录运行时）
        QApplication::applicationDirPath() + "/../../../models/",
        QApplication::applicationDirPath() + "/../../../../models/"
    };
    
    int loadedCount = 0;
    
    // 尝试加载每个模型
    for (const QString& modelName : modelNames) {
        bool modelLoaded = false;
        
        for (const QString& basePath : basePaths) {
            QString fullPath = basePath + modelName;
            QFileInfo fileInfo(fullPath);
            
            if (fileInfo.exists()) {
                cv::CascadeClassifier cascade;
                if (cascade.load(fullPath.toStdString())) {
                    faceCascades.append(cascade);
                    qDebug() << "人脸检测模型加载成功:" << fullPath;
                    loadedCount++;
                    modelLoaded = true;
                    break;  // 找到该模型后，不再尝试其他路径
                }
            }
        }
        
        if (!modelLoaded) {
            qDebug() << "未找到模型文件:" << modelName;
        }
    }
    
    if (faceCascades.isEmpty()) {
        qDebug() << "警告：无法加载任何人脸检测模型，自动人脸打码功能将不可用";
        qDebug() << "请确保以下模型文件之一存在于以下位置:";
        for (const QString& modelName : modelNames) {
            qDebug() << "  -" << modelName;
        }
        for (const QString& basePath : basePaths) {
            qDebug() << "    在:" << basePath;
        }
        return false;
    }
    
    qDebug() << "成功加载" << loadedCount << "个人脸检测模型，将使用多模型组合检测以提高精度";
    return true;
}

bool FaceDetector::loadDNNModel()
{
    // DNN 模型文件列表（按优先级）
    QStringList modelNames = {
        "opencv_face_detector_uint8.pb",      // 模型权重文件
        "opencv_face_detector.pbtxt",         // 模型配置文件
        "opencv_face_detector.pbtxt.txt"     // 备选配置文件名称
    };
    
    // 尝试多个可能的路径
    QStringList basePaths = {
        // 项目根目录（开发时）
        "models/",
        "",
        // 可执行文件所在目录（运行时）
        QApplication::applicationDirPath() + "/models/",
        QApplication::applicationDirPath() + "/",
        // 可执行文件上级目录
        QApplication::applicationDirPath() + "/../models/",
        QApplication::applicationDirPath() + "/../../models/",
        // 项目根目录（从build目录运行时）
        QApplication::applicationDirPath() + "/../../../models/",
        QApplication::applicationDirPath() + "/../../../../models/"
    };
    
    QString modelPath, configPath;
    bool foundModel = false, foundConfig = false;
    
    // 查找模型权重文件
    for (const QString& basePath : basePaths) {
        QString fullPath = basePath + modelNames[0];
        QFileInfo fileInfo(fullPath);
        if (fileInfo.exists()) {
            modelPath = fullPath;
            foundModel = true;
            break;
        }
    }
    
    // 查找模型配置文件（尝试多个可能的文件名）
    for (int i = 1; i < modelNames.size(); ++i) {
        for (const QString& basePath : basePaths) {
            QString fullPath = basePath + modelNames[i];
            QFileInfo fileInfo(fullPath);
            if (fileInfo.exists()) {
                configPath = fullPath;
                foundConfig = true;
                break;
            }
        }
        if (foundConfig) break;
    }
    
    if (!foundModel || !foundConfig) {
        qDebug() << "未找到 DNN 模型文件";
        if (!foundModel) {
            qDebug() << "  缺少模型权重文件:" << modelNames[0];
        }
        if (!foundConfig) {
            qDebug() << "  缺少模型配置文件:" << modelNames[1];
        }
        qDebug() << "请从以下地址下载模型文件:";
        qDebug() << "  https://github.com/opencv/opencv_extra/tree/master/testdata/dnn";
        dnnModelLoaded = false;
        return false;
    }
    
    // 加载 DNN 模型
    try {
        faceDNNNet = cv::dnn::readNetFromTensorflow(modelPath.toStdString(), 
                                                     configPath.toStdString());
        
        if (faceDNNNet.empty()) {
            qDebug() << "DNN 模型加载失败（模型文件为空）";
            dnnModelLoaded = false;
            return false;
        }
        
        // 尝试设置后端和目标（可选，提高性能）
        // faceDNNNet.setPreferableBackend(cv::dnn::DNN_BACKEND_OPENCV);
        // faceDNNNet.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);
        
        qDebug() << "DNN 模型加载成功:";
        qDebug() << "  模型文件:" << modelPath;
        qDebug() << "  配置文件:" << configPath;
        dnnModelLoaded = true;
        return true;
    } catch (const cv::Exception& e) {
        qDebug() << "DNN 模型加载异常:" << e.what();
        dnnModelLoaded = false;
        return false;
    }
}

#endif // NO_OPENCV
