#ifndef VIDEOPLAYER_H
#define VIDEOPLAYER_H

#include <QMainWindow>
#include <QMediaPlayer>
#include <QVideoWidget>
#include <QMenu>
#include <QFileDialog>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QAudioOutput>  // 必须添加这行
#include <qslider.h>

#include <QSettings>  // 新增

#include <QMimeDatabase>
#include <QVideoSink>
#include <QVideoFrame>
#include <qlabel.h>
#include <qtoolbutton.h>

class VideoPlayer : public QMainWindow
{
    Q_OBJECT

public:
    VideoPlayer(QWidget *parent = nullptr);
    ~VideoPlayer();

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;

private slots:
    void openVideo();
    void togglePlayback();
    void setVolume(int volume);
    void setPlaybackRate(float rate);

private:
    void setupUI();
    void setupMenu();
    void loadDefaultVideo();

    QMediaPlayer *player;
    QVideoWidget *videoWidget;
    QMenu *contextMenu;
    QPoint dragPosition;
    float currentScale;

protected:
    void closeEvent(QCloseEvent *event) override;  // 新增关闭事件

private:
    QSettings *settings;  // 设置保存

    QAudioOutput* audioOutput;//音频输出指针

private:
    QString volumeText;      // 当前音量文本
    bool showVolumeText = false; // 是否显示音量提示
    QTimer *hideVolumeTimer; // 隐藏文字的定时器


    void adjustVolume(float delta);

protected:
    void paintEvent(QPaintEvent *event) override;  // 重绘事件
protected:
    void keyPressEvent(QKeyEvent *event) override;  // 新增键盘事件处理

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;


public slots:
    void openFile(const QString &filePath);  // 用于接收文件路径
private:
    QMimeDatabase mimeDb;  // 声明为成员变量以便复用

protected:
    void registerFileAssociation()  ;//注册表

    // 在类定义中添加这些成员
private:
    bool isLooping = false;  // 循环播放状态
    QAction *loopAction;     // 循环播放菜单项

       QTimer *volumeHideTimer;

    void handleLoopPlayback(QMediaPlayer::PlaybackState state);
       void handleFLVFile(const QString &filePath);
    void showVolumeChange(float volume);
private slots:
    void toggleLoopPlayback();  // 切换循环播放

    // 在 VideoPlayer 类中添加以下成员变量声明：
private:
    // 进度条相关
    QSlider *positionSlider;
    QLabel *timeLabel;
    QTimer *updateTimer;  // 用于定时更新进度

    // 控制面板相关
    QWidget *controlPanel;
    QToolButton *playPauseButton;
    QToolButton *stopButton;

    // 防止拖动进度条时频繁更新
    bool isSliderBeingDragged;
    void leaveEvent(QEvent *event);
    void enterEvent(QEnterEvent *event);

    void updateVolumeButtonIcon(int volumePercent);
private slots:
    // 添加进度控制相关槽函数
    void setupProgressControls();
    void setupControlPanel();
    void updateProgress();
    void updateTimeLabel(qint64 current, qint64 total);
    QString formatTime(qint64 ms);
    // 进度控制相关槽函数
    // void updateProgressDisplay();
    // 在 VideoPlayer 类中添加以下成员变量声明
private:
    // AB点循环相关
    qint64 loopPointA;      // A点位置（毫秒）
    qint64 loopPointB;      // B点位置（毫秒）
    bool isABLoopEnabled;   // AB点循环是否启用
    QAction *loopABAction;  // 右键菜单中的AB循环动作
    QAction *setAAction;    // 设置A点
    QAction *setBAction;    // 设置B点
    QAction *clearABAction; // 清除AB点
    QLabel *abInfoLabel;    // 显示AB点信息

    void checkABLoop();
    void toggleABLoop();
    void updateABInfoDisplay();
    void showABLoopInfo();
    void updateABMenuState();
    void clearLoopPoints();
    void setLoopPointB();
    void setLoopPointA();
    void drawABMarkers(QPaintEvent *event);

private slots:
    // 导出当前帧图像
    void exportCurrentFrame();
    QImage videoFrameToImage(const QVideoFrame &frame);
    void setupVideoSink();  // 初始化视频接收器
private:
    // 新增成员变量
    QString lastExportDir;
private:
    // 新增成员变量
    QVideoSink *videoSink;  // 视频接收器，用于获取视频帧
    QImage currentVideoFrame;  // 当前视频帧的缓存

    QImage convertVideoFrameToImage(const QVideoFrame &frame);
    QImage captureWindowScreenshot();
    QImage getVideoFrameFromSink();
    QImage convertVideoFrameManually(const QVideoFrame &frame);
    QImage convertYUVtoRGB(const QVideoFrame &frame, QVideoFrameFormat::PixelFormat format);
    void showBackendInfo();

    // 图像处理相关函数
    bool hasCorruption(const QImage &image);
    QImage fixCorruptedImage(const QImage &image);
    QImage createTestImage();
    QImage getCurrentFrameSimple();
    QImage getCurrentVideoFrame();
    QImage createFallbackImage(int width, int height, const QString &message = QString());

    // 像素格式转换
    QImage::Format pixelFormatToImageFormat(QVideoFrameFormat::PixelFormat pixelFormat);
private:
    // YUV转换函数
    void convertNV12ToRGB(const QVideoFrame &frame, QImage &rgbImage);
    void convertYUV420PToRGB(const QVideoFrame &frame, QImage &rgbImage);
    void convertGenericYUVToRGB(const QVideoFrame &frame, QImage &rgbImage, QVideoFrameFormat::PixelFormat format);


    bool isBlackImage(const QImage &image, int threshold);

private:
    void showImagePreview(const QImage &image);
    void analyzeImage(const QImage &image);


private:
    void forceVideoRender();
    QImage tryMultipleCaptureMethods();
    int evaluateImageQuality(const QImage &image);
    QImage captureWithWorkaround();
    QImage captureScreenArea();
    // 新增的截图分析函数
    void analyzeAndShowScreenshot(const QImage &screenshot);
        QString imageFormatToString(QImage::Format format);

    // 在私有成员变量部分添加
private:
    // 播放速度控制相关
    QSlider *speedSlider;         // 播放速度滑块
    QLabel *speedLabel;           // 速度显示标签
    QToolButton *speedButton;     // 速度控制按钮（可选）
    float currentPlaybackRate;    // 当前播放速度
    QMenu *speedMenu;             // 速度预设菜单（可选）

    // 速度预设值
    QVector<float> speedPresets = {0.25f, 0.5f, 0.75f, 1.0f, 1.25f, 1.5f, 1.75f, 2.0f, 3.0f, 4.0f};
    QWidget *speedControlWidget;     // 速度控制容器

    // 在私有槽函数部分添加
private slots:
    void setupSpeedControls();                    // 设置速度控制界面
    void updateSpeedControl();                    // 更新速度控制显示
    void setPlaybackRateFromSlider(int value);    // 从滑块设置播放速度
    void setPlaybackRatePreset(float rate);       // 设置预设播放速度
    void resetPlaybackRate();                     // 重置播放速度
    void updateSpeedLabel(float rate);            // 更新速度标签
    void showSpeedControl(bool show = true);      // 显示/隐藏速度控制
    void toggleSpeedControl();                    // 切换速度控制显示

private slots:
    void exportABLoop();   // 导出 AB 循环片段

private:
    QTimer *controlPanelHideTimer;
    int controlPanelHideDelay = 3000; // 3秒后隐藏
    void updateControlPanelPosition();
    void resizeEvent(QResizeEvent *event);
    void updateVolumeSlider(float volume);
    void drawMarkersDirectlyOnSlider();

    void stopPlayback();
    bool controlPanelVisible;
    void showControlPanel();
    void hideControlPanel();
private:
    QLabel *osdLabel;   // OSD 标签

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

};

#endif // VIDEOPLAYER_H



// 拆分方案：
//     1. videoplayer.cpp - 主文件，保留核心构造函数和基本方法
//       2. videoplayer_ui.cpp - UI相关：界面设置、控制面板、布局
//       3. videoplayer_playback.cpp - 播放控制：播放、暂停、进度控制、循环
//       4. videoplayer_screenshot.cpp - 截图功能：截图、图像处理、分析
//       5. videoplayer_menu.cpp - 菜单相关：右键菜单、AB点控制
//       6. videoplayer_speed.cpp - 速度控制：播放速度调整
//       7. videoplayer_events.cpp - 事件处理：鼠标、键盘、拖放事件
