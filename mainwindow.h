#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QAction>
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

private slots:
    void onCaptureScreen();
    void onCaptureArea();
    void onCaptureWindow();
    void onSettings();
    void onAbout();
    void onTrayIconActivated(QSystemTrayIcon::ActivationReason reason);

private:
    void setupUI();
    void setupTrayIcon();
    void setupConnections();

    Ui::MainWindow *ui;
    QSystemTrayIcon *trayIcon;
    QMenu *trayMenu;
};

#endif // MAINWINDOW_H
