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

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow), trayIcon(nullptr), trayMenu(nullptr), currentLanguage("zh")
{
    ui->setupUi(this);

    loadLanguageSettings();

    setWindowTitle(getText("app_title", "ScreenSniper - 截图工具"));
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

    // 添加按钮（使用多语言文本）
    QPushButton *btnFullScreen = new QPushButton(getText("btn_fullscreen", "截取全屏 (Ctrl+Shift+F)"), this);
    QPushButton *btnArea = new QPushButton(getText("btn_area", "截取区域 (Ctrl+Shift+A)"), this);
    QPushButton *btnSettings = new QPushButton(getText("btn_settings", "设置"), this);

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
    trayIcon->setToolTip(getText("tray_tooltip", "ScreenSniper - 截图工具"));

    // 创建托盘菜单（使用多语言文本）
    trayMenu = new QMenu(this);

    QAction *actionFullScreen = new QAction(getText("tray_fullscreen", "截取全屏"), this);
    QAction *actionArea = new QAction(getText("tray_area", "截取区域"), this);
    QAction *actionShow = new QAction(getText("tray_show", "显示主窗口"), this);
    QAction *actionAbout = new QAction(getText("tray_about", "关于"), this);
    QAction *actionQuit = new QAction(getText("tray_quit", "退出"), this);

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
    // 结束时会卡一下，看不到写的信息，所以直接注释掉了，后续可以加类似飞书的消息提示
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
    // 结束时会卡一下，看不到写的信息，所以直接注释掉了，后续可以加类似飞书的消息提示
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
            switchLanguage(newLanguage);
            updateUI();
            QMessageBox::information(this,
                                     getText("settings_title", "设置"),
                                     getText("settings_language_changed", "语言已更改，部分界面将在重启后生效。"));
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
    if (translations.contains(key))
    {
        return translations[key].toString();
    }
    return defaultText.isEmpty() ? key : defaultText;
}

// 加载语言设置
void MainWindow::loadLanguageSettings()
{
    QSettings settings("ScreenSniper", "ScreenSniper");
    currentLanguage = settings.value("language", "zh").toString();

    switchLanguage(currentLanguage);
}

// 保存语言设置
void MainWindow::saveLanguageSettings()
{
    QSettings settings("ScreenSniper", "ScreenSniper");
    settings.setValue("language", currentLanguage);
}

// 切换语言
void MainWindow::switchLanguage(const QString &language)
{
    currentLanguage = language;

    // 构建语言文件路径
    QString langFile = QString(":/locales/%1.json").arg(language);

    // 如果资源文件不存在，尝试从文件系统读取
    if (!QFile::exists(langFile))
    {
        langFile = QString("locales/%1.json").arg(language);
    }

    QFile file(langFile);
    if (!file.open(QIODevice::ReadOnly))
    {
        qWarning() << "无法打开语言文件:" << langFile;
        qWarning() << "提示: 如果是首次编译，请先运行 'npm install && npm run install-locales' 安装翻译文件";
        translations = QJsonObject(); // 清空翻译
        return;
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull() || !doc.isObject())
    {
        qWarning() << "语言文件格式错误:" << langFile;
        translations = QJsonObject();
        return;
    }

    translations = doc.object();
    qDebug() << "成功加载语言文件:" << langFile << "包含" << translations.keys().size() << "个键";

    saveLanguageSettings();
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

    // 注意：由于按钮和菜单在 setupUI 和 setupTrayIcon 中创建，
    // 完整的界面更新需要重启应用程序
}
