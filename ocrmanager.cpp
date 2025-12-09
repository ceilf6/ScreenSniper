#include "ocrmanager.h"
#include <QImage>
#include <QDebug>
#include <QBuffer>
#include <QThreadPool>
#include <QRunnable>
#include <QFuture>
#include <QtConcurrent/QtConcurrent>

// 如果定义了 USE_TESSERACT，则使用 Tesseract
// 否则在 macOS 上尝试使用 Vision，其他平台返回错误
#ifdef USE_TESSERACT
#include <tesseract/baseapi.h>
#include <leptonica/allheaders.h>
#endif

#if defined(Q_OS_MAC) && !defined(USE_TESSERACT)
#include "macocr.h"
#endif

// 初始化静态成员
OcrManager *OcrManager::m_instance = nullptr;
QMutex OcrManager::m_instanceMutex;

OcrManager::OcrManager()
#ifdef USE_TESSERACT
    : m_tesseractApi(nullptr), m_tesseractInitialized(false),
      m_language("chi_sim+eng"), m_imageScaleFactor(1.0)
#else
    : m_language("chi_sim+eng"), m_imageScaleFactor(1.0)
#endif
{
    // 初始化结果缓存，最多缓存50个结果
    m_resultCache.setMaxCost(50);

    // 预初始化OCR引擎
    initializeOcr();
}

OcrManager::~OcrManager()
{
    cleanup();
}

OcrManager *OcrManager::instance()
{
    // 双重检查锁定模式，确保线程安全
    if (m_instance == nullptr)
    {
        QMutexLocker locker(&m_instanceMutex);
        if (m_instance == nullptr)
        {
            m_instance = new OcrManager();
        }
    }
    return m_instance;
}

bool OcrManager::initializeOcr()
{
#ifdef USE_TESSERACT
    if (m_tesseractInitialized)
        return true;

    if (m_tesseractApi == nullptr)
    {
        m_tesseractApi = new tesseract::TessBaseAPI();
    }

    // 初始化 Tesseract
    // 尝试初始化中文简体和英文
    // 注意：需要确保 tessdata 目录下有 chi_sim.traineddata 和 eng.traineddata
    // 如果初始化失败，尝试只初始化英文
    if (m_tesseractApi->Init(NULL, m_language.toStdString().c_str()))
    {
        qDebug() << "Could not initialize tesseract with" << m_language << ", trying eng...";
        if (m_tesseractApi->Init(NULL, "eng"))
        {
            qDebug() << "Could not initialize tesseract with eng.";
            return false;
        }
        m_language = "eng";
    }

    m_tesseractInitialized = true;
    return true;
#else
    // 非Tesseract模式，不需要初始化
    return true;
#endif
}

// 辅助函数：计算QPixmap的哈希值
static uint computePixmapHash(const QPixmap &pixmap)
{
    // 缩放图像以加快哈希计算
    QPixmap scaledPixmap = pixmap.scaled(100, 100, Qt::KeepAspectRatio, Qt::SmoothTransformation);

    // 转换为灰度图像
    QImage image = scaledPixmap.toImage().convertToFormat(QImage::Format_Grayscale8);

    // 计算哈希值，使用sizeInBytes替代deprecated的byteCount
    QByteArray data((const char *)image.bits(), image.sizeInBytes());
    return qHash(data);
}

QString OcrManager::recognize(const QPixmap &pixmap)
{
    if (pixmap.isNull())
    {
        return QString();
    }

    // 获取单例实例
    OcrManager *ocrInstance = instance();

    // 计算图像哈希值作为缓存键
    uint cacheKey = computePixmapHash(pixmap);

    // 检查缓存
    if (ocrInstance->m_resultCache.contains(cacheKey))
    {
        return *ocrInstance->m_resultCache[cacheKey];
    }

    // 调用实例的识别方法
    QString result = ocrInstance->doRecognize(pixmap);

    // 缓存结果
    ocrInstance->m_resultCache.insert(cacheKey, new QString(result), 1);

    return result;
}

QString OcrManager::doRecognize(const QPixmap &pixmap)
{
    QMutexLocker locker(&m_mutex);

    // 确保OCR引擎已初始化
    if (!initializeOcr())
    {
        return "Error: Could not initialize OCR engine.";
    }

    QImage image = pixmap.toImage();

    // 图像预处理
    image = preprocessImage(image);

    image = image.convertToFormat(QImage::Format_RGB888);

#ifdef USE_TESSERACT
    // 设置图像
    m_tesseractApi->SetImage(image.bits(), image.width(), image.height(), 3, image.bytesPerLine());

    // 识别
    char *outText = m_tesseractApi->GetUTF8Text();
    QString result = QString::fromUtf8(outText);

    // 清理结果
    delete[] outText;

    return result.trimmed();
#else
#if defined(Q_OS_MAC)
    // macOS 原生 OCR
    return MacOCR::recognizeText(QPixmap::fromImage(image)).trimmed();
#else
    return "OCR not configured. Please enable Tesseract in .pro file.";
#endif
#endif
}

QImage OcrManager::preprocessImage(const QImage &image)
{
    QImage processedImage = image;

    // 应用缩放因子
    if (m_imageScaleFactor != 1.0 && (m_imageScaleFactor > 1.0 || processedImage.width() > 2000 || processedImage.height() > 2000))
    {
        int newWidth = static_cast<int>(processedImage.width() * m_imageScaleFactor);
        int newHeight = static_cast<int>(processedImage.height() * m_imageScaleFactor);
        processedImage = processedImage.scaled(newWidth, newHeight, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }

    // 转换为灰度图像（如果不是已经灰度）
    if (processedImage.format() != QImage::Format_Grayscale8)
    {
        processedImage = processedImage.convertToFormat(QImage::Format_Grayscale8);
    }

    return processedImage;
}

QString OcrManager::getBackendType()
{
#ifdef USE_TESSERACT
    return "Tesseract";
#elif defined(Q_OS_MAC)
    return "Native";
#else
    return "None";
#endif
}

void OcrManager::recognizeAsync(const QPixmap &pixmap, std::function<void(const QString &)> callback)
{
    if (pixmap.isNull())
    {
        callback("");
        return;
    }

    // 使用Qt Concurrent进行异步识别
    QtConcurrent::run([pixmap, callback]()
                      {
        QString result = recognize(pixmap);
        callback(result); });
}

void OcrManager::cleanup()
{
    QMutexLocker locker(&m_instanceMutex);

    if (m_instance != nullptr)
    {
#ifdef USE_TESSERACT
        if (m_instance->m_tesseractApi != nullptr)
        {
            m_instance->m_tesseractApi->End();
            delete m_instance->m_tesseractApi;
            m_instance->m_tesseractApi = nullptr;
            m_instance->m_tesseractInitialized = false;
        }
#endif

        delete m_instance;
        m_instance = nullptr;
    }
}

void OcrManager::setLanguage(const QString &language)
{
    OcrManager *ocrInstance = instance();
    QMutexLocker locker(&ocrInstance->m_mutex);

    if (ocrInstance->m_language != language)
    {
        ocrInstance->m_language = language;

        // 重新初始化OCR引擎
#ifdef USE_TESSERACT
        if (ocrInstance->m_tesseractInitialized)
        {
            ocrInstance->m_tesseractApi->End();
            ocrInstance->m_tesseractInitialized = false;
        }
#endif

        ocrInstance->initializeOcr();
    }
}

void OcrManager::setImageScaleFactor(double factor)
{
    if (factor > 0.1 && factor < 5.0)
    {
        OcrManager *ocrInstance = instance();
        QMutexLocker locker(&ocrInstance->m_mutex);
        ocrInstance->m_imageScaleFactor = factor;
    }
}
