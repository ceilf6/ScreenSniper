QT       += core gui widgets network

# 明确移除 NO_OPENCV 定义（如果存在）
DEFINES -= NO_OPENCV

win32 {
    LIBS += -lPsapi -lDwmapi
}
macx {
    LIBS += -framework CoreGraphics
    LIBS += -framework Vision
    LIBS += -framework AppKit
    
    OBJECTIVE_SOURCES += macocr.mm
    HEADERS += macocr.h
    
    # Tesseract configuration for macOS (Homebrew)
    # Uncomment the following lines to use Tesseract instead of native Vision
    DEFINES += USE_TESSERACT
    INCLUDEPATH += /opt/homebrew/include
    LIBS += -L/opt/homebrew/lib -ltesseract -lleptonica

    # OpenCV configuration for macOS (Homebrew)
    INCLUDEPATH += /opt/homebrew/opt/opencv/include/opencv4
    LIBS += -L/opt/homebrew/opt/opencv/lib \
            -lopencv_core \
            -lopencv_imgproc \
            -lopencv_highgui \
            -lopencv_imgcodecs \
            -lopencv_videoio \
            -lopencv_dnn \
            -lopencv_objdetect
}
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

# OpenCV 配置 - 跨平台版本
# Windows 特定的链接库
win32 {
    LIBS += -luser32
    LIBS += -lgdi32
    LIBS += -ldwmapi

    # 如果需要其他 Windows 特有库
    # LIBS += -lole32
    # LIBS += -loleaut32
    # LIBS += -luuid
}

# Linux 特定的链接库
unix:!macx {
    # X11 相关库（用于窗口操作）
    LIBS += -lX11
    LIBS += -lXfixes
    LIBS += -lXext

    # 如果需要图形相关
    # LIBS += -lXrender
    # LIBS += -lXrandr
}
win32 {
    # Windows 配置
    # OpenCV 配置
    # 明确移除 NO_OPENCV 定义（如果之前有定义）
    DEFINES -= NO_OPENCV
    
    # OpenCV头文件路径
    INCLUDEPATH += D:/rr/opencv/opencv/opencv_bulid/install/include
    
    # OpenCV库文件路径 + 链接库
    LIBS += -LD:/rr/opencv/opencv/opencv_bulid/lib
    LIBS += -lopencv_calib3d4120 -lopencv_core4120 -lopencv_dnn4120 -lopencv_features2d4120 \
            -lopencv_flann4120 -lopencv_highgui4120 -lopencv_imgcodecs4120 -lopencv_imgproc4120 \
            -lopencv_ml4120 -lopencv_objdetect4120 -lopencv_photo4120 -lopencv_stitching4120 \
            -lopencv_ts4120 -lopencv_video4120 -lopencv_videoio4120

    # Tesseract configuration for Windows
    # 请根据实际安装路径修改
    # DEFINES += USE_TESSERACT
    # INCLUDEPATH += "C:/Program Files/Tesseract-OCR/include"
    # LIBS += -L"C:/Program Files/Tesseract-OCR/lib" -ltesseract51

    LIBS += -lPsapi -lDwmapi
}





# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
# DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0


SOURCES += \
    aiconfigmanager.cpp \
    aimanager.cpp \
    main.cpp \
    mainwindow.cpp \
    pinwidget.cpp \
    screenshotwidget.cpp \
    ocrmanager.cpp \
    ocrresultdialog.cpp \
    watermark_robust.cpp \
    i18nmanager.cpp \
    facedetector.cpp

HEADERS += \
    aiconfigmanager.h \
    aimanager.h \
    mainwindow.h \
    pinwidget.h \
    screenshotwidget.h \
    ocrmanager.h \
    ocrresultdialog.h \
    watermark_robust.h \
    i18nmanager.h \
    facedetector.h

FORMS += \
    mainwindow.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

RESOURCES += \
    resources.qrc

# Optional per-developer local config (ignored by git). Put machine-specific INCLUDEPATH/LIBS here.
# Example: local_config.pri (not committed). This allows each developer to keep custom library
# paths (Tesseract, OpenCV, etc.) out of the shared `.pro` file.
exists(local_config.pri) {
    include(local_config.pri)
}
