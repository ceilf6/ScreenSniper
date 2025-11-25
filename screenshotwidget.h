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

class ScreenshotWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ScreenshotWidget(QWidget *parent = nullptr);
    ~ScreenshotWidget();

    void startCapture();
    void startCaptureFullScreen(); // 直接截取全屏并显示工具栏

signals:
    void screenshotTaken();
    void screenshotCancelled();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:

    //高斯模糊相关函数：
    void increaseBlurStrength();
    void decreaseBlurStrength();
    void updateBlurStrengthLabel();
    void updateBlurToolbarPosition();
    //马赛克相关函数：
    enum DrawMode
    {
        None,
        Rectangle,
        Arrow,
        Text,
        Pen,
        Mosaic,
        Blur
    };DrawMode currentDrawMode;
    QList<DrawMode> effectTypes;        // 存储效果类型
    void increaseEffectStrength();
    void decreaseEffectStrength();
    void setupEffectToolbar(); // 新增工具栏设置
    QPixmap applyEffect(const QPixmap &source, const QRect &area, int strength, DrawMode mode);//模糊应用函数
    QPixmap applyBlur(const QPixmap &source, const QRect &area, int radius);//高斯模糊应用
    QPixmap applyMosaic(const QPixmap &source, const QRect &area, int strength);//马赛克应用
    void setupToolbar();
    void updateToolbarPosition();

    void saveScreenshot();
    void copyToClipboard();
    void cancelCapture();
    void drawArrow(QPainter &painter, const QPointF &start, const QPointF &end, const QColor &color, int width, double scale = 1.0);
    void updateEffectToolbarPosition();
    void updateStrengthLabel();


    QPixmap screenPixmap; // 屏幕截图
    QPoint startPoint;    // 选择起始点
    QPoint endPoint;      // 选择结束点
    bool selecting;       // 是否正在选择
    bool selected;        // 是否已选择完成
    QRect selectedRect;   // 选中的矩形区域

    // 工具栏
    QWidget *toolbar;
    QPushButton *btnSave;
    QPushButton *btnCopy;
    QPushButton *btnCancel;
    QPushButton *btnRect;  // 矩形工具
    QPushButton *btnArrow; // 箭头工具
    QPushButton *btnText;  // 文字工具
    QPushButton *btnPen;   // 画笔工具
    QPushButton *btnMosaic;  // 马赛克按钮
    QPushButton *btnBlur;//高斯模糊按钮
    // 尺寸显示标签
    QLabel *sizeLabel;



    //模糊相关
    bool drawingEffect = false;
    QPoint EffectStartPoint;
    QPoint EffectEndPoint;
    QList<QRect> EffectAreas; // 存储所有模糊区域
    QList<int> EffectStrengths;

    // 强度调节工具栏
    int currentEffectStrength = 20;//强度
    int EffectBlockSize;     // 块大小
    QWidget *EffectToolbar= nullptr;
    QPushButton *btnStrengthUp;
    QPushButton *btnStrengthDown;
    QLabel *strengthLabel;
    // ============ 马赛克成员变量结束 ============


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


    // 绘制相关

    // 存储绘制的形状
    QVector<DrawnArrow> arrows;
    QVector<DrawnRectangle> rectangles;

    // 当前绘制的临时数据
    bool isDrawing;
    QPoint drawStartPoint;
    QPoint drawEndPoint;
};

#endif // SCREENSHOTWIDGET_H
