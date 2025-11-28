#include "mainwindow.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    // 设置应用程序名称
    a.setApplicationName("ScreenSniper");
    a.setApplicationDisplayName("屏幕截图工具");
    a.setOrganizationName("ScreenSniper");

    MainWindow w;
    w.show();

    return a.exec();
}
