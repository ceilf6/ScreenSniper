# 概述

需求：实现一个截屏工具

![截屏2025-12-09 11.49.24.png](attachment:77135be2-108f-49b4-852c-39e2fd5c1b82:截屏2025-12-09_11.49.24.png)

我的目标：确保基本功能的基础上，首先优化工具的性能（因为本项目是以C++开发、如果不以性能为目标的话个人感觉用 Electron（Web+Node.js）更适合），提升工具的跨平台能力、多语言支持，优化代码的可维护性和可拓展性

# 模块化

在拿到任务后，我以功能为区分，考量了各个模块的接口交互后设立了 截屏管理类screenshotwidget ，国际化多语言管理类i18nManager 等类（后面同学们好像直接把功能实现合到了 screenshotwidget ，后续有时间可以将具体的绘画功能等等从 screenshotwidget 文件解耦出来，我感觉一个文件 1000 行已经算比较冗余了）

> 下面就是产品构思、细节实现、代码优化等等
> 

# 区域截屏

- Commit 9a3353

## 1. 技术选型

- 像 macOS 有 CGDIsplay ，Win 有 BitBit ，都是性能极佳而且能获取到最真实的像素，但是需要写多端支持、不利于维护，不符合轻量化要求
- 而像用 OpenCV 截屏就需要引入过大依赖，工具过于冗余且性能开销会变大，过度设计了
- 所以以 Qt 的优点就体现出来了：跨平台、稳定

实现聚焦在 Screenshotwidget 类

首先会截取一次全屏作为背景，保存为 screenPixmap

```jsx
screenPixmap = currentScreen->grabWindow(0);
```

然后监听鼠标事件，实时更新选区的绘制

## 2. 启动截屏

```jsx
void ScreenshotWidget::startCapture()
{
    // 1. 获取鼠标当前位置所在的屏幕（通过包含鼠标判断）
    QPoint cursorPos = QCursor::pos();
    QScreen *currentScreen = nullptr;
    QList<QScreen *> screens = QGuiApplication::screens();
    for (QScreen *scr : screens) {
        if (scr->geometry().contains(cursorPos)) {
            currentScreen = scr;
            break;
        }
    }
    
    // 2. 如果没有找到，使用主屏幕
    if (!currentScreen) {
        currentScreen = QGuiApplication::primaryScreen();
    }
    
    if (currentScreen) {
        // 3. 获取设备像素比，支持高分屏
        devicePixelRatio = currentScreen->devicePixelRatio();
        
        // 4. 获取当前屏幕的几何信息
        QRect screenGeometry = currentScreen->geometry();
        
        // 5. 保存屏幕的原点位置
        virtualGeometryTopLeft = screenGeometry.topLeft();
        
        // 6. 核心：使用Qt的grabWindow(0)获取当前屏幕截图
        screenPixmap = currentScreen->grabWindow(0);
        
        // 7. 设置窗口标志，显示在最上层
        setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool | Qt::BypassWindowManagerHint);
        
        // 8. 设置窗口大小和位置为当前屏幕
        setGeometry(screenGeometry);
        
        // 9. 显示窗口
        show();
        
        // 10. 初始化状态
        selecting = false;
        selected = false;
        selectedRect = QRect();
        showMagnifier = true; // 启用放大镜
    }
}
```

## 3. 实时绘制

在开始到结束即 selecting 标识位为真期间，会通过 **paintEvent** 实现框选区域明亮与周边的暗色区分，并且通过 update 进行重绘，在标识符 selected 为真时展示工具栏

```jsx
// screenshotwidget.cpp - paintEvent

// 1. 绘制背景截图（底图）
painter.drawPixmap(windowRect, screenPixmap, sourceRect);

// 2. 绘制半透明遮罩（实现“周边的暗色”）
painter.fillRect(rect(), QColor(0, 0, 0, 100));

// 3. 如果有选中区域
// if (selecting || selected) ...
{
    // ...计算 currentRect...

    // 4. 在遮罩之上，再次绘制选区部分的// screenshotwidget.cpp - paintEvent

// 1. 绘制背景截图（底图）
painter.drawPixmap(windowRect, screenPixmap, sourceRect);

// 2. 绘制半透明遮罩（实现“周边的暗色”）
painter.fillRect(rect(), QColor(0, 0, 0, 100));

// 3. 如果有选中区域
// if (selecting || selected) ...
{
    // ...计算 currentRect...

    // 4. 在遮罩之上，再次绘制选区部分的
```

# 全屏截屏

- Commit b7fa1c3

复用区域截图的 `startCapture()`，通过直接设置标志 `selected = true` 跳过鼠标框选阶段

```jsx
void ScreenshotWidget::startCaptureFullScreen()
{
    // 1. 先启动常规截图，获取屏幕图像
    startCapture();
    
    // 2. 使用 QPointer 防止 lambda 中悬空指针（类似智能指针）
    QPointer<ScreenshotWidget> self(this);
    
    // 3. 延迟设置为全屏模式，确保截图已完成
    QTimer::singleShot(150, this, [self]()
                       {
                           // 检查对象是否仍然存在
                           if (!self) {
                               return;
                           }
                           
                           // 4. 核心：将选中区域设置为整个窗口
                           self->selectedRect = self->rect();
                           self->selected = true;
                           self->selecting = false;
                           
                           // 5. 显示工具栏，进入编辑模式
                           if (self->toolbar) {
                               self->toolbar->setParent(self);
                               self->toolbar->adjustSize();
                               self->updateToolbarPosition();
                               self->toolbar->setWindowFlags(Qt::Widget);
                               self->toolbar->raise();
                               self->toolbar->show();
                               self->toolbar->activateWindow();
                           }
                           
                           // 6. 更新界面
                           self->update(); 
                       });
}
```

# 保存

## 1. 预处理

如果没有选中区域，直接 **提前剪枝** 返回，避免无效的性能开销

```jsx
    if (selectedRect.isEmpty())
    {
        return;
    }
```

如果有的话，就进行坐标转换，实现高 DPI 屏幕的适配

其中是使用了 qRound 四舍五入，避免 qFloor/qCeil 导致的像素偏差（qCeil可能会导致越界的、qFloor边缘处理不够丝滑）

```jsx
int x = qRound(selectedRect.x() * devicePixelRatio);
int y = qRound(selectedRect.y() * devicePixelRatio);
int w = qRound(selectedRect.width() * devicePixelRatio);
int h = qRound(selectedRect.height() * devicePixelRatio);
```

## 2. 作用所有绘制

ScreenshotWidget::saveScreenshot() 的 2235 - 2550 行

### pre: 技术选型

虽然我习惯用 Canvas/OffscreenCanvas 进行离屏图层渲染，但是考虑到本工具需求

- 原生像素支持
- 支持离线，且需要性能、低延迟

于是选择了用 **QPainter** ，原生支持跨平台（底层调用 CoreGraphics/GDI+/X11），同时与 QPixmap 和 QImage 高度整合，方便做 DPR 的适配

## 3. 打开对话框

```jsx
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
        if (suffix == "png")
        {
            croppedPixmap.save(fileName, "PNG", 100);
        }
        else if (suffix == "jpg" || suffix == "jpeg")
        {
            croppedPixmap.save(fileName, "JPEG", 95);
        }
        emit screenshotTaken(); // 发射截图完成信号
        hide();                 // 隐藏当前窗口
        QApplication::quit();   // 退出整个应用程序
        // 如果用户取消保存，不做任何操作，保持当前状态（工具栏仍然可见）
    }
```

其中 [croppedPixmap.save](http://croppedPixmap.save) 就是 Qt 的 QPixmap 类中用于保存的方法，是自动支持跨平台的

最后btnSave 信号槽连接到 saveScreensho

# 复制

本质上保存和复制前面的代码都是一样的，都需要预处理和应用更改

只不过最后要调用的API不同，于是我想能不能优化代码的复用性和可拓展性，参考 **依赖注入** 思想（例如React父组件单向数据流传输许多类子组件中），想到了

## **⭐工厂模式-高阶函数**

将差异点即最后要调用的API放到一个回调函数中作为参数传入统一的函数 processScreenshot （像 React 的 Render Props ）

```jsx
// 通用截图处理函数，使用回调函数处理最终输出
void ScreenshotWidget::processScreenshot(std::function<void(QPixmap &)> outputHandler)
```

```jsx
    // 调用回调函数处理最终输出（保存或复制）
    outputHandler(croppedPixmap);

    // 通用的收尾工作
    emit screenshotTaken();
    hide();
    if (mainWindow)
    {
        mainWindow->show();
    }
    close();
```

```jsx
void ScreenshotWidget::copyToClipboard()
{
    processScreenshot([](QPixmap &pixmap)
                      {
        // 复制到剪贴板
        QClipboard *clipboard = QGuiApplication::clipboard();
        clipboard->setPixmap(pixmap); });
}
```

最后 btnCopy 信号槽连接到 copyToClipboard 函数

# UI绘制

## 1. 主界面

首先通过 UI文件 框定窗口架构模版（类 HTML ）

qmake 会在构建阶段通过 **UIC 将** mainwindow.ui 文件处理生成 ui_mainwindow.h

然后去 mainwindow.cpp 通过 setupUI 方法加载模版后创建按钮，并在使用时插入槽函数（类 <script

```cpp
MainWindow::MainWindow() {
    ui->setupUi(this);  // 加载UI文件
    setupUI();          // 代码创建按钮
    setupTrayIcon();    // 创建托盘图标
}

void setupUI() {
    // 创建中心部件和垂直布局
    QWidget *centralWidget = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(centralWidget);
    
    // 创建按钮 - 使用多语言文本
    btnFullScreen = new QPushButton(getText("btn_fullscreen", "截取全屏"));
    btnArea = new QPushButton(getText("btn_area", "截取区域"));
    btnScroll = new QPushButton(getText("btn_scroll", "滚动截图"));
    btnSettings = new QPushButton(getText("btn_settings", "设置"));
    
    // 添加到布局
    layout->addWidget(btnFullScreen);
    layout->addWidget(btnArea);
    // ...
    
    setCentralWidget(centralWidget);
}
```

## 2. 工具栏

```cpp
void ScreenshotWidget::setupToolbar() {
    toolbar = new QWidget(this);

    // 样式表定义外观 - 半透明黑色背景 + 圆角
    toolbar->setStyleSheet(
        "QWidget#toolbar { "
        "background-color: rgba(40, 40, 40, 220); "  // 半透明黑色
        "border-radius: 8px; "                        // 圆角
        "border: 1px solid rgba(255, 255, 255, 25); "
        "} "
        "QPushButton { "
        "background-color: transparent; "
        "color: white; "
        "padding: 8px; "
        "border-radius: 6px; "
        "} "
        "QPushButton:hover { "
        "background-color: rgba(255, 255, 255, 40); " // 悬停效果
        "}"
    );

    // 水平布局
    QHBoxLayout *layout = new QHBoxLayout(toolbar);
    layout->setSpacing(5);
    layout->setContentsMargins(10, 5, 10, 5);

    // 创建按钮 - 使用SVG图标
    btnShapes = new QPushButton(toolbar);
    btnShapes->setIcon(QIcon(":/icons/icons/shapes.svg"));
    btnShapes->setIconSize(QSize(22, 22));
    btnShapes->setFixedSize(40, 40);

    // 添加所有按钮...
    layout->addWidget(btnShapes);
    layout->addWidget(btnText);
    layout->addWidget(btnPen);
    // ...
}
```

工具栏的模版结构不像主界面一样是通过UI文件，而是通过 C++ new 出来的控件树，并通过 addWidget 方法将按钮控件加入布局（类似于 flex, grid 布局）

同时通过 setStyleSheet 定义了样式

## 技术考量

我认为，主界面的话基本只有区域截屏、全屏截屏、设置等，可变性比较小，用 ui 文件足矣

但是工具栏是功能实现的核心，需要代码有较强的可拓展性和可维护性，所以静态的 XML 相较于通过实例组件来的要麻烦，并且 Qt Designer 是只能固定位置的，那么就不能根据截屏的方位去进行碰撞检测从而实时更新位置

# 有效空间的跨平台支持

## 问题1: 菜单栏挤压窗口

- **Commit 7e8c96a**

### 问题现象

截屏窗口在 macOS 下会被系统菜单栏挤压，导致底部内容被裁剪

### 根因分析

- `screen->geometry()` 返回整个屏幕区域（包含菜单栏）
- macOS 窗口管理器默认会自动缩放窗口，避开菜单栏（22px）和 Dock
- 导致实际绘制区域 ≠ 预期的全屏区域

### 解决方案

绕过系统窗口管理策略，手动控制了绘制区域，避免被系统UI挤压

1. **加上 Qt::BypassWindowManagerHint**，让窗口不受 macOS 窗口管理器干涉，能够覆盖到完整屏幕。
2. **不使用 WindowFullScreen 模式( showFullScreen() )**，改成普通 show()，避免 Qt 触发系统全屏模式，导致被强制调整尺寸

- Qt::BypassWindowManagerHint 在 macOS 中会调用 ObjC 层窗口属性，在 Windows 会调用 Win32 API ，Linux 中会调用 X11 / Wayland Flags，所以是原生支持跨平台的

## **问题2：Dock 遮挡工具栏**

- **Commit a8b4e31**

### 问题分析

绕过窗口管理器后，工具栏可能被 Dock 遮挡

### 技术选型

为实现工具栏自适应避开拓展坞，原先我的思路是拿到拓展坞高度属性后去计算，这样的话就需要通过系统 API 去查询 Dock 高度，复杂而且跨平台维护性价比低

不如直接调 Qt 的拿到可用空间的 API 来的快，即 **availableGeometry** 

```jsx
QScreen *screen = QGuiApplication::screenAt(geometry().center());
QRect availableGeometry = screen->availableGeometry();
```

后再做一层基点校验处理

```jsx
// 窗口左上角的全局坐标
QPoint windowTopLeft = geometry().topLeft();

// 计算安全区域的底部边界相对于窗口内部的 Y 坐标
int availableBottomY = availableGeometry.bottom() - windowTopLeft.y();
```

然后用于对于面积大、难以展示工具栏的极端情况进行特判处理

```jsx
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
```

后面对功能类似的做了层级统一，产生了子工具栏的概念

# 子工具栏

## 1. 遮挡问题

需要考虑到不同的子工具栏的规格是不一样的，所以直接在可选区域里面写死肯定是不正确的

所以首先考虑到代码可拓展性，通过 globalPos 实现做屏幕的坐标系转换，进行统一规范 

```jsx
		int maxY = availableGeometry.bottom() - globalPos.y();
    int maxX = availableGeometry.right() - globalPos.x();
    int minX = availableGeometry.left() - globalPos.x();
    int minY = availableGeometry.top() - globalPos.y();
```

然后通过不断**碰断检测**（例如 if (y + toolbarHeight > availableBottomY) ）后如果不符合就进行**挪动更新**

## 2. 子工具栏状态管理问题

- commit 0ce7814

原先是各个子工具栏的打开和关闭的**逻辑分散**在各自的按钮点击事件中，代码可维护性极低，考虑到各个子工具栏的排他性，我想到了 

### **⭐JS单线程管理DOM树防止UI操作混乱**

### **⭐浏览器的事件循环机制**

的 **单一调度器模式** 思想 ，类似地实现了**子工具栏调度函数 toggleSubToolbar** 在其中统一了逻辑管理，优化了代码的稳定性和可拓展性，实现了中心化调度

```jsx
void ScreenshotWidget::toggleSubToolbar(QWidget* targetToolbar)
{
    // 1. 隐藏所有其他子工具栏
    if (penToolbar && penToolbar != targetToolbar) penToolbar->hide();
    if (shapesToolbar && shapesToolbar != targetToolbar) shapesToolbar->hide();
    if (fontToolbar && fontToolbar != targetToolbar) fontToolbar->hide();
    if (EffectToolbar && EffectToolbar != targetToolbar) EffectToolbar->hide();

    // 2. 切换目标工具栏状态
    if (targetToolbar) {
        if (targetToolbar->isVisible()) {
            targetToolbar->hide();
        } else {
            // 更新位置并显示
            if (targetToolbar == penToolbar) updatePenToolbarPosition();
            else if (targetToolbar == shapesToolbar) updateShapesToolbarPosition();
            else if (targetToolbar == fontToolbar) updateFontToolbarPosition();
            else if (targetToolbar == EffectToolbar) updateEffectToolbarPosition();
            
            targetToolbar->show();
            targetToolbar->raise();
        }
    }
}
```

## 3. SVG支持（高 DPI 优化）

- **Commit da76be8**

### 技术考量

由于原先是直接文字展示、观感不是很好，于是为了优化UI效果以及高精度UI支持，选择用 SVG 矢量图进行替换，这样不仅体积小、无损，还支持CSS样式控制

### 实现流程

1. 去 https://iconify.design/ 中拉下来 svg 代码
2. 然后在 Qt资源系统 resources.qrc 中导入，之后它们会被编译成 C++ 代码即可在代码中通过 相对路径 访问（类似于 webpack 的 @import  ）
3. 代码上体现为，通过 QIcon 类可以直接加载和使用 SVG

```jsx
btnText->setIcon(QIcon(":/icons/icons/text.svg"));
```

### 性能考量

Qt 会自动缓存 SVG 渲染结果，避免重复解析，所以是契合的

# 放大镜

- **Commit 7ffb63d**

## 1. 技术选型

- 首先我去搜集了信息，Qt没有内置的放大镜API，windows上有 Magnification API ，macOS上有 Accessibility ，但是不符合跨平台的目标
- 用 QGraphicsView 的变换能力模拟实现放大镜的话，实现复杂、性能一般
- 不如手动维护：裁剪像素后用 drawPixmap 进行放大，虽然要处理 DPR 和边界判断，但是完全可控（开销仅仅 0.5ms/帧 ）、跨平台且开销小

## 2. 绘制思路

直接通过放大像素实现，通过 dPR 实现 逻辑像素到放大后的物理像素 的转换，然后结合 Qt 暴露的 **drawPixmap** 进行绘制

控制过程：

1. 在 startCapture() 函数中刚开始截图就启动：设置 showManifier = true
2. 在 mouseMoveEvent() 函数中设置随着鼠标移动事件实时更新放大镜

同时和工具栏一样做了**碰撞检测**防止超出边界

## 3. 实现

a. 相关变量

```jsx
// 控制放大镜是否显示
bool showMagnifier;

// 当前鼠标位置，用于确定放大镜跟随位置
QPoint currentMousePos;

// 设备像素比，用于处理高分屏
double devicePixelRatio;

// 原始截图，用于获取放大区域
QPixmap screenPixmap;
```

b. 核心绘制

```jsx
// 绘制放大镜
if (showMagnifier && !selected)
{
    // 1. 设置放大镜参数
    int magnifierSize = 120; // 放大镜显示大小（像素）
    int magnifierScale = 4;  // 放大倍数（4倍放大）

    // 2. 计算放大镜位置（初始在鼠标右下方）
    int magnifierX = currentMousePos.x() + 20;
    int magnifierY = currentMousePos.y() + 20;

    // 3. 确保放大镜不超出屏幕边界
    if (magnifierX + magnifierSize > width())
        magnifierX = currentMousePos.x() - magnifierSize - 20;
    if (magnifierY + magnifierSize > height())
        magnifierY = currentMousePos.y() - magnifierSize - 20;

    // 4. 计算需要放大的局部区域
    // 源区域大小 = 放大镜大小 / 放大倍数
    int sourceSize = magnifierSize / magnifierScale;
    
    // 逻辑坐标下的源区域（鼠标中心）
    QRect logicalSourceRect(
        currentMousePos.x() - sourceSize / 2,
        currentMousePos.y() - sourceSize / 2,
        sourceSize,
        sourceSize);

    // 5. 确保源区域在窗口范围内
    logicalSourceRect = logicalSourceRect.intersected(QRect(0, 0, width(), height()));

    // 6. 转换为物理像素坐标（处理高分屏）
    QPoint windowPos = geometry().topLeft();
    QPoint offset = windowPos - virtualGeometryTopLeft;

    QRect physicalSourceRect(
        (logicalSourceRect.x() + offset.x()) * devicePixelRatio,
        (logicalSourceRect.y() + offset.y()) * devicePixelRatio,
        logicalSourceRect.width() * devicePixelRatio,
        logicalSourceRect.height() * devicePixelRatio);

    // 7. 如果源区域有效，绘制放大镜
    if (!physicalSourceRect.isEmpty())
    {
        // 7.1 绘制放大镜背景（白色背景，蓝色边框）
        painter.setPen(QPen(QColor(0, 150, 255), 2));
        painter.setBrush(Qt::white);
        painter.drawRect(magnifierX, magnifierY, magnifierSize, magnifierSize);

        // 7.2 绘制放大的图像
        QRect targetRect(magnifierX, magnifierY, magnifierSize, magnifierSize);
        painter.drawPixmap(targetRect, screenPixmap, physicalSourceRect);

        // 7.3 绘制十字准星（红色十字线，便于精确定位）
        painter.setPen(QPen(Qt::red, 1));
        int centerX = magnifierX + magnifierSize / 2;
        int centerY = magnifierY + magnifierSize / 2;
        painter.drawLine(centerX - 10, centerY, centerX + 10, centerY);
        painter.drawLine(centerX, centerY - 10, centerX, centerY + 10);
    }
}
```

# 绘制箭头

- **Commit 9c90479**

## 1. 首先构建了一个结构体存储相关信息

基于 **OOP** 的思想，使用结构体封装箭头属性，便于扩展和维护

```jsx
struct DrawnArrow {
    QPoint start;    
    QPoint end;      
    QColor color;    
    int width;       
};
```

然后使用 QVector<DrawnArrow> arrows 向量来存储所有已绘制的箭头

## 2. 绘制管理

当 currentDrawMode 当前绘制模式为 Arrow 的时候，监听

1. 鼠标按下：
    - isDrawing = true
    - drawStartPoint = event → pos()
2. 鼠标移动：
    - drawEndPoint = event->pos() 更新终点的同时触发重绘实时显示效果
3. 鼠标释放：
    - isDrawing = false
    - 同时创建 DrawnArrow 对象保存结构体需要的
        - 起点
        - 终点
        - 颜色
        - 宽度
        
        后加入到之前说的 arrows 向量中
        

## 3. **高 DPI 双重缩放导致的放大问题**

### 问题现象

箭头绘制偏移且放大了两倍

### 查找根因

```cpp
window.devicePixelRatio
```

或

```cpp
QScreen *screen = QGuiApplication::primaryScreen();
qreal dpr = screen->devicePixelRatio();
qDebug() << "Device Pixel Ratio:" << dpr;
```

我检查了屏幕的 DPR ，发现和箭头的始末点放大倍数一致！都为2

于是我分析

1. screenPixmap 存储物理像素（3840×2160）
2. 代码中手动 × DPR 计算物理坐标  // 第一次缩放
3. croppedPixmap 继承 DPR = 2 属性
4. QPainter 自动 × DPR 绘制          // Qt 的第二次缩放

### 调试

为了代码的统一性考虑，我控制 **所有绘制走物理像素，由自己管理 DPR** ，

```cpp
croppedPixmap.setDevicePixelRatio(1.0); 
```

禁止了 Qt 用 DPR 进行的自动缩放

# OCR 文字识别

## 1. 原生API（兜底处理）

- **Commit 06cd33a**

出于精度、免安装等考虑，我先尝试用macOS 原生的 **Vision Framework 支持OCR**，串联链路

在 [macocr](http://macocr.mm) 中进行 Vision API 的封装

```jsx
#include <Vision/Vision.h>

QString MacOCR::recognizeText(const QPixmap& pixmap) {
    // 1. QPixmap 转 CGImage
    QImage image = pixmap.toImage();
    QByteArray byteArray;
    QBuffer buffer(&byteArray);
    image.save(&buffer, "PNG");
    
    NSData* nsData = [NSData dataWithBytes:byteArray.constData() length:byteArray.size()];
    NSImage* nsImage = [[NSImage alloc] initWithData:nsData];
    CGImageRef cgImage = [nsImage CGImageForProposedRect:nil context:nil hints:nil];
    
    // 2. 创建并配置 Vision 文本识别请求
    VNRecognizeTextRequest *request = [[VNRecognizeTextRequest alloc] initWithCompletionHandler:^(VNRequest *request, NSError *error) {
        // 处理识别结果
        NSMutableString *fullText = [NSMutableString string];
        for (VNRecognizedTextObservation *observation in request.results) {
            NSArray<VNRecognizedText *> *topCandidates = [observation topCandidates:1];
            if (topCandidates.count > 0) {
                [fullText appendString:topCandidates[0].string];
                [fullText appendString:@"\n"];
            }
        }
        resultText = QString::fromNSString(fullText).trimmed();
    }];
    
    // 配置识别参数
    request.recognitionLevel = VNRequestTextRecognitionLevelAccurate;
    request.usesLanguageCorrection = YES;
    request.recognitionLanguages = @[@"zh-Hans", @"en-US"];
    
    // 3. 执行识别请求
    VNImageRequestHandler *handler = [[VNImageRequestHandler alloc] initWithCGImage:cgImage options:@{}];
    NSError *error = nil;
    [handler performRequests:@[request] error:&error];
    
    return resultText;
}
```

然后在 screenshotwidget 中完善调用链

```jsx
		// 获取当前截图（包含绘制的内容）
    QPixmap capture = this->grab(selectedRect);
    // 调用 OCR
    QString text = MacOCR::recognizeText(capture);
```

## 2. ⭐渐进增强：**Tesseract**

- **Commit a59921f**

### pre: 技术选型

为了支持，我搜集了一些常见的库，例如 Tesseract 、 EasyOCR 、PaddleOCR 等等，其中 PaddleOCR 虽然精度可以但是模型体积大，不适合打包进轻量工具，EasyOCR的话是用 python ，虽然我之前尝试过一些 py 和 C++ 串联的项目 https://github.com/ceilf6/SmartFruits 、有经验，但是考虑到稳定性、工具的轻量化，我最终选择了 Tesseract 库

在 OcrManager 类中判断，如果定义了 USE_TESSERACT 则使用 Tesseract （否则的话在 mac 上使用 Vision 、同时提示可以通过下载库进行增强，其他平台的话弹窗提示需要下载依赖库）

同时在调用前会通过 Leptonica 进行图像预处理，提升OCR的识别效果

```jsx
QString OcrManager::doRecognize(const QPixmap &pixmap) {
    // 确保引擎已初始化
    if (!initializeOcr()) {
        return "Error: Could not initialize OCR engine.";
    }
    
    // 图像预处理
    QImage image = pixmap.toImage();
    image = preprocessImage(image);
    image = image.convertToFormat(QImage::Format_RGB888);
    
    // 设置图像数据
    m_tesseractApi->SetImage(image.bits(), image.width(), image.height(), 3, image.bytesPerLine());
    
    // 执行OCR识别
    char *outText = m_tesseractApi->GetUTF8Text();
    QString result = QString::fromUtf8(outText);
    
    // 清理资源
    delete[] outText;
    
    return result.trimmed();
}
```

注：上面这个是之前的同步写法，现在是通过 Qt Concurrent 实现了 recognizeAsync 性能优化的异步模式，避免了UI的冻结

## 3. 性能优化

- **Commit a19b469**

### **a. ⭐单例模式**

**删除拷贝函数和赋值运算符，构造私有化，通过静态getter获取唯一实例（线程安全）**

避免每次调用都重新初始化Tesseract引擎（初始化耗时得有个 200ms）

PS: C++11之前，为了线程安全是需要手动加锁的，但是C++11 之后之需要开启 static 静态实例就能保证

### b. ⭐记忆化缓存

使用 **图像哈希值** 作为键，防止重复识别图像（类似于 **LRU** 、 **useMemo** 、python3.8的**@cache**实现记忆型回溯 等）

```jsx
    if (ocrInstance->m_resultCache.contains(cacheKey))
    {
        return *ocrInstance->m_resultCache[cacheKey];
    }
```

### c. ⭐异步识别-防止UI冻结

由于OCR是无UI的计算任务，不用通过QThread，可以直接使用 **Qt Concurrent** 进行异步识别，是线程池自动管理的，类似于 Web Worker

```jsx
    QtConcurrent::run([pixmap, callback]()
                      {
        QString result = recognize(pixmap);
        callback(result); });
```

## 4. 优化交互界面

- **Commit a705c69**

原先是通过 QMessageBox::information 系统自带的对话框进行展示识别的文本结果、很窄很长，很多文本的时候对用户非常的不友好

于是为了优化工具交互、用户体验和结果展示，创建了 ocrresultdialog 

### a. 文本优化

- 通过 QTextEdit 控件显示结果，支持多行文本和滚动查看
- 设置 12pt 提高可读性
- 用 WidgetWidth 支持文本换行、响应式适应不同屏幕的尺寸

### b. 样式优化

- #f9f9f9 浅色背景
- 圆角

### c. 交互优化

- 支持全选
- 一键全选和一键复制
- 同时按钮都有 hover , pressed 状态变化提示

# 国际化支持

## 1. 使用 ⭐MCP 工具进行硬编码文本替换

- commit **746f7ef**

通过 i18n MCP 进行了死文本的国际化文本替换，构建了文本数据集

```jsx
{
  "mcpServers": {
    "block-translate-tool": {
      "command": "pnpm",
      "args": ["dlx", "@block/translate-tool@latest", "-tools=block-translate-tool"]
    }
  }
}
```

## 2. 翻译文案通过 ⭐npm 包管理

- commit 9f2e577

将 简体中文、英文、繁体中文 的翻译文件发布到了 npm 空间上，同时在构建脚本中添加了

```cpp
npm install @screensniper/locales
```

package.json 依赖文件中设置

```cpp
  "dependencies": {
    "@screensniper/locales": "latest"
  },
```

同时更新了构建脚本

```cpp
// build.sh
# 国际化支持
echo "📥 更新 locales 包..."
# 修复 npm 缓存权限问题
if [ -d "$HOME/.npm" ]; then
    sudo chown -R $(id -u):$(id -g) "$HOME/.npm" 2>/dev/null || true
    rm -rf ~/.npm
fi
npm install @screensniper/locales
npm run install-locales
echo ""
```

```cpp
// build.bat
REM 国际化支持
echo 📥 更新 locales 包...
REM 修复 npm 缓存权限问题（Windows通过清理缓存解决）
if exist "%USERPROFILE%\.npm" (
    echo 🧹 清理 npm 缓存...
    call npm cache clean --force
)
call npm install @screensniper/locales
call npm run install-locales
echo.
```

## 3. 代码使用

I18nManager::instance()->getText(key, **defaultValue**)

### 兜底处理

getText 方法的后一个参数就是默认值，当依赖包由于某些原因缺失时仍然不会影响工具使用

## 4. ⭐事件发射器动态更新机制

- **Commit 400af94**
1. 在 switchLanguage 中发射变更信号

```jsx
    // 发射语言变化信号，通知所有连接的组件
    emit languageChanged(language);
```

1. 然后需要在 screenshotWidget 中添加槽函数响应语言变化，进行回调

```jsx
void onLanguageChanged(); // 响应语言变化的回调函数列表
```

```jsx
void ScreenshotWidget::setMainWindow(QWidget *mainWin)
{
    mainWindow = mainWin;
    // 连接信号槽：当 mainWindow 发射 languageChanged 信号时，
    // 自动调用 this 的 onLanguageChanged 槽函数
    connect(mainWindow, SIGNAL(languageChanged(QString)),
            this, SLOT(onLanguageChanged()));
    updateTooltips();
}

void ScreenshotWidget::onLanguageChanged()
{
    updateTooltips();  // 更新所有工具提示
    updateUI();        // 主页面的重绘
    update();          // 请求重绘
}
```

## 5. 解耦合

- **Commit cc89a63**

依赖于 mainWindow

翻译数据都存储在 mainWindow 中，ScreenshotWidget 无法知道这个类的存在。通过**成员变量建立连接 - 关联模式，耦合度高！**

后面通过 **单例模式（删除拷贝函数和赋值运算符，构造私有化，通过静态getter获取唯一实例）** 管理独立的 I18nManager 类，提高了代码的可维护性和可拓展性

- 小bug：由于load后没有save导致的每次重启没有记忆化配置而是使用了默认语言
- **Commit 4d9f4af**

这样每次启动都会从单例访问器中拿到 I18nManager::instance()->currentLanguage(); 语言配置后确保全局的统一且变更后会保存到 QSettings 

# AI支持

## 1. 技术选型

### a. 直接接入 SDK

从 https://platform.openai.com/docs/quickstart 首先看到可以通过接入 官方的SDK，然后直接调用其方法，不用去写底层的请求，但是我好像只找到 Py / JS 的SDK，没有找到契合我们项目的，于是出于高性能以及**实战学习**的角度出发选择了第二种方案（后面我好像看到开源社区有 Unofficial 是 C++ 的 SDK，后面有时间可以优化）

### b. 通过 AIConfigManager 中接入多种模型进行管理

- **Commit 88c4097**

在 **Commit 0bae492** 键至同学的基础上，根据配置文件的服务类型动态选择调用阿里云还是OpenAI，统一了图片处理流程支持base64编码的需求

同时支持代码的可拓展性、强健性，后续需要添加模型只需要在配置文件中添加模版后在 aimanager 中添加分流并具体处理对应模型需要的 JSON 文件格式

## 2. 按照请求体模版构建JSON对象并POST请求

```jsx
        if (serviceType == AIConfigManager::OpenAI)
        {
            // OpenAI API 调用
            QString endpoint = AIConfigManager::instance().getEndpoint();
            QUrl url(endpoint);
            QNetworkRequest request(url);

            // 设置 Header
            request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
            request.setRawHeader("Authorization", ("Bearer " + apiKey).toUtf8());

            /*
curl https://api.openai.com/v1/responses \
  -H "Content-Type: application/json" \
  -H "Authorization: Bearer $OPENAI_API_KEY" \
  -d '{
    "model": "gpt-4.1",
    "input": [
      {
        "role": "user",
        "content": [
          {"type": "input_text", "text": "what is in this image?"},
          {
            "type": "input_image",
            "image_url": "https://upload.wikimedia.org/wikipedia/commons/thumb/d/dd/Gfp-wisconsin-madison-the-nature-boardwalk.jpg/2560px-Gfp-wisconsin-madison-the-nature-boardwalk.jpg"
          }
        ]
      }
    ]
  }'

            */

        // image_url 对象
        // QJsonObject imgUrlObj;
        // imgUrlObj["url"] = "data:image/png;base64," + base64Image;

        // content 数组
        QJsonArray contentArray;

        // text
        QJsonObject textObj;
        textObj["type"] = "input_text";
        textObj["text"] = "请描述这张图片的内容";
        contentArray.append(textObj);

        // image
        QJsonObject imgObj;
        imgObj["type"] = "input_image";
        imgObj["image_url"] = "data:image/png;base64," + base64Image;
        // 注意新版是直接传URL！！
        contentArray.append(imgObj);

        // user message
        QJsonObject messageObj;
        messageObj["role"] = "user";
        messageObj["content"] = contentArray;

        QJsonArray messagesArray;
        messagesArray.append(messageObj);

        // final body
        QJsonObject jsonBody;
        jsonBody["model"] = AIConfigManager::instance().getModelName();
        jsonBody["input"] = messagesArray; // 注意这里是 input !
        // jsonBody["max_tokens"] = 800; // 官方文档里面没有最大tokens数的字段

        // 创建一个用于显示的简化版本（截断 base64）
        QString base64Preview = base64Image.left(100) + "...[省略]..." + base64Image.right(50);

        QJsonObject imgObjPreview;
        imgObjPreview["type"] = "input_image";
        imgObjPreview["image_url"] = "data:image/png;base64," + base64Preview;

        QJsonArray contentArrayPreview;
        contentArrayPreview.append(textObj);
        contentArrayPreview.append(imgObjPreview);

        QJsonObject messageObjPreview;
        messageObjPreview["role"] = "user";
        messageObjPreview["content"] = contentArrayPreview;

        QJsonArray messagesArrayPreview;
        messagesArrayPreview.append(messageObjPreview);

        QJsonObject jsonBodyPreview;
        jsonBodyPreview["model"] = AIConfigManager::instance().getModelName();
        jsonBodyPreview["input"] = messagesArrayPreview;
```

## 3. 配置

ApiKey: 去 https://platform.openai.com/api-keys 拿到你专属账号的接口key

Endpoint: 标准接口的访问地址，可以去 https://platform.openai.com/docs/guides/images-vision?api-mode=responses 可以看到，POST https://api.openai.com/v1/responses 所以我的 Endpoint 就填了这个端口

然后你可以根据你账号支持的模型 https://platform.openai.com/docs/models 选择你想要的模型填入到 Model

## 4. 我认为的后续可以优化的地方

1. 对话框、图片修改 https://platform.openai.com/docs/api-reference/images/createEdit?utm_source=chatgpt.com
2. 通过监听UI鼠标事件自动切换模型
3. Unofficial

# 附录

## 相关名词

- DPI: **Dots Per Inch（每英寸像素点数量）** 的缩写，用来描述屏幕的 **像素密度。**即逻辑像素到物理像素的比例
- DPR: **device Pixel Ratio（设备像素比）。**和 DPI 对应，代表 物理像素到逻辑像素 的比例
- CoreGraphics：macOS 底层的 2D 绘图引擎，GPU和CPU共同驱动
- GDI：Windows 上的 2D 绘图系统，CPU渲染、不利用GPU，导致性能较弱
- X11：Linux/Unix 图形界面的基础协议