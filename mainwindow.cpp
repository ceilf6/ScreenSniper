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
#include <QSettings>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>
#include <QDialog>
#include <QComboBox>
#include <QLabel>
#include <QHBoxLayout>
#include <QDialogButtonBox>
#include <QPointer>
#include "ScrollCapture.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      ui(new Ui::MainWindow),
      trayIcon(nullptr),
      trayMenu(nullptr),
      btnFullScreen(nullptr),
      btnArea(nullptr),
      btnSettings(nullptr),
      actionFullScreen(nullptr),
      actionArea(nullptr),
      actionShow(nullptr),
      actionAbout(nullptr),
      actionQuit(nullptr)
{
    ui->setupUi(this);

    // 先连接I18nManager的语言变化信号
    connect(I18nManager::instance(), &I18nManager::languageChanged, this, [this](const QString &newLanguage)
            {
        currentLanguage = newLanguage;
        updateUI();
        emit languageChanged(newLanguage); });

    // 从I18nManager获取当前语言
    currentLanguage = I18nManager::instance()->currentLanguage();

    setWindowTitle(getText("app_title", "ScreenSniper - 截图工具"));
    resize(400, 350);

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

    // 添加按钮（使用多语言文本）
    btnFullScreen = new QPushButton(getText("btn_fullscreen", "截取全屏 (Ctrl+Shift+F)"), this);
    btnArea = new QPushButton(getText("btn_area", "截取区域 (Ctrl+Shift+A)"), this);
    btnScroll = new QPushButton(getText("btnScroll ", "滚动截图"), this);
    btnSettings = new QPushButton(getText("btn_settings", "设置"), this);

    btnFullScreen->setMinimumHeight(40);
    btnArea->setMinimumHeight(40);
    btnScroll->setMinimumHeight(40); // 设置相同的高度
    btnSettings->setMinimumHeight(40);

    layout->addWidget(btnFullScreen);
    layout->addWidget(btnArea);
    layout->addWidget(btnScroll); // 将按钮加入布局
    layout->addWidget(btnSettings);
    layout->addStretch();

    setCentralWidget(centralWidget);

    // 连接按钮信号
    connect(btnFullScreen, &QPushButton::clicked, this, &MainWindow::onCaptureScreen);
    connect(btnArea, &QPushButton::clicked, this, &MainWindow::onCaptureArea);
    connect(btnScroll, &QPushButton::clicked, this, &MainWindow::onCaptureScroll);
    connect(btnSettings, &QPushButton::clicked, this, &MainWindow::onSettings);
}

void MainWindow::setupTrayIcon()
{
    // 创建托盘图标
    trayIcon = new QSystemTrayIcon(this);
    trayIcon->setIcon(QIcon(":/icons/app_icon.png"));
    trayIcon->setToolTip(getText("tray_tooltip", "ScreenSniper - 截图工具"));

    // 创建托盘菜单（使用多语言文本）
    trayMenu = new QMenu(this);

    actionFullScreen = new QAction(getText("tray_fullscreen", "截取全屏"), this);
    actionArea = new QAction(getText("tray_area", "截取区域"), this);
    actionScroll= new QAction(getText("tray_roll", "滚动截图"), this);
    actionShow = new QAction(getText("tray_show", "显示主窗口"), this);

    actionAbout = new QAction(getText("tray_about", "关于"), this);
    actionQuit = new QAction(getText("tray_quit", "退出"), this);

    trayMenu->addAction(actionFullScreen);
    trayMenu->addAction(actionArea);
    trayMenu->addAction(actionScroll);
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
    connect(actionScroll, &QAction::triggered, this, &MainWindow::onCaptureScroll);
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
    widget->setMainWindow(this);
    // 结束时会卡一下，看不到写的信息，所以直接注释掉了，后续可以加类似飞书的消息提示
    //    connect(widget, &ScreenshotWidget::screenshotTaken, this, [this]()
    //            {
    //        //show();
    //        trayIcon->showMessage("截图成功", "全屏截图已保存", QSystemTrayIcon::Information, 2000); });

    //    connect(widget, &ScreenshotWidget::screenshotCancelled, this, [this]()
    //            { show(); });

    // 使用 QPointer 安全地捕获指针，防止悬空指针
    QPointer<ScreenshotWidget> safeWidget(widget);
    // 延迟让窗口完全隐藏后再截图
    QTimer::singleShot(300, this, [safeWidget]()
                       { 
                           // 检查对象是否仍然有效
                           if (safeWidget) {
                               safeWidget->startCaptureFullScreen(); 
                           } });
}

void MainWindow::onCaptureArea()
{
    // 隐藏主窗口和托盘图标提示
    hide();

    // 每次都重新创建截图窗口
    ScreenshotWidget *widget = new ScreenshotWidget();
    widget->setMainWindow(this);
    // 结束时会卡一下，看不到写的信息，所以直接注释掉了，后续可以加类似飞书的消息提示
    //    connect(widget, &ScreenshotWidget::screenshotTaken, this, [this]()
    //            {
    //        show();
    //        trayIcon->showMessage("截图成功", "区域截图已保存到剪赴板", QSystemTrayIcon::Information, 2000); });

    //    connect(widget, &ScreenshotWidget::screenshotCancelled, this, [this]()
    //            { show(); });

    // 使用 QPointer 安全地捕获指针，防止悬空指针
    QPointer<ScreenshotWidget> safeWidget(widget);
    // 延迟让窗口完全隐藏后再截图
    QTimer::singleShot(300, this, [safeWidget]()
                       { 
                           // 检查对象是否仍然有效
                           if (safeWidget) {
                               safeWidget->startCapture(); 
                           } });
}

void MainWindow::onCaptureWindow()
{
    onCaptureArea();
}
void MainWindow::onCaptureScroll()
{
    hide();
    ScrollCaptureWindow *scrollWin = new ScrollCaptureWindow(nullptr);

    connect(scrollWin, &ScrollCaptureWindow::captureFinished, this, [this](QImage result){
        if(result.isNull()) {
            this->show();
            return;
        }

        // 直接进入 ScreenshotWidget 编辑/查看
        ScreenshotWidget *widget = new ScreenshotWidget();
        widget->setCapturedImage(result);
        widget->show();
    });

    QTimer::singleShot(300, scrollWin, &ScrollCaptureWindow::startCapture);
}
void MainWindow::onSettings()
{
    QDialog dialog(this);
    dialog.setWindowTitle(getText("settings_title", "设置"));
    dialog.resize(400, 150);

    QVBoxLayout *mainLayout = new QVBoxLayout(&dialog);

    // 语言设置区域
    QHBoxLayout *langLayout = new QHBoxLayout();
    QLabel *langLabel = new QLabel(getText("settings_language", "语言："), &dialog);
    QComboBox *langCombo = new QComboBox(&dialog);

    // 添加语言选项
    langCombo->addItem(getText("lang_zh", "简体中文"), "zh");
    langCombo->addItem(getText("lang_en", "English"), "en");
    langCombo->addItem(getText("lang_zhHK", "繁體中文"), "zhHK");

    // 设置当前语言
    int currentIndex = langCombo->findData(currentLanguage);
    if (currentIndex >= 0)
    {
        langCombo->setCurrentIndex(currentIndex);
    }

    langLayout->addWidget(langLabel);
    langLayout->addWidget(langCombo);
    langLayout->addStretch();

    mainLayout->addLayout(langLayout);
    mainLayout->addStretch();

    // 对话框按钮
    QDialogButtonBox *buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    mainLayout->addWidget(buttonBox);

    connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() == QDialog::Accepted)
    {
        QString newLanguage = langCombo->currentData().toString();
        if (newLanguage != currentLanguage)
        {
            // 使用I18nManager切换语言
            I18nManager::instance()->loadLanguage(newLanguage);
        }
    }
}

void MainWindow::onAbout()
{
    QString aboutText = QString("<h3>%1</h3>"
                                "<p>%2</p>"
                                "<p>%3</p>"
                                "<p>%4</p>"
                                "<ul>"
                                "<li>%5</li>"
                                "<li>%6</li>"
                                "<li>%7</li>"
                                "</ul>")
                            .arg(getText("app_title", "ScreenSniper - 截图工具"))
                            .arg(getText("about_version", "版本：1.0.0"))
                            .arg(getText("about_description", "一个简单易用的截图工具"))
                            .arg(getText("about_features", "功能特性："))
                            .arg(getText("feature_fullscreen", "全屏截图"))
                            .arg(getText("feature_area", "区域截图"))
                            .arg(getText("feature_edit", "图像编辑"));

    QMessageBox::about(this, getText("about_title", "关于 ScreenSniper"), aboutText);
}

void MainWindow::onTrayIconActivated(QSystemTrayIcon::ActivationReason reason)
{
    if (reason == QSystemTrayIcon::DoubleClick)
    {
        show();
        activateWindow();
    }
}

// 通过键获取翻译文本
QString MainWindow::getText(const QString &key, const QString &defaultText) const
{
    return I18nManager::instance()->getText(key, defaultText);
}

// 更新界面文本
void MainWindow::updateUI()
{
    // 更新窗口标题
    setWindowTitle(getText("app_title", "ScreenSniper - 截图工具"));

    // 更新托盘图标提示
    if (trayIcon)
    {
        trayIcon->setToolTip(getText("tray_tooltip", "ScreenSniper - 截图工具"));
    }

    // 更新主窗口按钮文本
    if (btnFullScreen)
        btnFullScreen->setText(getText("btn_fullscreen", "截取全屏 (Ctrl+Shift+F)"));
    if (btnArea)
        btnArea->setText(getText("btn_area", "截取区域 (Ctrl+Shift+A)"));
    if (btnArea)
        btnArea->setText(getText("btn_area", "截取区域 (Ctrl+Shift+A)"));
    if (btnSettings)
        btnSettings->setText(getText("btn_settings", "设置"));

    // 更新托盘菜单文本
    if (actionFullScreen)
        actionFullScreen->setText(getText("tray_fullscreen", "截取全屏"));
    if (actionArea)
        actionArea->setText(getText("tray_area", "截取区域"));
    if (actionShow)
        actionShow->setText(getText("tray_show", "显示主窗口"));
    if (actionAbout)
        actionAbout->setText(getText("tray_about", "关于"));
    if (actionQuit)
        actionQuit->setText(getText("tray_quit", "退出"));
}
