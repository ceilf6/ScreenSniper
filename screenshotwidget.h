#ifndef SCREENSHOTWIDGET_H
#define SCREENSHOTWIDGET_H

#include <QWidget>
#include <QPixmap>
#include <QRect>
#include <QPushButton>
#include <QLabel>
#include <QVector>
#include <QPoint>
#include <QColor>
#include <QTextEdit>
#include <QLineEdit>
#include <QPointer>
#include "i18nmanager.h"
#include "pinwidget.h"
#include "aimanager.h"
#ifndef NO_OPENCV
#include "facedetector.h"
#endif

// 绘制形状数据结构
struct DrawnArrow
{
    QPoint start;
    QPoint end;
    QColor color;
    int width;
};

struct DrawnRectangle
{
    QRect rect;
    QColor color;
    int width;
};

// 绘制椭圆结构体
struct DrawnEllipse
{
    QRect rect;
    QColor color;
    int width;
};

// 绘制文本数据结构
struct DrawnText
{
    QString text;
    QRect rect;
    QPoint position;
    QColor color;
    int fontSize;
    QFont font;
};

// 画笔数据结构
struct DrawnPenStroke
{
    QVector<QPoint> points;
    QColor color;
    int width;
};

#ifdef Q_OS_WIN
#include <windows.h>
#endif

// 窗口信息结构体
typedef struct WindowInfo
{
#ifdef Q_OS_WIN
    HWND hwnd;
#else
    size_t windowId;
#endif
    QRect rect;
    bool isValid() const { return rect.isValid(); }
} WindowInfo;

class ScreenshotWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ScreenshotWidget(QWidget* parent = nullptr);
    ~ScreenshotWidget();

    void startCapture();
    void startCaptureFullScreen(); // 直接截取全屏并显示工具栏

    // 国际化支持
    void setMainWindow(QWidget *mainWin);
    QString getText(const QString &key, const QString &defaultText = "") const;
    void updateTooltips(); // 更新所有工具提示文本

private slots:
    void performOCR();
    void onLanguageChanged(); // 响应语言变化

signals:
    void screenshotTaken();
    void screenshotCancelled();
    void penColorChanged();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private slots:
    void onTextInputFinished();
    void onColorPickerClicked();
    void onPenButtonClicked();
    void increasePenWidth();
    void decreasePenWidth();
    void updatePenWidthLabel();
    void onTextColorClicked();
    void increaseFontSize();
    void decreaseFontSize();
    void onFontFamilyClicked();
    void onFontSizeInputChanged();

private:
    // 初始化I18n连接
    void initializeI18nConnection();

    // 越界判断相关：
    // 水印相关
#ifndef NO_OPENCV
    void showWatermarkDialog();
#endif
    // 高斯模糊相关函数：
    void increaseBlurStrength();
    void decreaseBlurStrength();
    void updateBlurStrengthLabel();
    void updateBlurToolbarPosition();
    // 马赛克相关函数：
    enum DrawMode
    {
        None,
        Rectangle,
        Ellipse,
        Arrow,
        Text,
        Pen,
        Mosaic,
        Blur,
        Watermark
    };
    DrawMode currentDrawMode;
    QList<DrawMode> effectTypes; // 存储效果类型
    void increaseEffectStrength();
    void decreaseEffectStrength();
    void setupEffectToolbar();                                                                  // 新增工具栏设置
    QPixmap applyEffect(const QPixmap &source, const QRect &area, int strength, DrawMode mode); // 模糊应用函数
    QPixmap applyBlur(const QPixmap &source, const QRect &area, int radius);                    // 高斯模糊应用
    QPixmap applyMosaic(const QPixmap &source, const QRect &area, int strength);                // 马赛克应用
    void setupToolbar();
    void updateToolbarPosition();
    void toggleSubToolbar(QWidget *targetToolbar); // 统一管理子工具栏显示
    void updatePreviewCanvas(QLabel *canvas);
    QPushButton *createColorButton(QWidget *parent, const QColor &color); // 创建统一的颜色按钮
    void updateColorButton(QPushButton *button, const QColor &color);     // 更新颜色按钮样式

    void saveScreenshot();
    void copyToClipboard();
    void cancelCapture();
    void breakCapture();
    void drawArrow(QPainter &painter, const QPointF &start, const QPointF &end, const QColor &color, int width, double scale = 1.0);
    void updateEffectToolbarPosition();
    void updateStrengthLabel();

    // 添加文本相关函数
    void setupTextInput();
    void drawText(QPainter &painter, const QPoint &position, const QString &text, const QColor &color, const QFont &font);
    void setTextToolbar();
    void updateFontToolbar();
    void updateTextInputStyle();
    void updateTextInputSize();
    void handleTextModeClick(const QPoint &clickPos);
    void updateFontToolbarPosition();
    void editExistingText(int textIndex);
    void handleNoneMode(const QPoint &clickPos);
    void selectTextForPropertyEdit(int textIndex);  // 单击文字时修改属性
    void saveTextProperties();  // 保存文字属性（不修改文本内容）

    // pin到桌面
    void pinToDesktop();
    // 窗口识别函数
    void captureWindow(QPoint mousePos);
    // 判断鼠标是否点击调节位置
    void mouseIsAdjust(QPoint mousePos);
    // 枚举系统所有有效顶层窗口
    QList<WindowInfo> enumAllValidWindows();

#ifdef Q_OS_WIN
    // 回调函数
    static BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam);
    static BOOL CALLBACK EnumChildProc(HWND childHwnd, LPARAM lParam);

#endif

    // 精准获取窗口边界（跨平台）
    QRect getAccurateWindowRect(const WindowInfo &window);

    // 画笔相关函数
    void setupPenToolbar();
    void updatePenToolbarPosition();

    // 调整大小的方向和移动枚举
    enum AdjectDirection
    {
        TopLeftCorner,
        TopRightCorner,
        LeftBottom,
        RightBottom,
        LeftCenterPoint,
        RightCenterPoint,
        TopCenterPoint,
        BottomCenterPoint,
        MoveAll
    };
    enum AdjectDirection m_adjectDirection;

    QPixmap screenPixmap;             // 屏幕截图
    QPoint startPoint;                // 选择起始点
    QPoint endPoint;                  // 选择结束点
    bool selecting;                   // 是否正在选择
    bool selected;                    // 是否已选择完成
    QRect selectedRect;               // 选中的矩形区域
    QRect m_relativeDistance;         // 保存拉动时鼠标与startPos的距离
    bool m_isadjust;                  // 是否正在调整截取矩形大小
    QList<WindowInfo> m_validWindows; // 有效窗口列表
    bool m_selectedWindow;            // 是否已选择窗口
    WindowInfo m_hoverWindow;         // 高亮窗口

    // 工具栏
    QWidget *toolbar;
    QPushButton *btnSave;
    QPushButton *btnCopy;
    QPushButton *btnCancel;
    QPushButton *btnBreak;
    QPushButton *btnRect;    // 矩形工具
    QPushButton *btnEllipse; // 椭圆工具
    QPushButton *btnArrow;   // 箭头工具
    QPushButton *btnText;    // 文字工具
    QPushButton *btnPen;     // 画笔工具
    QPushButton *btnMosaic;  // 马赛克按钮
    QPushButton *btnBlur;    // 高斯模糊按钮
    QPushButton *btnAutoFaceBlur;  // 自动人脸打码按钮
    QPushButton *btnPin;     // Pin到桌面按钮
#ifndef NO_OPENCV
    QPushButton *btnWatermark; // 水印按钮
#endif
    QPushButton *btnOCR;           // OCR 按钮
    QPushButton *btnAIDescription; // AI图片描述按钮

    // 尺寸显示标签
    QLabel* sizeLabel;

    // 模糊相关
    bool drawingEffect = false;
    QPoint EffectStartPoint;
    QPoint EffectEndPoint;
    QList<QRect> EffectAreas; // 存储所有模糊区域
    QList<int> EffectStrengths;

    // 强度调节工具栏
    int currentEffectStrength = 20; // 强度
    int EffectBlockSize;            // 块大小
    QWidget *EffectToolbar = nullptr;
    QPushButton *btnStrengthUp;
    QPushButton *btnStrengthDown;
    QLabel *strengthLabel;
    // ============ 马赛克成员变量结束 ============

    // 添加画笔选择相关
    QColor currentPenColor; // 当前画笔颜色
    int currentPenWidth;    // 当前画笔粗细

    // 画笔轨迹相关
    QVector<QPoint> currentPenStroke;
    QVector<DrawnPenStroke> penStrokes;

    // 画笔工具栏
    QWidget *penToolbar;
    QPushButton *btnPenWidthUp;
    QPushButton *btnPenWidthDown;
    QLabel *penWidthLabel;

    // 形状工具栏
    QWidget *shapesToolbar;
    QPushButton *btnShapes; // 主工具栏上的形状按钮
    QPushButton *btnShapeColor;
    QPushButton *btnShapeWidthUp;
    QPushButton *btnShapeWidthDown;
    QLabel *shapeWidthLabel;
    void setupShapesToolbar();
    void updateShapesToolbarPosition();
    void updateShapeWidthLabel();

    // 高斯模糊相关
    QWidget *blurToolbar;
    QPushButton *btnBlurStrengthDown;
    QPushButton *btnBlurStrengthUp;
    QLabel *blurStrengthLabel;
    int currentBlurStrength;

    QList<QRect> blurAreas;
    QList<int> blurStrengths;
    QPoint blurStartPoint;
    QPoint blurEndPoint;
    bool drawingBlur;
    // 屏幕设备像素比
    qreal devicePixelRatio;

    // 虚拟桌面原点（用于多屏幕支持）
    QPoint virtualGeometryTopLeft;

    // 放大镜相关
    QPoint currentMousePos;
    bool showMagnifier;

    // 文本输入相关
    QLineEdit *textInput;
    bool isTextInputActive;
    QPoint textInputPosition;

    // 文字移动相关
    bool isTextMoving;
    DrawnText *movingText;
    QPoint dragStartOffset;
    QPoint textClickStartPos; // 文字点击起始位置
    int textClickIndex;       // 文字点击索引
    bool potentialTextDrag;   // 是否可能是文字拖拽
    bool textDoubleClicked;   // 是否发生了双击（用于区分单击和双击）

    // 字体工具栏相关
    QWidget *fontToolbar;
    QPushButton *btnFontColor;
    QPushButton *btnFontSizeUp;
    QPushButton *btnFontSizeDown;
    QLineEdit *fontSizeInput;
    QPushButton *btnFontFamily;
    QFont currentTextFont;
    QColor currentTextColor;
    int currentFontSize;
    int editingTextIndex; // 当前正在编辑的文本索引

    // 绘制相关

    // 存储绘制的形状
    QVector<DrawnArrow> arrows;
    QVector<DrawnRectangle> rectangles;
    QVector<DrawnEllipse> ellipses;
    QVector<DrawnText> texts;

    // 当前绘制的临时数据
    bool isDrawing;
    QPoint drawStartPoint;
    QPoint drawEndPoint;

    // 当前鼠标下的窗口矩形（自动吸附用）
    // QRect currentWindowRect;
    // 水印相关;
#ifndef NO_OPENCV
    QString watermarkText;
#endif
    // QRect currentWindowRect;

    // 国际化相关
    QPointer<QWidget> mainWindow;

    // 图片生成文字描述相关
    AiManager *m_aiManager;
    void onAiDescriptionBtnClicked();
    
    // 自动人脸打码相关函数
    void autoDetectAndBlurFaces();  // 自动检测并打码人脸
    
#ifndef NO_OPENCV
    // 自动人脸打码相关
    FaceDetector* faceDetector;  // 人脸检测器实例
    bool autoFaceBlurEnabled;  // 是否启用自动人脸打码（默认false）
#else
    void* faceDetector;  // 占位符
    bool autoFaceBlurEnabled;  // 是否启用自动人脸打码（默认false）
#endif
};

#endif // SCREENSHOTWIDGET_H
