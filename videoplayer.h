#ifndef VIDEOPLAYER_H
#define VIDEOPLAYER_H

#include <QMainWindow>
#include <QMediaPlayer>
#include <QVideoWidget>
#include <QMenu>
#include <QFileDialog>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QAudioOutput>
#include <QSlider>
#include <QSettings>
#include <QMimeDatabase>
#include <QVideoSink>
#include <QVideoFrame>
#include <QLabel>
#include <QToolButton>
#include <QTimer>
#include <QVector>
#include <QAction>
#include <QCloseEvent>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QKeyEvent>
#include <QResizeEvent>
#include <QContextMenuEvent>
#include <QEnterEvent>
#include <QEvent>
#include <QImage>
#include <QPainter>

class VideoPlayer : public QMainWindow
{
    Q_OBJECT

public:
    explicit VideoPlayer(QWidget *parent = nullptr);
    ~VideoPlayer();

public slots:
    void openFile(const QString &filePath);  // 用于接收文件路径

protected:
    // ---------- 事件处理 ----------
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    bool eventFilter(QObject *obj, QEvent *event) override;

private slots:
    // ---------- 播放控制 ----------
    void openVideo();
    void togglePlayback();
    void stopPlayback();
    void setVolume(int volume);
    void setPlaybackRate(float rate);
    void toggleLoopPlayback();

    // ---------- 进度控制 ----------
    void updateProgress();
    void updateTimeLabel(qint64 current, qint64 total);

    // ---------- AB 点循环 ----------
    void toggleABLoop();
    void setLoopPointA();
    void setLoopPointB();
    void clearLoopPoints();
    void exportABLoop();

    // ---------- 截图 ----------
    void exportCurrentFrame();

    // ---------- 速度控制 ----------
    void setPlaybackRateFromSlider(int value);
    void setPlaybackRatePreset(float rate);
    void resetPlaybackRate();
    void updateSpeedLabel(float rate);
    void showSpeedControl(bool show = true);
    void toggleSpeedControl();
    void updateSpeedControl();

    // ---------- UI 控制 ----------
    void showControlPanel();
    void hideControlPanel();
    void updateVolumeButtonIcon(int volumePercent);

private:
    // ---------- 初始化与设置 ----------
    void setupUI();
    void setupMenu();
    void loadDefaultVideo();
    void setupProgressControls();
    void setupControlPanel();
    void setupSpeedControls();
    void setupVideoSink();

    // ---------- 播放辅助 ----------
    void adjustVolume(float delta);
    void handleFLVFile(const QString &filePath);
    void showVolumeChange(float volume);
    void updateVolumeSlider(float volume);
    void checkABLoop();
    void updateABInfoDisplay();
    void showABLoopInfo();
    void updateABMenuState();
    void updateControlPanelPosition();
    void drawMarkersDirectlyOnSlider();
    QString formatTime(qint64 ms);

    // ---------- 截图与图像处理 ----------
    QImage videoFrameToImage(const QVideoFrame &frame);
    QImage convertVideoFrameToImage(const QVideoFrame &frame);
    QImage captureWindowScreenshot();
    QImage getVideoFrameFromSink();
    QImage convertVideoFrameManually(const QVideoFrame &frame);
    QImage convertYUVtoRGB(const QVideoFrame &frame, QVideoFrameFormat::PixelFormat format);
    void showBackendInfo();
    bool hasCorruption(const QImage &image);
    QImage fixCorruptedImage(const QImage &image);
    QImage getCurrentFrameSimple();
    QImage getCurrentVideoFrame();
    QImage createFallbackImage(int width, int height, const QString &message = QString());
    QImage::Format pixelFormatToImageFormat(QVideoFrameFormat::PixelFormat pixelFormat);
    void convertNV12ToRGB(const QVideoFrame &frame, QImage &rgbImage);
    void convertYUV420PToRGB(const QVideoFrame &frame, QImage &rgbImage);
    void convertGenericYUVToRGB(const QVideoFrame &frame, QImage &rgbImage, QVideoFrameFormat::PixelFormat format);
    bool isBlackImage(const QImage &image, int threshold);
    void showImagePreview(const QImage &image);
    void analyzeImage(const QImage &image);
    void forceVideoRender();
    QImage tryMultipleCaptureMethods();
    int evaluateImageQuality(const QImage &image);
    QImage captureScreenArea();
    void analyzeAndShowScreenshot(const QImage &screenshot);
    QString imageFormatToString(QImage::Format format);

private:
    // ---------- 核心播放 ----------
    QMediaPlayer *player;
    QVideoWidget *videoWidget;
    QAudioOutput *audioOutput;
    QVideoSink *videoSink;
    QImage currentVideoFrame;

    // ---------- UI 组件 ----------
    QMenu *contextMenu;
    QSlider *positionSlider;
    QLabel *timeLabel;
    QWidget *controlPanel;
    QToolButton *playPauseButton;
    QToolButton *stopButton;
    QLabel *abInfoLabel;
    QSlider *speedSlider;
    QLabel *speedLabel;
    QToolButton *speedButton;
    QMenu *speedMenu;
    QWidget *speedControlWidget;
    QLabel *osdLabel;

    // ---------- 状态与设置 ----------
    QSettings *settings;
    QPoint dragPosition;
    float currentScale;
    QString volumeText;
    bool showVolumeText = false;
    QTimer *hideVolumeTimer;
    QTimer *volumeHideTimer;
    QMimeDatabase mimeDb;
    bool isLooping = false;
    QAction *loopAction;
    QTimer *updateTimer;
    bool isSliderBeingDragged;
    qint64 loopPointA;
    qint64 loopPointB;
    bool isABLoopEnabled;
    QAction *loopABAction;
    QAction *setAAction;
    QAction *setBAction;
    QAction *clearABAction;
    QString lastExportDir;
    float currentPlaybackRate;
    QVector<float> speedPresets = {0.25f, 0.5f, 0.75f, 1.0f, 1.25f, 1.5f, 1.75f, 2.0f, 3.0f, 4.0f};
    QTimer *controlPanelHideTimer;
    int controlPanelHideDelay = 3000;
    bool controlPanelVisible;
    bool m_loopingJumpInProgress = false;
    void adjustVolumeByWheel(int deltaY);
};

#endif // VIDEOPLAYER_H

// 拆分方案：
//     1. videoplayer.cpp - 主文件，保留核心构造函数和基本方法
//     2. videoplayer_ui.cpp - UI相关：界面设置、控制面板、布局
//     3. videoplayer_playback.cpp - 播放控制：播放、暂停、进度控制、循环
//     4. videoplayer_screenshot.cpp - 截图功能：截图、图像处理、分析
//     5. videoplayer_menu.cpp - 菜单相关：右键菜单、AB点控制
//     6. videoplayer_speed.cpp - 速度控制：播放速度调整
//     7. videoplayer_events.cpp - 事件处理：鼠标、键盘、拖放事件
