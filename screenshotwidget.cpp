#include "screenshotwidget.h"
#include <QPainter>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QScreen>
#include <QGuiApplication>
#include <QApplication>
#include <QClipboard>
#include <QFileDialog>
#include <QStandardPaths>
#include <QDateTime>
#include <QDebug>
#include <QTimer>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDesktopServices>
#include <cmath>


ScreenshotWidget::ScreenshotWidget(QWidget *parent)
    : QWidget(parent), selecting(false), selected(false), currentDrawMode(None), toolbar(nullptr), devicePixelRatio(1.0), showMagnifier(false), isDrawing(false)

{
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
    setAttribute(Qt::WA_TranslucentBackground);
    setMouseTracking(true);
    setCursor(Qt::CrossCursor);
    setFocusPolicy(Qt::StrongFocus); // 确保窗口能接收键盘事件

    setupToolbar();
    //setupMosaicToolbar();
    // 创建尺寸标签
    sizeLabel = new QLabel(this);
    sizeLabel->setStyleSheet("QLabel { background-color: rgba(0, 0, 0, 180); color: white; "
                             "padding: 5px; border-radius: 3px; font-size: 12px; }");
    sizeLabel->hide();
}

ScreenshotWidget::~ScreenshotWidget()
{
}

void ScreenshotWidget::setupToolbar()
{
    // 主工具栏设置
    toolbar = new QWidget(this);
    toolbar->setStyleSheet(
        "QWidget { background-color: rgba(40, 40, 40, 200); border-radius: 5px; }"
        "QPushButton { background-color: rgba(60, 60, 60, 255); color: white; "
        "border: none; padding: 8px 15px; border-radius: 3px; font-size: 13px; }"
        "QPushButton:hover { background-color: rgba(80, 80, 80, 255); }"
        "QPushButton:pressed { background-color: rgba(50, 50, 50, 255); }"
        "QLabel { background-color: transparent; color: white; padding: 5px; font-size: 12px; }");

    QHBoxLayout *layout = new QHBoxLayout(toolbar);
    layout->setSpacing(5);
    layout->setContentsMargins(10, 5, 10, 5);

    // 绘制工具
    btnRect = new QPushButton("矩形", toolbar);
    btnArrow = new QPushButton("箭头", toolbar);
    btnText = new QPushButton("文字", toolbar);
    btnPen = new QPushButton("画笔", toolbar);
    btnMosaic = new QPushButton("马赛克", toolbar);
    btnBlur = new QPushButton("高斯模糊", toolbar);  // 新增模糊按钮
    // 操作按钮
    btnSave = new QPushButton("保存", toolbar);
    btnCopy = new QPushButton("复制", toolbar);
    btnCancel = new QPushButton("取消", toolbar);

    layout->addWidget(btnRect);
    layout->addWidget(btnArrow);
    layout->addWidget(btnText);
    layout->addWidget(btnPen);
    layout->addWidget(btnMosaic);//马赛克按钮
    layout->addWidget(btnBlur);  // 高斯模糊按钮
    layout->addSpacing(10);
    layout->addWidget(btnSave);
    layout->addWidget(btnCopy);
    layout->addWidget(btnCancel);

    // 子工具栏（马赛克强度调节）
    EffectToolbar = new QWidget(this);
    EffectToolbar->setStyleSheet(
        "QWidget { background-color: rgba(40, 40, 40, 200); border-radius: 5px; }"
        "QPushButton { background-color: rgba(60, 60, 60, 255); color: white; "
        "border: none; padding: 5px; border-radius: 3px; font-size: 13px; }"
        "QPushButton:hover { background-color: rgba(80, 80, 80, 255); }"
        "QPushButton:pressed { background-color: rgba(50, 50, 50, 255); }"
        "QLabel { background-color: transparent; color: white; padding: 5px; font-size: 12px; }");

    QHBoxLayout *EffectLayout = new QHBoxLayout(EffectToolbar);
    EffectLayout->setSpacing(5);
    EffectLayout->setContentsMargins(16, 8, 16, 8);

    btnStrengthDown = new QPushButton("-", EffectToolbar);
    strengthLabel = new QLabel("20(强）", EffectToolbar);
    btnStrengthUp = new QPushButton("+", EffectToolbar);

    EffectLayout->addWidget(new QLabel("模糊强度:", EffectToolbar));
    EffectLayout->addWidget(btnStrengthDown);
    EffectLayout->addWidget(strengthLabel);
    EffectLayout->addWidget(btnStrengthUp);


    // 连接信号与槽
    connect(btnSave, &QPushButton::clicked, this, &ScreenshotWidget::saveScreenshot);
    connect(btnCopy, &QPushButton::clicked, this, &ScreenshotWidget::copyToClipboard);
    connect(btnCancel, &QPushButton::clicked, this, &ScreenshotWidget::cancelCapture);

    connect(btnRect, &QPushButton::clicked, this, [this]()
            { currentDrawMode = Rectangle; toolbar->show(); EffectToolbar->hide(); });
    connect(btnArrow, &QPushButton::clicked, this, [this]()
            { currentDrawMode = Arrow; toolbar->show(); EffectToolbar->hide(); });
    connect(btnText, &QPushButton::clicked, this, [this]()
            { currentDrawMode = Text; toolbar->show(); EffectToolbar->hide(); });
    connect(btnPen, &QPushButton::clicked, this, [this]()
            { currentDrawMode = Pen; toolbar->show(); EffectToolbar->hide(); });
    connect(btnMosaic, &QPushButton::clicked, this, [this]()
            {
                currentDrawMode = Mosaic;
                toolbar->show(); // 保持主工具栏显示

                // 切换马赛克工具栏的显示状态
                // if (EffectToolbar->isVisible()) {
                //     EffectToolbar->hide();
                // } else {
                //     // 获取马赛克按钮在屏幕中的位置
                //     QPoint mosaicBtnPos = btnMosaic->mapToGlobal(QPoint(0, 0));
                //     // 转换为当前widget的坐标
                //     QPoint localPos = this->mapFromGlobal(mosaicBtnPos);

                //     // 将马赛克工具栏放在马赛克按钮正下方
                //     int x = localPos.x();
                //     int y = localPos.y() + btnMosaic->height() + 5;

                //     EffectToolbar->move(x, y);
                //     EffectToolbar->show();
                //     EffectToolbar->raise();

                //     // 如果有已绘制的马赛克区域，立即更新预览
                //     if (!EffectAreas.isEmpty()) {
                //         update();
                //     }
                // }

            });
    connect(btnBlur, &QPushButton::clicked, this, [this]()
            {
                currentDrawMode = Blur;
                toolbar->show(); // 保持主工具栏显示

                // // 切换马赛克工具栏的显示状态
                // if (EffectToolbar->isVisible()) {
                //     EffectToolbar->hide();
                // } else {
                //     // 获取马赛克按钮在屏幕中的位置
                //     QPoint BlurBtnPos = btnBlur->mapToGlobal(QPoint(0, 0));
                //     // 转换为当前widget的坐标
                //     QPoint localPos = this->mapFromGlobal(BlurBtnPos);

                //     // 将马赛克工具栏放在马赛克按钮正下方
                //     int x = localPos.x();
                //     int y = localPos.y() + btnBlur->height() + 5;

                //     EffectToolbar->move(x, y);
                //     EffectToolbar->show();
                //     EffectToolbar->raise();

                //     // 如果有已绘制的马赛克区域，立即更新预览
                //     if (!EffectAreas.isEmpty()) {
                //         update();
                //     }
                // }

            });
    connect(btnStrengthUp, &QPushButton::clicked, this, &ScreenshotWidget::increaseEffectStrength);
    connect(btnStrengthDown, &QPushButton::clicked, this, &ScreenshotWidget::decreaseEffectStrength);

    toolbar->adjustSize();
    EffectToolbar->adjustSize();

    // 默认隐藏所有工具栏
    toolbar->hide();
    EffectToolbar->hide();
}

void ScreenshotWidget::increaseEffectStrength()
{
    if (currentEffectStrength < 30) { // 最大强度限制
        currentEffectStrength += 2;
        updateStrengthLabel();

        // 实时更新预览：如果正在绘制马赛克区域，更新最后一个区域的强度
        if ((currentDrawMode == Mosaic || currentDrawMode == Blur) && !EffectAreas.isEmpty()) {
            // 更新最后一个马赛克区域的强度
            if (!EffectStrengths.isEmpty()) {
                EffectStrengths.last() = currentEffectStrength;
            }
        }
        update(); // 刷新显示
    }
}

void ScreenshotWidget::decreaseEffectStrength()
{
    if (currentEffectStrength > 2) { // 最小强度限制
        currentEffectStrength -= 2;
        updateStrengthLabel();

        // 实时更新预览：如果正在绘制马赛克区域，更新最后一个区域的强度
        if ((currentDrawMode == Mosaic || currentDrawMode == Blur) && !EffectAreas.isEmpty()) {
            // 更新最后一个马赛克区域的强度
            if (!EffectStrengths.isEmpty()) {
                EffectStrengths.last() = currentEffectStrength;
            }
        }
        update(); // 刷新显示
    }
}

void ScreenshotWidget::updateStrengthLabel()
{
    QString strengthText;
    if (currentEffectStrength <= 8) {
        strengthText = "弱";
    } else if (currentEffectStrength <= 20) {
        strengthText = "中";
    } else {
        strengthText = "强";
    }
    strengthLabel->setText(QString("%1 (%2)").arg(currentEffectStrength).arg(strengthText));
}

QPixmap ScreenshotWidget::applyEffect(const QPixmap &source, const QRect &area, int strength, DrawMode mode)
{
    if (area.isEmpty() || strength <= 0) {
        return source;
    }

    switch (mode) {
    case Mosaic:
        return applyMosaic(source, area, strength);
    case Blur:
        return applyBlur(source, area, strength);
    default:
        return source;
    }
}

QPixmap ScreenshotWidget::applyMosaic(const QPixmap &source, const QRect &area, int blockSize)
{
    if (area.isEmpty() || blockSize <= 0) {
        return source;
    }

    QPixmap result = source;
    QPainter painter(&result);
    painter.setRenderHint(QPainter::Antialiasing, false);

    // 确保马赛克区域在图片范围内
    QRect validArea = area.intersected(source.rect());

    // 遍历马赛克区域，按块处理
    for (int x = validArea.left(); x < validArea.right(); x += blockSize) {
        for (int y = validArea.top(); y < validArea.bottom(); y += blockSize) {
            // 计算当前块的实际大小（边缘可能小于blockSize）
            int currentBlockWidth = qMin(blockSize, validArea.right() - x);
            int currentBlockHeight = qMin(blockSize, validArea.bottom() - y);

            // 获取当前块的平均颜色
            QRect blockRect(x, y, currentBlockWidth, currentBlockHeight);
            QImage blockImage = source.copy(blockRect).toImage();

            // 计算平均颜色
            int totalRed = 0, totalGreen = 0, totalBlue = 0;
            int pixelCount = currentBlockWidth * currentBlockHeight;

            for (int i = 0; i < currentBlockWidth; ++i) {
                for (int j = 0; j < currentBlockHeight; ++j) {
                    QColor color = blockImage.pixelColor(i, j);
                    totalRed += color.red();
                    totalGreen += color.green();
                    totalBlue += color.blue();
                }
            }

            QColor averageColor(
                totalRed / pixelCount,
                totalGreen / pixelCount,
                totalBlue / pixelCount
                );

            // 用平均颜色填充整个块
            painter.fillRect(blockRect, averageColor);
        }
    }

    painter.end();
    return result;
}
// 高斯模糊效果算法
QPixmap ScreenshotWidget::applyBlur(const QPixmap &source, const QRect &area, int radius)
{
    if (area.isEmpty() || radius <= 0) {
        return source;
    }

    QPixmap result = source;

    // 确保模糊区域在图片范围内
    QRect validArea = area.intersected(source.rect());
    if (validArea.isEmpty()) {
        return result;
    }

    // 提取要模糊的区域
    QImage sourceImage = source.copy(validArea).toImage();
    QImage blurredImage(sourceImage.size(), QImage::Format_ARGB32);

    // 计算高斯核
    int kernelSize = radius * 2 + 1;
    QVector<double> kernel(kernelSize);
    double sigma = radius / 3.0;  // 标准差
    double sum = 0.0;

    // 生成高斯核
    for (int i = 0; i < kernelSize; ++i) {
        int x = i - radius;
        kernel[i] = qExp(-(x * x) / (2 * sigma * sigma)) / (qSqrt(2 * M_PI) * sigma);
        sum += kernel[i];
    }

    // 归一化
    for (int i = 0; i < kernelSize; ++i) {
        kernel[i] /= sum;
    }

    // 水平模糊
    QImage tempImage(sourceImage.size(), QImage::Format_ARGB32);
    for (int y = 0; y < sourceImage.height(); ++y) {
        for (int x = 0; x < sourceImage.width(); ++x) {
            double r = 0, g = 0, b = 0, a = 0;

            for (int i = -radius; i <= radius; ++i) {
                int sampleX = qBound(0, x + i, sourceImage.width() - 1);
                QColor color = sourceImage.pixelColor(sampleX, y);

                double weight = kernel[i + radius];
                r += color.red() * weight;
                g += color.green() * weight;
                b += color.blue() * weight;
                a += color.alpha() * weight;
            }

            tempImage.setPixelColor(x, y, QColor(
                                              qBound(0, int(r), 255),
                                              qBound(0, int(g), 255),
                                              qBound(0, int(b), 255),
                                              qBound(0, int(a), 255)
                                              ));
        }
    }

    // 垂直模糊
    for (int x = 0; x < tempImage.width(); ++x) {
        for (int y = 0; y < tempImage.height(); ++y) {
            double r = 0, g = 0, b = 0, a = 0;

            for (int i = -radius; i <= radius; ++i) {
                int sampleY = qBound(0, y + i, tempImage.height() - 1);
                QColor color = tempImage.pixelColor(x, sampleY);

                double weight = kernel[i + radius];
                r += color.red() * weight;
                g += color.green() * weight;
                b += color.blue() * weight;
                a += color.alpha() * weight;
            }

            blurredImage.setPixelColor(x, y, QColor(
                                                 qBound(0, int(r), 255),
                                                 qBound(0, int(g), 255),
                                                 qBound(0, int(b), 255),
                                                 qBound(0, int(a), 255)
                                                 ));
        }
    }

    // 将模糊后的图像绘制回原图
    QPainter painter(&result);
    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.drawImage(validArea, blurredImage);
    painter.end();

    return result;
}
void ScreenshotWidget::startCapture()
{
    // 获取鼠标当前位置所在的屏幕
    QPoint cursorPos = QCursor::pos();
    QScreen *currentScreen = nullptr;
    
    QList<QScreen *> screens = QGuiApplication::screens();
    for (QScreen *scr : screens)
    {
        if (scr->geometry().contains(cursorPos))
        {
            currentScreen = scr;
            break;
        }
    }
    
    // 如果没有找到，使用主屏幕
    if (!currentScreen)
    {
        currentScreen = QGuiApplication::primaryScreen();
    }
    
    if (currentScreen)
    {
        devicePixelRatio = currentScreen->devicePixelRatio();
        
        // 获取当前屏幕的几何信息
        QRect screenGeometry = currentScreen->geometry();
        
        // 保存屏幕的原点位置
        virtualGeometryTopLeft = screenGeometry.topLeft();
        
        // 只截取当前屏幕
        screenPixmap = currentScreen->grabWindow(0);
        
        // 设置窗口标志以绕过窗口管理器
        setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool | Qt::BypassWindowManagerHint);
        
        // 设置窗口大小和位置为当前屏幕
        setGeometry(screenGeometry);
        
        // 直接显示，不使用全屏模式
        show();
        
        // 确保窗口获得焦点以接收键盘事件
        setFocus();
        activateWindow();
        raise();
        
        selecting = false;
        selected = false;
        selectedRect = QRect();
        showMagnifier = true; // 在截图开始时就启用放大镜
    }
}

void ScreenshotWidget::startCaptureFullScreen()
{
    // 先启动常规截图
    startCapture();

    // 然后立即设置为全屏模式
    QTimer::singleShot(150, this, [this]()
                       {
                           selectedRect = rect();
                           selected = true;
                           selecting = false;

                           toolbar->setParent(this);
                           toolbar->adjustSize();
                           updateToolbarPosition();
                           toolbar->setWindowFlags(Qt::Widget);
                           toolbar->raise();
                           toolbar->show();
                           toolbar->activateWindow();

                           update(); });
}

void ScreenshotWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);

    // 绘制背景截图
    // screenPixmap包含物理像素，需要缩放到窗口大小（逻辑像素）
    // 窗口坐标是相对于虚拟桌面的，所以需要考虑偏移
    QRect windowRect = rect();
    QRect sourceRect(0, 0, screenPixmap.width(), screenPixmap.height());
    painter.drawPixmap(windowRect, screenPixmap, sourceRect);

    // 绘制半透明遮罩
    painter.fillRect(rect(), QColor(0, 0, 0, 100));

    // 如果有选中区域，显示选中区域的原始图像
    if (selecting || selected)
    {
        QRect currentRect;
        if (selecting)
        {
            currentRect = QRect(startPoint, endPoint).normalized();
        }
        else
        {
            currentRect = selectedRect;
        }

        if (!currentRect.isEmpty())
        {
            // 显示选中区域的原始图像
            // 将窗口坐标转换为截图坐标（考虑虚拟桌面偏移）
            QPoint windowPos = geometry().topLeft();
            QPoint offset = windowPos - virtualGeometryTopLeft;
            
            QRect physicalRect(
                (currentRect.x() + offset.x()) * devicePixelRatio,
                (currentRect.y() + offset.y()) * devicePixelRatio,
                currentRect.width() * devicePixelRatio,
                currentRect.height() * devicePixelRatio);
            painter.drawPixmap(currentRect, screenPixmap, physicalRect);

            // 绘制选中框
            QPen pen(QColor(0, 150, 255), 2);
            painter.setPen(pen);
            painter.drawRect(currentRect);

            // 绘制尺寸信息
            QString sizeText = QString("%1 x %2").arg(currentRect.width()).arg(currentRect.height());
            sizeLabel->setText(sizeText);

            // 调整标签位置
            int labelX = currentRect.x();
            int labelY = currentRect.y() - sizeLabel->height() - 5;
            if (labelY < 0)
            {
                labelY = currentRect.y() + 5;
            }
            sizeLabel->move(labelX, labelY);
            sizeLabel->adjustSize();
            sizeLabel->show();

            // 绘制调整手柄
            if (selected)
            {
                int handleSize = 8;
                painter.setBrush(QColor(0, 150, 255));
                painter.setPen(Qt::white);

                // 四个角
                painter.drawRect(currentRect.left() - handleSize / 2, currentRect.top() - handleSize / 2,
                                 handleSize, handleSize);
                painter.drawRect(currentRect.right() - handleSize / 2, currentRect.top() - handleSize / 2,
                                 handleSize, handleSize);
                painter.drawRect(currentRect.left() - handleSize / 2, currentRect.bottom() - handleSize / 2,
                                 handleSize, handleSize);
                painter.drawRect(currentRect.right() - handleSize / 2, currentRect.bottom() - handleSize / 2,
                                 handleSize, handleSize);
            }
        }
    }
    // 在 paintEvent 函数中，修改模糊区域的绘制部分：
    if (!EffectAreas.isEmpty() && selected)
    {
        for (int i = 0; i < EffectAreas.size(); ++i) {
            const QRect &area = EffectAreas[i];
            int strength =EffectStrengths[i]; // 获取对应的强度值
             DrawMode mode = effectTypes[i];  // 获取保存的效果类型

            // 将 area 转为物理坐标（用于从 screenPixmap 取图）
            QRect logicalArea = area.intersected(selectedRect); // 限制在选区内
            if (logicalArea.isEmpty()) continue;

            QRect physicalArea(
                logicalArea.x() * devicePixelRatio,
                logicalArea.y() * devicePixelRatio,
                logicalArea.width() * devicePixelRatio,
                logicalArea.height() * devicePixelRatio);

            // 从原始截图裁剪该区域
            QPixmap sourcePart = screenPixmap.copy(physicalArea);
            // 应用效果，传入强度值
            QPixmap EffectPart = applyEffect(sourcePart, sourcePart.rect(), strength, mode);
            // 绘制回逻辑坐标位置
            painter.drawPixmap(logicalArea, EffectPart);
        }
    }

    // 修改正在拖拽的模糊区域预览：
    if ((currentDrawMode == Mosaic || currentDrawMode == Blur) && drawingEffect && selected)
    {
        QRect currentEffectRect = QRect(EffectStartPoint, EffectEndPoint).normalized()
        .intersected(selectedRect);
        if (!currentEffectRect.isEmpty())
        {
            QRect physicalRect(
                currentEffectRect.x() * devicePixelRatio,
                currentEffectRect.y() * devicePixelRatio,
                currentEffectRect.width() * devicePixelRatio,
                currentEffectRect.height() * devicePixelRatio);

            QPixmap sourcePart = screenPixmap.copy(physicalRect);
            // 使用当前强度值预览
            QPixmap EffectPreview = applyEffect(sourcePart, sourcePart.rect(), currentEffectStrength, currentDrawMode);

            painter.drawPixmap(currentEffectRect, EffectPreview);

            // 可选：再叠加一个半透明红框表示边界
            painter.setPen(QPen(QColor(255, 0, 0, 100), 2, Qt::DashLine));
            painter.setBrush(Qt::NoBrush);
            painter.drawRect(currentEffectRect);
        }
    }
    // 绘制放大镜
    if (showMagnifier && !selected)
    {
        int magnifierSize = 120; // 放大镜大小
        int magnifierScale = 4;  // 放大倍数

        // 计算放大镜位置(在鼠标右下方)
        int magnifierX = currentMousePos.x() + 20;
        int magnifierY = currentMousePos.y() + 20;

        // 确保放大镜不超出屏幕
        if (magnifierX + magnifierSize > width())
            magnifierX = currentMousePos.x() - magnifierSize - 20;
        if (magnifierY + magnifierSize > height())
            magnifierY = currentMousePos.y() - magnifierSize - 20;

        // 从原始截图中获取鼠标位置附近的区域（物理像素）
        int sourceSize = magnifierSize / magnifierScale;
        QRect logicalSourceRect(
            currentMousePos.x() - sourceSize / 2,
            currentMousePos.y() - sourceSize / 2,
            sourceSize,
            sourceSize);

        // 确保源区域在窗口范围内
        logicalSourceRect = logicalSourceRect.intersected(QRect(0, 0, width(), height()));

        // 转换为物理像素坐标（考虑虚拟桌面偏移）
        QPoint windowPos = geometry().topLeft();
        QPoint offset = windowPos - virtualGeometryTopLeft;
        
        QRect physicalSourceRect(
            (logicalSourceRect.x() + offset.x()) * devicePixelRatio,
            (logicalSourceRect.y() + offset.y()) * devicePixelRatio,
            logicalSourceRect.width() * devicePixelRatio,
            logicalSourceRect.height() * devicePixelRatio);

        if (!physicalSourceRect.isEmpty())
        {
            // 绘制放大镜背景
            painter.setPen(QPen(QColor(0, 150, 255), 2));
            painter.setBrush(Qt::white);
            painter.drawRect(magnifierX, magnifierY, magnifierSize, magnifierSize);

            // 绘制放大的图像
            QRect targetRect(magnifierX, magnifierY, magnifierSize, magnifierSize);
            painter.drawPixmap(targetRect, screenPixmap, physicalSourceRect);

            // 绘制十字准星
            painter.setPen(QPen(Qt::red, 1));
            int centerX = magnifierX + magnifierSize / 2;
            int centerY = magnifierY + magnifierSize / 2;
            painter.drawLine(centerX - 10, centerY, centerX + 10, centerY);
            painter.drawLine(centerX, centerY - 10, centerX, centerY + 10);
        }
    }

    // 绘制已完成的箭头
    for (const DrawnArrow &arrow : arrows)
    {
        drawArrow(painter, arrow.start, arrow.end, arrow.color, arrow.width);
    }

    // 绘制已完成的矩形
    painter.setPen(QPen(Qt::red, 2));
    painter.setBrush(Qt::NoBrush);
    for (const DrawnRectangle &rect : rectangles)
    {
        painter.setPen(QPen(rect.color, rect.width));
        painter.drawRect(rect.rect);
    }

    // 绘制当前正在绘制的形状
    if (isDrawing && selected)
    {
        if (currentDrawMode == Arrow)
        {
            drawArrow(painter, drawStartPoint, drawEndPoint, QColor(255, 0, 0), 3);
        }
        else if (currentDrawMode == Rectangle)
        {
            painter.setPen(QPen(QColor(255, 0, 0), 3));
            painter.setBrush(Qt::NoBrush);
            painter.drawRect(QRect(drawStartPoint, drawEndPoint).normalized());
        }
    }
}

void ScreenshotWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
    {
        // 如果已经选中区域且处于图形绘制模式
        if (selected && (currentDrawMode == Rectangle || currentDrawMode == Arrow))
        {
            isDrawing = true;
            drawStartPoint = event->pos();
            drawEndPoint = event->pos();
        }
        //如果已经选中区域且处于马赛克或者高斯模糊模式
        else if ((currentDrawMode == Mosaic || currentDrawMode == Blur)&& selected) {
            // 开始绘制马赛克和高斯模糊的区域
            EffectStartPoint = event->pos();
            EffectEndPoint = event->pos();
            drawingEffect = true;
            EffectToolbar->hide(); // 开始绘制时隐藏强度工具栏
        } else {
            // 否则开始选择选择新区域

            startPoint = event->pos();
            endPoint = event->pos();
            currentMousePos = event->pos();
            selecting = true;
            selected = false;
            // showMagnifier已经在startCapture时设置为true，这里不需要重复设置
            toolbar->hide();
            showMagnifier = true;
            toolbar->hide();
            EffectAreas.clear(); // 清除之前的模糊区域
            EffectToolbar->hide(); // 隐藏马赛克工具栏
            EffectAreas.clear();
            EffectStrengths.clear();
            effectTypes.clear();  // 清除效果类型
        }
        update();
    }
}

void ScreenshotWidget::mouseMoveEvent(QMouseEvent *event)
{
    currentMousePos = event->pos();

    if (selecting)
    {
        endPoint = event->pos();
        showMagnifier = true;
        update();
    }
    else if (isDrawing)
    {
        drawEndPoint = event->pos();
        update();
    }
    else if (drawingEffect &&(currentDrawMode == Mosaic || currentDrawMode == Blur) )
    {
        EffectEndPoint = event->pos();
        update();
    }
    else if (!selected)
    {
        // 在框选前的鼠标移动时也触发更新，以显示放大镜
        update();
    }
}


void ScreenshotWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
    {
        if (drawingEffect && (currentDrawMode == Mosaic || currentDrawMode == Blur)) {
            // 完成模糊区域绘制
            drawingEffect = false;
            QRect EffectRect = QRect(EffectStartPoint, EffectEndPoint).normalized();
            if (EffectRect.width() > 5 && EffectRect.height() > 5) {
                EffectAreas.append(EffectRect);
                EffectStrengths.append(currentEffectStrength); // 保存当前强度
                effectTypes.append(currentDrawMode);  // 保存效果类型
                // 显示强度调节工具栏
                updateEffectToolbarPosition();
                EffectToolbar->show();
                EffectToolbar->raise();
                // 立即更新显示，确保新区域可见
                update();
            }
        }
        else if (isDrawing)
        {
            isDrawing = false;
            drawEndPoint = event->pos();

            // 保存绘制的形状
            if (currentDrawMode == Arrow)
            {
                DrawnArrow arrow;
                arrow.start = drawStartPoint;
                arrow.end = drawEndPoint;
                arrow.color = QColor(255, 0, 0);
                arrow.width = 3;
                arrows.append(arrow);
            }
            else if (currentDrawMode == Rectangle)
            {
                DrawnRectangle rect;
                rect.rect = QRect(drawStartPoint, drawEndPoint).normalized();
                rect.color = QColor(255, 0, 0);
                rect.width = 3;
                rectangles.append(rect);
            }

            update();
        }
        //else if(selecting)
        else  {
            // 原有选择逻辑
            selecting = false;
            selected = true;
            showMagnifier = false;
            selectedRect = QRect(startPoint, endPoint).normalized();
            EffectAreas.clear();
            EffectStrengths.clear();
            EffectToolbar->hide();

            if (!selectedRect.isEmpty()) {
                updateToolbarPosition();
                toolbar->show();
            }
        }
        update();
    }
}


void ScreenshotWidget::keyPressEvent(QKeyEvent *event)
{
    qDebug() << "Key pressed:" << event->key() << "selecting:" << selecting << "selected:" << selected;

    if (event->key() == Qt::Key_Escape)
    {
        cancelCapture();
    }
    else if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)
    {
        if (selected && !selectedRect.isEmpty())
        {
            copyToClipboard();
        }
        else if (!selecting && !selected)
        {
            // 截取全屏
            selectedRect = rect();
            selected = true;
            selecting = false;

            qDebug() << "Full screen capture:" << selectedRect;
            qDebug() << "Window size:" << size();

            // 确保工具栏大小正确
            toolbar->adjustSize();
            qDebug() << "Toolbar size after adjust:" << toolbar->size();
            qDebug() << "Toolbar sizeHint:" << toolbar->sizeHint();

            updateToolbarPosition();
            qDebug() << "Toolbar position:" << toolbar->pos();

            toolbar->raise(); // 确保工具栏在最上层
            toolbar->show();

            qDebug() << "Toolbar visible:" << toolbar->isVisible();
            qDebug() << "Toolbar geometry:" << toolbar->geometry();

            update();
        }
    }
}
void ScreenshotWidget::updateEffectToolbarPosition()
{
    if (EffectAreas.isEmpty()) return;

    // 获取最后一个马赛克区域的位置
    QRect lastEffectArea = EffectAreas.last();

    int toolbarWidth = EffectToolbar->sizeHint().width();
    int toolbarHeight = EffectToolbar->sizeHint().height();

    // 将工具栏放在马赛克区域上方
    int x = lastEffectArea.x();
    int y = lastEffectArea.y() - toolbarHeight - 5;

    // 如果上方空间不够，放在下方
    if (y < 0) {
        y = lastEffectArea.bottom() + 5;
    }

    // 确保不超出屏幕边界
    if (x + toolbarWidth > width()) {
        x = width() - toolbarWidth - 5;
    }
    if (x < 5) x = 5;
    if (y + toolbarHeight > height()) {
        y = height() - toolbarHeight - 5;
    }

    EffectToolbar->move(x, y);
}
void ScreenshotWidget::cancelCapture()
{
    close(); // 示例实现
}

void ScreenshotWidget::updateToolbarPosition()
{
    if (!selected || selectedRect.isEmpty())
    {
        return;
    }

    int toolbarWidth = toolbar->sizeHint().width();
    int toolbarHeight = toolbar->sizeHint().height();

    // 获取当前屏幕的可用区域（避开 Dock 和菜单栏）
    QScreen *screen = QGuiApplication::screenAt(geometry().center());
    if (!screen) screen = QGuiApplication::primaryScreen();
    
    // availableGeometry 是全局坐标
    QRect availableGeometry = screen->availableGeometry();
    
    // 窗口左上角的全局坐标
    QPoint windowTopLeft = geometry().topLeft();
    
    // 可用区域相对于窗口的底部 Y 坐标
    int availableBottomY = availableGeometry.bottom() - windowTopLeft.y();

    int x, y;

    // 如果是全屏截图或接近全屏，将工具栏放在屏幕底部中央偏上
    if (selectedRect.width() >= width() - 10 && selectedRect.height() >= height() - 10)
    {
        x = (width() - toolbarWidth) / 2;
        // 使用 availableBottomY 确保不被 Dock 遮挡
        y = availableBottomY - toolbarHeight - 20; 
    }
    else
    {
        // 尝试将工具栏放在选中区域下方
        x = selectedRect.x() + (selectedRect.width() - toolbarWidth) / 2;
        y = selectedRect.bottom() + 10;

        // 如果超出可用区域底部，则放在选中区域上方
        if (y + toolbarHeight > availableBottomY)
        {
            y = selectedRect.top() - toolbarHeight - 10;
        }

        // 确保不超出屏幕左右边界
        if (x < 10)
            x = 10;
        if (x + toolbarWidth > width() - 10)
        {
            x = width() - toolbarWidth - 10;
        }
    }

    toolbar->move(x, y);
}

void ScreenshotWidget::saveScreenshot()
{
    if (!selected || selectedRect.isEmpty())
    {
        return;
    }

    qDebug() << "=== Save Screenshot Debug Info ===";
    qDebug() << "Device Pixel Ratio:" << devicePixelRatio;
    qDebug() << "Selected Rect (Logical):" << selectedRect;
    qDebug() << "Screen Pixmap Size:" << screenPixmap.size();
    qDebug() << "Screen Pixmap DPR:" << screenPixmap.devicePixelRatio();

    // 从原始截图中裁剪选中区域
    // screenPixmap中存储的是物理像素，需要将逻辑坐标转换为物理坐标
    // 使用 qRound 避免精度丢失
    int x = qRound(selectedRect.x() * devicePixelRatio);
    int y = qRound(selectedRect.y() * devicePixelRatio);
    int w = qRound(selectedRect.width() * devicePixelRatio);
    int h = qRound(selectedRect.height() * devicePixelRatio);
    
    QRect physicalRect(x, y, w, h);
    qDebug() << "Physical Rect:" << physicalRect;


    // 从原始像素数据中裁剪
    QImage tempImage = screenPixmap.toImage();
    qDebug() << "Temp Image Size:" << tempImage.size();
    
    QImage croppedImage = tempImage.copy(physicalRect);
    QPixmap croppedPixmap = QPixmap::fromImage(croppedImage);
    
    // 强制将 DPR 设置为 1.0，以便我们直接在物理像素上进行绘制
    // 这样可以避免 QPainter 自动应用 DPR 导致的双重缩放
    croppedPixmap.setDevicePixelRatio(1.0);
    
    qDebug() << "Cropped Pixmap Size:" << croppedPixmap.size();
    qDebug() << "Cropped Pixmap DPR:" << croppedPixmap.devicePixelRatio();

    // 在裁剪后的图片上绘制箭头和矩形
    QPainter painter(&croppedPixmap);
    painter.setRenderHint(QPainter::Antialiasing);

    // 绘制所有箭头（需要调整坐标到裁剪区域）
    for (const DrawnArrow &arrow : arrows)
    {
        // 计算相对于选中区域左上角的逻辑坐标
        QPointF relativeStart = arrow.start - selectedRect.topLeft();
        QPointF relativeEnd = arrow.end - selectedRect.topLeft();
        
        // 转换为物理坐标
        QPointF adjustedStart = relativeStart * devicePixelRatio;
        QPointF adjustedEnd = relativeEnd * devicePixelRatio;
        
        qDebug() << "Arrow:" << arrow.start << "->" << arrow.end;
        qDebug() << "Relative:" << relativeStart << "->" << relativeEnd;
        qDebug() << "Adjusted (Physical):" << adjustedStart << "->" << adjustedEnd;

        drawArrow(painter, adjustedStart, adjustedEnd, arrow.color, arrow.width * devicePixelRatio, devicePixelRatio);
    }

    // 绘制所有矩形
    for (const DrawnRectangle &rect : rectangles)
    {
        QRectF adjustedRect(
            (rect.rect.x() - selectedRect.x()) * devicePixelRatio,
            (rect.rect.y() - selectedRect.y()) * devicePixelRatio,
            rect.rect.width() * devicePixelRatio,
            rect.rect.height() * devicePixelRatio
        );
        painter.setPen(QPen(rect.color, rect.width * devicePixelRatio));
        painter.setBrush(Qt::NoBrush);
        painter.drawRect(adjustedRect);
    }

    painter.end();

    // 应用马赛克效果（使用各自保存的强度）
    if (!EffectAreas.isEmpty()) {
        QPixmap processedPixmap = croppedPixmap;

        for (int i = 0; i < EffectAreas.size(); ++i) {
            const QRect &EffectArea =EffectAreas[i];
            int strength = EffectStrengths[i];
            DrawMode mode = effectTypes[i];  // 使用保存的效果类型

            QRect relativeEffectArea = EffectArea.translated(-selectedRect.topLeft());
            relativeEffectArea = QRect(
                relativeEffectArea.x() * devicePixelRatio,
                relativeEffectArea.y() * devicePixelRatio,
                relativeEffectArea.width() * devicePixelRatio,
                relativeEffectArea.height() * devicePixelRatio
                );

             processedPixmap = applyEffect(processedPixmap, relativeEffectArea, strength, mode);
        }
        croppedPixmap = processedPixmap;
    }


    // 获取默认保存路径
    QString defaultPath = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    QString defaultFileName = defaultPath + "/screenshot_" +
                              QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss") + ".png";

    // 打开保存对话框
    QString fileName = QFileDialog::getSaveFileName(this,
                                                    "保存截图",
                                                    defaultFileName,
                                                    "PNG图片 (*.png);;JPEG图片 (*.jpg);;所有文件 (*.*)");

    if (!fileName.isEmpty())
    {
        if (croppedPixmap.save(fileName))
        {
            emit screenshotTaken();
            hide();
            QApplication::quit();
        }
    }
    // 如果用户取消保存，不做任何操作，保持当前状态（工具栏仍然可见）
    else
    {
        hide();
        QApplication::quit();
    }
}

void ScreenshotWidget::copyToClipboard()
{
    if (!selected || selectedRect.isEmpty())
    {
        return;
    }

    qDebug() << "=== Copy to Clipboard Debug Info ===";
    qDebug() << "Device Pixel Ratio:" << devicePixelRatio;
    
    // 从原始截图中裁剪选中区域
    // screenPixmap中存储的是物理像素，需要将逻辑坐标转换为物理坐标
    int x = qRound(selectedRect.x() * devicePixelRatio);
    int y = qRound(selectedRect.y() * devicePixelRatio);
    int w = qRound(selectedRect.width() * devicePixelRatio);
    int h = qRound(selectedRect.height() * devicePixelRatio);
    
    QRect physicalRect(x, y, w, h);


    // 从原始像素数据中裁剪
    QImage tempImage = screenPixmap.toImage();
    QImage croppedImage = tempImage.copy(physicalRect);
    QPixmap croppedPixmap = QPixmap::fromImage(croppedImage);
    
    // 强制将 DPR 设置为 1.0，以便我们直接在物理像素上进行绘制
    // 这样可以避免 QPainter 自动应用 DPR 导致的双重缩放
    croppedPixmap.setDevicePixelRatio(1.0);

    // 在裁剪后的图片上绘制箭头和矩形
    QPainter painter(&croppedPixmap);
    painter.setRenderHint(QPainter::Antialiasing);

    // 绘制所有箭头（需要调整坐标到裁剪区域）
    for (const DrawnArrow &arrow : arrows)
    {
        // 计算相对于选中区域左上角的逻辑坐标
        QPointF relativeStart = arrow.start - selectedRect.topLeft();
        QPointF relativeEnd = arrow.end - selectedRect.topLeft();
        
        // 转换为物理坐标
        QPointF adjustedStart = relativeStart * devicePixelRatio;
        QPointF adjustedEnd = relativeEnd * devicePixelRatio;
        
        drawArrow(painter, adjustedStart, adjustedEnd, arrow.color, arrow.width * devicePixelRatio, devicePixelRatio);
    }

    // 绘制所有矩形
    for (const DrawnRectangle &rect : rectangles)
    {
        QRectF adjustedRect(
            (rect.rect.x() - selectedRect.x()) * devicePixelRatio,
            (rect.rect.y() - selectedRect.y()) * devicePixelRatio,
            rect.rect.width() * devicePixelRatio,
            rect.rect.height() * devicePixelRatio
        );
        painter.setPen(QPen(rect.color, rect.width * devicePixelRatio));
        painter.setBrush(Qt::NoBrush);
        painter.drawRect(adjustedRect);
    }

    painter.end();

    // 应用模糊效果（如果有模糊区域）
    if (!EffectAreas.isEmpty()) {
        QPixmap processedPixmap = croppedPixmap;

        for (int i = 0; i < EffectAreas.size(); ++i) {
            const QRect &EffectArea = EffectAreas[i];
            int strength = EffectStrengths[i]; // 使用保存的强度值
            DrawMode mode = effectTypes[i];  // 使用保存的效果类型


            QRect relativeEffectArea = EffectArea.translated(-selectedRect.topLeft());
            relativeEffectArea = QRect(
                relativeEffectArea.x() * devicePixelRatio,
                relativeEffectArea.y() * devicePixelRatio,
                relativeEffectArea.width() * devicePixelRatio,
                relativeEffectArea.height() * devicePixelRatio
                );

            processedPixmap = applyEffect(processedPixmap, relativeEffectArea, strength, mode);
        }
        croppedPixmap = processedPixmap;
    }

    // 复制到剪贴板
    QClipboard *clipboard = QGuiApplication::clipboard();
    clipboard->setPixmap(croppedPixmap);

    emit screenshotTaken();
    hide();
    QApplication::quit();
}

void ScreenshotWidget::drawArrow(QPainter &painter, const QPointF &start, const QPointF &end, const QColor &color, int width, double scale)
{
    painter.setPen(QPen(color, width));
    painter.setBrush(color);

    // 绘制箭头线条
    painter.drawLine(start, end);

    // 计算箭头头部
    double angle = std::atan2(end.y() - start.y(), end.x() - start.x());
    double arrowSize = 15.0 * scale; // 箭头大小
    double arrowAngle = M_PI / 6; // 箭头角度 (30度)

    QPointF arrowP1 = end - QPointF(
        arrowSize * std::cos(angle - arrowAngle),
        arrowSize * std::sin(angle - arrowAngle)
    );

    QPointF arrowP2 = end - QPointF(
        arrowSize * std::cos(angle + arrowAngle),
        arrowSize * std::sin(angle + arrowAngle)
    );

    // 绘制箭头头部（实心三角形）
    QPolygonF arrowHead;
    arrowHead << end << arrowP1 << arrowP2;
    painter.drawPolygon(arrowHead);
}

