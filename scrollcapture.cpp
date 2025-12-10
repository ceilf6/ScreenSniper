#include "ScrollCapture.h"
#include <algorithm>
#include <QDebug>
#include <QApplication>
#include <QScreen>
#undef min
#undef max
#ifdef Q_OS_WIN
#include <windows.h>
#endif
#include "ScrollCapture.h"

ScrollStitcher::ScrollStitcher(QObject *parent) : QObject(parent) {}

void ScrollStitcher::processStitch(QImage baseImage, QImage newImage)
{
    if (baseImage.isNull()) {
        emit stitchFinished(newImage);
        return;
    }

    cv::Mat matBase = qImageToCvMat(baseImage);
    cv::Mat matNew = qImageToCvMat(newImage);

    // 计算重叠位移
    int offset = calcOffsetByTemplate(matBase, matNew);

    // offset <= 0 表示没有找到匹配，或者图像完全没动
    if (offset <= 0) {
        emit stitchFailed(baseImage);
        return;
    }

    // --- 开始拼接 ---
    // 有效的新图高度 = 原新图高度 - 重叠部分(offset)
    int validNewH = matNew.rows - offset;

    // 修改这里：即使validNewH很小，也不停止，而是直接使用完整图像
    // 这样可以处理滚动距离很小的情况
    if (validNewH <= 0) {
        // 如果完全没有新内容，发送失败信号
        emit stitchFailed(baseImage);
        return;
    }

    int totalH = matBase.rows + validNewH;
    cv::Mat resultMat(totalH, matBase.cols, CV_8UC3);

    // 1. 拷贝旧图 (全部)
    matBase.copyTo(resultMat(cv::Rect(0, 0, matBase.cols, matBase.rows)));

    // 2. 拷贝新图 (从 offset 开始往下)
    matNew(cv::Rect(0, offset, matNew.cols, validNewH))
        .copyTo(resultMat(cv::Rect(0, matBase.rows, matNew.cols, validNewH)));

    QImage result = cvMatToQImage(resultMat);
    emit stitchFinished(result);
}

/**
 * @brief 使用模板匹配寻找重叠区域
 * 原理：取 Base 图底部的一块区域作为模板，在 New 图顶部搜索这个模板。
 */
int ScrollStitcher::calcOffsetByTemplate(const cv::Mat &matBase, const cv::Mat &matNew)
{
    if (matBase.empty() || matNew.empty()) return -1;
    if (matBase.cols != matNew.cols) return -1;

    // 1. 转灰度 (加速匹配，且减少颜色干扰)
    cv::Mat grayBase, grayNew;
    cv::cvtColor(matBase, grayBase, cv::COLOR_RGB2GRAY);
    cv::cvtColor(matNew, grayNew, cv::COLOR_RGB2GRAY);

    // 2. 定义模板 (Template)
    // 从 Base 图的最底部往上取，取高度为 probeHeight 的条带
    // 为了避开底部的滚动条或水印，可以稍微留一点余量，这里取倒数第 150~50 行
    int probeHeight = 100;
    int bottomMargin = 20; // 避开最底部的潜在边框

    // 如果图片太小，做保护
    if (grayBase.rows < probeHeight + bottomMargin || grayNew.rows < probeHeight + bottomMargin) {
        return -1;
    }

    // 截取模板：Base 图底部区域
    cv::Rect templateRect(0, grayBase.rows - probeHeight - bottomMargin, grayBase.cols, probeHeight);

    // 优化：去掉左右两边各 15% 的区域，防止垂直滚动条或侧边栏干扰匹配
    int cropX = templateRect.width * 0.15;
    int cropW = templateRect.width * 0.70;
    templateRect.x += cropX;
    templateRect.width = cropW;

    cv::Mat templ = grayBase(templateRect);

    // 3. 定义搜索区域 (Search Region)
    // 在 New 图的【上半部分】搜索这个模板
    // 我们假设一次滚动不会超过屏幕高度的 80%
    int searchHeight = grayNew.rows * 0.8;
    cv::Rect searchRect(cropX, 0, cropW, searchHeight); // X坐标必须和模板一致
    cv::Mat searchImg = grayNew(searchRect);

    // 4. 执行模板匹配
    int resultCols = searchImg.cols - templ.cols + 1;
    int resultRows = searchImg.rows - templ.rows + 1;
    if (resultCols <= 0 || resultRows <= 0) return -1;

    cv::Mat result;
    // TM_CCOEFF_NORMED 是归一化相关系数匹配，结果 1.0 表示完全匹配，-1 表示完全不匹配
    cv::matchTemplate(searchImg, templ, result, cv::TM_CCOEFF_NORMED);

    // 5. 寻找最佳匹配位置
    double minVal, maxVal;
    cv::Point minLoc, maxLoc;
    cv::minMaxLoc(result, &minVal, &maxVal, &minLoc, &maxLoc);

    // 阈值判断：如果相关系数太低，说明没找到重叠区
    if (maxVal < 0.85) {
        // 匹配度不够，可能页面闪烁或变化太大
        return -1;
    }

    int Y_src = grayBase.rows - probeHeight - bottomMargin;
    int Y_dst = maxLoc.y;

    int scrollDistance = Y_src - Y_dst;

    if (scrollDistance <= 0) return -1;

    int offset = grayBase.rows - scrollDistance;

    if (offset <= 0 || offset >= grayNew.rows) return -1;

    return offset;
}

cv::Mat ScrollStitcher::qImageToCvMat(const QImage &inImage)
{
    QImage image = inImage.convertToFormat(QImage::Format_RGB888);
    cv::Mat mat(image.height(), image.width(), CV_8UC3, (void*)image.constBits(), image.bytesPerLine());
    return mat.clone();
}

QImage ScrollStitcher::cvMatToQImage(const cv::Mat &inMat)
{
    if (inMat.empty()) return QImage();
    QImage img((const uchar*)inMat.data, inMat.cols, inMat.rows, inMat.step, QImage::Format_RGB888);
    return img.copy();
}

// ============================================================================
// ScrollCaptureWindow 实现
// ============================================================================

ScrollCaptureWindow::ScrollCaptureWindow(QWidget *parent)
    : QWidget(parent), m_state(IDLE)
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_DeleteOnClose);

    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);

    m_scrollTimer = new QTimer(this);
    m_scrollTimer->setInterval(400);
    connect(m_scrollTimer, &QTimer::timeout, this, &ScrollCaptureWindow::onScrollTimer);

    m_thread = new QThread(this);
    m_stitcher = new ScrollStitcher;
    m_stitcher->moveToThread(m_thread);

    connect(m_thread, &QThread::finished, m_stitcher, &QObject::deleteLater);
    connect(m_stitcher, &ScrollStitcher::stitchFinished, this, &ScrollCaptureWindow::onStitchResult);

    connect(m_stitcher, &ScrollStitcher::stitchFailed, this, [=](QImage){
        m_sameImgCount++;
        m_isStitching = false;
        if (m_sameImgCount >= 10) {  // 增加失败次数阈值,防止过早的结束
            finishCapture();
        } else {
            // 继续尝试，不立即停止
            qDebug() << "拼接失败，继续尝试，失败次数:" << m_sameImgCount;
        }
    });

    m_thread->start();
}

ScrollCaptureWindow::~ScrollCaptureWindow()
{
    if (m_thread->isRunning()) {
        m_thread->quit();
        m_thread->wait();
    }
}

void ScrollCaptureWindow::startCapture()
{
    QScreen *screen = QApplication::primaryScreen();
    setGeometry(screen->geometry());

    m_state = SELECTING;
    m_stopRequested = false;

    show();
    raise();
    activateWindow();
    setFocus();
    grabKeyboard();

    setCursor(Qt::CrossCursor);
}

void ScrollCaptureWindow::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.fillRect(rect(), QColor(0, 0, 0, 100));

    if (!m_selectRect.isNull()) {
        painter.setCompositionMode(QPainter::CompositionMode_Clear);
        painter.fillRect(m_selectRect, Qt::transparent);
        painter.setCompositionMode(QPainter::CompositionMode_SourceOver);

        if (m_state == SELECTING) {
            painter.setPen(Qt::white);
            QFont font("Microsoft YaHei", 10);
            painter.setFont(font);

            QString tipText = QString("选区大小: %1 x %2").arg(m_selectRect.width()).arg(m_selectRect.height());

            QFontMetrics fm(font);
            int textW = fm.horizontalAdvance(tipText) + 10;
            int textH = fm.height() + 6;
            QRect textBgRect(m_selectRect.x(), m_selectRect.bottom() + 5, textW, textH);

            painter.fillRect(textBgRect, QColor(0, 0, 0, 180));
            painter.drawText(textBgRect, Qt::AlignCenter, tipText);
        }
        else if (m_state == SCROLLING) {
            // 将停止按钮放在选区外部右下角
            int btnWidth = 60;
            int btnHeight = 25;
            int margin = 10; // 与选区的距离

            // 放在选区右下方，与选区保持距离
            m_stopButtonRect = QRect(
                m_selectRect.right() + margin,  // 选区右侧 + 间距
                m_selectRect.bottom() - btnHeight,  // 与选区底部对齐
                btnWidth,
                btnHeight
                );

            // 白色按钮
            painter.fillRect(m_stopButtonRect, QColor(255, 255, 255, 220));

            // 灰色边框
            painter.setPen(QColor(150, 150, 150));
            painter.drawRect(m_stopButtonRect);

            // 停止文字
            painter.setPen(Qt::black);
            painter.drawText(m_stopButtonRect, Qt::AlignCenter, "停止");
        }
    }
}

void ScrollCaptureWindow::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        if (m_state == SELECTING) {
            m_startPoint = event->pos();
            m_selectRect = QRect();
            update();
        }
        else if (m_state == SCROLLING) {
            if (m_stopButtonRect.contains(event->pos())) {
                m_stopRequested = true;
                finishCapture();
            }
        }
    }
}

void ScrollCaptureWindow::mouseMoveEvent(QMouseEvent *event)
{
    if (m_state == SELECTING && (event->buttons() & Qt::LeftButton)) {
        m_selectRect = QRect(m_startPoint, event->pos()).normalized();
        update();
    }
}

void ScrollCaptureWindow::mouseReleaseEvent(QMouseEvent *event)
{
    if (m_state == SELECTING && event->button() == Qt::LeftButton) {
        if (m_selectRect.width() < 50 || m_selectRect.height() < 50) {
            m_selectRect = QRect();
            update();
            return;
        }

        m_state = SCROLLING;
        setCursor(Qt::ArrowCursor);

        m_fullCanvas = QImage();
        m_scrollCount = 0;
        m_sameImgCount = 0;
        m_isStitching = false;
        m_stopRequested = false;

        m_fullCanvas = grabCurrentArea();
        m_scrollTimer->start();
        update();
    }
}

void ScrollCaptureWindow::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) {
        if (m_state == SCROLLING) {
            finishCapture();
        } else {
            close();
        }
    }
}

QImage ScrollCaptureWindow::grabCurrentArea()
{
    QScreen *screen = QApplication::primaryScreen();
    if (!screen) return QImage();

    QImage img = screen->grabWindow(0).copy(m_selectRect).toImage();
    return img;
}

void ScrollCaptureWindow::onScrollTimer()
{
#ifdef Q_OS_WIN
    if (GetAsyncKeyState(VK_LBUTTON) & 0x8000) {
        POINT pt;
        GetCursorPos(&pt);
        QPoint mousePos(pt.x, pt.y);
        mousePos = mapFromGlobal(mousePos);

        if (m_stopButtonRect.contains(mousePos)) {
            m_stopRequested = true;
            finishCapture();
            return;
        }
    }
#endif

    if (m_stopRequested) {
        finishCapture();
        return;
    }
    if (m_isStitching) return;

    // 增加最大滚动次数，避免过早停止
    if (m_scrollCount >= 200) {  // 增加最大滚动次数
        qDebug() << "达到最大滚动次数，停止截图";
        finishCapture();
        return;
    }

    sendMouseWheel(-120);

    QTimer::singleShot(250, this, [=](){
        if (m_state != SCROLLING || m_stopRequested) {
            finishCapture();
            return;
        }

        QImage img = grabCurrentArea();
        if (img.isNull()) {
            qDebug() << "截图失败，继续尝试";
            m_isStitching = false;  // 允许继续尝试
            return;
        }

        m_isStitching = true;
        QMetaObject::invokeMethod(m_stitcher, "processStitch",
                                  Qt::QueuedConnection,
                                  Q_ARG(QImage, m_fullCanvas),
                                  Q_ARG(QImage, img));
        m_scrollCount++;
        qDebug() << "滚动次数:" << m_scrollCount << "，失败次数:" << m_sameImgCount;
    });
}


void ScrollCaptureWindow::onStitchResult(QImage result)
{
    m_fullCanvas = result;
    m_isStitching = false;
    m_sameImgCount = 0;
}

void ScrollCaptureWindow::finishCapture()
{
    if (m_state == IDLE) return;
    m_state = IDLE;
    m_scrollTimer->stop();
    releaseKeyboard();

    emit captureFinished(m_fullCanvas);
    close();
}

void ScrollCaptureWindow::sendMouseWheel(int delta)
{
#ifdef Q_OS_WIN
    INPUT input;
    ZeroMemory(&input, sizeof(input));
    input.type = INPUT_MOUSE;
    input.mi.dwFlags = MOUSEEVENTF_WHEEL;
    input.mi.mouseData = delta;
    input.mi.time = 0;

    SendInput(1, &input, sizeof(INPUT));
#endif
}
