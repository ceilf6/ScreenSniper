#ifndef SCROLLCAPTURE_H
#define SCROLLCAPTURE_H

#include <QWidget>
#include <QTimer>
#include <QImage>
#include <QThread>
#include <QPainter>
#include <QMouseEvent>
#include <QKeyEvent>

// OpenCV
#include <opencv2/opencv.hpp>
#include <opencv2/imgproc.hpp>

#ifdef Q_OS_WIN
#include <Windows.h>
#endif

// 拼接工作类 (保持不变)
class ScrollStitcher : public QObject
{
    Q_OBJECT
public:
    explicit ScrollStitcher(QObject *parent = nullptr);
public slots:
    void processStitch(QImage baseImage, QImage newImage);
signals:
    void stitchFinished(QImage result);
    void stitchFailed(QImage original);
private:
    int calcOffsetByTemplate(const cv::Mat &matBase, const cv::Mat &matNew);
    cv::Mat qImageToCvMat(const QImage &inImage);
    QImage cvMatToQImage(const cv::Mat &inMat);
};

// 窗口类
class ScrollCaptureWindow : public QWidget
{
    Q_OBJECT
public:
    explicit ScrollCaptureWindow(QWidget *parent = nullptr);
    ~ScrollCaptureWindow();

    void startCapture();

signals:
    void captureFinished(QImage resultImage);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private slots:
    void onScrollTimer();
    void onStitchResult(QImage result);

private:
    enum State { IDLE, SELECTING, SCROLLING };
    State m_state;

    QRect m_selectRect;
    QPoint m_startPoint;
    QTimer *m_scrollTimer;

    QImage m_fullCanvas;
    QThread *m_thread;
    ScrollStitcher *m_stitcher;

    int m_scrollCount;
    int m_sameImgCount;
    bool m_isStitching;
    bool m_stopRequested;
    QRect m_stopButtonRect;  // 停止按钮区域
    void sendMouseWheel(int delta);
    QImage grabCurrentArea();
    void finishCapture();
};

#endif // SCROLLCAPTURE_H
