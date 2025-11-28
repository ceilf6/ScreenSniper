#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QMessageBox>
#include <QScreen>
#include <QGuiApplication>
#include <QClipboard>
#include <QFileDialog>
#include <QStandardPaths>
#include <QVBoxLayout>
#include <QPushButton>
#include <QTimer>
#include <QDateTime>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow), trayIcon(nullptr), trayMenu(nullptr)
{
    ui->setupUi(this);
    setWindowTitle("ScreenSniper - 截图工具");
    resize(400, 300);

    setupUI();
    setupTrayIcon();
    setupConnections();
}

MainWindow::~MainWindow()
{
    delete ui;
    // screenshotWidget会通过deleteLater()自动删除
}

void MainWindow::setupUI()
{
    // 创建中心部件
    QWidget *centralWidget = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(centralWidget);

    // 添加按钮
    QPushButton *btnFullScreen = new QPushButton("截取全屏 (Ctrl+Shift+F)", this);
    QPushButton *btnArea = new QPushButton("截取区域 (Ctrl+Shift+A)", this);
    QPushButton *btnSettings = new QPushButton("设置", this);

    btnFullScreen->setMinimumHeight(40);
    btnArea->setMinimumHeight(40);
    btnSettings->setMinimumHeight(40);

    layout->addWidget(btnFullScreen);
    layout->addWidget(btnArea);
    layout->addWidget(btnSettings);
    layout->addStretch();

    setCentralWidget(centralWidget);

    // 连接按钮信号
    connect(btnFullScreen, &QPushButton::clicked, this, &MainWindow::onCaptureScreen);
    connect(btnArea, &QPushButton::clicked, this, &MainWindow::onCaptureArea);
    connect(btnSettings, &QPushButton::clicked, this, &MainWindow::onSettings);
}

void MainWindow::setupTrayIcon()
{
    // 创建托盘图标
    trayIcon = new QSystemTrayIcon(this);
    trayIcon->setIcon(QIcon(":/icons/app_icon.png"));
    trayIcon->setToolTip("ScreenSniper - 截图工具");

    // 创建托盘菜单
    trayMenu = new QMenu(this);

    QAction *actionFullScreen = new QAction("截取全屏", this);
    QAction *actionArea = new QAction("截取区域", this);
    QAction *actionShow = new QAction("显示主窗口", this);
    QAction *actionAbout = new QAction("关于", this);
    QAction *actionQuit = new QAction("退出", this);

    trayMenu->addAction(actionFullScreen);
    trayMenu->addAction(actionArea);
    trayMenu->addSeparator();
    trayMenu->addAction(actionShow);
    trayMenu->addAction(actionAbout);
    trayMenu->addSeparator();
    trayMenu->addAction(actionQuit);

    trayIcon->setContextMenu(trayMenu);
    trayIcon->show();

    // 连接托盘信号
    connect(actionFullScreen, &QAction::triggered, this, &MainWindow::onCaptureScreen);
    connect(actionArea, &QAction::triggered, this, &MainWindow::onCaptureArea);
    connect(actionShow, &QAction::triggered, this, &MainWindow::show);
    connect(actionAbout, &QAction::triggered, this, &MainWindow::onAbout);
    connect(actionQuit, &QAction::triggered, qApp, &QApplication::quit);
    connect(trayIcon, &QSystemTrayIcon::activated, this, &MainWindow::onTrayIconActivated);
}

void MainWindow::setupConnections()
{
    // 设置全局快捷键
    // 注意：需要使用平台相关的API或第三方库实现全局快捷键
}

void MainWindow::onCaptureScreen()
{
    // 隐藏主窗口和托盘图标提示
    hide();

    // 每次都重新创建截图窗口
    ScreenshotWidget *widget = new ScreenshotWidget();
    //结束时会卡一下，看不到写的信息，所以直接注释掉了，后续可以加类似飞书的消息提示
//    connect(widget, &ScreenshotWidget::screenshotTaken, this, [this]()
//            {
//        //show();
//        trayIcon->showMessage("截图成功", "全屏截图已保存", QSystemTrayIcon::Information, 2000); });

//    connect(widget, &ScreenshotWidget::screenshotCancelled, this, [this]()
//            { show(); });

    // 延迟让窗口完全隐藏后再截图
    QTimer::singleShot(300, widget, [widget]()
                       { widget->startCaptureFullScreen(); });
}

void MainWindow::onCaptureArea()
{
    // 隐藏主窗口和托盘图标提示
    hide();

    // 每次都重新创建截图窗口
    ScreenshotWidget *widget = new ScreenshotWidget();
    //结束时会卡一下，看不到写的信息，所以直接注释掉了，后续可以加类似飞书的消息提示
//    connect(widget, &ScreenshotWidget::screenshotTaken, this, [this]()
//            {
//        show();
//        trayIcon->showMessage("截图成功", "区域截图已保存到剪赴板", QSystemTrayIcon::Information, 2000); });

//    connect(widget, &ScreenshotWidget::screenshotCancelled, this, [this]()
//            { show(); });

    // 延迟让窗口完全隐藏后再截图
    QTimer::singleShot(300, widget, &ScreenshotWidget::startCapture);
}

void MainWindow::onCaptureWindow()
{
    onCaptureArea();
}

void MainWindow::onSettings()
{
    QMessageBox::information(this, "设置", "设置功能开发中...");
}

void MainWindow::onAbout()
{
    QMessageBox::about(this, "关于 ScreenSniper",
                       "<h3>ScreenSniper 截图工具</h3>"
                       "<p>版本：1.0.0</p>"
                       "<p>一个简单易用的截图工具</p>"
                       "<p>功能特性：</p>"
                       "<ul>"
                       "<li>全屏截图</li>"
                       "<li>区域截图</li>"
                       "<li>图像编辑</li>"
                       "</ul>");
}

void MainWindow::onTrayIconActivated(QSystemTrayIcon::ActivationReason reason)
{
    if (reason == QSystemTrayIcon::DoubleClick)
    {
        show();
        activateWindow();
    }
}
