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
    toolbar = new QWidget(this);
    toolbar->setStyleSheet(
        "QWidget { background-color: rgba(40, 40, 40, 200); border-radius: 5px; }"
        "QPushButton { background-color: rgba(60, 60, 60, 255); color: white; "
        "border: none; padding: 8px 15px; border-radius: 3px; font-size: 13px; }"
        "QPushButton:hover { background-color: rgba(80, 80, 80, 255); }"
        "QPushButton:pressed { background-color: rgba(50, 50, 50, 255); }");

    QHBoxLayout *layout = new QHBoxLayout(toolbar);
    layout->setSpacing(5);
    layout->setContentsMargins(10, 5, 10, 5);

    // 绘制工具
    btnRect = new QPushButton("矩形", toolbar);
    btnArrow = new QPushButton("箭头", toolbar);
    btnText = new QPushButton("文字", toolbar);
    btnPen = new QPushButton("画笔", toolbar);

    // 操作按钮
    btnSave = new QPushButton("保存", toolbar);
    btnCopy = new QPushButton("复制", toolbar);
    btnCancel = new QPushButton("取消", toolbar);

    layout->addWidget(btnRect);
    layout->addWidget(btnArrow);
    layout->addWidget(btnText);
    layout->addWidget(btnPen);
    layout->addSpacing(10);
    layout->addWidget(btnSave);
    layout->addWidget(btnCopy);
    layout->addWidget(btnCancel);

    // 连接信号
    connect(btnSave, &QPushButton::clicked, this, &ScreenshotWidget::saveScreenshot);
    connect(btnCopy, &QPushButton::clicked, this, &ScreenshotWidget::copyToClipboard);
    connect(btnCancel, &QPushButton::clicked, this, &ScreenshotWidget::cancelCapture);

    connect(btnRect, &QPushButton::clicked, this, [this]()
            { currentDrawMode = Rectangle; });
    connect(btnArrow, &QPushButton::clicked, this, [this]()
            { currentDrawMode = Arrow; });
    connect(btnText, &QPushButton::clicked, this, [this]()
            { currentDrawMode = Text; });
    connect(btnPen, &QPushButton::clicked, this, [this]()
            { currentDrawMode = Pen; });

    toolbar->adjustSize();
    toolbar->hide();
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
        // 如果已经选中区域且处于绘制模式
        if (selected && currentDrawMode != None)
        {
            isDrawing = true;
            drawStartPoint = event->pos();
            drawEndPoint = event->pos();
        }
        // 否则开始新的区域选择
        else if (!selected)
        {
            startPoint = event->pos();
            endPoint = event->pos();
            currentMousePos = event->pos();
            selecting = true;
            selected = false;
            // showMagnifier已经在startCapture时设置为true，这里不需要重复设置
            toolbar->hide();
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
        if (selecting)
        {
            selecting = false;
            selected = true;
            showMagnifier = false;
            selectedRect = QRect(startPoint, endPoint).normalized();

            // 显示工具栏
            if (!selectedRect.isEmpty())
            {
                updateToolbarPosition();
                toolbar->show();
            }

            update();
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

    // 从原始像素数据中裁剪，不使用DPR
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
            hide();               // 立即隐藏窗口
            QApplication::quit(); // 直接退出应用程序
        }
    }
    // 如果用户取消保存，不做任何操作，保持当前状态（工具栏仍然可见）
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

    // 从原始像素数据中裁剪，不使用DPR
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

    // 复制到剪贴板
    QClipboard *clipboard = QGuiApplication::clipboard();
    clipboard->setPixmap(croppedPixmap);

    emit screenshotTaken();
    hide();               // 立即隐藏窗口
    QApplication::quit(); // 直接退出应用程序
}

void ScreenshotWidget::cancelCapture()
{
    emit screenshotCancelled();
    hide();               // 立即隐藏窗口
    QApplication::quit(); // 直接退出应用程序
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
