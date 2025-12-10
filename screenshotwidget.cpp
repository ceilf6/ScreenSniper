#include "screenshotwidget.h"
#include <QPointer>
#include "pinwidget.h"
#include "ocrresultdialog.h"
#include <QPainter>
#include <QPainterPath>
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
#include <QtMath>
#include <QLineEdit>
#include <QFontDialog>
#include <QColorDialog>
#include <QMetaObject>
#include <QMetaMethod>
#ifndef NO_OPENCV
#include <opencv2/opencv.hpp>
#include "watermark_robust.h"
#endif
#include "facedetector.h"
#include "ocrmanager.h"
#include <QString>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QDialog>
#include <QMessageBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QStyle>
#ifdef Q_OS_WIN
#include <psapi.h>
#include <dwmapi.h>
#endif

#ifdef Q_OS_MAC
#include <CoreGraphics/CoreGraphics.h>
#endif

ScreenshotWidget::ScreenshotWidget(QWidget *parent)
    : QWidget(parent),
      m_adjectDirection(TopLeftCorner),
      selecting(false),
      selected(false),
      m_isadjust(false),
      captureMode(NormalMode),  // 初始化为普通模式
      m_selectedWindow(true),
      toolbar(nullptr),
      currentPenColor(Qt::red),
      currentPenWidth(3),
      penToolbar(nullptr),
      shapesToolbar(nullptr),
      blurToolbar(nullptr),
      devicePixelRatio(1.0),
      showMagnifier(false),
      textInput(nullptr),
      isTextInputActive(false),
      isTextMoving(false),
      movingText(nullptr),
      currentTextFont("Arial", 18),
      currentTextColor(Qt::red),
      currentFontSize(18),
      editingTextIndex(-1),
      currentDrawMode(None),
      isDrawing(false),
      mainWindow(nullptr),
      m_aiManager(nullptr),
      fontToolbar(nullptr),
      EffectToolbar(nullptr),
      sizeLabel(nullptr),
      btnShapes(nullptr),
      btnText(nullptr),
      btnPen(nullptr),
      btnMosaic(nullptr),
      btnBlur(nullptr),
      btnAutoFaceBlur(nullptr),
#ifndef NO_OPENCV
      btnWatermark(nullptr),
#endif
      btnOCR(nullptr),
      btnAIDescription(nullptr),
      btnSave(nullptr),
      btnCopy(nullptr),
      btnPin(nullptr),
      btnCancel(nullptr),
      btnBreak(nullptr),
      btnRect(nullptr),
      btnEllipse(nullptr),
      btnArrow(nullptr),
      btnShapeColor(nullptr),
      btnShapeWidthUp(nullptr),
      btnShapeWidthDown(nullptr),
      shapeWidthLabel(nullptr),
      btnPenWidthUp(nullptr),
      btnPenWidthDown(nullptr),
      penWidthLabel(nullptr),
      btnFontColor(nullptr),
      btnFontSizeUp(nullptr),
      btnFontSizeDown(nullptr),
      fontSizeInput(nullptr),
      btnFontFamily(nullptr),
      btnStrengthUp(nullptr),
      btnStrengthDown(nullptr),
      strengthLabel(nullptr),
      btnBlurStrengthDown(nullptr),
      btnBlurStrengthUp(nullptr),
      blurStrengthLabel(nullptr),
      drawingBlur(false),
      drawingEffect(false),
      currentBlurStrength(20),
      currentEffectStrength(20),
      EffectBlockSize(10),
      textClickIndex(-1),
      potentialTextDrag(false),
      textDoubleClicked(false),
      autoFaceBlurEnabled(false) // 默认禁用自动人脸打码，需要用户手动启用
#ifndef NO_OPENCV
      ,
      faceDetector(new FaceDetector(true)) // 创建人脸检测器，优先使用 DNN 模型
#else
      ,
      faceDetector(nullptr) // OpenCV未启用时为空
#endif
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_DeleteOnClose); // 关闭时自动删除
    setMouseTracking(true);
    setCursor(Qt::CrossCursor);
    setFocusPolicy(Qt::StrongFocus); // 确保窗口能接收键盘事件

    // 初始化I18n连接
    initializeI18nConnection();

    // 初始化人脸检测器
#ifndef NO_OPENCV
    faceDetector->initialize();
#endif

    // 图片生成文字描述
    m_aiManager = new AiManager(this);

    // 连接 AI 处理成功的信号 - 使用 QPointer 确保对象生命周期安全
    // 注意：不能在 lambda 内直接检查 this，必须用 QPointer 在外部捕获
    QPointer<ScreenshotWidget> self1(this);
    connect(m_aiManager, &AiManager::descriptionGenerated, this, [self1](QString text)
            {
                // 检查对象是否仍然有效
                if (!self1) return;
                
                // 1. 将生成的文字复制到剪贴板
                QClipboard *clipboard = QGuiApplication::clipboard();
                if (clipboard) {
                    clipboard->setText(text);
                }

                // 2. 弹窗提示用户
                QMessageBox::information(self1, self1->getText("ai_description_complete_title", "AI 识别完成"),
                                         self1->getText("ai_description_copied", "描述内容已复制到剪贴板：\n\n") + text); });

    // 连接 AI 处理失败的信号 - 使用 QPointer 确保对象生命周期安全
    QPointer<ScreenshotWidget> self2(this);
    connect(m_aiManager, &AiManager::errorOccurred, this, [self2](QString errorMsg)
            { 
                // 检查对象是否仍然有效
                if (!self2) return;
                
                QMessageBox::warning(self2, self2->getText("ai_description_failed_title", "AI 识别失败"), errorMsg); });

    setupToolbar();
    setTextToolbar();
    setupPenToolbar();
    setupShapesToolbar();
    setupTextInput();
    currentPenStroke.clear();
    penStrokes.clear();
    // setupMosaicToolbar();
    //  创建尺寸标签
    sizeLabel = new QLabel(this);
    sizeLabel->setStyleSheet("QLabel { background-color: rgba(0, 0, 0, 180); color: white; "
                             "padding: 5px; border-radius: 3px; font-size: 12px; }");
    sizeLabel->hide();
    // 枚举系统所有有效窗口
    m_validWindows = enumAllValidWindows();
    // 准备截图状态
}

ScreenshotWidget::~ScreenshotWidget()
{
    // 断开所有信号连接以防止悬空指针访问
    if (m_aiManager)
    {
        disconnect(m_aiManager, nullptr, this, nullptr);
    }

    disconnect(I18nManager::instance(), nullptr, this, nullptr);

    // 清理文本输入框
    if (textInput)
    {
        disconnect(textInput, nullptr, this, nullptr);
        textInput->deleteLater();
        textInput = nullptr;
    }

    // 清理工具栏
    if (toolbar)
    {
        toolbar->deleteLater();
        toolbar = nullptr;
    }
    if (penToolbar)
    {
        penToolbar->deleteLater();
        penToolbar = nullptr;
    }
    if (shapesToolbar)
    {
        shapesToolbar->deleteLater();
        shapesToolbar = nullptr;
    }
    if (fontToolbar)
    {
        fontToolbar->deleteLater();
        fontToolbar = nullptr;
    }
    if (EffectToolbar)
    {
        EffectToolbar->deleteLater();
        EffectToolbar = nullptr;
    }
    if (blurToolbar)
    {
        blurToolbar->deleteLater();
        blurToolbar = nullptr;
    }
    if (sizeLabel)
    {
        sizeLabel->deleteLater();
        sizeLabel = nullptr;
    }
}

QString ScreenshotWidget::getText(const QString &key, const QString &defaultText) const
{
    // 使用 I18nManager 获取翻译，不再依赖 MainWindow
    return I18nManager::instance()->getText(key, defaultText);
}

void ScreenshotWidget::setMainWindow(QWidget *mainWin)
{
    mainWindow = mainWin;

    // 初始化时更新工具提示
    updateTooltips();
}

void ScreenshotWidget::onLanguageChanged()
{
    // 语言变化时自动更新所有工具提示
    updateTooltips();
    // 请求重绘界面（如果有需要显示的文本）
    update();
}

// 初始化连接I18nManager信号
void ScreenshotWidget::initializeI18nConnection()
{
    // 直接连接到I18nManager的语言变化信号
    connect(I18nManager::instance(), &I18nManager::languageChanged,
            this, &ScreenshotWidget::onLanguageChanged);
}
void ScreenshotWidget::setCapturedImage(const QImage& originalImage)
{
    if (originalImage.isNull()) return;

    // 设置为长截图模式
    captureMode = ScrollMode;

    // 1. 基础属性清理 - 长截图用深色背景
    setAttribute(Qt::WA_TranslucentBackground, false);
    setAttribute(Qt::WA_OpaquePaintEvent, true);

    // 长截图模式：深灰色背景，衬托图片
    setStyleSheet("background-color: #333333;");

    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);

    // 2. 获取屏幕大小，限制图片最大为屏幕的 85%
    QScreen *screen = QGuiApplication::primaryScreen();
    QSize maxSize = screen ? screen->availableSize() * 0.85 : QSize(1600, 900);

    // 3. 生成最终画布图像
    QImage finalImage = originalImage;
    if (finalImage.width() > maxSize.width() || finalImage.height() > maxSize.height()) {
        finalImage = finalImage.scaled(maxSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }

    // 4. 保存图像
    devicePixelRatio = 1.0;
    screenPixmap = QPixmap::fromImage(finalImage);

    // 5. 布局计算
    int imgW = finalImage.width();
    int imgH = finalImage.height();

    toolbar->setParent(this);
    toolbar->adjustSize();
    toolbar->show();

    // 工具栏放在图片正下方，居中显示
    int toolX = (imgW - toolbar->width()) / 2;
    if (toolX < 0) toolX = 0;
    int toolY = imgH + 10;

    toolbar->move(toolX, toolY);

    // 6. 设定窗口总大小
    int winW = qMax(imgW, toolbar->width());
    int winH = imgH + 10 + toolbar->height() + 10;

    resize(winW, winH);

    // 7. 窗口居中
    if (screen) {
        setGeometry(QStyle::alignedRect(
            Qt::LeftToRight,
            Qt::AlignCenter,
            this->size(),
            screen->availableGeometry()
            ));
    }

    // 8. 关键修改：长截图也需要启用选区，但选区是整张图片
    selectedRect = QRect(0, 0, imgW, imgH);
    selected = true;  // 必须为 true，绘图功能才能工作
    selecting = false;

    // 9. 清除之前的绘图内容（可选）
    arrows.clear();
    rectangles.clear();
    ellipses.clear();
    texts.clear();
    penStrokes.clear();
    EffectAreas.clear();
    EffectStrengths.clear();
    effectTypes.clear();

    show();
    activateWindow();

    setWindowTitle("长截图编辑");
}
void ScreenshotWidget::updateTooltips()
{
    // 不再依赖 mainWindow,直接更新工具提示
    // updateTooltips 使用 getText() 从 I18nManager 获取翻译,无需 mainWindow

    // 更新主工具栏按钮的工具提示
    if (btnShapes)
        btnShapes->setToolTip(getText("tooltip_shapes", "形状"));
    if (btnText)
        btnText->setToolTip(getText("tooltip_text", "文字"));
    if (btnPen)
        btnPen->setToolTip(getText("tooltip_pen", "画笔"));
    if (btnMosaic)
        btnMosaic->setToolTip(getText("tooltip_mosaic", "马赛克"));
    if (btnBlur)
        btnBlur->setToolTip(getText("tooltip_blur", "高斯模糊"));
    if (btnAutoFaceBlur)
        btnAutoFaceBlur->setToolTip(getText("tooltip_auto_face_blur", "自动人脸打码"));
#ifndef NO_OPENCV
    if (btnWatermark)
        btnWatermark->setToolTip(getText("tooltip_watermark", "隐水印"));
#endif
    if (btnOCR)
        btnOCR->setToolTip(getText("tooltip_ocr", "OCR文字识别"));
    if (btnAIDescription)
        btnAIDescription->setToolTip(getText("tooltip_ai_description", "AI图片描述"));
    if (btnSave)
        btnSave->setToolTip(getText("tooltip_save", "保存"));
    if (btnCopy)
        btnCopy->setToolTip(getText("tooltip_copy", "复制"));
    if (btnPin)
        btnPin->setToolTip(getText("tooltip_pin", "贴图"));
    if (btnCancel)
        btnCancel->setToolTip(getText("tooltip_cancel", "取消"));
    if (btnBreak)
        btnBreak->setToolTip(getText("tooltip_break", "退出"));

    // 更新形状工具栏按钮的工具提示
    if (btnRect)
        btnRect->setToolTip(getText("tooltip_rect", "矩形"));
    if (btnEllipse)
        btnEllipse->setToolTip(getText("tooltip_ellipse", "椭圆"));
    if (btnArrow)
        btnArrow->setToolTip(getText("tooltip_arrow", "箭头"));
    if (btnShapeColor)
        btnShapeColor->setToolTip(getText("tooltip_color", "选择颜色"));
}

void ScreenshotWidget::setupToolbar()
{
    // 主工具栏设置
    toolbar = new QWidget(this);
    toolbar->setStyleSheet(
        "QWidget { background-color: rgba(40, 40, 40, 200); border-radius: 5px; }"
        "QPushButton { background-color: rgba(60, 60, 60, 255); color: white; "
        "border: none; padding: 6px; border-radius: 3px; }"
        "QPushButton:hover { background-color: rgba(80, 80, 80, 255); }"
        "QPushButton:pressed { background-color: rgba(50, 50, 50, 255); }"
        "QLabel { background-color: transparent; color: white; padding: 5px; font-size: 12px; }"
        "QPushButton:checked { background-color: rgba(0, 150, 255, 255); }");

    QHBoxLayout *layout = new QHBoxLayout(toolbar);
    layout->setSpacing(5);
    layout->setContentsMargins(10, 5, 10, 5);

    // 绘制工具
    btnShapes = new QPushButton(toolbar);
    btnShapes->setIcon(QIcon(":/icons/icons/shapes.svg"));
    btnShapes->setIconSize(QSize(20, 20));
    btnShapes->setToolTip(getText("tooltip_shapes", "形状"));
    btnShapes->setFixedSize(36, 36);

    btnText = new QPushButton(toolbar);
    btnText->setIcon(QIcon(":/icons/icons/text.svg"));
    btnText->setIconSize(QSize(20, 20));
    btnText->setToolTip(getText("tooltip_text", "文字"));
    btnText->setFixedSize(36, 36);

    btnPen = new QPushButton(toolbar);
    btnPen->setIcon(QIcon(":/icons/icons/pen.svg"));
    btnPen->setIconSize(QSize(20, 20));
    btnPen->setToolTip(getText("tooltip_pen", "画笔"));
    btnPen->setFixedSize(36, 36);

    btnMosaic = new QPushButton(toolbar);
    btnMosaic->setIcon(QIcon(":/icons/icons/mosaic.svg"));
    btnMosaic->setIconSize(QSize(20, 20));
    btnMosaic->setToolTip(getText("tooltip_mosaic", "马赛克"));
    btnMosaic->setFixedSize(36, 36);

    btnBlur = new QPushButton(toolbar);
    btnBlur->setIcon(QIcon(":/icons/icons/blur.svg"));
    btnBlur->setIconSize(QSize(20, 20));
    btnBlur->setToolTip(getText("tooltip_blur", "高斯模糊"));
    btnBlur->setFixedSize(36, 36);

#ifndef NO_OPENCV
    btnWatermark = new QPushButton(toolbar);
    btnWatermark->setIcon(QIcon(":/icons/icons/watermark.svg"));
    btnWatermark->setIconSize(QSize(20, 20));
    btnWatermark->setToolTip(getText("tooltip_watermark", "隐水印"));
    btnWatermark->setFixedSize(36, 36);
#endif

    btnOCR = new QPushButton(toolbar);
    btnOCR->setIcon(QIcon(":/icons/icons/ocr.svg"));
    btnOCR->setIconSize(QSize(20, 20));
    btnOCR->setToolTip(getText("tooltip_ocr", "OCR文字识别"));
    btnOCR->setFixedSize(36, 36);

    btnAIDescription = new QPushButton(toolbar);
    btnAIDescription->setIcon(QIcon(":/icons/icons/aidesc.svg"));
    btnAIDescription->setIconSize(QSize(20, 20));
    btnAIDescription->setToolTip(getText("tooltip_ai_description", "AI图片描述"));
    btnAIDescription->setFixedSize(36, 36);

    // 操作按钮
    btnSave = new QPushButton(toolbar);
    btnSave->setIcon(QIcon(":/icons/icons/save.svg"));
    btnSave->setIconSize(QSize(20, 20));
    btnSave->setToolTip(getText("tooltip_save", "保存"));
    btnSave->setFixedSize(36, 36);

    btnCopy = new QPushButton(toolbar);
    btnCopy->setIcon(QIcon(":/icons/icons/copy.svg"));
    btnCopy->setIconSize(QSize(20, 20));
    btnCopy->setToolTip(getText("tooltip_copy", "复制"));
    btnCopy->setFixedSize(36, 36);

    btnPin = new QPushButton(toolbar);
    btnPin->setIcon(QIcon(":/icons/icons/pin.svg"));
    btnPin->setIconSize(QSize(20, 20));
    btnPin->setToolTip(getText("tooltip_pin", "贴图"));
    btnPin->setFixedSize(36, 36);

    btnCancel = new QPushButton(toolbar);
    btnCancel->setIcon(QIcon(":/icons/icons/cancel.svg"));
    btnCancel->setIconSize(QSize(20, 20));
    btnCancel->setToolTip(getText("tooltip_cancel", "取消"));
    btnCancel->setFixedSize(36, 36);

    btnBreak = new QPushButton(toolbar);
    btnBreak->setIcon(QIcon(":/icons/icons/break.svg"));
    btnBreak->setIconSize(QSize(20, 20));
    btnBreak->setToolTip(getText("tooltip_break", "退出"));
    btnBreak->setFixedSize(36, 36);

    layout->addWidget(btnShapes);
    layout->addWidget(btnText);
    layout->addWidget(btnPen);

    layout->addWidget(btnMosaic); // 马赛克按钮
    layout->addWidget(btnBlur);   // 高斯模糊按钮

    btnAutoFaceBlur = new QPushButton(toolbar);
    btnAutoFaceBlur->setIcon(QIcon(":/icons/icons/face_blur.svg"));
    btnAutoFaceBlur->setIconSize(QSize(20, 20));
    btnAutoFaceBlur->setToolTip(getText("tooltip_auto_face_blur", "自动人脸打码"));
    btnAutoFaceBlur->setFixedSize(36, 36);
    layout->addWidget(btnAutoFaceBlur); // 自动人脸打码按钮

#ifndef NO_OPENCV
    layout->addWidget(btnWatermark); // 水印按钮
#endif
    layout->addWidget(btnOCR);           // OCR 按钮
    layout->addWidget(btnAIDescription); // AI图片描述按钮

    layout->addSpacing(10);
    layout->addWidget(btnSave);
    layout->addWidget(btnCopy);
    layout->addWidget(btnPin);
    layout->addWidget(btnCancel);
    layout->addWidget(btnBreak);

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
    strengthLabel = new QLabel("20(" + getText("strength_strong", "强") + ")", EffectToolbar);
    btnStrengthUp = new QPushButton("+", EffectToolbar);

    EffectLayout->addWidget(new QLabel(getText("blur_strength", "模糊强度:"), EffectToolbar));
    EffectLayout->addWidget(btnStrengthDown);
    EffectLayout->addWidget(strengthLabel);
    EffectLayout->addWidget(btnStrengthUp);

    // 连接信号与槽
    connect(btnSave, &QPushButton::clicked, this, &ScreenshotWidget::saveScreenshot);
    connect(btnCopy, &QPushButton::clicked, this, &ScreenshotWidget::copyToClipboard);
    connect(btnPin, &QPushButton::clicked, this, &ScreenshotWidget::pinToDesktop);
    connect(btnCancel, &QPushButton::clicked, this, &ScreenshotWidget::cancelCapture);
    connect(btnBreak, &QPushButton::clicked, this, &ScreenshotWidget::breakCapture);
    connect(btnOCR, &QPushButton::clicked, this, &ScreenshotWidget::performOCR);
    connect(btnAIDescription, &QPushButton::clicked, this, &ScreenshotWidget::onAiDescriptionBtnClicked);

    connect(btnShapes, &QPushButton::clicked, this, [this]()
            { selected = true;toggleSubToolbar(shapesToolbar); });

    connect(btnText, &QPushButton::clicked, this, [this]()
            {
                selected = true;
                currentDrawMode = Text;
                if (toolbar)
                    toolbar->show();
                
                if (!fontToolbar) {
                    setTextToolbar();
                }
                toggleSubToolbar(fontToolbar); });

    connect(btnPen, &QPushButton::clicked, this, [this]()
            {
                selected = true;
                currentDrawMode = Pen;
                if (toolbar)
                    toolbar->show();
                
                toggleSubToolbar(penToolbar);

                // 如果有已绘制的画笔轨迹，立即更新预览
                if (penToolbar && penToolbar->isVisible() && !penStrokes.isEmpty()) {
                    update();
                } });

    connect(btnMosaic, &QPushButton::clicked, this, [this]()
            {
                selected = true;
                currentDrawMode = Mosaic;
                if (toolbar)
                    toolbar->show();
                toggleSubToolbar(nullptr); });

    connect(btnBlur, &QPushButton::clicked, this, [this]()
            {
                selected = true;
                currentDrawMode = Blur;
                if (toolbar)
                    toolbar->show();
                toggleSubToolbar(nullptr); });

    // 连接自动人脸打码按钮 - 点击后直接触发人脸检测和打码
    connect(btnAutoFaceBlur, &QPushButton::clicked, this, [this]()
            {
#ifndef NO_OPENCV
                selected=true;
                if (!selected || selectedRect.isEmpty())
                {
                    QMessageBox::warning(this,
                                         getText("face_blur_error_title", "错误"),
                                         getText("face_blur_no_selection", "请先选择截图区域"));
                    return;
                }

                // 检查人脸检测器是否可用
                if (!faceDetector)
                {
                    QMessageBox::critical(this,
                                          getText("face_blur_error_title", "错误"),
                                          getText("face_blur_detector_null", "人脸检测器未初始化，请检查 OpenCV 配置"));
                    return;
                }

                // 检查是否已初始化
                if (!faceDetector->isReady())
                {
                    qDebug() << "人脸检测器未初始化，尝试重新初始化...";
                    if (!faceDetector->initialize())
                    {
                        QMessageBox::critical(this,
                                              getText("face_blur_error_title", "错误"),
                                              getText("face_blur_init_failed", "人脸检测器初始化失败\n\n可能的原因：\n1. 模型文件缺失（opencv_face_detector_uint8.pb 和 opencv_face_detector.pbtxt.txt）\n2. OpenCV 库未正确配置\n\n请检查 models 目录下是否有模型文件"));
                        return;
                    }
                }

                qDebug() << "点击自动打码按钮，触发人脸检测";
                QTimer::singleShot(100, this, [this]()
                                   { autoDetectAndBlurFaces(); });
#else
                QMessageBox::information(this,
                                         getText("face_blur_not_available_title", "功能不可用"),
                                         getText("face_blur_opencv_disabled", "人脸识别功能需要 OpenCV 支持\n\n当前编译版本已禁用 OpenCV（NO_OPENCV 已定义）\n\n要启用此功能，请：\n1. 安装 OpenCV 库\n2. 在 ScreenSniper.pro 中配置 OpenCV 路径\n3. 移除 DEFINES += NO_OPENCV 定义\n4. 重新编译项目"));
#endif
            });

#ifndef NO_OPENCV
    connect(btnWatermark, &QPushButton::clicked, this, [this]()
            {
        toggleSubToolbar(nullptr);
        showWatermarkDialog(); });
#endif

    connect(btnStrengthUp, &QPushButton::clicked, this, &ScreenshotWidget::increaseEffectStrength);
    connect(btnStrengthDown, &QPushButton::clicked, this, &ScreenshotWidget::decreaseEffectStrength);

    if (toolbar)
        toolbar->adjustSize();
    if (EffectToolbar)
        EffectToolbar->adjustSize();

    // 默认隐藏所有工具栏
    if (toolbar)
        toolbar->hide();
    if (EffectToolbar)
        EffectToolbar->hide();
}

// 创建统一的圆形颜色按钮
QPushButton *ScreenshotWidget::createColorButton(QWidget *parent, const QColor &color)
{
    QPushButton *button = new QPushButton(parent);
    button->setObjectName("colorBtn");
    button->setToolTip(getText("tooltip_color", "选择颜色"));
    button->setFixedSize(40, 40);
    updateColorButton(button, color);
    return button;
}

// 更新颜色按钮样式
void ScreenshotWidget::updateColorButton(QPushButton *button, const QColor &color)
{
    if (!button)
        return;

    button->setStyleSheet(QString(
                              "QPushButton#colorBtn{"
                              "background-color:%1; "
                              "border: 2px solid rgba(255,255,255,0.6); "
                              "border-radius: 20px; "
                              "min-width: 40px; "
                              "min-height: 40px; "
                              "padding: 0px; "
                              "}"
                              "QPushButton#colorBtn:hover{"
                              "border: 2px solid rgba(255,255,255,0.9); "
                              "}")
                              .arg(color.name()));
}

void ScreenshotWidget::increaseEffectStrength()
{
    selected = true;
    if (currentEffectStrength < 30)
    { // 最大强度限制
        currentEffectStrength += 2;
        updateStrengthLabel();

        // 实时更新预览：如果正在绘制马赛克区域，更新最后一个区域的强度
        if ((currentDrawMode == Mosaic || currentDrawMode == Blur) && !EffectAreas.isEmpty())
        {
            // 更新最后一个马赛克区域的强度
            if (!EffectStrengths.isEmpty())
            {
                EffectStrengths.last() = currentEffectStrength;
            }
        }
        update(); // 刷新显示
    }
}

void ScreenshotWidget::decreaseEffectStrength()
{
    selected = true;
    if (currentEffectStrength > 2)
    { // 最小强度限制
        currentEffectStrength -= 2;
        updateStrengthLabel();

        // 实时更新预览：如果正在绘制马赛克区域，更新最后一个区域的强度
        if ((currentDrawMode == Mosaic || currentDrawMode == Blur) && !EffectAreas.isEmpty())
        {
            // 更新最后一个马赛克区域的强度
            if (!EffectStrengths.isEmpty())
            {
                EffectStrengths.last() = currentEffectStrength;
            }
        }
        update(); // 刷新显示
    }
}

void ScreenshotWidget::updateStrengthLabel()
{
    QString strengthText;
    if (currentEffectStrength <= 8)
    {
        strengthText = getText("strength_weak", "弱");
    }
    else if (currentEffectStrength <= 20)
    {
        strengthText = getText("strength_medium", "中");
    }
    else
    {
        strengthText = getText("strength_strong", "强");
    }
    strengthLabel->setText(QString("%1 (%2)").arg(currentEffectStrength).arg(strengthText));
}

QPixmap ScreenshotWidget::applyEffect(const QPixmap &source, const QRect &area, int strength, DrawMode mode)
{
    if (area.isEmpty() || strength <= 0)
    {
        return source;
    }

    switch (mode)
    {
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
    if (area.isEmpty() || blockSize <= 0)
    {
        return source;
    }

    QPixmap result = source;
    QPainter painter(&result);
    painter.setRenderHint(QPainter::Antialiasing, false);

    // 确保马赛克区域在图片范围内
    QRect validArea = area.intersected(source.rect());

    // 遍历马赛克区域，按块处理
    for (int x = validArea.left(); x < validArea.right(); x += blockSize)
    {
        for (int y = validArea.top(); y < validArea.bottom(); y += blockSize)
        {
            // 计算当前块的实际大小（边缘可能小于blockSize）
            int currentBlockWidth = qMin(blockSize, validArea.right() - x);
            int currentBlockHeight = qMin(blockSize, validArea.bottom() - y);

            // 获取当前块的平均颜色
            QRect blockRect(x, y, currentBlockWidth, currentBlockHeight);
            QImage blockImage = source.copy(blockRect).toImage();

            // 计算平均颜色
            int totalRed = 0, totalGreen = 0, totalBlue = 0;
            int pixelCount = currentBlockWidth * currentBlockHeight;

            for (int i = 0; i < currentBlockWidth; ++i)
            {
                for (int j = 0; j < currentBlockHeight; ++j)
                {
                    QColor color = blockImage.pixelColor(i, j);
                    totalRed += color.red();
                    totalGreen += color.green();
                    totalBlue += color.blue();
                }
            }

            QColor averageColor(
                totalRed / pixelCount,
                totalGreen / pixelCount,
                totalBlue / pixelCount);

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
    if (area.isEmpty() || radius <= 0)
    {
        return source;
    }

    QPixmap result = source;

    // 确保模糊区域在图片范围内
    QRect validArea = area.intersected(source.rect());
    if (validArea.isEmpty())
    {
        return result;
    }

    // 提取要模糊的区域
    QImage sourceImage = source.copy(validArea).toImage();
    QImage blurredImage(sourceImage.size(), QImage::Format_ARGB32);

    // 计算高斯核
    int kernelSize = radius * 2 + 1;
    QVector<double> kernel(kernelSize);
    double sigma = radius / 3.0; // 标准差
    double sum = 0.0;

    // 生成高斯核
    for (int i = 0; i < kernelSize; ++i)
    {
        int x = i - radius;
        kernel[i] = qExp(-(x * x) / (2 * sigma * sigma)) / (qSqrt(2 * M_PI) * sigma);
        sum += kernel[i];
    }

    // 归一化
    for (int i = 0; i < kernelSize; ++i)
    {
        kernel[i] /= sum;
    }

    // 水平模糊
    QImage tempImage(sourceImage.size(), QImage::Format_ARGB32);
    for (int y = 0; y < sourceImage.height(); ++y)
    {
        for (int x = 0; x < sourceImage.width(); ++x)
        {
            double r = 0, g = 0, b = 0, a = 0;

            for (int i = -radius; i <= radius; ++i)
            {
                int sampleX = qBound(0, x + i, sourceImage.width() - 1);
                QColor color = sourceImage.pixelColor(sampleX, y);

                double weight = kernel[i + radius];
                r += color.red() * weight;
                g += color.green() * weight;
                b += color.blue() * weight;
                a += color.alpha() * weight;
            }

            tempImage.setPixelColor(x, y, QColor(qBound(0, int(r), 255), qBound(0, int(g), 255), qBound(0, int(b), 255), qBound(0, int(a), 255)));
        }
    }

    // 垂直模糊
    for (int x = 0; x < tempImage.width(); ++x)
    {
        for (int y = 0; y < tempImage.height(); ++y)
        {
            double r = 0, g = 0, b = 0, a = 0;

            for (int i = -radius; i <= radius; ++i)
            {
                int sampleY = qBound(0, y + i, tempImage.height() - 1);
                QColor color = tempImage.pixelColor(x, sampleY);

                double weight = kernel[i + radius];
                r += color.red() * weight;
                g += color.green() * weight;
                b += color.blue() * weight;
                a += color.alpha() * weight;
            }

            blurredImage.setPixelColor(x, y, QColor(qBound(0, int(r), 255), qBound(0, int(g), 255), qBound(0, int(b), 255), qBound(0, int(a), 255)));
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

    // 使用 QPointer 安全地捕获 this 指针，防止对象被提前销毁
    QPointer<ScreenshotWidget> self(this);

    // 然后立即设置为全屏模式
    QTimer::singleShot(150, this, [self]()
                       {
            // 检查对象是否仍然存在
            if (!self) {
                return;
            }
            
            self->selectedRect = self->rect();
            self->selected = true;
            self->selecting = false;

            if (self->toolbar) {
                self->toolbar->setParent(self);
                self->toolbar->adjustSize();
                self->updateToolbarPosition();
                self->toolbar->setWindowFlags(Qt::Widget);
                self->toolbar->raise();
                self->toolbar->show();
                self->toolbar->activateWindow();
            }

        // 自动检测并打码人脸（延迟执行，避免阻塞UI）
#ifndef NO_OPENCV
            if (self->autoFaceBlurEnabled) {
                QTimer::singleShot(100, self, [self]() {
                    if (self) {
                        self->autoDetectAndBlurFaces();
                    }
                });
            }
#endif

            self->update(); });
}

void ScreenshotWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    // 开启高质量绘图抗锯齿
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    // =========================================================
    // 1. 准备公共变量与坐标转换辅助函数
    // =========================================================

    // 决定当前的有效绘图区域 (Clip Rect)
    // ScrollMode: 整个窗口/图片区域
    // NormalMode: 用户框选的区域 (selectedRect)
    QRect paintAreaRect;

    // 辅助 Lambda：将逻辑坐标转为物理坐标 (处理 DPI 和 偏移)
    // 注意：ScrollMode 下通常不需要减去 virtualGeometryTopLeft，除非你的长截图逻辑特殊
    auto toPhysicalRect = [&](const QRect &logicalRect, bool useOffset = true) -> QRect {
        if (logicalRect.isEmpty()) return QRect();

        double x = logicalRect.x();
        double y = logicalRect.y();

        // 仅在普通模式下需要处理屏幕偏移
        if (useOffset && captureMode != ScrollMode) {
            QPoint offset = geometry().topLeft() - virtualGeometryTopLeft;
            x += offset.x();
            y += offset.y();
        }

        return QRect(
            qRound(x * devicePixelRatio),
            qRound(y * devicePixelRatio),
            qRound(logicalRect.width() * devicePixelRatio),
            qRound(logicalRect.height() * devicePixelRatio)
            );
    };

    // =========================================================
    // 2. 背景与底图绘制 (模式分离)
    // =========================================================

    if (captureMode == ScrollMode)
    {
        // --- 长截图模式 ---
        paintAreaRect = rect(); // 整个区域都可绘图

        // 2.1 深色背景
        painter.fillRect(rect(), QColor(51, 51, 51));

        // 2.2 绘制完整长图
        if (!screenPixmap.isNull()) {
            // 假设 screenPixmap 已经是拼接好的长图，且 devicePixelRatio 设置正确
            painter.drawPixmap(0, 0, screenPixmap);
        }

        // 2.3 长截图模式下的特效绘制 (模糊/马赛克)
        if (!EffectAreas.isEmpty()) {
            for (int i = 0; i < EffectAreas.size(); ++i) {
                // 长截图不需要 intersected(selectedRect)，因为是全图
                QRect logicalArea = EffectAreas[i];
                if (logicalArea.isEmpty()) continue;

                // 转换物理坐标 (useOffset = false)
                QRect physicalArea = toPhysicalRect(logicalArea, false);

                // 性能优化：应该使用缓存，这里演示实时计算的正确逻辑
                QPixmap sourcePart = screenPixmap.copy(physicalArea);
                sourcePart.setDevicePixelRatio(1.0);
                QPixmap effectPart = applyEffect(sourcePart, sourcePart.rect(), EffectStrengths[i], effectTypes[i]);
                effectPart.setDevicePixelRatio(1.0);

                painter.drawPixmap(logicalArea, effectPart);
            }
        }
    }
    else
    {
        // --- 普通截图模式 ---

        // 计算当前选区
        QRect currentSelection = selecting ? QRect(startPoint, endPoint).normalized() : selectedRect;
        paintAreaRect = currentSelection; // 限制绘图只在选区内

        // 2.1 绘制全屏背景
        QRect windowRect = rect();
        QRect sourceRect(0, 0, screenPixmap.width(), screenPixmap.height());
        painter.drawPixmap(windowRect, screenPixmap, sourceRect);

        // 2.2 绘制半透明遮罩
        painter.fillRect(windowRect, QColor(0, 0, 0, 100));

        // 2.3 绘制选区亮色部分
        if (!currentSelection.isEmpty())
        {
            // 获取选区对应的物理像素图像 (useOffset = true)
            QRect physicalRect = toPhysicalRect(currentSelection, true);
            painter.drawPixmap(currentSelection, screenPixmap, physicalRect);

            // 绘制选区边框
            QPen borderPen(QColor(0, 150, 255), 2);
            painter.setPen(borderPen);
            painter.setBrush(Qt::NoBrush);
            painter.drawRect(currentSelection);

            // 绘制尺寸标签
            QString sizeText = QString("%1 x %2").arg(currentSelection.width()).arg(currentSelection.height());
            sizeLabel->setText(sizeText);
            // 智能调整标签位置
            int labelY = currentSelection.top() - sizeLabel->height() - 5;
            if (labelY < 0) labelY = currentSelection.top() + 5;
            sizeLabel->move(currentSelection.left(), labelY);
            sizeLabel->adjustSize();
            sizeLabel->show();

            // 绘制普通模式下的特效 (限制在 paintAreaRect 内)
            if (!EffectAreas.isEmpty() && selected)
            {
                for (int i = 0; i < EffectAreas.size(); ++i)
                {
                    QRect logicalArea = EffectAreas[i].intersected(paintAreaRect);
                    if (logicalArea.isEmpty()) continue;

                    QRect physicalArea = toPhysicalRect(logicalArea, true);
                    physicalArea = physicalArea.intersected(screenPixmap.rect()); // 保护边界

                    if (physicalArea.isEmpty()) continue;

                    QPixmap sourcePart = screenPixmap.copy(physicalArea);
                    sourcePart.setDevicePixelRatio(1.0);
                    QPixmap effectPart = applyEffect(sourcePart, sourcePart.rect(), EffectStrengths[i], effectTypes[i]);
                    effectPart.setDevicePixelRatio(1.0);

                    painter.drawPixmap(logicalArea, effectPart);
                }
            }

            // 绘制拖拽调整手柄 (仅在非拖拽选取状态)
            if (selected) {
                // 此处调用绘制手柄的函数 (代码略，参考之前)
                int h = 8;
                painter.setBrush(QColor(0, 150, 255)); painter.setPen(Qt::white);
                painter.drawRect(currentSelection.topLeft().x()-h/2, currentSelection.topLeft().y()-h/2, h, h);
                painter.drawRect(currentSelection.bottomRight().x()-h/2, currentSelection.bottomRight().y()-h/2, h, h);
                // ... 其他手柄
            }
        } else {
            sizeLabel->hide();
        }

        // 放大镜通常只在普通模式且非选区调整时显示
        if (showMagnifier && !selected) {
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
    }

    // =========================================================
    // 3. 统一绘制标注 (形状、画笔、文字)
    // =========================================================
    // 核心思想：无论哪种模式，都在 paintAreaRect 范围内绘制。
    // 如果没有选区(且非长截图)，paintAreaRect为空，自然不会画出东西

    bool shouldDrawAnnotations = (captureMode == ScrollMode) || (selected && !paintAreaRect.isEmpty());

    if (shouldDrawAnnotations)
    {
        // 统一设置裁剪区域 (极大地简化代码，不用在每个循环里判断 intersects)
        painter.save();
        painter.setClipRect(paintAreaRect);

        // 3.1 箭头
        for (const auto &arrow : arrows) {
            drawArrow(painter, arrow.start, arrow.end, arrow.color, arrow.width);
        }

        // 3.2 矩形
        painter.setBrush(Qt::NoBrush);
        for (const auto &rect : rectangles) {
            painter.setPen(QPen(rect.color, rect.width));
            painter.drawRect(rect.rect);
        }

        // 3.3 椭圆
        for (const auto &ellipse : ellipses) {
            painter.setPen(QPen(ellipse.color, ellipse.width));
            painter.drawEllipse(ellipse.rect);
        }

        // 3.4 文本
        if (selected)
        { // 只在选中区域后绘制文字
            for (int i = 0; i < texts.size(); ++i)
            {
                // 如果正在编辑这个文字的内容（双击编辑，显示输入框），跳过绘制（隐藏原文字，只显示输入框）
                // 但如果只是单击修改属性（isTextInputActive = false），不隐藏文字
                if (editingTextIndex == i && isTextInputActive)
                {
                    continue;  // 跳过绘制正在编辑的文字
                }
                const DrawnText &text = texts[i];
                // 检查文字是否与截图框有重叠
                if (text.rect.intersects(selectedRect))
                {
                    // 绘制文字，使用原始位置，不再添加额外偏移
                    drawText(painter, text.position, text.text, text.color, text.font);
                }
            }
        }

        // 3.5 画笔轨迹 (已完成)
        for (const auto &stroke : penStrokes) {
            if (stroke.points.size() < 2) continue;
            QPen pen(stroke.color, stroke.width);
            pen.setCapStyle(Qt::RoundCap);
            pen.setJoinStyle(Qt::RoundJoin);
            painter.setPen(pen);

            QPainterPath path;
            path.moveTo(stroke.points.first());
            for (int k = 1; k < stroke.points.size(); ++k) path.lineTo(stroke.points[k]);
            painter.drawPath(path);
        }

        // 3.6 正在绘制的内容 (Preview)
        if (isDrawing) {
            painter.setPen(QPen(currentPenColor, currentPenWidth));
            painter.setBrush(Qt::NoBrush);
            QRect drawRect = QRect(drawStartPoint, drawEndPoint).normalized();

            if (currentDrawMode == Arrow) {
                drawArrow(painter, drawStartPoint, drawEndPoint, currentPenColor, currentPenWidth);
            } else if (currentDrawMode == Rectangle) {
                painter.drawRect(drawRect);
            } else if (currentDrawMode == Ellipse) {
                painter.drawEllipse(drawRect);
            } else if (currentDrawMode == Pen && currentPenStroke.size() > 1) {
                QPen p(currentPenColor, currentPenWidth);
                p.setCapStyle(Qt::RoundCap);
                p.setJoinStyle(Qt::RoundJoin);
                painter.setPen(p);
                QPainterPath path;
                path.moveTo(currentPenStroke.first());
                for (int k = 1; k < currentPenStroke.size(); ++k) path.lineTo(currentPenStroke[k]);
                painter.drawPath(path);
            }
        }

        // 3.7 特效拖拽预览 (Blur/Mosaic)
        if ((currentDrawMode == Mosaic || currentDrawMode == Blur) && drawingEffect) {
            QRect previewRect = QRect(EffectStartPoint, EffectEndPoint).normalized();
            // 在 ClipRect 作用下，这里不需要手动 intersected(paintAreaRect)

            // 预览需要取图，这里逻辑略复杂，需区分模式
            // 简单处理：只画个框示意，或者复用上面的 applyEffect 逻辑
            painter.setPen(QPen(QColor(255, 0, 0, 100), 2, Qt::DashLine));
            painter.drawRect(previewRect);
        }

        painter.restore(); // 恢复裁剪
    }

    // 4. 工具栏背景 (如果需要自行绘制)
    if (captureMode == ScrollMode && toolbar && toolbar->isVisible()) {
        painter.fillRect(toolbar->geometry(), QColor(51, 51, 51));
    }
}

void ScreenshotWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
    {
        isDrawing = false;
        drawingEffect = false;
        isTextMoving = false;
        QPoint clickPos = event->pos();

        // 处理文本输入框相关逻辑
        if (selected && isTextInputActive && textInput && textInput->isVisible())
        {
            if (!textInput->geometry().contains(clickPos))
            {
                onTextInputFinished();
                currentDrawMode = Text;
                isTextInputActive = false;
            }
            return;
        }

        // 优先检查是否点击了已存在的文字（无论什么模式）
        if (selected && !isTextInputActive)
        {
            for (int i = texts.size() - 1; i >= 0; i--)
            {
                if (texts[i].rect.contains(clickPos))
                {
                    // 记录点击位置和文字索引，用于区分单击和拖拽
                    textClickStartPos = clickPos;
                    textClickIndex = i;
                    potentialTextDrag = true;
                    textDoubleClicked = false; // 重置双击标志

                    // 准备拖拽（如果鼠标移动则拖拽，否则单击修改属性）
                    isTextMoving = false; // 先不开始拖拽，等待鼠标移动
                    return;               // 直接返回，不处理其他逻辑
                }
            }
        }

        // 如果处于文字绘制模式（优先处理，避免触发区域重新选择）
        if (selected && currentDrawMode == Text && !isTextInputActive)
        {
            // 如果已经选中区域，直接添加文字
            if (selected)
            {
                handleTextModeClick(clickPos);
                return;
            }
            // 如果未选中区域，先选择区域，然后添加文字
            // 但这里不应该触发区域重新选择，而是应该允许在未选中区域时也能添加文字
            // 实际上，文字模式应该要求先有选中区域，所以这里直接返回
            return;
        }

        // 处理文字编辑模式（None模式下的其他操作）
        if (selected && currentDrawMode == None)
        {
            // 如果点击在选中矩形外部，重新开始选择区域
            if (!selectedRect.contains(clickPos))
            {
                // 重新开始选择区域
                startPoint = clickPos;
                endPoint = clickPos;
                currentMousePos = clickPos;
                selecting = true;
                selected = false;

                // 清除之前的状态
                if (toolbar)
                    toolbar->hide();
                if (shapesToolbar)
                    shapesToolbar->hide();
                if (penToolbar)
                    penToolbar->hide();
                if (fontToolbar)
                    fontToolbar->hide();
                if (EffectToolbar)
                    EffectToolbar->hide();

                showMagnifier = true;
                EffectAreas.clear();
                EffectStrengths.clear();
                effectTypes.clear();
                update();
                return;
            }

            // 没有点击文字，调用handleNoneMode
            handleNoneMode(clickPos);
            return;
        }
        // 如果已经选中区域且处于图形绘制模式
        else if (selected && (currentDrawMode == Rectangle || currentDrawMode == Arrow ||
                              currentDrawMode == Ellipse))
        {
            isDrawing = true;
            drawStartPoint = event->pos();
            drawEndPoint = event->pos();

            // 隐藏文本输入框
            if (textInput)
            {
                textInput->hide();
                isTextInputActive = false;
            }
            update();
        }
        // 如果已经选中区域且处于马赛克或者高斯模糊模式
        else if ((currentDrawMode == Mosaic || currentDrawMode == Blur) && selected)
        {
            // 开始绘制马赛克和高斯模糊的区域
            EffectStartPoint = event->pos();
            EffectEndPoint = event->pos();
            drawingEffect = true;
            if (EffectToolbar)
            {
                EffectToolbar->hide(); // 开始绘制时隐藏强度工具栏
            }
        }
        // 如果已经选中区域且处于画笔状态
        else if (currentDrawMode == Pen && selected)
        {

            isDrawing = true;

            currentPenStroke.clear();
            currentPenStroke.append(event->pos());
        }
        else if (!selected)
        {
            // 是否处于调节位置
            if (cursor().shape() != Qt::CrossCursor)
            {
                m_isadjust = true;
                if (cursor().shape() == Qt::SizeAllCursor)
                {
                    m_relativeDistance.setX(event->pos().x() - startPoint.x());
                    m_relativeDistance.setY(event->pos().y() - startPoint.y());
                    m_relativeDistance.setWidth(abs(startPoint.x() - endPoint.x()));
                    m_relativeDistance.setHeight(abs(startPoint.y() - endPoint.y()));
                }
                if (toolbar)
                    toolbar->hide();
            }
            else
            {
                startPoint = event->pos();
                endPoint = event->pos();
                currentMousePos = event->pos();
            }

            selecting = true;
            selected = false;

            // showMagnifier已经在startCapture时设置为true，这里不需要重复设置
            if (toolbar)
                toolbar->hide();
            if (shapesToolbar)
                shapesToolbar->hide();
            if (penToolbar)
                penToolbar->hide();
            if (fontToolbar)
                fontToolbar->hide();
            if (EffectToolbar)
                EffectToolbar->hide();

            showMagnifier = true;

            EffectAreas.clear(); // 清除之前的模糊区域
            EffectStrengths.clear();
            effectTypes.clear(); // 清除效果类型
        }
        update();
    }
    if (event->button() == Qt::RightButton)
    {
        cancelCapture();
    }
}

void ScreenshotWidget::mouseMoveEvent(QMouseEvent *event)
{
    currentMousePos = event->pos();

    if (selecting && !m_isadjust)
    {
        // 关闭截取窗口
        m_selectedWindow = false;
        endPoint = event->pos();
        showMagnifier = true;
        update();
    }
    else if (isDrawing && currentDrawMode == Pen && selected)
    {
        // 限制画笔点在截图框范围内
        QPoint limitedPos = event->pos();
        if (!selectedRect.contains(limitedPos))
        {
            // 如果点不在截图框内，将其限制在截图框的边界上
            limitedPos.setX(qMax(selectedRect.left(), qMin(selectedRect.right(), limitedPos.x())));
            limitedPos.setY(qMax(selectedRect.top(), qMin(selectedRect.bottom(), limitedPos.y())));
        }
        currentPenStroke.append(limitedPos);
        update();
    }
    else if (isDrawing)
    {
        drawEndPoint = event->pos();
        update();
    }
    else if (drawingEffect && (currentDrawMode == Mosaic || currentDrawMode == Blur))
    {
        EffectEndPoint = event->pos();
        update();
    }
    else if (potentialTextDrag && textClickIndex >= 0 && textClickIndex < texts.size())
    {
        // 检查鼠标是否移动超过阈值，如果是则开始拖拽
        QPoint moveDelta = event->pos() - textClickStartPos;
        int moveDistance = qAbs(moveDelta.x()) + qAbs(moveDelta.y());

        if (moveDistance > 5)
        { // 移动超过5像素，认为是拖拽
            // 开始拖拽文字
            if (!isTextMoving)
            {
                isTextMoving = true;
                movingText = &texts[textClickIndex];
                dragStartOffset = event->pos() - texts[textClickIndex].rect.topLeft();
                setCursor(Qt::ClosedHandCursor);
            }

            // 拖拽移动文字：实时更新位置
            QPoint newPos = event->pos() - dragStartOffset;

            // 确保文字不会移出截图框边界（与画笔限制保持一致）
            if (selected)
            {
                newPos.setX(qMax(selectedRect.left(), qMin(selectedRect.right() - movingText->rect.width(), newPos.x())));
                newPos.setY(qMax(selectedRect.top(), qMin(selectedRect.bottom() - movingText->rect.height(), newPos.y())));
            }
            else
            {
                // 未选中区域时，限制在屏幕边界
                newPos.setX(qMax(0, qMin(width() - movingText->rect.width(), newPos.x())));
                newPos.setY(qMax(0, qMin(height() - movingText->rect.height(), newPos.y())));
            }

            movingText->rect.moveTopLeft(newPos);
            movingText->position = newPos;
            update();
            return; // 拖拽时直接返回，不处理其他逻辑
        }
    }
    else if (isTextMoving && movingText)
    {
        // 拖拽移动文字：实时更新位置
        QPoint newPos = event->pos() - dragStartOffset;

        // 确保文字不会移出截图框边界（与画笔限制保持一致）
        if (selected)
        {
            newPos.setX(qMax(selectedRect.left(), qMin(selectedRect.right() - movingText->rect.width(), newPos.x())));
            newPos.setY(qMax(selectedRect.top(), qMin(selectedRect.bottom() - movingText->rect.height(), newPos.y())));
        }
        else
        {
            // 未选中区域时，限制在屏幕边界
            newPos.setX(qMax(0, qMin(width() - movingText->rect.width(), newPos.x())));
            newPos.setY(qMax(0, qMin(height() - movingText->rect.height(), newPos.y())));
        }

        movingText->rect.moveTopLeft(newPos);
        movingText->position = newPos;
        update();
        return; // 拖拽时直接返回，不处理其他逻辑
    }
    else if (!selected && m_isadjust && selecting)
    {
        if (cursor().shape() != Qt::CrossCursor)
        {
            // 更新坐标
            mouseIsAdjust(event->pos());
        }
        showMagnifier = true;
        update();
    }
    else if (!selected)
    {
        // 查询窗口
        if (m_selectedWindow)
        {
            m_hoverWindow = WindowInfo(); // 重置
            captureWindow(event->pos());

            // 获取窗口大小
            if (m_hoverWindow.isValid())
            {
                selectedRect = getAccurateWindowRect(m_hoverWindow);
            }
        }
        else
        {
            // 判断鼠标是否处于调节区域
            mouseIsAdjust(event->pos());
        }
        // 在框选前的鼠标移动时也触发更新，以显示放大镜
        captureWindow(event->pos());
        update();
    }
    else
    {
        // 检查鼠标是否悬停在文字上
        bool overText = false;
        if (selected && !isTextInputActive)
        { // 只在选中区域且非输入状态下检查文字悬停
            for (const DrawnText &text : texts)
            {
                if (text.rect.contains(event->pos()))
                {
                    setCursor(Qt::PointingHandCursor);
                    overText = true;
                    break;
                }
            }
        }
        if (!overText)
        {
            // 检查是否在调整手柄上，使用 mouseIsAdjust 函数（只在 None 模式下）
            if (selected && !selectedRect.isEmpty() && !m_isadjust && currentDrawMode == None)
            {
                // 更新 startPoint 和 endPoint 以便 mouseIsAdjust 使用
                startPoint = selectedRect.topLeft();
                endPoint = selectedRect.bottomRight();
                mouseIsAdjust(event->pos());
            }
            else
            {
                // 其他模式下使用默认光标
                setCursor(Qt::CrossCursor);
            }
        }
    }
}

void ScreenshotWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
    {
        if (drawingEffect && (currentDrawMode == Mosaic || currentDrawMode == Blur))
        {
            // 完成模糊区域绘制
            drawingEffect = false;
            QRect EffectRect = QRect(EffectStartPoint, EffectEndPoint).normalized();
            if (EffectRect.width() > 5 && EffectRect.height() > 5)
            {
                EffectAreas.append(EffectRect);
                EffectStrengths.append(currentEffectStrength); // 保存当前强度
                effectTypes.append(currentDrawMode);           // 保存效果类型
                // 显示强度调节工具栏
                if (EffectToolbar)
                {
                    updateEffectToolbarPosition();
                    EffectToolbar->show();
                    EffectToolbar->raise();
                }
                // 立即更新显示，确保新区域可见
                update();
            }
        }
        else if (isDrawing)
        {
            if (currentDrawMode == Pen)
            {
                // 先处理画笔，因为它不需要drawEndPoint
                isDrawing = false;
                // 保存画笔笔迹
                if (currentPenStroke.size() > 1)
                {
                    DrawnPenStroke stroke;
                    stroke.points = currentPenStroke;
                    stroke.color = currentPenColor;
                    stroke.width = currentPenWidth;
                    penStrokes.append(stroke);
                }
                currentPenStroke.clear();
                update();
            }
            else
            {
                isDrawing = false;
                drawEndPoint = event->pos();

                // 保存绘制的形状
                if (currentDrawMode == Arrow)
                {
                    DrawnArrow arrow;
                    arrow.start = drawStartPoint;
                    arrow.end = drawEndPoint;
                    arrow.color = currentPenColor;
                    arrow.width = currentPenWidth;
                    arrows.append(arrow);
                }
                else if (currentDrawMode == Rectangle)
                {
                    DrawnRectangle rect;
                    rect.rect = QRect(drawStartPoint, drawEndPoint).normalized();
                    rect.color = currentPenColor;
                    rect.width = currentPenWidth;
                    rectangles.append(rect);
                }
                else if (currentDrawMode == Ellipse)
                {
                    DrawnEllipse ellipse;
                    ellipse.rect = QRect(drawStartPoint, drawEndPoint).normalized();
                    ellipse.color = currentPenColor;
                    ellipse.width = currentPenWidth;
                    ellipses.append(ellipse);
                }
            }
        }
        else if (isTextMoving && movingText)
        {
            // 松开鼠标左键，停止拖拽移动
            isTextMoving = false;
            movingText = nullptr;
            potentialTextDrag = false;
            textClickIndex = -1;
            setCursor(Qt::CrossCursor);
            update();
            return;
        }
        else if (potentialTextDrag && textClickIndex >= 0 && textClickIndex < texts.size())
        {
            // 如果发生了双击，不处理单击
            if (textDoubleClicked)
            {
                potentialTextDrag = false;
                textClickIndex = -1;
                textDoubleClicked = false;
                return;
            }

            // 如果点击了文字但没有拖拽（移动距离小于阈值），则修改属性
            QPoint moveDelta = event->pos() - textClickStartPos;
            int moveDistance = qAbs(moveDelta.x()) + qAbs(moveDelta.y());

            if (moveDistance <= 5)
            { // 没有移动或移动很小，认为是单击
                // 保存当前值，用于定时器回调
                int savedTextIndex = textClickIndex;

                // 使用定时器延迟处理单击，以便区分双击
                QTimer::singleShot(QApplication::doubleClickInterval(), this, [this, savedTextIndex]()
                                   {
                    // 检查是否发生了双击（如果发生了双击，textDoubleClicked会被设置为true）
                    if (!textDoubleClicked && savedTextIndex >= 0 && savedTextIndex < texts.size()) {
                        // 单击文字：修改属性（颜色、大小、字体）
                        selectTextForPropertyEdit(savedTextIndex);
                    }
                    // 重置双击标志（无论是否处理了单击）
                    textDoubleClicked = false; });
            }

            // 重置状态
            potentialTextDrag = false;
            textClickIndex = -1;
            return; // 直接返回，不处理区域选择逻辑
        }
        // else if(selecting)
        else if (selecting)
        {
            // 原有选择逻辑
            selecting = false;
            selected = false;
            showMagnifier = false;
            // 关闭调节
            if (m_isadjust || m_selectedWindow)
            {
                if (m_isadjust)
                {
                    m_isadjust = false;
                    selectedRect = QRect(startPoint, endPoint).normalized();
                }
                if (m_selectedWindow && (startPoint.x() == endPoint.x()) && (startPoint.y() == endPoint.y()))
                {
                    m_selectedWindow = false;
                }
                startPoint.setX(selectedRect.x());
                startPoint.setY(selectedRect.y());
                endPoint.setX(startPoint.x() + selectedRect.width());
                endPoint.setY(startPoint.y() + selectedRect.height());
                mouseIsAdjust(event->pos());
            }
            else
            {
                selectedRect = QRect(startPoint, endPoint).normalized();
            }

            // 只有在选择新区域时才清除之前的马赛克效果
            EffectAreas.clear();
            EffectStrengths.clear();
            effectTypes.clear();
            if (EffectToolbar)
            {
                EffectToolbar->hide();
            }

            if (!selectedRect.isEmpty())
            {
                updateToolbarPosition();
                toolbar->show();

                // 自动检测并打码人脸（延迟执行，避免阻塞UI）
#ifndef NO_OPENCV
                if (autoFaceBlurEnabled)
                {
                    QTimer::singleShot(100, this, [this]()
                                       { autoDetectAndBlurFaces(); });
                }
#endif
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
        if (!selectedRect.isEmpty())
        {
            copyToClipboard();
        }
        else
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

    // 可以删除选中的文字
    if ((event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace) && selected)
    {
        if (movingText)
        {
            for (int i = 0; i < texts.size(); i++)
            {
                if (&texts[i] == movingText)
                {
                    texts.remove(i);
                    isTextMoving = false;
                    movingText = nullptr;
                    setCursor(Qt::CrossCursor);
                    update();
                    break;
                }
            }
        }
    }
}
void ScreenshotWidget::updateEffectToolbarPosition()
{
    if (EffectAreas.isEmpty() || !EffectToolbar)
        return;

    // 获取最后一个马赛克区域的位置
    QRect lastEffectArea = EffectAreas.last();

    int toolbarWidth = EffectToolbar->sizeHint().width();
    int toolbarHeight = EffectToolbar->sizeHint().height();

    QScreen *screen = QGuiApplication::primaryScreen();
    QRect availableGeometry = screen->availableGeometry();
    QPoint globalPos = mapToGlobal(QPoint(0, 0)); // 以左上角原点为基点，实现坐标系转换

    int maxY = availableGeometry.bottom() - globalPos.y();
    int maxX = availableGeometry.right() - globalPos.x();
    int minX = availableGeometry.left() - globalPos.x();
    int minY = availableGeometry.top() - globalPos.y();

    // 将工具栏放在马赛克区域上方
    int x = lastEffectArea.x();
    int y = lastEffectArea.y() - toolbarHeight - 5;

    // 如果上方空间不够，放在下方
    if (y < minY)
    {
        y = lastEffectArea.bottom() + 5;
    }

    // 确保不超出屏幕边界
    if (x + toolbarWidth > maxX)
    {
        x = maxX - toolbarWidth - 5;
    }
    if (x < minX + 5)
        x = minX + 5;

    if (y + toolbarHeight > maxY)
    {
        y = maxY - toolbarHeight - 5;
    }
    if (y < minY + 5)
        y = minY + 5;

    EffectToolbar->move(x, y);
}
void ScreenshotWidget::cancelCapture()
{
    if (mainWindow)
    {
        QMetaObject::invokeMethod(mainWindow, "onCaptureArea");
    }
    // QApplication::quit()
    close();
}

void ScreenshotWidget::breakCapture()
{
    // QApplication::quit()
    if (mainWindow)
    {
        mainWindow->show();
    }
    close();
}

void ScreenshotWidget::updateToolbarPosition()
{
    if (!toolbar || selectedRect.isEmpty())
    {
        return;
    }

    int toolbarWidth = toolbar->sizeHint().width();
    int toolbarHeight = toolbar->sizeHint().height();

    // 获取当前屏幕的可用区域（避开 Dock 和菜单栏）
    QScreen *screen = QGuiApplication::primaryScreen();

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

    // 更新所有可见的子工具栏位置
    if (penToolbar && penToolbar->isVisible())
        updatePenToolbarPosition();
    if (shapesToolbar && shapesToolbar->isVisible())
        updateShapesToolbarPosition();
    if (fontToolbar && fontToolbar->isVisible())
        updateFontToolbarPosition();
    if (EffectToolbar && EffectToolbar->isVisible())
        updateEffectToolbarPosition();
}

// 通用截图处理函数，使用回调函数处理最终输出
void ScreenshotWidget::processScreenshot(std::function<void(QPixmap &)> outputHandler)
{
    if (selectedRect.isEmpty())
    {
        return;
    }

    qDebug() << "=== Process Screenshot Debug Info ===";
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
            rect.rect.height() * devicePixelRatio);
        painter.setPen(QPen(rect.color, rect.width * devicePixelRatio));
        painter.setBrush(Qt::NoBrush);
        painter.drawRect(adjustedRect);
    }
    // 绘制所有椭圆
    for (const DrawnEllipse &ellipse : ellipses)
    {
        QRectF adjustedRect(
            (ellipse.rect.x() - selectedRect.x()) * devicePixelRatio,
            (ellipse.rect.y() - selectedRect.y()) * devicePixelRatio,
            ellipse.rect.width() * devicePixelRatio,
            ellipse.rect.height() * devicePixelRatio);
        painter.setPen(QPen(ellipse.color, ellipse.width * devicePixelRatio));
        painter.setBrush(Qt::NoBrush);
        painter.drawRect(adjustedRect);
    }

    // 绘制文本
    for (int i = 0; i < texts.size(); ++i)
    {
        const DrawnText &text = texts[i];
        // 如果正在编辑这个文字的内容（双击编辑，显示输入框），跳过绘制（隐藏原文字，只显示输入框）
        if (editingTextIndex == i && isTextInputActive)
        {
            continue; // 跳过绘制正在编辑的文字
        }
        QPoint adjustedPosition(
            (text.position.x() - selectedRect.x()) * devicePixelRatio,
            (text.position.y() - selectedRect.y()) * devicePixelRatio);

        // 绘制文字
        drawText(painter, adjustedPosition + QPoint(5, text.fontSize + 5),
                 text.text, text.color, text.font);
    }

    // 绘制画笔轨迹
    for (const DrawnPenStroke &stroke : penStrokes)
    {
        if (stroke.points.size() < 2)
            continue;

        QPen pen(stroke.color, stroke.width * devicePixelRatio);
        pen.setCapStyle(Qt::RoundCap);
        pen.setJoinStyle(Qt::RoundJoin);
        painter.setPen(pen);
        painter.setRenderHint(QPainter::Antialiasing);

        for (int i = 1; i < stroke.points.size(); i++)
        {
            QPointF start = (stroke.points[i - 1] - selectedRect.topLeft()) * devicePixelRatio;
            QPointF end = (stroke.points[i] - selectedRect.topLeft()) * devicePixelRatio;
            painter.drawLine(start, end);
        }
    }

    painter.end();
    // 应用马赛克/模糊效果（使用各自保存的强度）
    if (!EffectAreas.isEmpty())
    {
        QPixmap processedPixmap = croppedPixmap;

        for (int i = 0; i < EffectAreas.size(); ++i)
        {
            const QRect &EffectArea = EffectAreas[i];
            int strength = EffectStrengths[i];
            DrawMode mode = effectTypes[i]; // 使用保存的效果类型

            QRect relativeEffectArea = EffectArea.translated(-selectedRect.topLeft());
            relativeEffectArea = QRect(
                relativeEffectArea.x() * devicePixelRatio,
                relativeEffectArea.y() * devicePixelRatio,
                relativeEffectArea.width() * devicePixelRatio,
                relativeEffectArea.height() * devicePixelRatio);

            processedPixmap = applyEffect(processedPixmap, relativeEffectArea, strength, mode);
        }
        croppedPixmap = processedPixmap;
    }
    // ==================== 嵌入水印 ====================
#ifndef NO_OPENCV
    if (!watermarkText.isEmpty())
    {
        qDebug() << getText("watermark_embed_start", "开始嵌入水印:") << watermarkText;

        // 将 QPixmap 转换为 cv::Mat
        QImage imageWithAnnotations = croppedPixmap.toImage();

        // 确保图像格式正确
        if (imageWithAnnotations.format() != QImage::Format_RGB32 &&
            imageWithAnnotations.format() != QImage::Format_ARGB32)
        {
            imageWithAnnotations = imageWithAnnotations.convertToFormat(QImage::Format_RGB32);
        }

        // 转换为 OpenCV Mat
        cv::Mat cvImage(imageWithAnnotations.height(), imageWithAnnotations.width(), CV_8UC4,
                        (void *)imageWithAnnotations.constBits(), imageWithAnnotations.bytesPerLine());

        // 创建深拷贝以避免内存问题
        cv::Mat cvImageCopy = cvImage.clone();

        // 转换为 BGR 格式
        cv::Mat cvImageBGR;
        cv::cvtColor(cvImageCopy, cvImageBGR, cv::COLOR_RGBA2BGR);

        // 嵌入水印
        bool watermarkSuccess = embedRobustWatermark(cvImageBGR,
                                                     watermarkText.toStdString());

        if (watermarkSuccess)
        {
            qDebug() << getText("watermark_embed_success", "水印嵌入成功");

            // 将处理后的图像转换回 QImage
            cv::Mat cvImageRGBA;
            cv::cvtColor(cvImageBGR, cvImageRGBA, cv::COLOR_BGR2RGBA);

            // 创建 QImage 并使用 copy() 确保内存安全
            QImage watermarkedImage(cvImageRGBA.data,
                                    cvImageRGBA.cols,
                                    cvImageRGBA.rows,
                                    cvImageRGBA.step,
                                    QImage::Format_RGBA8888);

            croppedPixmap = QPixmap::fromImage(watermarkedImage.copy());
        }
        else
        {
            qDebug() << getText("watermark_embed_failed", "水印嵌入失败");
            QMessageBox::warning(this, getText("watermark_success_title", "警告"), getText("watermark_embed_error", "水印嵌入失败，将保存无水印图片"));
        }
    }
    else
    {
        qDebug() << getText("watermark_not_set", "未设置水印内容");
    }
#endif

    // 调用回调函数处理最终输出（保存或复制）
    outputHandler(croppedPixmap);

    // 就不进行收尾了，因为从产品用户的角度，在取消保存或者复制之后可能还想要操作
    // // 通用的收尾工作
    // // emit screenshotTaken();
    // // hide();
    // // if (mainWindow)
    // // {
    // //     mainWindow->show();
    // // }
    // // close();
}

void ScreenshotWidget::saveScreenshot()
{
    processScreenshot([this](QPixmap &pixmap)
                      {
        // 获取默认保存路径
        QString defaultPath = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
        QString defaultFileName = defaultPath + "/screenshot_" +
                                  QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss") + ".png";

        // 打开保存对话框
        QString fileName = QFileDialog::getSaveFileName(this,
                                                        getText("save_screenshot", "保存截图"),
                                                        defaultFileName,
                                                        getText("file_filter", "PNG图片 (*.png);;JPEG图片 (*.jpg);;所有文件 (*.*)"));

        if (!fileName.isEmpty())
        {
            // 根据文件扩展名确定保存格式
            QString suffix = QFileInfo(fileName).suffix().toLower();
            if (suffix != "png" && suffix != "jpg" && suffix != "jpeg")
            {
                fileName += ".png"; // 默认使用 PNG
                suffix = "png";
            }

            // 保存图片
            bool saveSuccess = false;
            if (suffix == "png")
            {
                saveSuccess = pixmap.save(fileName, "PNG", 100);
            }
            else if (suffix == "jpg" || suffix == "jpeg")
            {
                saveSuccess = pixmap.save(fileName, "JPEG", 95);
            }
            
            // 显示保存结果提示
            if (saveSuccess)
            {
                QMessageBox::information(this, 
                                       getText("save_success_title", "保存成功"),
                                       getText("save_success_message", "截图已成功保存到：\n") + fileName);
            }
            else
            {
                QMessageBox::warning(this,
                                   getText("save_failed_title", "保存失败"),
                                   getText("save_failed_message", "截图保存失败，请检查文件路径和权限。"));
            }
        } });
}

void ScreenshotWidget::copyToClipboard()
{
    processScreenshot([this](QPixmap &pixmap)
                      {
        // 复制到剪贴板
        QClipboard *clipboard = QGuiApplication::clipboard();
        clipboard->setPixmap(pixmap);
        
        // 显示复制成功提示
        QMessageBox::information(this,
                               getText("copy_success_title", "复制成功"),
                               getText("copy_success_message", "截图已成功复制到剪贴板。")); });
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
    double arrowAngle = M_PI / 6;    // 箭头角度 (30度)

    QPointF arrowP1 = end - QPointF(
                                arrowSize * std::cos(angle - arrowAngle),
                                arrowSize * std::sin(angle - arrowAngle));

    QPointF arrowP2 = end - QPointF(
                                arrowSize * std::cos(angle + arrowAngle),
                                arrowSize * std::sin(angle + arrowAngle));

    // 绘制箭头头部（实心三角形）
    QPolygonF arrowHead;
    arrowHead << end << arrowP1 << arrowP2;
    painter.drawPolygon(arrowHead);
}

void ScreenshotWidget::setupTextInput()
{
    // 添加文本输入框设置
    textInput = new QLineEdit(this);
    textInput->setStyleSheet(
        "QLineEdit{ background-color:rgba(255,255,255,240);color: black; "
        "border: 2px solid #0096FF; border-radius: 3px;padding: 5px;font-size: 14px; }"
        "QLineEdit:focus{border-color:#FF5500;}");
    textInput->setPlaceholderText(getText("text_placeholder", "输入文字..."));
    textInput->hide();

    // 连接信号
    connect(textInput, &QLineEdit::editingFinished, this, &ScreenshotWidget::onTextInputFinished);
    connect(textInput, &QLineEdit::returnPressed, this, &ScreenshotWidget::onTextInputFinished);
}

void ScreenshotWidget::onTextInputFinished()
{
    if (!textInput || textInput->text().isEmpty())
    {
        if (textInput)
        {
            textInput->hide();
            textInput->clear();
        }
        isTextInputActive = false;
        // 保持当前模式，不强制设为None，允许继续添加文字
        if (fontToolbar)
        {
            fontToolbar->hide();
        }
        return;
    }

    // 保存文本
    DrawnText drawnText;
    drawnText.position = textInputPosition;
    drawnText.text = textInput->text();
    drawnText.color = currentTextColor;
    drawnText.fontSize = currentFontSize;
    drawnText.font = currentTextFont;

    // 计算文字矩形大小
    QFontMetrics metrics(drawnText.font);
    QRect textRect = metrics.boundingRect(drawnText.text);
    textRect.moveTopLeft(textInputPosition);
    textRect.adjust(-2, -2, 2, 2); // 添加一些内边距

    // 如果在选中区域内，确保文字不超出截图框边界
    if (selected)
    {
        // 计算限制在截图框内的位置
        QPoint limitedPos = textInputPosition;
        int maxX = selectedRect.right() - textRect.width();
        int maxY = selectedRect.bottom() - textRect.height();

        limitedPos.setX(qMax(selectedRect.left(), qMin(maxX, limitedPos.x())));
        limitedPos.setY(qMax(selectedRect.top(), qMin(maxY, limitedPos.y())));

        // 更新文字位置和矩形
        textRect.moveTopLeft(limitedPos);
        drawnText.position = limitedPos;
    }

    drawnText.rect = textRect;

    // 根据是否正在编辑来决定是添加新文字还是替换现有文字
    if (editingTextIndex >= 0 && editingTextIndex < texts.size())
    {
        // 替换现有文字
        texts[editingTextIndex] = drawnText;
        editingTextIndex = -1; // 重置编辑索引
    }
    else
    {
        // 添加新文字
        texts.append(drawnText);
    }

    // 隐藏输入框并清除内容
    textInput->hide();
    textInput->clear();
    isTextInputActive = false;
    fontToolbar->hide();

    // 保持文本模式，允许继续添加文字
    currentDrawMode = Text;

    // 使用update()而不是repaint()，让Qt优化重绘过程
    update();
}

// 文本绘制函数
void ScreenshotWidget::drawText(QPainter &painter, const QPoint &position, const QString &text, const QColor &color, const QFont &font)
{
    painter.save();
    painter.setPen(color);
    painter.setFont(font);

    // 如果在选中区域内，创建裁剪区域，确保文字只在截图框内显示
    if (selected)
    {
        painter.setClipRect(selectedRect);
    }

    painter.drawText(position, text);
    painter.restore();
}

// 字体工具栏设置
void ScreenshotWidget::setTextToolbar()
{
    // 如果字体工具栏已经存在，直接返回
    if (fontToolbar)
    {
        return;
    }

    fontToolbar = new QWidget(this);
    fontToolbar->setStyleSheet(
        "QWidget { background-color: rgba(40, 40, 40, 200); border-radius: 5px; }"
        "QPushButton { background-color: rgba(60, 60, 60, 255); color: white; "
        "border: none; padding: 8px 12px; border-radius: 3px; font-size: 12px; }"
        "QPushButton:hover { background-color: rgba(80, 80, 80, 255); }"
        "QPushButton:pressed { background-color: rgba(50, 50, 50, 255); }"
        "QLabel { background-color: transparent; color: white; padding: 5px; font-size: 12px; }"
        "QPushButton#colorBtn { min-width: 60px; font-weight: bold; }");
    QHBoxLayout *fontLayout = new QHBoxLayout(fontToolbar);
    fontLayout->setSpacing(5);
    fontLayout->setContentsMargins(10, 5, 10, 5);

    // 字体颜色 - 使用统一的圆形颜色按钮
    btnFontColor = createColorButton(fontToolbar, currentTextColor);

    // 字体大小调节
    btnFontSizeDown = new QPushButton("-", fontToolbar);
    fontSizeInput = new QLineEdit(fontToolbar);
    fontSizeInput->setFixedWidth(50);
    fontSizeInput->setAlignment(Qt::AlignCenter);
    fontSizeInput->setStyleSheet("QLineEdit { background-color: rgba(80, 80, 80, 255); color: white; border: 1px solid rgba(100, 100, 100, 255); border-radius: 3px; padding: 5px; font-size: 12px; }");
    btnFontSizeUp = new QPushButton("+", fontToolbar);

    // 字体选择按钮
    btnFontFamily = new QPushButton(getText("font_button", "字体"), fontToolbar);

    fontLayout->addWidget(new QLabel(getText("text_settings", "文字设置："), fontToolbar));
    fontLayout->addWidget(btnFontColor);
    fontLayout->addWidget(btnFontSizeDown);
    fontLayout->addWidget(fontSizeInput);
    fontLayout->addWidget(btnFontSizeUp);
    fontLayout->addWidget(btnFontFamily);

    fontToolbar->adjustSize();
    fontToolbar->hide();

    // 初始化字体设置
    currentTextFont = QFont("Arial", 18);
    currentTextColor = Qt::red;
    currentFontSize = 18;
    updateFontToolbar();

    // 连接信号
    connect(btnFontColor, &QPushButton::clicked, this, &ScreenshotWidget::onTextColorClicked);
    connect(btnFontSizeUp, &QPushButton::clicked, this, &ScreenshotWidget::increaseFontSize);
    connect(btnFontSizeDown, &QPushButton::clicked, this, &ScreenshotWidget::decreaseFontSize);
    connect(btnFontFamily, &QPushButton::clicked, this, &ScreenshotWidget::onFontFamilyClicked);
    connect(fontSizeInput, &QLineEdit::editingFinished, this, &ScreenshotWidget::onFontSizeInputChanged);
}

void ScreenshotWidget::onFontSizeInputChanged()
{
    if (fontSizeInput)
    {
        bool ok;
        int newSize = fontSizeInput->text().toInt(&ok);
        if (ok && newSize > 0 && newSize <= 100)
        { // 限制字体大小范围
            currentFontSize = newSize;
            currentTextFont.setPixelSize(currentFontSize);
            updateFontToolbar(); // 确保显示的是正确值
            updateTextInputStyle();

            // 如果单击选择了文字（editingTextIndex >= 0 但 isTextInputActive = false），立即保存属性
            if (!isTextInputActive && editingTextIndex >= 0 && editingTextIndex < texts.size())
            {
                saveTextProperties();
            }
        }
        else
        {
            // 输入无效，恢复原值
            fontSizeInput->setText(QString("%1").arg(currentFontSize));
        }
    }
}

void ScreenshotWidget::updateFontToolbar()
{
    if (fontSizeInput)
    {
        fontSizeInput->setText(QString("%1").arg(currentFontSize));
    }
    // 更新颜色按钮显示 - 使用统一的更新方法
    if (btnFontColor)
    {
        updateColorButton(btnFontColor, currentTextColor);
    }
}

// 字体颜色选择
void ScreenshotWidget::onTextColorClicked()
{
    QColor color = QColorDialog::getColor(currentTextColor, this, getText("select_text_color", "选择文字颜色"));
    if (color.isValid())
    {
        currentTextColor = color;
        updateFontToolbar();

        // 如果正在编辑文本，更新输入框的样式
        if (isTextInputActive && textInput)
        {
            QPalette palette = textInput->palette();
            palette.setColor(QPalette::Text, currentTextColor);
            textInput->setPalette(palette);

            // 更新当前编辑的文本样式并重新计算大小
            updateTextInputSize();
        }

        // 如果单击选择了文字（editingTextIndex >= 0 但 isTextInputActive = false），立即保存属性
        if (!isTextInputActive && editingTextIndex >= 0 && editingTextIndex < texts.size())
        {
            saveTextProperties();
        }

        update();
    }
}

// 增大字号
void ScreenshotWidget::increaseFontSize()
{
    if (currentFontSize < 72)
    {
        currentFontSize++;
        currentTextFont.setPixelSize(currentFontSize);
        updateFontToolbar();

        // 如果正在编辑文本，更新输入框的样式
        if (isTextInputActive && textInput)
        {
            QFont font = textInput->font();
            font.setPixelSize(currentFontSize);
            textInput->setFont(font);

            // 更新当前编辑的文本样式并重新计算大小
            updateTextInputSize();
        }

        // 如果单击选择了文字（editingTextIndex >= 0 但 isTextInputActive = false），立即保存属性
        if (!isTextInputActive && editingTextIndex >= 0 && editingTextIndex < texts.size())
        {
            saveTextProperties();
        }

        update();
    }
}

// 减小字号
void ScreenshotWidget::decreaseFontSize()
{
    if (currentFontSize > 8)
    {
        currentFontSize--;
        currentTextFont.setPixelSize(currentFontSize);
        updateFontToolbar();

        // 如果正在编辑文本，更新输入框的样式
        if (isTextInputActive && textInput)
        {
            QFont font = textInput->font();
            font.setPixelSize(currentFontSize);
            textInput->setFont(font);

            // 更新当前编辑的文本样式并重新计算大小
            updateTextInputSize();
        }

        // 如果单击选择了文字（editingTextIndex >= 0 但 isTextInputActive = false），立即保存属性
        if (!isTextInputActive && editingTextIndex >= 0 && editingTextIndex < texts.size())
        {
            saveTextProperties();
        }

        update();
    }
}

// 字体选择
void ScreenshotWidget::onFontFamilyClicked()
{
    bool ok;
    QFont font = QFontDialog::getFont(&ok, currentTextFont, this, getText("select_font", "选择字体"));
    if (ok)
    {
        currentTextFont = font;
        // 优先使用 pixelSize，如果为 -1 则使用 pointSize 转换
        int pixelSize = font.pixelSize();
        if (pixelSize == -1)
        {
            // 如果使用 point size，转换为 pixel size（假设 96 DPI）
            pixelSize = font.pointSize() * 96 / 72;
        }
        if (pixelSize > 0)
        {
            currentFontSize = pixelSize;
            currentTextFont.setPixelSize(currentFontSize);
        }
        updateFontToolbar();

        // 如果正在编辑文本，更新输入框的样式
        if (isTextInputActive && textInput)
        {
            textInput->setFont(currentTextFont);

            // 更新当前编辑的文本样式并重新计算大小
            updateTextInputSize();
        }

        // 如果单击选择了文字（editingTextIndex >= 0 但 isTextInputActive = false），立即保存属性
        if (!isTextInputActive && editingTextIndex >= 0 && editingTextIndex < texts.size())
        {
            saveTextProperties();
        }

        update();
    }
}

void ScreenshotWidget::setupPenToolbar()
{
    // 画笔工具栏设置
    penToolbar = new QWidget(this);
    penToolbar->setStyleSheet(
        "QWidget{background-color:rgba(40, 40, 40, 200);border-radius: 5px;}"
        "QPushButton { background-color: rgba(60, 60, 60, 255); color: white; "
        "border: none; padding: 8px 15px; border-radius: 3px; font-size: 13px; }"
        "QPushButton:hover { background-color: rgba(80, 80, 80, 255); }"
        "QPushButton:pressed { background-color: rgba(50, 50, 50, 255); }"
        "QLabel { background-color: transparent; color: white; padding: 5px; font-size: 12px; }"
        "QPushButton#colorBtn { min-width: 60px; font-weight: bold; }");

    QHBoxLayout *penLayout = new QHBoxLayout(penToolbar);
    penLayout->setSpacing(5);
    penLayout->setContentsMargins(10, 8, 10, 8);

    // 颜色选择按钮 - 使用统一的圆形颜色按钮
    QPushButton *btnColorPicker = createColorButton(penToolbar, currentPenColor);

    // 粗细调节按钮
    btnPenWidthUp = new QPushButton("+", penToolbar);
    btnPenWidthDown = new QPushButton("-", penToolbar);
    penWidthLabel = new QLabel(QString("%1px").arg(currentPenWidth), penToolbar);
    penWidthLabel->setFixedWidth(40);
    penWidthLabel->setAlignment(Qt::AlignCenter);

    // 添加预览标签
    QLabel *previewLabel = new QLabel(getText("preview", "预览:"), penToolbar);

    // 添加预览画布
    QLabel *previewCanvas = new QLabel(penToolbar);
    previewCanvas->setFixedSize(30, 30);
    updatePreviewCanvas(previewCanvas);

    penLayout->addWidget(previewLabel);
    penLayout->addWidget(previewCanvas);
    penLayout->addSpacing(10);
    penLayout->addWidget(btnColorPicker);
    penLayout->addSpacing(5);
    penLayout->addWidget(new QLabel(getText("thickness", "粗细："), penToolbar));
    penLayout->addWidget(btnPenWidthDown);
    penLayout->addWidget(penWidthLabel);
    penLayout->addWidget(btnPenWidthUp);

    penToolbar->adjustSize();
    penToolbar->hide();

    // 连接信号
    connect(btnPenWidthUp, &QPushButton::clicked, this, &ScreenshotWidget::increasePenWidth);
    connect(btnPenWidthDown, &QPushButton::clicked, this, &ScreenshotWidget::decreasePenWidth);
    connect(btnColorPicker, &QPushButton::clicked, this, &ScreenshotWidget::onColorPickerClicked);

    // 颜色改变时更新预览 - 使用统一的更新方法
    connect(this, &ScreenshotWidget::penColorChanged, this, [btnColorPicker, previewCanvas, this]()
            {
       updateColorButton(btnColorPicker, currentPenColor);
       updatePreviewCanvas(previewCanvas); });
}

void ScreenshotWidget::updatePreviewCanvas(QLabel *canvas)
{
    QPixmap pixmap(canvas->size());
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);

    // 绘制预览线条
    QPen pen(currentPenColor, currentPenWidth);
    pen.setCapStyle(Qt::RoundCap);
    painter.setPen(pen);

    // 绘制一条示例线
    int centerY = pixmap.height() / 2;
    painter.drawLine(5, centerY, pixmap.width() - 5, centerY);

    painter.end();
    canvas->setPixmap(pixmap);
}

void ScreenshotWidget::onPenButtonClicked()
{
    currentDrawMode = DrawMode::Pen;
    qDebug() << getText("pen_mode_activated", "画笔模式激活");
}

void ScreenshotWidget::onColorPickerClicked()
{
    QColor color = QColorDialog::getColor(currentPenColor, this, getText("select_pen_color", "选择画笔颜色"));
    if (color.isValid())
    {
        currentPenColor = color;
        emit penColorChanged();
        update();
    }
}
void ScreenshotWidget::increasePenWidth()
{
    if (currentPenWidth < 20)
    {
        currentPenWidth++;
        updatePenWidthLabel();
        emit penColorChanged();
        update();
    }
}
void ScreenshotWidget::decreasePenWidth()
{
    if (currentPenWidth > 1)
    {
        currentPenWidth--;
        updatePenWidthLabel();
        emit penColorChanged();
        update();
    }
}
void ScreenshotWidget::updatePenWidthLabel()
{
    if (penWidthLabel)
    {
        penWidthLabel->setText(QString("%1px").arg(currentPenWidth));
    }
    if (shapeWidthLabel)
    {
        shapeWidthLabel->setText(QString("%1px").arg(currentPenWidth));
    }
}

void ScreenshotWidget::updatePenToolbarPosition()
{
    if (!penToolbar || !btnPen) return;

    penToolbar->adjustSize();
    QSize toolbarSize = penToolbar->size();

    // 长截图模式使用第一个算法
    if (captureMode == ScrollMode) {
        // 方法1：直接使用按钮下方位置（窗口坐标）
        QPoint btnPos = btnPen->pos();
        int x = btnPos.x();
        int y = btnPos.y() + btnPen->height() + 5;

        // 转换为全局坐标检查是否在屏幕内
        QPoint globalPos = mapToGlobal(QPoint(x, y + toolbarSize.height()));
        QScreen *screen = QGuiApplication::primaryScreen();
        QRect availableGeometry = screen->availableGeometry();

        qDebug() << "画笔工具栏底部全局Y：" << globalPos.y();
        qDebug() << "屏幕底部Y：" << availableGeometry.bottom();

        // 如果会超出屏幕，放在按钮上方
        if (globalPos.y() > availableGeometry.bottom()) {
            y = btnPos.y() - toolbarSize.height() - 5;
            qDebug() << "调整为上方位置：" << y;
        }

        // 水平方向也检查
        globalPos = mapToGlobal(QPoint(x + toolbarSize.width(), y));
        if (globalPos.x() > availableGeometry.right()) {
            x = availableGeometry.right() - mapToGlobal(QPoint(0, 0)).x() - toolbarSize.width() - 10;
            qDebug() << "调整水平位置：" << x;
        }

        // 确保不超出窗口边界
        QRect windowRect = rect();
        if (x < 0) x = 0;
        if (x + toolbarSize.width() > windowRect.width()) {
            x = windowRect.width() - toolbarSize.width();
        }
        if (y < 0) y = 0;
        if (y + toolbarSize.height() > windowRect.height()) {
            y = windowRect.height() - toolbarSize.height();
        }

        penToolbar->move(x, y);

        qDebug() << "画笔工具栏最终位置：" << QPoint(x, y)
                 << "（全局：" << mapToGlobal(QPoint(x, y)) << "）";
    }
    // 区域截图和其他模式使用第二个算法
    else {
        if (!penToolbar || !selected)
            return;

        penToolbar->adjustSize();
        int toolbarWidth = penToolbar->sizeHint().width();
        int toolbarHeight = penToolbar->sizeHint().height();

        // 获取可用区域（避开Dock栏）
        QScreen *screen = QGuiApplication::primaryScreen();
        QRect availableGeometry = screen->availableGeometry();
        QPoint globalPos = mapToGlobal(QPoint(0, 0));

        int maxY = availableGeometry.bottom() - globalPos.y();
        int maxX = availableGeometry.right() - globalPos.x();
        int minX = availableGeometry.left() - globalPos.x();
        int minY = availableGeometry.top() - globalPos.y();

        // 尝试对齐到画笔按钮
        int x, y;
        if (btnPen && toolbar && toolbar->isVisible())
        {
            QPoint btnPos = btnPen->mapTo(this, QPoint(0, 0));
            x = btnPos.x();
            y = btnPos.y() + btnPen->height() + 5;

            // 如果下方空间不足，放上方
            if (y + toolbarHeight > maxY)
            {
                y = btnPos.y() - toolbarHeight - 5;
            }
        }
        else if (toolbar)
        {
            // 默认放在主工具栏下方
            x = toolbar->x();
            y = toolbar->y() + toolbar->height() + 5;

            if (y + toolbarHeight > maxY)
            {
                y = toolbar->y() - toolbarHeight - 5;
            }
        }
        else
        {
            // toolbar 为空，使用默认位置
            x = minX + 10;
            y = minY + 10;
        }

        // 水平方向边界检查
        if (x + toolbarWidth > maxX)
        {
            x = maxX - toolbarWidth - 5;
        }
        if (x < minX + 5)
            x = minX + 5;

        // 垂直方向再次检查
        if (y < minY + 5)
            y = minY + 5;
        if (y + toolbarHeight > maxY)
            y = maxY - toolbarHeight - 5;

        penToolbar->move(x, y);
    }
}

void ScreenshotWidget::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (selected && (currentDrawMode == None || currentDrawMode == Text))
    {
        QPoint clickPos = event->pos();
        for (int i = texts.size() - 1; i >= 0; i--)
        {
            if (texts[i].rect.contains(clickPos))
            {
                // 设置双击标志，防止单击处理
                textDoubleClicked = true;
                potentialTextDrag = false;
                textClickIndex = -1;

                // 双击编辑现有文字
                editExistingText(i);
                break;
            }
        }
    }
}

void ScreenshotWidget::editExistingText(int textIndex)
{
    if (textIndex < 0 || textIndex >= texts.size())
        return;

    DrawnText &text = texts[textIndex];

    // 切换到文本模式
    currentDrawMode = Text;

    // 保存正在编辑的文字索引（必须在设置 isTextInputActive 之前）
    editingTextIndex = textIndex;

    // 设置输入框位置和内容
    textInputPosition = text.position;

    // 确保textInput存在
    if (textInput)
    {
        textInput->move(text.position);
        textInput->resize(text.rect.size());
        textInput->setText(text.text);
        textInput->show();
        textInput->setFocus();
        textInput->selectAll();

        // 设置文本输入激活标志（必须在 editingTextIndex 之后）
        isTextInputActive = true;

        // 显示字体工具栏
        if (fontToolbar)
        {
            updateFontToolbar();
            updateFontToolbarPosition();
            fontToolbar->show();
            fontToolbar->raise();
        }

        // 强制重绘，隐藏原文字
        update();
        repaint(); // 立即重绘，确保文字被隐藏
    }
}

// 单击文字时修改属性（颜色、大小、字体）
void ScreenshotWidget::selectTextForPropertyEdit(int textIndex)
{
    if (textIndex < 0 || textIndex >= texts.size())
        return;

    DrawnText &text = texts[textIndex];

    // 切换到文本模式
    currentDrawMode = Text;

    // 保存正在编辑的文字索引（但不显示输入框）
    editingTextIndex = textIndex;

    // 加载文字的当前属性
    currentTextColor = text.color;
    currentTextFont = text.font;
    currentFontSize = text.fontSize;
    // 确保字体大小与 currentFontSize 一致
    currentTextFont.setPixelSize(currentFontSize);

    // 不显示输入框，只显示字体工具栏用于修改属性
    if (textInput)
    {
        textInput->hide();
    }
    isTextInputActive = false;

    // 显示字体工具栏
    if (fontToolbar)
    {
        updateFontToolbar();
        updateFontToolbarPosition();
        fontToolbar->show();
        fontToolbar->raise();
    }

    update();
}

// 保存文字属性（不修改文本内容）
void ScreenshotWidget::saveTextProperties()
{
    if (editingTextIndex < 0 || editingTextIndex >= texts.size())
    {
        return;
    }

    DrawnText &existingText = texts[editingTextIndex];

    // 保存新的属性
    existingText.color = currentTextColor;
    existingText.fontSize = currentFontSize;
    QFont fontToSave = currentTextFont;
    fontToSave.setPixelSize(currentFontSize);
    existingText.font = fontToSave;

    // 重新计算文字矩形大小（因为字体大小可能改变）
    QFontMetrics metrics(existingText.font);
    QRect textRect = metrics.boundingRect(existingText.text);
    textRect.moveTopLeft(existingText.position);
    textRect.adjust(-2, -2, 2, 2);
    existingText.rect = textRect;

    qDebug() << "保存文字属性 - 颜色:" << existingText.color.name()
             << "大小:" << existingText.fontSize
             << "编辑索引:" << editingTextIndex;

    update();
}

// Pin 到桌面
void ScreenshotWidget::pinToDesktop()
{
    // 1. 检查是否有选区
    if (selectedRect.isNull())
    {
        return;
    }

    // 2. 处理当前的绘图效果
    QPixmap finalPixmap = this->grab(selectedRect);

    // 3. 创建 Pin 窗口
    PinWidget *pin = new PinWidget(finalPixmap);
    pin->setMainWindow(mainWindow);

    // 4. 让贴图出现在选区的原位置
    QPoint globalPos = this->mapToGlobal(selectedRect.topLeft());
    pin->move(globalPos);

    // 5. 显示贴图
    pin->show();

    // 6. 关闭截图主窗口
    close();
    emit screenshotTaken();
}

QList<WindowInfo> ScreenshotWidget::enumAllValidWindows()
{
    QList<WindowInfo> windows;
#ifdef Q_OS_WIN
    EnumWindows(EnumWindowsProc, reinterpret_cast<LPARAM>(&windows));
#elif defined(Q_OS_MAC)
    CFArrayRef windowList = CGWindowListCopyWindowInfo(kCGWindowListOptionOnScreenOnly | kCGWindowListExcludeDesktopElements, kCGNullWindowID);
    if (windowList)
    {
        CFIndex count = CFArrayGetCount(windowList);
        for (CFIndex i = 0; i < count; ++i)
        {
            CFDictionaryRef dict = (CFDictionaryRef)CFArrayGetValueAtIndex(windowList, i);

            CFNumberRef layerNum = (CFNumberRef)CFDictionaryGetValue(dict, kCGWindowLayer);
            int layer = 0;
            if (layerNum)
                CFNumberGetValue(layerNum, kCFNumberIntType, &layer);
            if (layer != 0)
                continue;

            CGRect bounds;
            CFDictionaryRef boundsDict = (CFDictionaryRef)CFDictionaryGetValue(dict, kCGWindowBounds);
            if (boundsDict)
                CGRectMakeWithDictionaryRepresentation(boundsDict, &bounds);
            else
                continue;

            WindowInfo info;
            // info.title = "Window"; // Dummy title for isValid()

            // CoreGraphics 使用以屏幕左下角为原点的坐标系，Qt 使用左上角为原点，需翻转 Y 轴。
            CGRect mainBounds = CGDisplayBounds(CGMainDisplayID());
            double mainHeight = mainBounds.size.height;

            int qtX = static_cast<int>(bounds.origin.x);
            int qtY = static_cast<int>(mainHeight - (bounds.origin.y + bounds.size.height));
            int qtW = static_cast<int>(bounds.size.width);
            int qtH = static_cast<int>(bounds.size.height);

            info.rect = QRect(qtX, qtY, qtW, qtH);

            if (info.rect.width() < 10 || info.rect.height() < 10)
                continue;

            windows.append(info);
        }
        CFRelease(windowList);
    }
#endif
    return windows;
}

void ScreenshotWidget::captureWindow(QPoint mousePos)
{
    // 遍历窗口列表，找到鼠标所在的顶层窗口
    for (const auto &window : m_validWindows)
    {
#ifdef Q_OS_WIN
        // 使用精准边界判断（适配DWM和高DPI）
        QRect accurateRect = getAccurateWindowRect(window);
        if (accurateRect.contains(mousePos))
        {
            m_hoverWindow = window;
            break; // 顶层窗口优先（枚举顺序为顶层在前）
        }
#else
        // macOS: 直接使用窗口矩形
        if (window.rect.contains(mousePos))
        {
            m_hoverWindow = window;
            break;
        }
#endif
    }
}

// ============ 自动人脸打码功能实现 ============
// 自动检测并打码人脸
void ScreenshotWidget::autoDetectAndBlurFaces()
{
#ifndef NO_OPENCV
    if (!selected || selectedRect.isEmpty())
    {
        qDebug() << "未选中区域，无法进行人脸检测";
        return;
    }

    // 检查人脸检测器是否可用
    if (!faceDetector)
    {
        qDebug() << "错误：人脸检测器指针为空";
        return;
    }

    if (!faceDetector->isReady())
    {
        qDebug() << "人脸检测器未初始化，尝试重新初始化...";
        if (!faceDetector->initialize())
        {
            qDebug() << "人脸检测器初始化失败，跳过自动打码";
            QMessageBox::critical(this,
                                  getText("face_blur_error_title", "错误"),
                                  getText("face_blur_init_failed", "人脸检测器初始化失败\n\n可能的原因：\n1. 模型文件缺失（opencv_face_detector_uint8.pb 和 opencv_face_detector.pbtxt.txt）\n2. OpenCV 库未正确配置\n\n请检查 models 目录下是否有模型文件"));
            return;
        }
        if (!faceDetector->isReady())
        {
            qDebug() << "人脸检测器初始化后仍不可用，跳过自动打码";
            QMessageBox::critical(this,
                                  getText("face_blur_error_title", "错误"),
                                  getText("face_blur_not_ready", "人脸检测器初始化后仍不可用\n\n请检查：\n1. 模型文件是否完整\n2. OpenCV 库是否正确安装"));
            return;
        }
        qDebug() << "人脸检测器重新初始化成功";
    }

    qDebug() << "开始自动人脸检测，选中区域:" << selectedRect;

    // 从选中的区域提取图像（物理坐标）
    // 将窗口坐标转换为截图坐标（考虑虚拟桌面偏移，与paintEvent中的逻辑保持一致）
    QPoint windowPos = geometry().topLeft();
    QPoint offset = windowPos - virtualGeometryTopLeft;

    // 使用浮点数计算保持精度，避免多次舍入导致的累积误差
    double physicalX = (selectedRect.x() + offset.x()) * devicePixelRatio;
    double physicalY = (selectedRect.y() + offset.y()) * devicePixelRatio;
    double physicalW = selectedRect.width() * devicePixelRatio;
    double physicalH = selectedRect.height() * devicePixelRatio;

    QRect physicalRect(
        qRound(physicalX),
        qRound(physicalY),
        qRound(physicalW),
        qRound(physicalH));

    // 确保物理矩形在屏幕截图范围内
    physicalRect = physicalRect.intersected(screenPixmap.rect());

    if (physicalRect.isEmpty())
    {
        qDebug() << "物理矩形为空，无法提取图像";
        return;
    }

    QPixmap selectedPixmap = screenPixmap.copy(physicalRect);

    // 性能优化：如果图像太大，先缩放再检测（提高速度）
    // 提高最大检测尺寸以提高精度
    QPixmap detectionPixmap = selectedPixmap;
    double scaleFactor = 1.0;
    int maxDetectionSize = 3840; // 进一步提高到3840（4K分辨率），使用更高分辨率检测以提高精度

    if (selectedPixmap.width() > maxDetectionSize || selectedPixmap.height() > maxDetectionSize)
    {
        // 计算缩放比例，保持宽高比
        double widthScale = (double)maxDetectionSize / selectedPixmap.width();
        double heightScale = (double)maxDetectionSize / selectedPixmap.height();
        scaleFactor = qMin(widthScale, heightScale);

        // 使用浮点数计算保持精度
        double scaledWidth = selectedPixmap.width() * scaleFactor;
        double scaledHeight = selectedPixmap.height() * scaleFactor;
        // 只在最后一步进行四舍五入
        int finalWidth = qRound(scaledWidth);
        int finalHeight = qRound(scaledHeight);
        // 使用高质量缩放算法，提高检测精度
        detectionPixmap = selectedPixmap.scaled(finalWidth, finalHeight,
                                                Qt::KeepAspectRatio,
                                                Qt::SmoothTransformation);
        qDebug() << "图像过大，缩放检测:" << selectedPixmap.size() << "->" << detectionPixmap.size()
                 << "缩放因子:" << scaleFactor;
    }

    // 使用 FaceDetector 检测人脸（在缩放后的图像上）
    QList<QRect> detectedFaces = faceDetector->detectFaces(detectionPixmap);

    // 如果进行了缩放，需要将检测结果坐标还原
    // 使用更精确的浮点数计算，减少精度丢失
    if (scaleFactor < 1.0)
    {
        for (QRect &face : detectedFaces)
        {
            // 使用浮点数计算，保持精度
            double x = face.x() / scaleFactor;
            double y = face.y() / scaleFactor;
            double w = face.width() / scaleFactor;
            double h = face.height() / scaleFactor;
            // 只在最后一步进行四舍五入
            face.setX(qRound(x));
            face.setY(qRound(y));
            face.setWidth(qRound(w));
            face.setHeight(qRound(h));
        }
    }

    if (detectedFaces.isEmpty())
    {
        qDebug() << "未检测到人脸，检测图像尺寸:" << detectionPixmap.size();
        return;
    }

    qDebug() << "检测到" << detectedFaces.size() << "个人脸，开始应用打码";

    // 将检测到的人脸区域转换为逻辑坐标，并添加到EffectAreas
    // offset已在函数开始时计算，直接使用
    for (const QRect &faceRect : detectedFaces)
    {
        // faceRect是相对于selectedPixmap的坐标（物理坐标）
        // selectedPixmap是从screenPixmap的physicalRect位置提取的
        // physicalRect = (selectedRect.x() + offset.x()) * devicePixelRatio, ...
        // 所以faceRect的坐标是相对于physicalRect的物理像素坐标
        // 需要转换为窗口逻辑坐标（EffectAreas中存储的是窗口逻辑坐标）

        // 1. 先转换为逻辑坐标（相对于physicalRect的逻辑坐标）
        // 使用浮点数计算保持精度，避免多次舍入导致的累积误差
        double logicalX = faceRect.x() / devicePixelRatio;
        double logicalY = faceRect.y() / devicePixelRatio;
        double logicalW = faceRect.width() / devicePixelRatio;
        double logicalH = faceRect.height() / devicePixelRatio;

        // 2. 转换为窗口逻辑坐标
        // physicalRect的逻辑坐标是 (selectedRect.x() + offset.x(), selectedRect.y() + offset.y())
        // 所以需要加上这个偏移量
        double windowX = logicalX + selectedRect.x() + offset.x();
        double windowY = logicalY + selectedRect.y() + offset.y();

        // 只在最后一步进行四舍五入，减少精度丢失
        QRect logicalFaceRect(
            qRound(windowX),
            qRound(windowY),
            qRound(logicalW),
            qRound(logicalH));

        // 3. 确保在选中区域内
        logicalFaceRect = logicalFaceRect.intersected(selectedRect);

        if (!logicalFaceRect.isEmpty())
        {
            // 再次验证检测框是否符合人脸特征（在逻辑坐标下）
            // 宽高比验证：人脸通常是接近正方形的
            double aspectRatio = (double)logicalFaceRect.width() / logicalFaceRect.height();
            if (aspectRatio < 0.4 || aspectRatio > 2.5)
            {
                qDebug() << "过滤掉宽高比异常的人脸检测框:" << logicalFaceRect
                         << "宽高比:" << aspectRatio;
                continue; // 宽高比异常，可能是误检（如文字、矩形框等）
            }

            // 添加较小的边距，确保完全覆盖人脸边缘，但避免覆盖文字
            // 策略：主要在垂直方向（上下）添加边距，水平方向（左右）使用更小的边距
            // 因为文字通常在身份证、证件等场景中位于人脸左右两侧
            int faceSize = qMax(logicalFaceRect.width(), logicalFaceRect.height());
            double marginRatioX, marginRatioY;

            if (faceSize < 50)
            {
                // 小人脸：水平方向3%，垂直方向5%
                marginRatioX = 0.03;
                marginRatioY = 0.05;
            }
            else if (faceSize < 100)
            {
                // 中等大小人脸：水平方向5%，垂直方向8%
                marginRatioX = 0.05;
                marginRatioY = 0.08;
            }
            else
            {
                // 大人脸：水平方向8%，垂直方向10%（最多，避免过大）
                marginRatioX = 0.08;
                marginRatioY = 0.10;
            }

            // 使用浮点数计算保持精度
            double marginX = logicalFaceRect.width() * marginRatioX;
            double marginY = logicalFaceRect.height() * marginRatioY;
            // 只在最后一步进行四舍五入
            logicalFaceRect.adjust(-qRound(marginX), -qRound(marginY),
                                   qRound(marginX), qRound(marginY));

            // 再次确保在选中区域内
            logicalFaceRect = logicalFaceRect.intersected(selectedRect);

            if (!logicalFaceRect.isEmpty() && logicalFaceRect.width() > 5 && logicalFaceRect.height() > 5)
            {
                // 添加到效果区域列表
                EffectAreas.append(logicalFaceRect);
                EffectStrengths.append(20); // 使用默认强度（模糊半径）
                effectTypes.append(Blur);   // 使用模糊效果（也可以改为Mosaic）

                qDebug() << "添加人脸打码区域:" << logicalFaceRect
                         << "原始检测坐标:" << faceRect
                         << "人脸大小:" << faceSize
                         << "水平边距比例:" << marginRatioX
                         << "垂直边距比例:" << marginRatioY
                         << "物理矩形:" << physicalRect
                         << "窗口偏移:" << offset
                         << "强度:" << 20;
            }
        }
    }

    qDebug() << "自动人脸打码完成，共添加" << detectedFaces.size() << "个打码区域";

    // 触发重绘
    update();
#else
    qDebug() << "OpenCV未启用，无法使用人脸检测功能";
#endif
}

#ifdef Q_OS_WIN
BOOL CALLBACK ScreenshotWidget::EnumWindowsProc(HWND hwnd, LPARAM lParam)
{
    QList<WindowInfo> *windowList = reinterpret_cast<QList<WindowInfo> *>(lParam);

    // 过滤：无效句柄、隐藏窗口、最小化窗口
    if (!hwnd || !IsWindowVisible(hwnd) || IsIconic(hwnd))
    {
        return TRUE;
    }

    WCHAR titleBuf[256] = {0};
    GetWindowTextW(hwnd, titleBuf, _countof(titleBuf));
    QString title = QString::fromWCharArray(titleBuf);
    LONG_PTR style = GetWindowLongPtrW(hwnd, GWL_STYLE);

    // 过滤无标题或系统设置窗口
    if (title.isEmpty())
    {
        if ((style & WS_CHILD) || ((style & WS_POPUP) && !(style & WS_CAPTION)))
        {
            return TRUE;
        }
    }

    // 过滤透明窗口
    style = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
    if (style & WS_EX_TRANSPARENT)
    {
        return TRUE;
    }

    // 过滤幻影窗口
    DWORD CloakedVal = 0;
    HRESULT hRes = DwmGetWindowAttribute(hwnd, DWMWA_CLOAKED, &CloakedVal, sizeof(CloakedVal));

    // 调用成功且窗口被隐藏
    if (SUCCEEDED(hRes) && (CloakedVal != 0))
    {
        return TRUE;
    }

    // 过滤自身进程窗口
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (hProcess)
    {
        WCHAR exeName[256] = {0};
        GetModuleBaseNameW(hProcess, nullptr, exeName, _countof(exeName));
        CloseHandle(hProcess);
        if (QString::fromWCharArray(exeName) == QCoreApplication::applicationName())
        {
            return TRUE;
        }
    }

    // 获取窗口基础矩形
    RECT winRect;
    GetWindowRect(hwnd, &winRect);
    int width = winRect.right - winRect.left;
    int height = winRect.bottom - winRect.top;

    // 过滤过小窗口
    if (width < 50 && height < 50)
    {
        return TRUE;
    }

    // 过滤全屏空窗口
    QScreen *primaryScreen = QGuiApplication::primaryScreen();
    QRect screenRect = primaryScreen->geometry();
    bool isNearFullScreen = (qAbs(width - screenRect.width()) < 20) &&
                            (qAbs(height - screenRect.height()) < 20);

    if (isNearFullScreen)
    {
        bool hasValidChild = false;
        EnumChildWindows(hwnd, EnumChildProc, reinterpret_cast<LPARAM>(&hasValidChild));
        if (!hasValidChild)
        {
            return TRUE;
        }
    }

    // 写入窗口信息
    WindowInfo info;
    info.hwnd = hwnd;
    info.rect = QRect(winRect.left, winRect.top, width, height);
    windowList->append(info);

    return TRUE;
}

BOOL CALLBACK ScreenshotWidget::EnumChildProc(HWND childHwnd, LPARAM lParam)
{
    bool *pHasValid = reinterpret_cast<bool *>(lParam);
    if (IsWindowVisible(childHwnd))
    {
        RECT childRect;
        GetWindowRect(childHwnd, &childRect);
        int cw = childRect.right - childRect.left;
        int ch = childRect.bottom - childRect.top;

        // 子窗口有效尺寸判断
        if (cw > 50 && ch > 50)
        {
            *pHasValid = true;
            return FALSE; // 找到即停止枚举
        }
    }
    return TRUE;
}
#endif

// 精准获取窗口边界（跨平台）
QRect ScreenshotWidget::getAccurateWindowRect(const WindowInfo &window)
{
#ifdef Q_OS_WIN
    // Windows: 先尝试读取DWM扩展边框
    RECT extendedFrameRect;

    HRESULT hr = DwmGetWindowAttribute(
        window.hwnd,
        DWMWA_EXTENDED_FRAME_BOUNDS,
        &extendedFrameRect,
        sizeof(extendedFrameRect));
    if (SUCCEEDED(hr))
    {
        // 高DPI适配：除以设备像素比
        return QRect(
            extendedFrameRect.left / devicePixelRatio,
            extendedFrameRect.top / devicePixelRatio,
            (extendedFrameRect.right - extendedFrameRect.left) / devicePixelRatio,
            (extendedFrameRect.bottom - extendedFrameRect.top) / devicePixelRatio);
    }

    // Windows 备用方案：获取普通窗口矩形并适配DPI
    RECT rect;
    GetWindowRect(window.hwnd, &rect);
    return QRect(rect.left / devicePixelRatio,
                 rect.top / devicePixelRatio,
                 (rect.right - rect.left) / devicePixelRatio,
                 (rect.bottom - rect.top) / devicePixelRatio);
#else
    // macOS: 直接返回窗口矩形（已在枚举时处理好坐标系转换和DPI）
    return window.rect;
#endif
}
#ifndef NO_OPENCV
void ScreenshotWidget::showWatermarkDialog() // 嵌入水印
{
    QDialog dlg(this);
    dlg.setStyleSheet(
        // 1. 设置 QDialog 背景为白色
        "QDialog { background-color: white; }"

        // 2. 设置 QLabel 默认文字为黑色
        "QLabel { background-color: transparent; color: black; }"

        // 3. 设置 QLineEdit 背景为白色，文字为黑色
        "QLineEdit { background-color: white; color: black; border: 1px solid gray; padding: 4px; }"

        // 4. 设置 QPushButton 按钮样式为浅色主题
        "QPushButton {"
        "background-color: #E0E0E0; " // 浅灰色背景
        "color: black; "              // 按钮文字为黑色
        "border: 1px solid #C0C0C0; " // 浅色边框
        "padding: 6px 12px; "
        "border-radius: 4px; "
        "}"
        "QPushButton:hover { background-color: #D0D0D0; }" // 悬停效果
        );
    dlg.setWindowTitle(getText("watermark_dialog_title", "设置隐水印参数"));
    dlg.setModal(true);
    dlg.resize(380, 180); // 高度减小，因为移除了参数设置

    QVBoxLayout *layout = new QVBoxLayout(&dlg);

    // 输入文本
    QLabel *note = new QLabel(getText("watermark_note", "水印内容最多 15 字符，不足自动补 '0' "));
    layout->addWidget(note);

    QLineEdit *editText = new QLineEdit();
    editText->setPlaceholderText(getText("watermark_placeholder", "请输入水印内容（≤15字符）"));
    editText->setMaxLength(15);
    layout->addWidget(editText);

    // 按钮栏
    QHBoxLayout *btnRow = new QHBoxLayout();
    QPushButton *btnOk = new QPushButton(getText("btn_ok", "确定"));
    QPushButton *btnCancel = new QPushButton(getText("btn_cancel", "取消"));
    btnRow->addWidget(btnOk);
    btnRow->addWidget(btnCancel);
    layout->addLayout(btnRow);

    connect(btnCancel, &QPushButton::clicked, &dlg, &QDialog::reject);

    connect(btnOk, &QPushButton::clicked, [&]()
            {
        QString txt = editText->text();

        if (txt.length() < 15)
            txt = txt + QString("0").repeated(15 - txt.length());

        watermarkText = txt;
        dlg.accept(); });

    if (dlg.exec() == QDialog::Accepted)
    {
        QMessageBox::information(this, getText("watermark_success_title", "成功"), getText("watermark_success_msg", "隐水印参数设置成功！\n保存截图时将自动嵌入水印。"));
    }
}
#endif

void ScreenshotWidget::mouseIsAdjust(QPoint mousePos)
{

    if (m_isadjust)
    {
        // 左上角
        if (m_adjectDirection == TopLeftCorner)
        {
            // if(startPoint.x())
            startPoint = mousePos;
        }
        // 右上角
        else if (m_adjectDirection == TopRightCorner)
        {
            startPoint.setY(mousePos.y());
            endPoint.setX(mousePos.x());
        }
        // 左下角
        else if (m_adjectDirection == LeftBottom)
        {
            startPoint.setX(mousePos.x());
            endPoint.setY(mousePos.y());
        }
        // 右下角
        else if (m_adjectDirection == RightBottom)
        {
            endPoint = mousePos;
        }
        // 左边中心点
        else if (m_adjectDirection == LeftCenterPoint)
        {
            startPoint.setX(mousePos.x());
        }
        // 上边中心点
        else if (m_adjectDirection == TopCenterPoint)
        {
            startPoint.setY(mousePos.y());
        }
        // 右边中心点
        else if (m_adjectDirection == RightCenterPoint)
        {
            endPoint.setX(mousePos.x());
        }
        // 下边中心点
        else if (m_adjectDirection == BottomCenterPoint)
        {
            endPoint.setY(mousePos.y());
        }
        // 整体移动
        else if (m_adjectDirection == MoveAll)
        {
            startPoint.setX(mousePos.x() - m_relativeDistance.x());
            startPoint.setY(mousePos.y() - m_relativeDistance.y());
            endPoint.setX(startPoint.x() + m_relativeDistance.width());
            endPoint.setY(startPoint.y() + m_relativeDistance.height());
        }
    }
    else
    {
        // 左上角
        if (mousePos.x() >= (startPoint.x() - 6) && mousePos.x() <= (startPoint.x() + 6) &&
            mousePos.y() >= (startPoint.y() - 6) && mousePos.y() <= (startPoint.y() + 6))
        {
            setCursor(Qt::SizeFDiagCursor);
            m_adjectDirection = TopLeftCorner;
        }
        // 右上角
        else if (mousePos.x() >= (endPoint.x() - 6) && mousePos.x() <= (endPoint.x() + 6) &&
                 mousePos.y() >= (startPoint.y() - 6) && mousePos.y() <= (startPoint.y() + 6))
        {
            setCursor(Qt::SizeBDiagCursor);
            m_adjectDirection = TopRightCorner;
        }
        // 左下角
        else if (mousePos.x() >= (startPoint.x() - 6) && mousePos.x() <= (startPoint.x() + 6) &&
                 mousePos.y() >= (endPoint.y() - 6) && mousePos.y() <= (endPoint.y() + 6))
        {
            m_adjectDirection = LeftBottom;
            setCursor(Qt::SizeBDiagCursor);
        }
        // 右下角
        else if (mousePos.x() >= (endPoint.x() - 6) && mousePos.x() <= (endPoint.x() + 6) &&
                 mousePos.y() >= (endPoint.y() - 6) && mousePos.y() <= (endPoint.y() + 6))
        {
            m_adjectDirection = RightBottom;
            setCursor(Qt::SizeFDiagCursor);
        }
        // 左边中心点
        else if (mousePos.x() >= (startPoint.x() - 6) && mousePos.x() <= (startPoint.x() + 6) &&
                 mousePos.y() >= ((startPoint.y() + endPoint.y()) / 2 - 6) && mousePos.y() <= ((startPoint.y() + endPoint.y()) / 2 + 6))
        {
            m_adjectDirection = LeftCenterPoint;
            setCursor(Qt::SizeHorCursor);
        }
        // 上边中心点
        else if (mousePos.x() >= ((startPoint.x() + endPoint.x()) / 2 - 6) && mousePos.x() <= ((startPoint.x() + endPoint.x()) / 2 + 6) &&
                 mousePos.y() >= (startPoint.y() - 6) && mousePos.y() <= (startPoint.y() + 6))
        {
            m_adjectDirection = TopCenterPoint;
            setCursor(Qt::SizeVerCursor);
        }
        // 右边中心点
        else if (mousePos.x() >= (endPoint.x() - 6) && mousePos.x() <= (endPoint.x() + 6) &&
                 mousePos.y() >= ((startPoint.y() + endPoint.y()) / 2 - 6) && mousePos.y() <= ((startPoint.y() + endPoint.y()) / 2 + 6))
        {
            m_adjectDirection = RightCenterPoint;
            setCursor(Qt::SizeHorCursor);
        }
        // 下边中心点
        else if (mousePos.x() >= ((startPoint.x() + endPoint.x()) / 2 - 6) && mousePos.x() <= ((startPoint.x() + endPoint.x()) / 2 + 6) &&
                 mousePos.y() >= (endPoint.y() - 6) && mousePos.y() <= (endPoint.y() + 6))
        {
            m_adjectDirection = BottomCenterPoint;
            setCursor(Qt::SizeVerCursor);
        }
        // 四条边
        else if (((mousePos.x() >= startPoint.x() - 6 && mousePos.x() <= startPoint.x() + 6) && (mousePos.y() >= startPoint.y() && mousePos.y() <= endPoint.y())) ||
                 ((mousePos.x() >= endPoint.x() - 6 && mousePos.x() <= endPoint.x() + 6) && (mousePos.y() >= startPoint.y() && mousePos.y() <= endPoint.y())) ||
                 ((mousePos.x() >= startPoint.x() && mousePos.x() <= endPoint.x()) && (mousePos.y() >= startPoint.y() - 6 && mousePos.y() <= startPoint.y() + 6)) ||
                 ((mousePos.x() >= startPoint.x() && mousePos.x() <= endPoint.x()) && (mousePos.y() >= endPoint.y() - 6 && mousePos.y() <= endPoint.y() + 6)))
        {
            m_adjectDirection = MoveAll;
            setCursor(Qt::SizeAllCursor);
        }
        // 其余位置恢复正常
        else
        {
            setCursor(Qt::CrossCursor);
        }
    }
}

void ScreenshotWidget::setupShapesToolbar()
{
    shapesToolbar = new QWidget(this);
    shapesToolbar->setStyleSheet(
        "QWidget { background-color: rgba(40, 40, 40, 200); border-radius: 5px; }"
        "QPushButton { background-color: rgba(60, 60, 60, 255); color: white; "
        "border: none; padding: 6px; border-radius: 3px; }"
        "QPushButton:hover { background-color: rgba(80, 80, 80, 255); }"
        "QPushButton:pressed { background-color: rgba(50, 50, 50, 255); }"
        "QLabel { background-color: transparent; color: white; padding: 5px; font-size: 12px; }"
        "QPushButton:checked { background-color: rgba(0, 150, 255, 255); }"
        "QPushButton#colorBtn { min-width: 40px; min-height: 36px; border: 2px solid rgba(255,255,255,0.6); border-radius: 18px; padding: 0px; }"
        "QPushButton#colorBtn:hover { border: 2px solid rgba(255,255,255,0.9); }");

    QHBoxLayout *layout = new QHBoxLayout(shapesToolbar);
    layout->setSpacing(5);
    layout->setContentsMargins(10, 8, 10, 8);

    btnRect = new QPushButton(shapesToolbar);
    btnRect->setIcon(QIcon(":/icons/icons/rectangle.svg"));
    btnRect->setIconSize(QSize(20, 20));
    btnRect->setToolTip(getText("tooltip_rect", "矩形"));
    btnRect->setFixedSize(36, 36);
    btnRect->setCheckable(true);

    btnEllipse = new QPushButton(shapesToolbar);
    btnEllipse->setIcon(QIcon(":/icons/icons/ellipse.svg"));
    btnEllipse->setIconSize(QSize(20, 20));
    btnEllipse->setToolTip(getText("tooltip_ellipse", "椭圆"));
    btnEllipse->setFixedSize(36, 36);
    btnEllipse->setCheckable(true);

    btnArrow = new QPushButton(shapesToolbar);
    btnArrow->setIcon(QIcon(":/icons/icons/arrow.svg"));
    btnArrow->setIconSize(QSize(20, 20));
    btnArrow->setToolTip(getText("tooltip_arrow", "箭头"));
    btnArrow->setFixedSize(36, 36);
    btnArrow->setCheckable(true);

    // 形状颜色选择 - 使用统一的圆形颜色按钮
    btnShapeColor = createColorButton(shapesToolbar, currentPenColor);

    // 形状粗细
    btnShapeWidthDown = new QPushButton("-", shapesToolbar);
    btnShapeWidthUp = new QPushButton("+", shapesToolbar);
    shapeWidthLabel = new QLabel(QString("%1px").arg(currentPenWidth), shapesToolbar);
    shapeWidthLabel->setFixedWidth(40);
    shapeWidthLabel->setAlignment(Qt::AlignCenter);

    layout->addWidget(btnRect);
    layout->addWidget(btnEllipse);
    layout->addWidget(btnArrow);
    layout->addSpacing(10);
    layout->addWidget(btnShapeColor);
    layout->addSpacing(5);
    layout->addWidget(new QLabel(getText("thickness", "粗细:"), shapesToolbar));
    layout->addWidget(btnShapeWidthDown);
    layout->addWidget(shapeWidthLabel);
    layout->addWidget(btnShapeWidthUp);

    shapesToolbar->adjustSize();
    shapesToolbar->hide();

    // Connect signals
    connect(btnRect, &QPushButton::clicked, this, [this]()
            {
        currentDrawMode = Rectangle;
        btnRect->setChecked(true);
        btnEllipse->setChecked(false);
        btnArrow->setChecked(false); });
    connect(btnEllipse, &QPushButton::clicked, this, [this]()
            {
        currentDrawMode = Ellipse;
        btnRect->setChecked(false);
        btnEllipse->setChecked(true);
        btnArrow->setChecked(false); });
    connect(btnArrow, &QPushButton::clicked, this, [this]()
            {
        currentDrawMode = Arrow;
        btnRect->setChecked(false);
        btnEllipse->setChecked(false);
        btnArrow->setChecked(true); });

    connect(btnShapeColor, &QPushButton::clicked, this, &ScreenshotWidget::onColorPickerClicked);
    connect(btnShapeWidthUp, &QPushButton::clicked, this, &ScreenshotWidget::increasePenWidth);
    connect(btnShapeWidthDown, &QPushButton::clicked, this, &ScreenshotWidget::decreasePenWidth);

    // 颜色改变时更新形状颜色按钮 - 使用统一的更新方法
    connect(this, &ScreenshotWidget::penColorChanged, this, [this]()
            {
        if (btnShapeColor) {
            updateColorButton(btnShapeColor, currentPenColor);
        } });
}

void ScreenshotWidget::updateShapesToolbarPosition()
{
    if (!shapesToolbar || !btnShapes) return;

    shapesToolbar->adjustSize();
    QSize toolbarSize = shapesToolbar->size();

    // 长截图模式
    if (captureMode == ScrollMode) {
        // 方法1：直接使用按钮下方位置（窗口坐标）
        QPoint btnPos = btnShapes->pos();
        int x = btnPos.x();
        int y = btnPos.y() + btnShapes->height() + 5;

        // 转换为全局坐标检查是否在屏幕内
        QPoint globalPos = mapToGlobal(QPoint(x, y + toolbarSize.height()));
        QScreen *screen = QGuiApplication::primaryScreen();
        QRect availableGeometry = screen->availableGeometry();

        qDebug() << "工具栏底部全局Y：" << globalPos.y();
        qDebug() << "屏幕底部Y：" << availableGeometry.bottom();

        // 如果会超出屏幕，放在按钮上方
        if (globalPos.y() > availableGeometry.bottom()) {
            y = btnPos.y() - toolbarSize.height() - 5;
            qDebug() << "调整为上方位置：" << y;
        }

        // 水平方向也检查
        globalPos = mapToGlobal(QPoint(x + toolbarSize.width(), y));
        if (globalPos.x() > availableGeometry.right()) {
            x = availableGeometry.right() - mapToGlobal(QPoint(0, 0)).x() - toolbarSize.width() - 10;
            qDebug() << "调整水平位置：" << x;
        }

        // 确保不超出窗口边界
        QRect windowRect = rect();
        if (x < 0) x = 0;
        if (x + toolbarSize.width() > windowRect.width()) {
            x = windowRect.width() - toolbarSize.width();
        }
        if (y < 0) y = 0;
        if (y + toolbarSize.height() > windowRect.height()) {
            y = windowRect.height() - toolbarSize.height();
        }

        shapesToolbar->move(x, y);

        qDebug() << "最终位置：" << QPoint(x, y)
                 << "（全局：" << mapToGlobal(QPoint(x, y)) << "）";
    }
    // 区域截图
    else {
        if (!shapesToolbar || !selected)
            return;

        shapesToolbar->adjustSize();
        int toolbarWidth = shapesToolbar->sizeHint().width();
        int toolbarHeight = shapesToolbar->sizeHint().height();

        // 获取可用区域（避开Dock栏）
        QScreen *screen = QGuiApplication::primaryScreen();
        QRect availableGeometry = screen->availableGeometry();
        QPoint globalPos = mapToGlobal(QPoint(0, 0));

        int maxY = availableGeometry.bottom() - globalPos.y();
        int maxX = availableGeometry.right() - globalPos.x();
        int minX = availableGeometry.left() - globalPos.x();
        int minY = availableGeometry.top() - globalPos.y();

        // 尝试对齐到形状按钮
        int x, y;
        if (btnShapes && toolbar && toolbar->isVisible())
        {
            QPoint btnPos = btnShapes->mapTo(this, QPoint(0, 0));
            x = btnPos.x();
            y = btnPos.y() + btnShapes->height() + 5;

            // 如果下方空间不足，放上方
            if (y + toolbarHeight > maxY)
            {
                y = btnPos.y() - toolbarHeight - 5;
            }
        }
        else if (toolbar)
        {
            // 默认放在主工具栏下方
            x = toolbar->x();
            y = toolbar->y() + toolbar->height() + 5;

            if (y + toolbarHeight > maxY)
            {
                y = toolbar->y() - toolbarHeight - 5;
            }
        }
        else
        {
            // toolbar 为空，使用默认位置
            x = minX + 10;
            y = minY + 10;
        }

        // 水平方向边界检查
        if (x + toolbarWidth > maxX)
        {
            x = maxX - toolbarWidth - 5;
        }
        if (x < minX + 5)
            x = minX + 5;

        // 垂直方向再次检查
        if (y < minY + 5)
            y = minY + 5;
        if (y + toolbarHeight > maxY)
            y = maxY - toolbarHeight - 5;

        shapesToolbar->move(x, y);
    }
}

void ScreenshotWidget::updateShapeWidthLabel()
{
    if (shapeWidthLabel)
    {
        shapeWidthLabel->setText(QString("%1px").arg(currentPenWidth));
    }
}

void ScreenshotWidget::handleNoneMode(const QPoint &clickPos)
{
    // Check if we clicked on any existing text to edit it
    for (int i = texts.size() - 1; i >= 0; i--)
    {
        if (texts[i].rect.contains(clickPos))
        {
            editExistingText(i);
            return;
        }
    }
}

void ScreenshotWidget::handleTextModeClick(const QPoint &clickPos)
{
    // If we are already editing text, finish it
    if (isTextInputActive)
    {
        onTextInputFinished();
        return;
    }

    // Check if we clicked on existing text
    for (int i = texts.size() - 1; i >= 0; i--)
    {
        if (texts[i].rect.contains(clickPos))
        {
            editExistingText(i);
            return;
        }
    }

    // Start new text input
    textInputPosition = clickPos;
    if (textInput)
    {
        textInput->move(textInputPosition);
        textInput->clear();
        textInput->resize(100, currentFontSize + 10); // Initial size
        textInput->show();
        textInput->setFocus();
        isTextInputActive = true;

        // 显示字体工具栏
        if (fontToolbar)
        {
            updateFontToolbarPosition();
            fontToolbar->show();
            fontToolbar->raise();
        }
    }
}

void ScreenshotWidget::updateTextInputSize()
{
    if (!textInput)
        return;

    QFontMetrics fm(textInput->font());
    QString text = textInput->text();
    if (text.isEmpty())
        text = " "; // Ensure minimum size

    int width = fm.horizontalAdvance(text) + 20;
    int height = fm.height() + 10;

    textInput->resize(width, height);
}

void ScreenshotWidget::updateTextInputStyle()
{
    if (!textInput)
        return;

    textInput->setFont(currentTextFont);
    QPalette palette = textInput->palette();
    palette.setColor(QPalette::Text, currentTextColor);
    textInput->setPalette(palette);

    updateTextInputSize();
}

void ScreenshotWidget::updateFontToolbarPosition()
{
    if (!fontToolbar || !textInput)
        return;

    fontToolbar->adjustSize();
    int toolbarWidth = fontToolbar->sizeHint().width();
    int toolbarHeight = fontToolbar->sizeHint().height();

    // 获取可用区域（避开Dock栏）
    QScreen *screen = QGuiApplication::primaryScreen();
    QRect availableGeometry = screen->availableGeometry();
    QPoint globalPos = mapToGlobal(QPoint(0, 0));

    int maxY = availableGeometry.bottom() - globalPos.y();
    int maxX = availableGeometry.right() - globalPos.x();
    int minX = availableGeometry.left() - globalPos.x();
    int minY = availableGeometry.top() - globalPos.y();

    int x = textInput->x();
    int y = textInput->y() + textInput->height() + 5;

    // 如果下方空间不足，放上方
    if (y + toolbarHeight > maxY)
    {
        y = textInput->y() - toolbarHeight - 5;
    }

    // 水平方向边界检查
    if (x + toolbarWidth > maxX)
    {
        x = maxX - toolbarWidth - 5;
    }
    if (x < minX + 5)
        x = minX + 5;

    // 垂直方向再次检查
    if (y < minY + 5)
        y = minY + 5;
    if (y + toolbarHeight > maxY)
        y = maxY - toolbarHeight - 5;

    fontToolbar->move(x, y);
}

void ScreenshotWidget::toggleSubToolbar(QWidget *targetToolbar)
{
    // 1. 隐藏所有其他子工具栏
    if (penToolbar && penToolbar != targetToolbar)
        penToolbar->hide();
    if (shapesToolbar && shapesToolbar != targetToolbar)
        shapesToolbar->hide();
    if (fontToolbar && fontToolbar != targetToolbar)
        fontToolbar->hide();
    if (EffectToolbar && EffectToolbar != targetToolbar)
        EffectToolbar->hide();

    // 2. 切换目标工具栏状态
    if (targetToolbar)
    {
        if (targetToolbar->isVisible())
        {
            targetToolbar->hide();
        }
        else
        {
            // 更新位置并显示
            if (targetToolbar == penToolbar)
                updatePenToolbarPosition();
            else if (targetToolbar == shapesToolbar)
                updateShapesToolbarPosition();
            else if (targetToolbar == fontToolbar)
                updateFontToolbarPosition();
            else if (targetToolbar == EffectToolbar)
                updateEffectToolbarPosition();

            targetToolbar->show();
            targetToolbar->raise();
        }
    }
}

void ScreenshotWidget::performOCR()
{
    if (selectedRect.isNull())
    {
        return;
    }

    QString backend = OcrManager::getBackendType();

    // Windows/Linux 未配置 Tesseract 的情况
    if (backend == "None")
    {
        QMessageBox::warning(this, getText("ocr_not_configured_title", "OCR 未配置"),
                             getText("ocr_not_configured_msg", "当前平台需要安装并配置 Tesseract OCR 库才能使用此功能。\n\n请下载 Tesseract 开发库并在 ScreenSniper.pro 中启用 USE_TESSERACT 选项。"));
        return;
    }

    // 获取当前截图（包含绘制的内容）
    QPixmap capture = this->grab(selectedRect);

    // 显示正在识别的提示
    QMessageBox *loadingMsg = new QMessageBox(this);
    loadingMsg->setWindowTitle(getText("ocr_recognizing_title", "正在识别"));
    loadingMsg->setText(getText("ocr_recognizing_msg", "正在进行 OCR 识别，请稍候..."));
    loadingMsg->setStandardButtons(QMessageBox::NoButton);
    loadingMsg->setWindowFlags(loadingMsg->windowFlags() | Qt::WindowStaysOnTopHint);
    loadingMsg->show();

    // 异步调用 OCR
    OcrManager::recognizeAsync(capture, [this, backend, loadingMsg](const QString &text)
                               {
        // 确保所有UI操作在主线程中执行
        QMetaObject::invokeMethod(this, [this, text, backend, loadingMsg]() {
            // 关闭正在识别的提示
            if (loadingMsg) {
                loadingMsg->close();
                loadingMsg->deleteLater();
            }

            if (!text.isEmpty() && !text.startsWith("Error"))
            {
                QClipboard *clipboard = QGuiApplication::clipboard();
                clipboard->setText(text);

                // 使用自定义对话框显示 OCR 结果
                OcrResultDialog *dialog = new OcrResultDialog(text, backend, this);
                dialog->exec();
                delete dialog;
            }
            else
            {
                QString errorMsg = text.isEmpty() ? getText("ocr_no_text", "未能识别到文字。") : text;
                QMessageBox::warning(this, getText("ocr_failed_title", "OCR 失败"), errorMsg);
            }
        }, Qt::QueuedConnection); });
}

// 图片生成文字描述
void ScreenshotWidget::onAiDescriptionBtnClicked()
{
    if (selectedRect.isNull() )
    {
        QMessageBox::warning(this, getText("ai_description_warning_title", "提示"),
                             getText("ai_description_no_selection", "请先选择截图区域"));
        return;
    }

    // 获取当前截图（包含绘制的内容）
    QPixmap capture = this->grab(selectedRect);
    QImage image = capture.toImage();

    // 调用AI管理器生成描述
    m_aiManager->generateImageDescription(image);
}
