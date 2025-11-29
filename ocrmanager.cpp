#include "ocrmanager.h"
#include <QImage>
#include <QDebug>
#include <QBuffer>

// 如果定义了 USE_TESSERACT，则使用 Tesseract
// 否则在 macOS 上尝试使用 Vision，其他平台返回错误
#ifdef USE_TESSERACT
#include <tesseract/baseapi.h>
#include <leptonica/allheaders.h>
#endif

#if defined(Q_OS_MAC) && !defined(USE_TESSERACT)
#include "macocr.h"
#endif

QString OcrManager::recognize(const QPixmap &pixmap)
{
    if (pixmap.isNull())
    {
        return QString();
    }

#ifdef USE_TESSERACT
    tesseract::TessBaseAPI *api = new tesseract::TessBaseAPI();

    // 初始化 Tesseract
    // 尝试初始化中文简体和英文
    // 注意：需要确保 tessdata 目录下有 chi_sim.traineddata 和 eng.traineddata
    // 如果初始化失败，尝试只初始化英文
    if (api->Init(NULL, "chi_sim+eng"))
    {
        qDebug() << "Could not initialize tesseract with chi_sim+eng, trying eng...";
        if (api->Init(NULL, "eng"))
        {
            qDebug() << "Could not initialize tesseract with eng.";
            delete api;
            return "Error: Could not initialize Tesseract OCR. Please check tessdata.";
        }
    }

    QImage image = pixmap.toImage();
    image = image.convertToFormat(QImage::Format_RGB888);

    api->SetImage(image.bits(), image.width(), image.height(), 3, image.bytesPerLine());

    // 识别
    char *outText = api->GetUTF8Text();
    QString result = QString::fromUtf8(outText);

    // 清理
    api->End();
    delete api;
    delete[] outText;

    return result.trimmed();

#elif defined(Q_OS_MAC)
    // macOS 原生 OCR 回退
    return MacOCR::recognizeText(pixmap);
#else
    return "OCR not configured. Please enable Tesseract in .pro file.";
#endif
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
