#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QAction>
#include "i18nmanager.h"
#include "screenshotwidget.h"

QT_BEGIN_NAMESPACE
namespace Ui
{
    class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    Q_INVOKABLE QString getText(const QString &key, const QString &defaultText = "") const;

signals:
    void languageChanged(const QString &newLanguage);

private slots:
    void onCaptureScreen();
    void onCaptureArea();
    void onCaptureWindow();
    void onCaptureScroll();
    void onSettings();
    void onAbout();
    void onTrayIconActivated(QSystemTrayIcon::ActivationReason reason);

private:
    void setupUI();
    void setupTrayIcon();
    void setupConnections();
    void updateUI();

    Ui::MainWindow *ui;
    QSystemTrayIcon *trayIcon;
    QMenu *trayMenu;
    QString currentLanguage;  // 当前语言设置: "zh", "en", "zhHK"

    // UI 元素引用，用于更新文本
    QPushButton *btnFullScreen;
    QPushButton *btnArea;
    QPushButton *btnSettings;
    QPushButton *btnScroll;
    QAction *actionFullScreen;
    QAction *actionScroll;
    QAction *actionArea;
    QAction *actionShow;
    QAction *actionAbout;
    QAction *actionQuit;
};

#endif // MAINWINDOW_H
