#include "videoplayer.h"
#include "qapplication.h"
#include <QBoxLayout>
#include <QSlider>
#include <QToolButton>
#include <QStyle>
#include <QDir>
#include <QLabel>
#include <QTimer>

#include <QPainter>
#include <QAudioDevice>
#include <QAudioOutput>

#include <QStandardPaths>
#include <QMimeData>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QUrl>
#include <QPushButton>

#include <QMainWindow>
#include <QMimeDatabase>
#include <qstyle.h>

#include <QSettings>
#include <QWindow>
#include <qvideoframeformat.h>

#include <QMessageBox>
#include <QStatusBar>

#include <QDesktopServices>
#include <QThread>
#include <QMediaMetaData>
#include <QTextEdit>
#include <qshortcut.h>

#include <cstdlib>  // 添加：用于std::rand()
#include <QProcess>
#include <QFileInfo>
#include <QMessageBox>
#include <QProgressDialog>

VideoPlayer::VideoPlayer(QWidget *parent)
    : QMainWindow(parent),
    currentScale(1.0f),
    isSliderBeingDragged(false),
    isLooping(false),           // 初始化循环状态
    isABLoopEnabled(false),     // 初始化AB循环状态
    showVolumeText(false),      // 初始化音量显示状态
    loopPointA(-1),             // 初始化循环点A
    loopPointB(-1),             // 初始化循环点B
    settings(nullptr),          // 初始化设置对象
    player(nullptr),            // 初始化播放器
    videoWidget(nullptr),       // 初始化视频窗口
    audioOutput(nullptr),       // 初始化音频输出
    videoSink(nullptr),         // 初始化视频接收器
    controlPanel(nullptr),      // 初始化控制面板
    playPauseButton(nullptr),   // 初始化播放/暂停按钮
    stopButton(nullptr),        // 初始化停止按钮
    positionSlider(nullptr),    // 初始化进度条
    timeLabel(nullptr),         // 初始化时间标签
    abInfoLabel(nullptr),       // 初始化AB点标签
    updateTimer(nullptr),       // 初始化更新定时器
    volumeHideTimer(nullptr),   // 初始化音量隐藏定时器
    contextMenu(nullptr),       // 初始化右键菜单
    loopAction(nullptr),        // 初始化循环动作
    loopABAction(nullptr),      // 初始化AB循环动作
    setAAction(nullptr),        // 初始化设置A点动作
    setBAction(nullptr),        // 初始化设置B点动作
    clearABAction(nullptr),      // 初始化清除AB点动作
    currentPlaybackRate(1.0f),  // 默认正常速度
    controlPanelVisible(false)
{
    // 初始化随机数种子
    std::srand(static_cast<unsigned>(std::time(nullptr)));

    registerFileAssociation();
    setAcceptDrops(true);  // 启用拖放

    // 获取应用程序配置目录（跨平台）
    QString configPath = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    QDir().mkpath(configPath);  // 确保目录存在
    qDebug() << "配置文件路径：" << configPath + "/VideoPlayer.ini";

    // 初始化 QSettings 使用INI文件格式
    settings = new QSettings(configPath + "/VideoPlayer.ini", QSettings::IniFormat, this);

    // 读取保存的窗口状态
    if (settings->contains("windowGeometry")) {
        restoreGeometry(settings->value("windowGeometry").toByteArray());
    } else {
        // 首次运行，设置默认窗口大小
        resize(800, 450); // 默认大小
    }

    // 初始化媒体播放器
    player = new QMediaPlayer(this);

    // 创建视频窗口（只初始化一次）
    videoWidget = new QVideoWidget(this);
    videoWidget->setAttribute(Qt::WA_OpaquePaintEvent);
    videoWidget->setAttribute(Qt::WA_NoSystemBackground);
    videoWidget->setAutoFillBackground(false);

    player->setVideoOutput(videoWidget);

    // 连接错误信号以便调试
    connect(player, &QMediaPlayer::errorOccurred, this, [this](QMediaPlayer::Error error, const QString &errorString) {
        qDebug() << "媒体播放器错误:" << error << "-" << errorString;
        qDebug() << "详细错误:" << player->errorString();

        // 如果硬件加速失败，尝试其他方法
        if (error == QMediaPlayer::ResourceError || error == QMediaPlayer::FormatError) {
            QMessageBox::warning(this, "播放错误",
                                 QString("视频播放失败:\n%1\n\n尝试启用软件解码模式...").arg(errorString));
        }
    });

    // 初始化音频输出
    audioOutput = new QAudioOutput(this);
    player->setAudioOutput(audioOutput);

    // 从设置加载音量
    float savedVolume = 0.5f; // 默认音量50%
    if (settings->contains("volume")) {
        bool ok;
        savedVolume = settings->value("volume").toFloat(&ok);
        if (!ok) {
            savedVolume = 0.5f;
        } else {
            qDebug() << "加载保存的音量:" << savedVolume;
        }
    }
    // 设置音量
    if (audioOutput) {
        audioOutput->setVolume(savedVolume);
        qDebug() << "初始音量设为:" << audioOutput->volume();
    }

    // 添加音量显示定时器
    volumeHideTimer = new QTimer(this);
    volumeHideTimer->setSingleShot(true);
    connect(volumeHideTimer, &QTimer::timeout, this, [this]() {
        showVolumeText = false;
        osdLabel->hide();          // 隐藏独立窗口
        update();                  // 如果需要刷新其他绘制（右上角已弃用，可忽略）
    });
    // 初始化更新定时器
    updateTimer = new QTimer(this);
    // 在构造函数中初始化
    controlPanelHideTimer = new QTimer(this);
    controlPanelHideTimer->setSingleShot(true);
    connect(controlPanelHideTimer, &QTimer::timeout, this, [this]() {
        if (controlPanel && controlPanel->isVisible()) {
            controlPanel->hide();
        }
    });


    setupUI();
    setupMenu();
    setupProgressControls();  // 新函数，用于设置进度控制
    setupSpeedControls();
    loadDefaultVideo();

    videoWidget->installEventFilter(this);
    videoWidget->setMouseTracking(true);  // 确保子控件也追踪鼠标
    // 构造函数中
    videoWidget->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    setMouseTracking(true);
    // 在 VideoPlayer 构造函数末尾添加
    qApp->installEventFilter(this);
    setFocusPolicy(Qt::StrongFocus);
}

VideoPlayer::~VideoPlayer() {
    // 保存当前音量
    if (settings && audioOutput) {
        settings->setValue("volume", audioOutput->volume());
        settings->sync();
        qDebug() << "保存音量:" << audioOutput->volume();
    }
    // QSettings和其他Qt对象会自动清理，因为它们的父对象是this
}

void VideoPlayer::closeEvent(QCloseEvent *event) {
    // 保存当前窗口状态 ini
    if (settings) {
        settings->setValue("windowGeometry", saveGeometry());
        settings->setValue("windowState", saveState());

        // 保存当前音量
        if (audioOutput) {
            float currentVolume = audioOutput->volume();
            settings->setValue("volume", currentVolume);
            qDebug() << "保存音量:" << currentVolume;
        }

        settings->sync();  // 立即写入
    }

    event->accept();
}

void VideoPlayer::setupUI()
{
    // 原来：
    // osdLabel = new QLabel(videoWidget);

    // 改为：
    osdLabel = new QLabel(nullptr);  // 没有父对象，独立窗口
    osdLabel->setWindowFlags(Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    osdLabel->setAlignment(Qt::AlignCenter);
    osdLabel->setStyleSheet(
        "QLabel {"
        "  background-color: rgba(0, 0, 0, 180);"
        "  color: white;"
        "  font: bold 20px;"
        "  border: 2px solid white;"
        "  border-radius: 10px;"
        "  padding: 10px;"
        "}"
        );
    osdLabel->hide();


    QWidget *centralWidget = new QWidget(this);
    QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);

    // 只添加视频窗口，控制面板独立管理
    mainLayout->addWidget(videoWidget, 1); // 视频占据所有可用空间

    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    centralWidget->setLayout(mainLayout);
    setCentralWidget(centralWidget);

    // 设置视频窗口属性
    videoWidget->setAcceptDrops(false);
    videoWidget->setAttribute(Qt::WA_TransparentForMouseEvents);
    videoWidget->setFocusPolicy(Qt::NoFocus);

    // 设置主窗口焦点策略
    setFocusPolicy(Qt::StrongFocus);

    // 创建独立的控制面板（不作为布局的一部分）
    setupControlPanel();

    // 设置控制面板为浮动状态
    if (controlPanel) {
        // 设置窗口标志为工具窗口，不会影响主布局
        controlPanel->setWindowFlags(Qt::Tool | Qt::FramelessWindowHint);
        controlPanel->setAttribute(Qt::WA_TranslucentBackground);
        controlPanel->setAttribute(Qt::WA_ShowWithoutActivating);

        // 初始位置设置在底部
        updateControlPanelPosition();
    }

    // 连接信号槽
    connect(player, &QMediaPlayer::errorOccurred, [](QMediaPlayer::Error error) {
        qDebug() << "Media player error:" << error;
    });

    // 定时更新进度条
    connect(updateTimer, &QTimer::timeout, this, [this]() {
        if (!isSliderBeingDragged) {
            updateProgress();
        }
        // 检查AB点循环
        checkABLoop();
    });
}


void VideoPlayer::updateControlPanelPosition()
{
    if (!controlPanel) return;

    // 获取主窗口的几何信息
    QRect windowRect = this->geometry();

    // 计算控制面板应该在的位置
    int panelWidth = windowRect.width() - 40;  // 留一些边距
    int panelHeight = controlPanel->height();
    int panelX = windowRect.x() + 20;  // 左边距
    int panelY = windowRect.y() + windowRect.height() - panelHeight - 20;  // 底部，留边距

    // 设置控制面板位置和大小
    controlPanel->setGeometry(panelX, panelY, panelWidth, panelHeight);

    // 确保控制面板始终在最上层
    controlPanel->raise();
}

void VideoPlayer::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);

    // 窗口大小改变时，更新控制面板位置
    updateControlPanelPosition();

    // 如果有视频正在播放，确保控制面板显示在正确位置
    if (controlPanel && controlPanel->isVisible()) {
        controlPanel->raise();
    }
}

void VideoPlayer::enterEvent(QEnterEvent *event)
{
    QMainWindow::enterEvent(event);

    // 鼠标进入窗口时显示控制面板
    if (controlPanel && !controlPanel->isVisible()) {
        updateControlPanelPosition();  // 先更新位置
        controlPanel->show();
        controlPanel->raise();
        controlPanelHideTimer->start(2000);  // 2秒后自动隐藏
    }
}

void VideoPlayer::leaveEvent(QEvent *event)
{
    QMainWindow::leaveEvent(event);

    // 鼠标离开窗口时隐藏控制面板
    if (controlPanel && controlPanel->isVisible()) {
        // 检查鼠标是否真的在窗口外和控制面板外
        QPoint globalPos = QCursor::pos();
        QPoint localPos = mapFromGlobal(globalPos);
        QPoint panelPos = controlPanel->mapFromGlobal(globalPos);

        if (!rect().contains(localPos) && !controlPanel->rect().contains(panelPos)) {
            controlPanel->hide();
            controlPanelHideTimer->stop();
        }
    }
}

// 添加AB点循环检查函数
void VideoPlayer::checkABLoop()
{
    if (!isABLoopEnabled || loopPointA == -1 || loopPointB == -1) {
        return;
    }

    qint64 currentPos = player->position();
    qint64 duration = player->duration();

    // 如果当前播放位置超过B点，跳转到A点
    if (currentPos >= loopPointB) {
        // 稍微提前一点跳转，避免卡顿
        player->setPosition(loopPointA);
        qDebug() << "AB点循环: 从" << formatTime(currentPos)
                 << "跳转到" << formatTime(loopPointA);
    }
}

void VideoPlayer::setupControlPanel()
{
    controlPanel = new QWidget(this);
    // 设置面板本身不获取焦点
    controlPanel->setFocusPolicy(Qt::NoFocus);

    // 遍历面板上所有子控件，全部设为 NoFocus
    for (QWidget *child : controlPanel->findChildren<QWidget*>()) {
        child->setFocusPolicy(Qt::NoFocus);
    }
    controlPanel->setFixedHeight(30);
    controlPanel->setStyleSheet(R"(
        background-color: rgba(0, 0, 0, 160);
        border-top: 1px solid rgba(255, 255, 255, 50);
    )");
    //controlPanel->setAttribute(Qt::WA_TranslucentBackground, true);
    controlPanel->raise();

    QHBoxLayout *controlLayout = new QHBoxLayout(controlPanel);
    controlLayout->setContentsMargins(10, 5, 10, 5);
    controlLayout->setSpacing(10);

    // ---- 播放/暂停按钮 ----
    playPauseButton = new QToolButton(this);
    playPauseButton->setFocusPolicy(Qt::NoFocus);
    playPauseButton->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
    playPauseButton->setIconSize(QSize(24, 24));
    playPauseButton->setStyleSheet("background-color: transparent; border: none;");
    connect(playPauseButton, &QToolButton::clicked, this, &VideoPlayer::togglePlayback);

    // ---- 停止按钮 ----
    stopButton = new QToolButton(this);
    stopButton->setFocusPolicy(Qt::NoFocus);
    stopButton->setIcon(style()->standardIcon(QStyle::SP_MediaStop));
    stopButton->setIconSize(QSize(24, 24));
    stopButton->setStyleSheet("background-color: transparent; border: none;");
    connect(stopButton, &QToolButton::clicked, this, &VideoPlayer::stopPlayback);

    // ---- 进度条 ----
    positionSlider = new QSlider(Qt::Horizontal, this);
    positionSlider->setFocusPolicy(Qt::NoFocus);
    positionSlider->setRange(0, 1000);
    positionSlider->setStyleSheet(R"(
        QSlider::groove:horizontal {
            height: 12px; background: #555; border-radius: 6px;
        }
        QSlider::sub-page:horizontal {
            background: #1E90FF; border-radius: 6px;
        }
        QSlider::add-page:horizontal {
            background: #555; border-radius: 6px;
        }
        QSlider::handle:horizontal {
            background: white; width: 20px; height: 20px;
            margin: -4px 0; border-radius: 10px;
            border: 2px solid #1E90FF;
        }
        QSlider::handle:horizontal:hover {
            background: #f0f0f0; width: 22px; height: 22px;
            margin: -5px 0; border-radius: 11px;
        }
        QSlider::handle:horizontal:pressed {
            background: #d0d0d0;
        }
    )");
    positionSlider->setFocusPolicy(Qt::NoFocus);

    connect(positionSlider, &QSlider::sliderPressed, this, [this]() {
        isSliderBeingDragged = true;
    });
    connect(positionSlider, &QSlider::sliderReleased, this, [this]() {
        isSliderBeingDragged = false;
        if (player) {
            qint64 duration = player->duration();
            if (duration > 0) {
                qint64 newPosition = duration * positionSlider->value() / 1000;
                player->setPosition(newPosition);
            }
        }
        // ★ 延迟设置焦点
        QTimer::singleShot(0, this, [this]() { this->setFocus(); });
    });

    // ---- 时间标签 ----
    timeLabel = new QLabel("00:00 / 00:00", this);
    timeLabel->setFocusPolicy(Qt::NoFocus);
    timeLabel->setStyleSheet("color: white; font: 12px;");
    timeLabel->setFixedWidth(75);

    // ---- 音量控制 ----
    QToolButton *volumeButton = new QToolButton(this);
    volumeButton->setObjectName("volumeButton");   // 添加这行
    volumeButton->setFocusPolicy(Qt::NoFocus);
    volumeButton->setIcon(style()->standardIcon(QStyle::SP_MediaVolume));
    volumeButton->setIconSize(QSize(20, 20));
    volumeButton->setStyleSheet("background-color: transparent; border: none;");

    QSlider *volumeSlider = new QSlider(Qt::Horizontal, this);
    volumeSlider->setFocusPolicy(Qt::NoFocus);
    volumeSlider->setRange(0, 100);
    if (audioOutput) {
        int currentVolume = static_cast<int>(audioOutput->volume() * 100);
        volumeSlider->setValue(currentVolume);
    } else {
        volumeSlider->setValue(50);
    }
    volumeSlider->setFixedWidth(50);
    volumeSlider->setStyleSheet(R"(
        QSlider::groove:horizontal {
            height: 12px; background: #555; border-radius: 6px;
        }
        QSlider::sub-page:horizontal {
            background: #1E90FF; border-radius: 6px;
        }
        QSlider::add-page:horizontal {
            background: #555; border-radius: 6px;
        }
        QSlider::handle:horizontal {
            background: white; width: 12px; height: 12px;
            margin: -3px 0; border-radius: 6px;
        }
        QSlider::handle:horizontal:hover {
            background: #f0f0f0; width: 14px; height: 14px;
            margin: -4px 0; border-radius: 7px;
        }
    )");

    connect(volumeSlider, &QSlider::valueChanged, this, [this](int value) {
        if (audioOutput) {
            float volume = value / 100.0f;
            audioOutput->setVolume(volume);
            volumeText = QString("音量: %1%").arg(value);
            showVolumeText = true;
            update();
            if (volumeHideTimer) volumeHideTimer->start(2000);
            updateVolumeButtonIcon(value);
        }
        // ★ 移除这里的 this->setFocus()
    });
    // ★ 新增：音量滑块释放时延迟设置焦点
    connect(volumeSlider, &QSlider::sliderReleased, this, [this]() {
        QTimer::singleShot(0, this, [this]() { this->setFocus(); });
    });

    // 音量布局
    QHBoxLayout *volumeLayout = new QHBoxLayout();
    volumeLayout->setSpacing(5);
    volumeLayout->setContentsMargins(0, 0, 0, 0);
    volumeLayout->addWidget(volumeButton);
    volumeLayout->addWidget(volumeSlider);
    QWidget *volumeWidget = new QWidget(this);
    volumeWidget->setLayout(volumeLayout);

    // ---- AB点信息 ----
    abInfoLabel = new QLabel(this);
    abInfoLabel->setFocusPolicy(Qt::NoFocus);
    abInfoLabel->setStyleSheet("color: #FFD700; font: 10px;");
    abInfoLabel->setFixedWidth(105);
    abInfoLabel->setText("AB点: 未设置");

    // ---- 速度控制占位 ----
    speedControlWidget = new QWidget(this);
    QHBoxLayout *speedLayout = new QHBoxLayout(speedControlWidget);
    speedLayout->setContentsMargins(0, 0, 0, 0);
    speedLayout->setSpacing(5);
    speedControlWidget->setLayout(speedLayout);

    // 添加到布局
    controlLayout->addWidget(playPauseButton);
    controlLayout->addWidget(stopButton);
    controlLayout->addWidget(positionSlider, 1);
    controlLayout->addWidget(timeLabel);
    controlLayout->addWidget(abInfoLabel);
    controlLayout->addWidget(speedControlWidget);
    controlLayout->addWidget(volumeWidget);

    controlPanel->hide();
}

void VideoPlayer::updateVolumeButtonIcon(int volumePercent)
{
    if (!controlPanel) return;
    QToolButton *volumeButton = controlPanel->findChild<QToolButton*>("volumeButton");
    if (!volumeButton) return;

    // 根据音量选择图标（Qt标准图标只有两种，无法区分低中高）
    QStyle::StandardPixmap volumeIcon = (volumePercent <= 0)
                                            ? QStyle::SP_MediaVolumeMuted
                                            : QStyle::SP_MediaVolume;
    volumeButton->setIcon(style()->standardIcon(volumeIcon));
}

void VideoPlayer::setupProgressControls()
{
    if (!player) return;

    // 连接播放状态变化信号
    connect(player, &QMediaPlayer::playbackStateChanged, this, [this](QMediaPlayer::PlaybackState state) {
        if (state == QMediaPlayer::PlayingState) {
            // 开始定时更新进度
            if (updateTimer) {
                updateTimer->start(50);  // 每100ms更新一次
            }
            if (playPauseButton) {
                playPauseButton->setIcon(style()->standardIcon(QStyle::SP_MediaPause));
            }
        } else {
            // 暂停或停止时停止定时器
            if (updateTimer) {
                updateTimer->stop();
            }
            if (state == QMediaPlayer::PausedState && playPauseButton) {
                playPauseButton->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
            }
        }
    });

    // 连接时长变化信号
    connect(player, &QMediaPlayer::durationChanged, this, [this](qint64 duration) {
        if (duration > 0) {
            updateTimeLabel(0, duration);
        }
    });

}

void VideoPlayer::setupVideoSink()
{
    if (!player) return;

    // 创建视频接收器
    videoSink = new QVideoSink(this);

    // 将视频接收器设置为播放器的视频输出
    player->setVideoSink(videoSink);

    // 连接视频帧变化信号
    if (videoSink) {
        connect(videoSink, &QVideoSink::videoFrameChanged, this, [this](const QVideoFrame &frame) {
            // 缓存当前视频帧
            currentVideoFrame = videoFrameToImage(frame);
        });
    }
}

void VideoPlayer::updateProgress()
{
    if (!positionSlider || !player || player->duration() <= 0) return;

    qint64 current = player->position();
    qint64 total = player->duration();

    // ---- 新增：无缝循环提前跳转 ----
    if (isLooping && total > 0) {
        const qint64 earlyThreshold = 300; // 提前量（毫秒），可调
        // 如果当前播放位置距离末尾不足阈值，且尚未触发跳转
        if (total - current < earlyThreshold && current < total) {
            static bool jumping = false;
            if (!jumping) {
                jumping = true;

                // 可选：临时静音，消除解码重置时的爆音（如需要）
                float origVol = audioOutput ? audioOutput->volume() : 1.0f;
                if (audioOutput) audioOutput->setVolume(0.0f);

                // 跳转到开头（0毫秒）
                player->setPosition(0);
                // 如果播放状态意外丢失，重新启动（一般不会）
                if (player->playbackState() != QMediaPlayer::PlayingState) {
                    player->play();
                }

                // 延迟恢复音量（100ms 足够掩盖重置声）
                QTimer::singleShot(100, this, [this, origVol]() {
                    if (audioOutput) audioOutput->setVolume(origVol);
                    jumping = false;
                });
            }
            // 跳转后，本次更新不再继续更新进度条（避免显示负数）
            // 但为了保险，仍然执行下面的进度条更新（设置到0）
            current = 0; // 强制进度条归零
        }
    }
    // ---- 新增结束 ----

    // 更新进度条（使用修正后的current）
    positionSlider->setValue(static_cast<int>((current * 1000) / total));

    // 更新时间标签
    updateTimeLabel(current, total);
}

void VideoPlayer::updateTimeLabel(qint64 current, qint64 total)
{
    if (!timeLabel) return;

    QString currentTime = formatTime(current);
    QString totalTime = formatTime(total);

    timeLabel->setText(QString("%1 / %2").arg(currentTime, totalTime));
}

QString VideoPlayer::formatTime(qint64 ms)
{
    if (ms <= 0) return "00:00";

    int seconds = static_cast<int>(ms / 1000);
    int minutes = seconds / 60;
    int hours = minutes / 60;

    seconds %= 60;
    minutes %= 60;

    if (hours > 0) {
        return QString("%1:%2:%3")
            .arg(hours, 2, 10, QLatin1Char('0'))
            .arg(minutes, 2, 10, QLatin1Char('0'))
            .arg(seconds, 2, 10, QLatin1Char('0'));
    } else {
        return QString("%1:%2")
            .arg(minutes, 2, 10, QLatin1Char('0'))
            .arg(seconds, 2, 10, QLatin1Char('0'));
    }
}

void VideoPlayer::loadDefaultVideo()
{
    if (!player) return;

    // 尝试加载默认视频
    QString defaultVideo = QDir::currentPath() + "/default.mp4";

    if (QFile::exists(defaultVideo)) {
        player->setSource(QUrl::fromLocalFile(defaultVideo));
        player->play();
    } else {
        qDebug() << "默认视频文件未找到:" << defaultVideo;
    }
}

void VideoPlayer::openVideo()
{
    if (!player) return;

    QString fileName = QFileDialog::getOpenFileName(this, "打开视频文件", "",
                                                    "视频文件 (*.mp4 *.avi *.mkv *.mov *.wmv *.flv *.webm *.ts)");

    if (!fileName.isEmpty()) {
        player->setSource(QUrl::fromLocalFile(fileName));
        player->play();
    }
}

void VideoPlayer::togglePlayback()
{
    if (!player) return;

    if (player->playbackState() == QMediaPlayer::PlayingState) {
        player->pause();
    } else {
        player->play();
    }
}

void VideoPlayer::setVolume(int volume)
{
    if (player && player->audioOutput()) {
        player->audioOutput()->setVolume(volume / 100.0f);
    }
}

void VideoPlayer::setPlaybackRate(float rate)
{
    if (player) {
        player->setPlaybackRate(rate);
    }
}

void VideoPlayer::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        dragPosition = event->globalPosition().toPoint() - frameGeometry().topLeft();
        event->accept();
    }
}

void VideoPlayer::wheelEvent(QWheelEvent *event)
{
    if (!audioOutput) return;

    const float step = 0.01f;
    float currentVolume = audioOutput->volume();
    if (event->angleDelta().y() > 0)
        currentVolume = qMin(currentVolume + step, 1.0f);
    else
        currentVolume = qMax(currentVolume - step, 0.0f);

    audioOutput->setVolume(currentVolume);
    float newVolume = audioOutput->volume();

    int percent = static_cast<int>(newVolume * 100);

    osdLabel->setText(QString("音量: %1%").arg(percent));
    osdLabel->adjustSize();

    // 将 OSD 窗口移动到主窗口中央（使用全局坐标）
    QPoint center = this->mapToGlobal(this->rect().center());
    osdLabel->move(center - QPoint(osdLabel->width()/2, osdLabel->height()/2));
    osdLabel->show();

    // 启动定时器，2秒后隐藏
    volumeHideTimer->start(2000);

    event->accept();
}

void VideoPlayer::contextMenuEvent(QContextMenuEvent *event)
{
    if (contextMenu) {
        contextMenu->exec(event->globalPos());
    }
}

void VideoPlayer::setupMenu()
{
    contextMenu = new QMenu(this);

    contextMenu->addSeparator();
    QAction *playPauseAction = contextMenu->addAction("播放/暂停");
    connect(playPauseAction, &QAction::triggered, this, &VideoPlayer::togglePlayback);


    QAction *stopAction = contextMenu->addAction("停止");
    connect(stopAction, &QAction::triggered, this, &VideoPlayer::stopPlayback);


    // 创建循环播放动作
    loopAction = new QAction("循环播放", this);
    loopAction->setCheckable(true);
    loopAction->setChecked(isLooping);
    connect(loopAction, &QAction::triggered, this, &VideoPlayer::toggleLoopPlayback);
    contextMenu->addAction(loopAction);

    // AB点循环相关菜单项
    contextMenu->addSeparator();

    // AB点循环开关
    loopABAction = new QAction("AB点循环", this);
    loopABAction->setCheckable(true);
    loopABAction->setChecked(isABLoopEnabled);
    connect(loopABAction, &QAction::triggered, this, &VideoPlayer::toggleABLoop);
    contextMenu->addAction(loopABAction);

    // 设置A点
    setAAction = new QAction("设置A点", this);
    connect(setAAction, &QAction::triggered, this, [this]() {
        setLoopPointA();
    });
    contextMenu->addAction(setAAction);

    // 设置B点
    setBAction = new QAction("设置B点", this);
    connect(setBAction, &QAction::triggered, this, [this]() {
        setLoopPointB();
    });
    contextMenu->addAction(setBAction);

    // 清除AB点
    clearABAction = new QAction("清除AB点", this);
    connect(clearABAction, &QAction::triggered, this, [this]() {
        clearLoopPoints();
    });
    contextMenu->addAction(clearABAction);



    // 更新AB点菜单项状态
    updateABMenuState();


    contextMenu->addSeparator();
    QAction *openAction = contextMenu->addAction("打开视频");
    connect(openAction, &QAction::triggered, this, &VideoPlayer::openVideo);

    // 新增：导出当前帧
    QAction *exportFrameAction = contextMenu->addAction("导出当前帧为图片");
    connect(exportFrameAction, &QAction::triggered, this, &VideoPlayer::exportCurrentFrame);
    // 导出 AB 循环为 MP4
    QAction *exportABAction = contextMenu->addAction("导出 AB 循环为 MP4");
    connect(exportABAction, &QAction::triggered, this, &VideoPlayer::exportABLoop);
    exportABAction->setShortcut(QKeySequence("Ctrl+Shift+E"));

    // 添加快捷键提示
    exportFrameAction->setShortcut(QKeySequence("Ctrl+E"));
    exportFrameAction->setShortcutContext(Qt::ApplicationShortcut);



    contextMenu->addSeparator();
    QAction *exitAction = contextMenu->addAction("退出");
    connect(exitAction, &QAction::triggered, qApp, &QApplication::quit);
}

void VideoPlayer::toggleLoopPlayback()
{
    isLooping = !isLooping;

    if (loopAction) {
        loopAction->setChecked(isLooping);
    }

    // 设置或取消循环播放
    if (isLooping) {
        qDebug() << "循环播放已开启";
        // 连接播放结束信号
        if (player) {
            connect(player, &QMediaPlayer::playbackStateChanged,
                    this, &VideoPlayer::handleLoopPlayback);
        }
    } else {
        qDebug() << "循环播放已关闭";
        // 断开连接
        if (player) {
            disconnect(player, &QMediaPlayer::playbackStateChanged,
                       this, &VideoPlayer::handleLoopPlayback);
        }
    }
}

// 处理循环播放逻辑
void VideoPlayer::handleLoopPlayback(QMediaPlayer::PlaybackState state)
{
    if (!isLooping || !player) return; // 如果不是循环模式或播放器无效，直接返回

    // 当播放结束时（从播放状态变为停止状态）
    if (state == QMediaPlayer::StoppedState) {
        qDebug() << "视频播放结束，重新开始循环播放";

        // 延迟一小段时间后重新播放，避免立即重启的问题
        QTimer::singleShot(100, this, [this]() {
            if (isLooping && player) {
                player->setPosition(0);
                player->play();
            }
        });
    }
}

void VideoPlayer::paintEvent(QPaintEvent *event) {
    QMainWindow::paintEvent(event); // 先调用父类绘制

}

void VideoPlayer::drawABMarkers(QPaintEvent *event)
{
    Q_UNUSED(event);

    if (!positionSlider || !player || player->duration() <= 0) return;
    if (loopPointA == -1 && loopPointB == -1) return;

    if (!positionSlider->isVisible()) return;

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // 获取进度条位置
    QRect sliderRect = positionSlider->geometry();
    QPoint sliderTopLeft = positionSlider->mapTo(this, QPoint(0, 0));
    sliderRect.moveTopLeft(sliderTopLeft);

    qint64 duration = player->duration();
    if (duration <= 0) return;

    // 圆形标记半径
    int markerRadius = 8;

    // 绘制A点圆形标记
    if (loopPointA != -1) {
        double aRatio = static_cast<double>(loopPointA) / duration;
        aRatio = qBound(0.0, aRatio, 1.0);

        int xPos = sliderRect.left() + static_cast<int>(aRatio * sliderRect.width());
        xPos = qBound(sliderRect.left() + markerRadius, xPos, sliderRect.right() - markerRadius);
        int yPos = sliderRect.top() - markerRadius - 5;  // 在滑块上方

        // 绘制填充圆形
        painter.setBrush(QBrush(QColor(255, 100, 100, 220)));  // 半透明红色
        painter.setPen(QPen(Qt::red, 2));
        painter.drawEllipse(QPoint(xPos, yPos), markerRadius, markerRadius);

        // 绘制"A"文字
        painter.setPen(Qt::white);
        painter.setFont(QFont("Arial", 9, QFont::Bold));
        painter.drawText(xPos - 4, yPos + 4, "A");
    }

    // 绘制B点圆形标记
    if (loopPointB != -1) {
        double bRatio = static_cast<double>(loopPointB) / duration;
        bRatio = qBound(0.0, bRatio, 1.0);

        int xPos = sliderRect.left() + static_cast<int>(bRatio * sliderRect.width());
        xPos = qBound(sliderRect.left() + markerRadius, xPos, sliderRect.right() - markerRadius);
        int yPos = sliderRect.top() - markerRadius - 20;  // 在A点上方

        // 绘制填充圆形
        painter.setBrush(QBrush(QColor(100, 100, 255, 220)));  // 半透明蓝色
        painter.setPen(QPen(Qt::blue, 2));
        painter.drawEllipse(QPoint(xPos, yPos), markerRadius, markerRadius);

        // 绘制"B"文字
        painter.setPen(Qt::white);
        painter.setFont(QFont("Arial", 9, QFont::Bold));
        painter.drawText(xPos - 4, yPos + 4, "B");
    }
}

void VideoPlayer::keyPressEvent(QKeyEvent *event) {
    if (!player) {
        QMainWindow::keyPressEvent(event);
        return;
    }

    // 处理Ctrl+E截图
    if (event->key() == Qt::Key_E && event->modifiers() == Qt::ControlModifier) {
        qDebug() << "keyPressEvent: Ctrl+E detected";
        exportCurrentFrame();
        event->accept();
        return;
    }

    // 处理F5截图
    if (event->key() == Qt::Key_F5) {
        qDebug() << "keyPressEvent: F5 detected";
        exportCurrentFrame();
        event->accept();
        return;
    }

    const float step = 5.0f;  // 单步步长（秒）
    const float volumeStep = 0.05f; // 音量步进5%

    switch (event->key()) {
    case Qt::Key_Right:  // 方向键→：前进
        player->setPosition(player->position() + step * 1000);  // 转为毫秒
        qDebug() << "前进" << step << "秒";
        break;

    case Qt::Key_Left:   // 方向键←：后退
        player->setPosition(qMax(0.0, player->position() - step * 1000));
        qDebug() << "后退" << step << "秒";
        break;

    case Qt::Key_Space:
        togglePlayback();
        break;

    case Qt::Key_Up:    // 方向上键：音量+
        adjustVolume(volumeStep);
        break;

    case Qt::Key_Down:  // 方向下键：音量-
        adjustVolume(-volumeStep);
        break;

    default:
        QMainWindow::keyPressEvent(event);  // 其他按键交给父类处理
    }
}

void VideoPlayer::adjustVolume(float delta) {
    if (!audioOutput) return;

    float currentVolume = audioOutput->volume();
    float newVolume = qBound(0.0f, currentVolume + delta, 1.0f);

    // 设置新音量
    audioOutput->setVolume(newVolume);

    // 更新音量滑块（如果存在）
    updateVolumeSlider(newVolume);

    // 更新显示
    volumeText = QString("音量: %1%").arg(static_cast<int>(newVolume * 100));
    showVolumeText = true;
    update();

    // 保存到设置
    if (settings) {
        settings->setValue("volume", newVolume);
    }

    // 2秒后显示APP名称
    if (volumeHideTimer) {
        volumeHideTimer->start(2000);
    }

    // 设置窗口标题显示当前音量
    setWindowTitle(QString("视频播放器 - 音量: %1%").arg(static_cast<int>(newVolume * 100)));

    // 2秒后恢复显示APP名称
    QTimer::singleShot(2000, this, [this]() {
        setWindowTitle("视频播放器");
    });
}

// 更新音量滑块
void VideoPlayer::updateVolumeSlider(float volume)
{
    if (!controlPanel) return;

    // 查找控制面板中的音量滑块
    QSlider* volumeSlider = controlPanel->findChild<QSlider*>();

    // 或者使用更精确的方法：查找特定类型的控件
    QList<QSlider*> sliders = controlPanel->findChildren<QSlider*>();
    for (QSlider* slider : sliders) {
        // 检查这个滑块是否可能是音量滑块（通过范围或样式等）
        if (slider && slider->maximum() == 100 && slider != positionSlider) {
            int volumePercent = static_cast<int>(volume * 100);
            // 防止信号循环：先断开连接，更新值，再重新连接
            bool wasBlocked = slider->blockSignals(true);
            slider->setValue(volumePercent);
            slider->blockSignals(wasBlocked);

            // 更新音量按钮图标
            updateVolumeButtonIcon(volumePercent);
            break;
        }
    }
}

void VideoPlayer::openFile(const QString &filePath) {
    if (player && QFile::exists(filePath)) {
        player->setSource(QUrl::fromLocalFile(filePath));
        player->play();
    }
}

// 拖入事件处理
void VideoPlayer::dragEnterEvent(QDragEnterEvent *event)
{
    qDebug() << "dragEnterEvent triggered";

    // 接受所有有URL的拖放（放宽条件）
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void VideoPlayer::dropEvent(QDropEvent *event)
{
    qDebug() << "dropEvent triggered";

    const QMimeData* mimeData = event->mimeData();

    if (!mimeData->hasUrls() || !player) {
        qDebug() << "拖放内容不包含有效URL或播放器未初始化";
        return;
    }

    // 获取第一个文件的本地路径
    QString filePath = mimeData->urls().first().toLocalFile();
    qDebug() << "尝试打开拖放文件:" << filePath;

    // 检查文件是否存在
    if (!QFile::exists(filePath)) {
        qDebug() << "文件不存在:" << filePath;
        return;
    }

    // 放宽文件类型检查
    QMimeType mimeType = mimeDb.mimeTypeForFile(filePath);
    qDebug() << "文件MIME类型:" << mimeType.name();

    // 支持的视频扩展名列表
    static const QStringList videoExtensions = {
        "mp4", "avi", "mkv", "mov", "wmv", "flv", "webm" ,"ts"
    };

    QFileInfo fileInfo(filePath);
    QString suffix = fileInfo.suffix().toLower();

    if (videoExtensions.contains(suffix) || mimeType.name().startsWith("video/")) {
        qDebug() << "尝试播放视频文件:" << filePath;
        player->setSource(QUrl::fromLocalFile(filePath));
        // player->stop();



        event->acceptProposedAction();
    } else {
        qDebug() << "不支持的文件类型:" << mimeType.name() << "扩展名:" << suffix;
    }
}

void VideoPlayer::registerFileAssociation() {
    QSettings settings("HKEY_CLASSES_ROOT\\.mp4", QSettings::NativeFormat);
    settings.setValue(".", "VideoPlayer");  // 关联 .mp4 到 VideoPlayer

    QSettings appSettings("HKEY_CLASSES_ROOT\\VideoPlayer\\shell\\open\\command", QSettings::NativeFormat);
    appSettings.setValue(".", QString("\"%1\" \"%2\"").arg(QCoreApplication::applicationFilePath()).arg("%1"));
}



// 或者更简单的方法：在鼠标移动时显示控制面板
void VideoPlayer::mouseMoveEvent(QMouseEvent *event)
{
    static QPoint lastPos;
    QPoint currentPos = event->globalPosition().toPoint();

    if (currentPos != lastPos) {
        updateControlPanelPosition();
        lastPos = currentPos;

        // ★ 关键：如果控制面板隐藏，则显示它
        if (controlPanel && !controlPanel->isVisible()) {
            controlPanel->show();
            controlPanel->raise();
        }
        // 重置隐藏定时器（假设你已定义 controlPanelHideTimer）
        if (controlPanel && controlPanel->isVisible()) {
            controlPanelHideTimer->start(2000);   // 2秒后自动隐藏
        }
    }

    // 窗口拖动逻辑（保持不变）
    if (event->buttons() & Qt::LeftButton) {
        move(event->globalPosition().toPoint() - dragPosition);
        event->accept();
    }

    QMainWindow::mouseMoveEvent(event);
}



// 设置A点
void VideoPlayer::setLoopPointA()
{
    if (!player) return;

    qint64 currentPos = player->position();
    loopPointA = currentPos;

    qDebug() << "设置A点:" << formatTime(loopPointA);

    // 如果B点已设置且A点大于B点，自动交换
    if (loopPointB != -1 && loopPointA > loopPointB) {
        qSwap(loopPointA, loopPointB);
        qDebug() << "A点大于B点，已自动交换";
    }

    updateABMenuState();
    updateABInfoDisplay();
}

// 设置B点
void VideoPlayer::setLoopPointB()
{
    if (!player) return;

    qint64 currentPos = player->position();
    loopPointB = currentPos;

    qDebug() << "设置B点:" << formatTime(loopPointB);

    // 如果A点已设置且A点大于B点，自动交换
    if (loopPointA != -1 && loopPointA > loopPointB) {
        qSwap(loopPointA, loopPointB);
        qDebug() << "A点大于B点，已自动交换";
    }

    updateABMenuState();
    updateABInfoDisplay();
}

// 清除AB点
void VideoPlayer::clearLoopPoints()
{
    loopPointA = -1;
    loopPointB = -1;
    isABLoopEnabled = false;

    if (loopABAction) {
        loopABAction->setChecked(false);
    }

    qDebug() << "已清除AB点";
    updateABMenuState();
    updateABInfoDisplay();
}

// 切换AB点循环
void VideoPlayer::toggleABLoop()
{
    isABLoopEnabled = !isABLoopEnabled;

    if (isABLoopEnabled) {
        // 检查是否设置了A点和B点
        if (loopPointA == -1 || loopPointB == -1) {
            qDebug() << "请先设置A点和B点";
            isABLoopEnabled = false;
            if (loopABAction) {
                loopABAction->setChecked(false);
            }
            return;
        }

        qDebug() << "AB点循环已开启: A=" << formatTime(loopPointA)
                 << " B=" << formatTime(loopPointB);

        // 如果当前播放位置不在AB点之间，跳转到A点
        if (player) {
            qint64 currentPos = player->position();
            if (currentPos < loopPointA || currentPos > loopPointB) {
                player->setPosition(loopPointA);
            }
        }
    } else {
        qDebug() << "AB点循环已关闭";
    }

    if (loopABAction) {
        loopABAction->setChecked(isABLoopEnabled);
    }

    updateABMenuState();
    updateABInfoDisplay();
}

// 更新AB点菜单项状态
void VideoPlayer::updateABMenuState()
{
    if (setAAction) {
        setAAction->setEnabled(true);
    }

    if (setBAction) {
        setBAction->setEnabled(true);
    }

    if (clearABAction) {
        clearABAction->setEnabled(loopPointA != -1 || loopPointB != -1);
    }

    if (loopABAction) {
        loopABAction->setEnabled(loopPointA != -1 && loopPointB != -1);
        loopABAction->setChecked(isABLoopEnabled);
    }
}

// 显示AB点信息
void VideoPlayer::showABLoopInfo()
{
    if (loopPointA == -1 && loopPointB == -1) {
        qDebug() << "AB点: 未设置";
        return;
    }

    QString info;
    if (loopPointA != -1) {
        info += "A: " + formatTime(loopPointA);
    }
    if (loopPointB != -1) {
        if (!info.isEmpty()) info += " | ";
        info += "B: " + formatTime(loopPointB);
    }

    qDebug() << "AB点:" << info;

    // 可以在这里更新状态栏或显示临时消息
    if (isABLoopEnabled) {
        qDebug() << "AB点循环已启用";
    }
}

// 更新AB点信息显示
void VideoPlayer::updateABInfoDisplay()
{
    if (!abInfoLabel) return;

    if (loopPointA == -1 && loopPointB == -1) {
        abInfoLabel->setText("AB点: 未设置");
        abInfoLabel->setStyleSheet("color: #FFD700; font: 10px;");
        return;
    }

    QString text;
    if (loopPointA != -1) {
        text += "A:" + formatTime(loopPointA);
    }
    if (loopPointB != -1) {
        if (!text.isEmpty()) text += " ";
        text += "B:" + formatTime(loopPointB);
    }

    if (isABLoopEnabled) {
        text += " [循环]";
        abInfoLabel->setStyleSheet("color: #00FF00; font: 10px; font-weight: bold;");  // 绿色表示启用
    } else {
        abInfoLabel->setStyleSheet("color: #FFD700; font: 10px;");  // 金色表示已设置但未启用
    }

    abInfoLabel->setText(text);
}

void VideoPlayer::exportCurrentFrame()
{
    try {
        qDebug() << "\n=== 开始截图 ===";

        // 确保视频窗口可见
        if (!videoWidget || !videoWidget->isVisible()) {
            QMessageBox::warning(this, "错误", "视频窗口不可见");
            return;
        }

        // 暂停视频以获得稳定帧
        bool wasPlaying = false;
        if (player) {
            wasPlaying = (player->playbackState() == QMediaPlayer::PlayingState);
            if (wasPlaying) {
                player->pause();
                qDebug() << "已暂停视频";

                // 等待视频稳定
                QCoreApplication::processEvents();
                QThread::msleep(300);
            }
        }

        // 主方法：使用屏幕截图
        QImage screenshot = captureScreenArea();

        // 备用方法：如果屏幕截图失败，尝试Qt方法
        if (screenshot.isNull() || screenshot.width() < 100) {
            qDebug() << "屏幕截图失败，尝试Qt截图";
            screenshot = videoWidget->grab().toImage();
        }

        // 如果仍然失败，创建回退图像
        if (screenshot.isNull() || screenshot.width() < 100) {
            qDebug() << "所有截图方法失败";
            screenshot = createFallbackImage(
                videoWidget->width(),
                videoWidget->height(),
                "截图失败：视频可能在硬件加速层渲染"
                );
        }

        // 分析并显示结果
        analyzeAndShowScreenshot(screenshot);

        // 恢复播放
        if (wasPlaying && player) {
            player->play();
            qDebug() << "恢复播放";
        }

        qDebug() << "=== 截图结束 ===\n";
    } catch (const std::exception& e) {
        qDebug() << "截图异常:" << e.what();
        QMessageBox::critical(this, "错误", QString("截图过程中发生异常:\n%1").arg(e.what()));
    }
}

// 新增：显示图像预览
void VideoPlayer::showImagePreview(const QImage &image)
{
    if (image.isNull()) {
        QMessageBox::information(this, "提示", "图像为空");
        return;
    }

    QDialog dialog(this);
    dialog.setWindowTitle("截图预览");
    dialog.resize(700, 600);

    QVBoxLayout *layout = new QVBoxLayout(&dialog);

    // 标签显示图像
    QLabel *imageLabel = new QLabel(&dialog);
    QPixmap pixmap = QPixmap::fromImage(image);

    // 如果图像太大，缩放显示
    if (pixmap.width() > 600 || pixmap.height() > 400) {
        pixmap = pixmap.scaled(600, 400, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }

    imageLabel->setPixmap(pixmap);
    imageLabel->setAlignment(Qt::AlignCenter);

    // 信息标签
    QLabel *infoLabel = new QLabel(&dialog);
    infoLabel->setText(QString(
                           "<b>图像信息：</b><br>"
                           "尺寸：%1 × %2 像素<br>"
                           "格式：%3<br>"
                           "深度：%4 位")
                           .arg(image.width())
                           .arg(image.height())
                           .arg(imageFormatToString(image.format()))
                           .arg(image.depth()));
    infoLabel->setWordWrap(true);

    // 像素采样信息
    QLabel *pixelLabel = new QLabel(&dialog);
    if (!image.isNull() && image.width() > 0 && image.height() > 0) {
        QRgb centerPixel = image.pixel(image.width()/2, image.height()/2);
        pixelLabel->setText(QString(
                                "<b>中心像素采样：</b><br>"
                                "RGB: (%1, %2, %3)<br>"
                                "十六进制: #%4")
                                .arg(qRed(centerPixel))
                                .arg(qGreen(centerPixel))
                                .arg(qBlue(centerPixel))
                                .arg(QString::number(centerPixel, 16).right(6).toUpper()));
    }

    // 按钮布局
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    QPushButton *saveButton = new QPushButton("保存图像", &dialog);
    QPushButton *analyzeButton = new QPushButton("分析图像", &dialog);
    QPushButton *closeButton = new QPushButton("关闭", &dialog);

    buttonLayout->addWidget(saveButton);
    buttonLayout->addWidget(analyzeButton);
    buttonLayout->addWidget(closeButton);

    // 添加到主布局
    layout->addWidget(imageLabel);
    layout->addWidget(infoLabel);
    layout->addWidget(pixelLabel);
    layout->addLayout(buttonLayout);

    // 连接信号
    connect(saveButton, &QPushButton::clicked, [&]() {
        QString fileName = QFileDialog::getSaveFileName(
            &dialog, "保存截图",
            QStandardPaths::writableLocation(QStandardPaths::PicturesLocation) +
                "/screenshot_" + QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss") + ".png",
            "PNG 图像 (*.png);;JPEG 图像 (*.jpg);;BMP 图像 (*.bmp)");

        if (!fileName.isEmpty()) {
            if (image.save(fileName)) {
                QMessageBox::information(&dialog, "成功",
                                         QString("图像已保存到:\n%1").arg(fileName));
            } else {
                QMessageBox::warning(&dialog, "错误", "保存失败");
            }
        }
    });

    connect(analyzeButton, &QPushButton::clicked, [&]() {
        analyzeImage(image);
    });

    connect(closeButton, &QPushButton::clicked, &dialog, &QDialog::accept);

    dialog.exec();
}

// 新增：分析图像内容
void VideoPlayer::analyzeImage(const QImage &image)
{
    if (image.isNull()) {
        QMessageBox::information(this, "分析结果", "图像为空");
        return;
    }

    QString analysis = QString(
                           "<b>图像分析报告：</b><br><br>"
                           "尺寸：%1 × %2 像素<br>"
                           "格式：%3<br>"
                           "深度：%4 位<br><br>"
                           "<b>像素采样：</b><br>")
                           .arg(image.width())
                           .arg(image.height())
                           .arg(imageFormatToString(image.format()))
                           .arg(image.depth());

    // 采样多个点
    int points[5][2] = {{10, 10}, {image.width()/2, 10},
                        {image.width()/2, image.height()/2},
                        {image.width()-10, image.height()/2},
                        {image.width()/2, image.height()-10}};

    for (int i = 0; i < 5; i++) {
        int x = points[i][0];
        int y = points[i][1];

        if (x >= 0 && x < image.width() && y >= 0 && y < image.height()) {
            QRgb pixel = image.pixel(x, y);
            analysis += QString("位置(%1,%2): RGB(%3,%4,%5)<br>")
                            .arg(x).arg(y)
                            .arg(qRed(pixel)).arg(qGreen(pixel)).arg(qBlue(pixel));
        }
    }

    // 检查图像是否基本为黑色
    int darkCount = 0;
    for (int i = 0; i < 20; i++) {
        int x = std::rand() % image.width();
        int y = std::rand() % image.height();
        QRgb pixel = image.pixel(x, y);
        if (qRed(pixel) < 30 && qGreen(pixel) < 30 && qBlue(pixel) < 30) {
            darkCount++;
        }
    }

    analysis += QString("<br><b>黑色检测：</b>%1/20 个采样点为黑色<br>").arg(darkCount);

    QMessageBox::information(this, "图像分析", analysis);
}

QImage VideoPlayer::videoFrameToImage(const QVideoFrame &frame)
{
    if (!frame.isValid()) {
        qDebug() << "视频帧无效";
        return QImage();
    }

    QVideoFrame videoFrame = frame;

    // 记录帧格式信息
    QVideoFrameFormat format = videoFrame.surfaceFormat();
    qDebug() << "视频帧格式:" << format.pixelFormat();
    qDebug() << "视频帧尺寸:" << format.frameWidth() << "x" << format.frameHeight();
    qDebug() << "平面数量:" << format.planeCount();

    // 尝试直接使用Qt的toImage()方法（对某些格式有效）
    QImage image = videoFrame.toImage();
    if (!image.isNull()) {
        qDebug() << "使用toImage()成功获取图像";
        return image;
    }

    qDebug() << "toImage()失败，尝试手动映射";

    // 尝试映射到系统内存
    if (!videoFrame.map(QVideoFrame::ReadOnly)) {
        qDebug() << "无法映射视频帧到内存";

        // 如果是硬件加速的帧，尝试复制到可映射的帧
        QVideoFrame copyFrame(frame);
        if (copyFrame.map(QVideoFrame::ReadOnly)) {
            qDebug() << "通过复制帧成功映射";
            // 处理复制的帧...
            QImage result = convertVideoFrameManually(copyFrame);
            copyFrame.unmap();
            return result;
        }

        return QImage();
    }

    // 根据不同的像素格式处理
    QVideoFrameFormat::PixelFormat pixelFormat = format.pixelFormat();

    qDebug() << "像素格式枚举值:" << static_cast<int>(pixelFormat);
    qDebug() << "格式名称:" << format.pixelFormat();

    // 处理硬件解码的常见格式
    QImage resultImage;
    switch (pixelFormat) {
    case QVideoFrameFormat::Format_NV12:
    case QVideoFrameFormat::Format_NV21:
    case QVideoFrameFormat::Format_YUV420P:
    case QVideoFrameFormat::Format_YUV422P:
        // YUV格式 - 可能需要特殊处理
        resultImage = convertYUVtoRGB(videoFrame, pixelFormat);
        break;

    case QVideoFrameFormat::Format_RGBX8888:
    case QVideoFrameFormat::Format_ARGB8888:
    case QVideoFrameFormat::Format_BGRX8888:
    case QVideoFrameFormat::Format_BGRA8888:
    case QVideoFrameFormat::Format_ABGR8888:
        // RGB格式 - 可以直接创建图像
        resultImage = QImage(videoFrame.bits(0),
                             format.frameWidth(),
                             format.frameHeight(),
                             videoFrame.bytesPerLine(0),
                             QVideoFrameFormat::imageFormatFromPixelFormat(pixelFormat)).copy();
        break;

    default:
        // 未知格式，尝试通用方法
        resultImage = convertVideoFrameManually(videoFrame);
        break;
    }

    videoFrame.unmap();

    // 如果图像有问题，应用修复
    if (!resultImage.isNull() && hasCorruption(resultImage)) {
        resultImage = fixCorruptedImage(resultImage);
    }

    return resultImage;
}

// 从QVideoSink获取视频帧（Qt 6.4可用）
QImage VideoPlayer::getVideoFrameFromSink()
{
    // 如果还没有初始化videoSink，先初始化
    if (!videoSink) {
        setupVideoSink();
    }

    if (!videoSink) {
        qDebug() << "videoSink未初始化";
        return QImage();
    }

    // 获取当前视频帧
    QVideoFrame frame = videoSink->videoFrame();
    if (!frame.isValid()) {
        qDebug() << "视频帧无效";
        return QImage();
    }

    // 使用videoFrameToImage函数处理视频帧
    QImage image = videoFrameToImage(frame);

    if (image.isNull()) {
        qDebug() << "无法从videoSink获取图像";
    }

    return image;
}

// 手动转换视频帧（处理常见格式）
QImage VideoPlayer::convertVideoFrameManually(const QVideoFrame &frame)
{
    QVideoFrame videoFrame = frame;

    if (!videoFrame.map(QVideoFrame::ReadOnly)) {
        return QImage();
    }

    QVideoFrameFormat format = videoFrame.surfaceFormat();
    int width = format.frameWidth();
    int height = format.frameHeight();
    QVideoFrameFormat::PixelFormat pixelFormat = format.pixelFormat();

    QImage image;

    // 处理常见的像素格式 - 使用 Qt 6 正确的枚举值
    switch (pixelFormat) {
    case QVideoFrameFormat::Format_YUV420P:
    case QVideoFrameFormat::Format_YUV422P:
    case QVideoFrameFormat::Format_NV12:
    case QVideoFrameFormat::Format_NV21:
        // YUV格式 - 需要转换为RGB
        image = convertYUVtoRGB(videoFrame, pixelFormat);
        break;

    case QVideoFrameFormat::Format_ARGB8888:
    case QVideoFrameFormat::Format_ARGB8888_Premultiplied:
        // ARGB格式
        image = QImage(
                    videoFrame.bits(0),
                    width,
                    height,
                    videoFrame.bytesPerLine(0),
                    QImage::Format_ARGB32
                    ).copy();
        break;

    case QVideoFrameFormat::Format_XRGB8888:
        // XRGB格式
        image = QImage(
                    videoFrame.bits(0),
                    width,
                    height,
                    videoFrame.bytesPerLine(0),
                    QImage::Format_RGB32
                    ).copy();
        break;

    case QVideoFrameFormat::Format_BGRA8888:
        // BGRA格式
        image = QImage(
                    videoFrame.bits(0),
                    width,
                    height,
                    videoFrame.bytesPerLine(0),
                    QImage::Format_ARGB32
                    ).copy();
        // BGRA需要转换为RGBA
        if (!image.isNull()) {
            image = image.rgbSwapped();
        }
        break;

    case QVideoFrameFormat::Format_BGRX8888:
        // BGRX格式
        image = QImage(
                    videoFrame.bits(0),
                    width,
                    height,
                    videoFrame.bytesPerLine(0),
                    QImage::Format_RGB32
                    ).copy();
        // BGRX需要转换为RGBX
        if (!image.isNull()) {
            image = image.rgbSwapped();
        }
        break;

    case QVideoFrameFormat::Format_RGBX8888:
    case QVideoFrameFormat::Format_RGBA8888:
        // RGBX或RGBA格式
        image = QImage(
                    videoFrame.bits(0),
                    width,
                    height,
                    videoFrame.bytesPerLine(0),
                    QImage::Format_RGBA8888
                    ).copy();
        break;

    case QVideoFrameFormat::Format_ABGR8888:
        // ABGR格式（较少见）
        image = QImage(
                    videoFrame.bits(0),
                    width,
                    height,
                    videoFrame.bytesPerLine(0),
                    QImage::Format_ARGB32
                    ).copy();
        if (!image.isNull()) {
            // 转换ABGR到ARGB
            image = image.rgbSwapped();
        }
        break;

    case QVideoFrameFormat::Format_Y8:
        // 8位灰度
        image = QImage(
                    videoFrame.bits(0),
                    width,
                    height,
                    videoFrame.bytesPerLine(0),
                    QImage::Format_Grayscale8
                    ).copy();
        break;

    case QVideoFrameFormat::Format_Y16:
        // 16位灰度
        image = QImage(
                    videoFrame.bits(0),
                    width,
                    height,
                    videoFrame.bytesPerLine(0),
                    QImage::Format_Grayscale16
                    ).copy();
        break;

    case QVideoFrameFormat::Format_P010:
    case QVideoFrameFormat::Format_P016:
        // 10位或16位YUV格式
        qDebug() << "高精度YUV格式，需要特殊处理:" << pixelFormat;
        image = QImage(width, height, QImage::Format_RGB32);
        image.fill(Qt::gray);
        break;

    default:
        qDebug() << "不支持的像素格式:" << pixelFormat;
        // 尝试使用Qt的toImage方法
        image = videoFrame.toImage();
        if (image.isNull()) {
            // 创建空图像
            image = QImage(width, height, QImage::Format_RGB32);
            image.fill(Qt::black);
        }
        break;
    }

    videoFrame.unmap();
    return image;
}

// 转换YUV到RGB（简化版本）
QImage VideoPlayer::convertYUVtoRGB(const QVideoFrame &frame, QVideoFrameFormat::PixelFormat format)
{
    QVideoFrame videoFrame = frame;

    if (!videoFrame.map(QVideoFrame::ReadOnly)) {
        qDebug() << "无法映射YUV帧";
        return QImage();
    }

    int width = frame.surfaceFormat().frameWidth();
    int height = frame.surfaceFormat().frameHeight();

    // 创建输出图像
    QImage rgbImage(width, height, QImage::Format_RGB32);
    rgbImage.fill(Qt::black);

    try {
        // 根据格式使用不同的转换矩阵
        if (format == QVideoFrameFormat::Format_NV12) {
            // 使用更精确的转换系数
            convertNV12ToRGB(videoFrame, rgbImage);
        } else if (format == QVideoFrameFormat::Format_YUV420P) {
            convertYUV420PToRGB(videoFrame, rgbImage);
        } else {
            // 通用转换
            convertGenericYUVToRGB(videoFrame, rgbImage, format);
        }
    } catch (const std::exception& e) {
        qDebug() << "YUV转换异常:" << e.what();
        videoFrame.unmap();
        return createFallbackImage(width, height, "YUV转换失败");
    }

    videoFrame.unmap();
    return rgbImage;
}

// 捕获窗口截图（备用方法）
QImage VideoPlayer::captureWindowScreenshot()
{
    // 获取当前窗口的位置和大小
    QRect windowRect = this->geometry();

    // 获取主屏幕
    QScreen *screen = QGuiApplication::primaryScreen();
    if (!screen) {
        return QImage();
    }

    // 截取整个屏幕
    QPixmap screenPixmap = screen->grabWindow(0);

    // 截取窗口区域
    QPixmap windowPixmap = screenPixmap.copy(windowRect);

    return windowPixmap.toImage();
}

// 方法1：直接使用 Qt 的系统日志查看后端信息
void VideoPlayer::showBackendInfo() {
    // Qt 6 中检查媒体播放器状态的方式
    qDebug() << "播放器状态:" << (player ? player->playbackState() : -1);
    qDebug() << "是否有音频输出:" << (audioOutput ? "是" : "否");
    qDebug() << "是否有视频输出:" << (videoWidget ? "是" : "否");

    // 检查媒体源信息
    if (player && player->source().isValid()) {
        qDebug() << "当前媒体源:" << player->source();
        qDebug() << "媒体时长:" << player->duration() << "ms";
    }
}

void VideoPlayer::convertNV12ToRGB(const QVideoFrame &frame, QImage &rgbImage)
{
    const uchar* yPlane = frame.bits(0);
    const uchar* uvPlane = frame.bits(1);

    int yStride = frame.bytesPerLine(0);
    int uvStride = frame.bytesPerLine(1);

    int width = rgbImage.width();
    int height = rgbImage.height();

    for (int y = 0; y < height; ++y) {
        const uchar* yLine = yPlane + y * yStride;
        const uchar* uvLine = uvPlane + (y / 2) * uvStride;

        QRgb* rgbLine = reinterpret_cast<QRgb*>(rgbImage.scanLine(y));

        for (int x = 0; x < width; ++x) {
            int Y = yLine[x];
            int U = uvLine[(x / 2) * 2] - 128;
            int V = uvLine[(x / 2) * 2 + 1] - 128;

            // YUV to RGB 转换公式
            int R = qBound(0, Y + ((1436 * V) >> 10), 255);
            int G = qBound(0, Y - ((352 * U + 731 * V) >> 10), 255);
            int B = qBound(0, Y + ((1814 * U) >> 10), 255);

            rgbLine[x] = qRgb(R, G, B);
        }
    }
}

void VideoPlayer::convertYUV420PToRGB(const QVideoFrame &frame, QImage &rgbImage)
{
    const uchar* yPlane = frame.bits(0);
    const uchar* uPlane = frame.bits(1);
    const uchar* vPlane = frame.bits(2);

    int yStride = frame.bytesPerLine(0);
    int uStride = frame.bytesPerLine(1);
    int vStride = frame.bytesPerLine(2);

    int width = rgbImage.width();
    int height = rgbImage.height();

    for (int y = 0; y < height; ++y) {
        const uchar* yLine = yPlane + y * yStride;
        const uchar* uLine = uPlane + (y / 2) * uStride;
        const uchar* vLine = vPlane + (y / 2) * vStride;

        QRgb* rgbLine = reinterpret_cast<QRgb*>(rgbImage.scanLine(y));

        for (int x = 0; x < width; ++x) {
            int Y = yLine[x];
            int U = uLine[x / 2] - 128;
            int V = vLine[x / 2] - 128;

            // YUV to RGB 转换公式
            int R = qBound(0, Y + ((1436 * V) >> 10), 255);
            int G = qBound(0, Y - ((352 * U + 731 * V) >> 10), 255);
            int B = qBound(0, Y + ((1814 * U) >> 10), 255);

            rgbLine[x] = qRgb(R, G, B);
        }
    }
}

void VideoPlayer::convertGenericYUVToRGB(const QVideoFrame &frame, QImage &rgbImage, QVideoFrameFormat::PixelFormat format)
{
    // 简化处理：只显示Y分量（灰度）
    const uchar* yPlane = frame.bits(0);
    int yStride = frame.bytesPerLine(0);

    int width = rgbImage.width();
    int height = rgbImage.height();

    for (int y = 0; y < height; ++y) {
        const uchar* yLine = yPlane + y * yStride;
        QRgb* rgbLine = reinterpret_cast<QRgb*>(rgbImage.scanLine(y));

        for (int x = 0; x < width; ++x) {
            int Y = yLine[x];
            rgbLine[x] = qRgb(Y, Y, Y);
        }
    }
}

// 检查图像是否有损坏
bool VideoPlayer::hasCorruption(const QImage &image)
{
    if (image.isNull()) return true;

    // 简单的检查：确保图像有合理的尺寸
    if (image.width() <= 0 || image.height() <= 0) return true;

    // 检查几个随机像素点是否在有效范围内
    for (int i = 0; i < 10; ++i) {
        // 使用 std::rand() 替代 qrand()
        int x = std::rand() % image.width();
        int y = std::rand() % image.height();

        if (x >= 0 && x < image.width() && y >= 0 && y < image.height()) {
            QRgb pixel = image.pixel(x, y);
            // 检查RGB值是否在有效范围内
            int r = qRed(pixel);
            int g = qGreen(pixel);
            int b = qBlue(pixel);

            if (r < 0 || r > 255 || g < 0 || g > 255 || b < 0 || b > 255) {
                return true;
            }
        }
    }

    return false;
}

// 修复损坏的图像
QImage VideoPlayer::fixCorruptedImage(const QImage &image)
{
    if (image.isNull()) {
        return createFallbackImage(640, 480, "图像为空");
    }

    int width = image.width();
    int height = image.height();

    if (width <= 0 || height <= 0) {
        return createFallbackImage(640, 480, "图像尺寸无效");
    }

    // 创建新图像
    QImage fixedImage(width, height, QImage::Format_RGB32);

    // 尝试复制有效像素，填充无效像素
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            if (x < image.width() && y < image.height()) {
                QRgb pixel = image.pixel(x, y);
                int r = qRed(pixel);
                int g = qGreen(pixel);
                int b = qBlue(pixel);

                // 检查像素是否有效
                if (r >= 0 && r <= 255 && g >= 0 && g <= 255 && b >= 0 && b <= 255) {
                    fixedImage.setPixel(x, y, pixel);
                } else {
                    // 使用相邻像素的平均值
                    fixedImage.setPixel(x, y, qRgb(128, 128, 128));
                }
            } else {
                // 超出原图范围，使用灰色填充
                fixedImage.setPixel(x, y, qRgb(128, 128, 128));
            }
        }
    }

    return fixedImage;
}

// 创建回退图像
QImage VideoPlayer::createFallbackImage(int width, int height, const QString &message)
{
    QImage image(width, height, QImage::Format_RGB32);
    image.fill(QColor(30, 30, 30));

    QPainter painter(&image);

    // 绘制渐变背景
    QLinearGradient gradient(0, 0, width, height);
    gradient.setColorAt(0, QColor(50, 50, 100));
    gradient.setColorAt(1, QColor(100, 50, 50));
    painter.fillRect(image.rect(), gradient);

    // 绘制错误信息
    painter.setPen(Qt::white);
    painter.setFont(QFont("Arial", 14, QFont::Bold));

    QString fullMessage = QString("视频帧获取失败\n%1\n\n请检查视频格式和硬件加速设置").arg(message);
    painter.drawText(image.rect(), Qt::AlignCenter, fullMessage);

    // 绘制边框
    painter.setPen(QPen(Qt::yellow, 2));
    painter.drawRect(10, 10, width - 20, height - 20);

    painter.end();

    return image;
}

// 检查图像是否基本上是黑色的（更宽松的检查）
bool VideoPlayer::isBlackImage(const QImage &image, int threshold)
{
    if (image.isNull() || image.width() <= 0 || image.height() <= 0) {
        return true;
    }

    // 只检查少量像素点，避免性能问题
    int sampleCount = 20;
    int darkPixels = 0;

    // 采样图像中间区域（避免边缘黑边）
    int marginX = image.width() / 10;
    int marginY = image.height() / 10;
    int checkWidth = image.width() - 2 * marginX;
    int checkHeight = image.height() - 2 * marginY;

    if (checkWidth <= 0 || checkHeight <= 0) {
        // 图像太小，直接返回
        return false;
    }

    for (int i = 0; i < sampleCount; ++i) {
        int x = marginX + (std::rand() % checkWidth);
        int y = marginY + (std::rand() % checkHeight);

        QRgb pixel = image.pixel(x, y);
        int r = qRed(pixel);
        int g = qGreen(pixel);
        int b = qBlue(pixel);

        // 如果RGB值都很低，认为是黑色
        if (r < threshold && g < threshold && b < threshold) {
            darkPixels++;
        }
    }

    // 如果超过70%的采样点是黑色的，认为是黑色图像
    // 这个阈值可以调高一些，避免误判
    return (darkPixels * 100 / sampleCount) > 70;
}

QImage VideoPlayer::getCurrentVideoFrame()
{
    // 尝试从QVideoSink获取
    if (!videoSink) {
        setupVideoSink();
    }

    if (videoSink) {
        QVideoFrame frame = videoSink->videoFrame();
        if (frame.isValid()) {
            QImage image = videoFrameToImage(frame);
            if (!image.isNull() && !isBlackImage(image, 30)) {
                return image;
            }
        }
    }

    // 尝试直接截图视频窗口（不同方法）
    if (videoWidget) {
        // 方法1：使用grab
        QPixmap pixmap = videoWidget->grab();
        if (!pixmap.isNull()) {
            QImage image = pixmap.toImage();
            if (!isBlackImage(image, 30)) {
                return image;
            }
        }

        // 方法2：使用render
        QPixmap pixmap2(videoWidget->size());
        videoWidget->render(&pixmap2);
        QImage image = pixmap2.toImage();
        if (!isBlackImage(image, 30)) {
            return image;
        }
    }

    return QImage();
}

// 创建测试图像
QImage VideoPlayer::createTestImage()
{
    int width = 800;
    int height = 600;

    QImage image(width, height, QImage::Format_RGB32);

    // 填充背景
    image.fill(QColor(30, 30, 30));

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);

    // 绘制渐变背景
    QLinearGradient gradient(0, 0, width, height);
    gradient.setColorAt(0, QColor(50, 50, 100));
    gradient.setColorAt(0.5, QColor(80, 50, 80));
    gradient.setColorAt(1, QColor(100, 50, 50));
    painter.fillRect(image.rect(), gradient);

    // 绘制标题
    painter.setPen(QPen(Qt::white, 3));
    painter.setFont(QFont("Arial", 28, QFont::Bold));
    painter.drawText(QRect(0, 50, width, 60), Qt::AlignCenter, "视频截图预览");

    // 绘制边框
    painter.setPen(QPen(QColor(255, 215, 0), 4));  // 金色边框
    painter.drawRect(20, 20, width - 40, height - 40);

    // 绘制信息区域 - 修正播放状态判断
    painter.setPen(Qt::white);
    painter.setFont(QFont("Arial", 14));

    QString playerState = "未知状态";
    if (player) {
        switch (player->playbackState()) {
        case QMediaPlayer::PlayingState:
            playerState = "播放中";
            break;
        case QMediaPlayer::PausedState:
            playerState = "已暂停";
            break;
        case QMediaPlayer::StoppedState:
            playerState = "已停止";
            break;
        default:
            playerState = "未知";
            break;
        }
    } else {
        playerState = "无播放器";
    }

    QString infoText = QString(
                           "截图信息：\n"
                           "时间：%1\n"
                           "状态：%2\n"
                           "视频窗口：%3x%4\n"
                           "备注：如果看到此图像，表示视频截图功能\n"
                           "需要进一步调试或视频可能处于未播放状态")
                           .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss"))
                           .arg(playerState)
                           .arg(videoWidget ? videoWidget->width() : 0)
                           .arg(videoWidget ? videoWidget->height() : 0);

    painter.drawText(QRect(50, 150, width - 100, 250), Qt::AlignCenter | Qt::TextWordWrap, infoText);

    // 绘制一个简单的视频帧示意图
    QRect frameRect(width/4, height - 200, width/2, 150);
    painter.fillRect(frameRect, QColor(0, 0, 0, 200));

    // 在示意图中绘制一些内容
    painter.setPen(QPen(Qt::green, 2));
    for (int i = 0; i < 5; i++) {
        int x = frameRect.left() + i * (frameRect.width() / 4);
        painter.drawLine(x, frameRect.top(), x, frameRect.bottom());
    }

    painter.setPen(QPen(Qt::red, 2));
    painter.drawEllipse(frameRect.center(), 20, 20);

    painter.setPen(QPen(Qt::blue, 2));
    painter.drawText(frameRect, Qt::AlignCenter, "视频帧示意图");

    painter.end();

    qDebug() << "创建测试图像完成，尺寸:" << image.size();
    return image;
}

QImage VideoPlayer::captureScreenArea()
{
    if (!videoWidget) return QImage();

    qDebug() << "使用屏幕截图...";

    // 获取视频窗口的全局位置
    QPoint globalPos = videoWidget->mapToGlobal(QPoint(0, 0));
    QRect widgetRect(globalPos.x(), globalPos.y(),
                     videoWidget->width(), videoWidget->height());

    qDebug() << "窗口全局位置:" << widgetRect;

    // 截取整个屏幕
    QScreen *screen = QGuiApplication::primaryScreen();
    if (!screen) {
        qDebug() << "无法获取屏幕";
        return QImage();
    }

    // 截取窗口区域
    QPixmap screenPixmap = screen->grabWindow(0,
                                              widgetRect.x(), widgetRect.y(),
                                              widgetRect.width(), widgetRect.height());

    QImage result = screenPixmap.toImage();

    // 保存调试
    if (!result.isNull()) {
        QString path = QDir::tempPath() + "/screenshot_" +
                       QDateTime::currentDateTime().toString("hhmmss") + ".png";
        result.save(path);
        qDebug() << "屏幕截图保存到:" << path;
    }

    return result;
}

QImage VideoPlayer::captureWithWorkaround()
{
    qDebug() << "尝试硬件加速绕过方法...";

    // 方法A：禁用硬件加速（临时）
    qputenv("QT_MEDIA_DISABLE_HW_DECODING", "1");
    qDebug() << "临时禁用硬件加速";

    // 重新创建视频输出
    if (player) {
        player->setVideoOutput(nullptr);
    }
    QThread::msleep(100);

    QVideoWidget *tempWidget = new QVideoWidget(this);
    tempWidget->setAttribute(Qt::WA_OpaquePaintEvent, false);
    tempWidget->setAttribute(Qt::WA_NoSystemBackground, false);

    if (player) {
        player->setVideoOutput(tempWidget);
    }

    QCoreApplication::processEvents();
    QThread::msleep(200);

    // 截图
    QImage result = tempWidget->grab().toImage();

    // 恢复原来的视频窗口
    if (player) {
        player->setVideoOutput(videoWidget);
    }
    delete tempWidget;

    // 恢复硬件加速设置
    qunsetenv("QT_MEDIA_DISABLE_HW_DECODING");

    return result;
}

QImage VideoPlayer::tryMultipleCaptureMethods()
{
    QImage bestImage;
    int bestScore = 0;

    qDebug() << "尝试多种截图方法...";

    // 方法1：标准grab()
    if (videoWidget) {
        QPixmap pixmap = videoWidget->grab();
        if (!pixmap.isNull()) {
            QImage img = pixmap.toImage();
            int score = evaluateImageQuality(img);
            qDebug() << "方法1(grab): 尺寸" << img.size() << "评分" << score;
            if (score > bestScore) {
                bestImage = img;
                bestScore = score;
            }
        }
    }

    // 方法2：render()到pixmap
    if (videoWidget) {
        QPixmap pixmap(videoWidget->size());
        pixmap.fill(Qt::transparent);
        videoWidget->render(&pixmap);
        QImage img = pixmap.toImage();
        int score = evaluateImageQuality(img);
        qDebug() << "方法2(render): 尺寸" << img.size() << "评分" << score;
        if (score > bestScore) {
            bestImage = img;
            bestScore = score;
        }
    }

    // 方法3：grab()带矩形参数
    if (videoWidget) {
        QRect grabRect = videoWidget->rect();
        grabRect.adjust(1, 1, -1, -1);  // 稍微缩小一点
        QPixmap pixmap = videoWidget->grab(grabRect);
        if (!pixmap.isNull()) {
            QImage img = pixmap.toImage();
            int score = evaluateImageQuality(img);
            qDebug() << "方法3(grab with rect): 尺寸" << img.size() << "评分" << score;
            if (score > bestScore) {
                bestImage = img;
                bestScore = score;
            }
        }
    }

    // 方法4：使用屏幕截图（最后的手段）
    if (bestScore < 10) {  // 如果其他方法都不好
        qDebug() << "尝试屏幕截图方法";
        bestImage = captureScreenArea();
    }

    qDebug() << "最佳图像评分:" << bestScore;
    return bestImage;
}

int VideoPlayer::evaluateImageQuality(const QImage &image)
{
    if (image.isNull()) return 0;

    int score = 0;

    // 尺寸得分
    int sizeScore = qMin(image.width() * image.height() / 10000, 50);
    score += sizeScore;

    // 非黑色像素得分
    int nonBlackPixels = 0;
    int samplePoints = 20;

    for (int i = 0; i < samplePoints; i++) {
        int x = std::rand() % image.width();
        int y = std::rand() % image.height();
        QRgb pixel = image.pixel(x, y);

        if (qRed(pixel) > 10 || qGreen(pixel) > 10 || qBlue(pixel) > 10) {
            nonBlackPixels++;
        }
    }

    score += (nonBlackPixels * 50 / samplePoints);

    return score;
}

void VideoPlayer::forceVideoRender()
{
    if (!videoWidget) return;

    qDebug() << "强制视频渲染...";

    // 方法1：触发多个重绘事件
    videoWidget->update();
    videoWidget->repaint();

    // 方法2：发送多个重绘请求
    for (int i = 0; i < 3; i++) {
        QEvent paintEvent(QEvent::Paint);
        QApplication::sendEvent(videoWidget, &paintEvent);
        QCoreApplication::processEvents();
        QThread::msleep(50);
    }

    // 方法3：改变窗口属性（可能触发重绘）
    bool wasVisible = videoWidget->isVisible();
    if (!wasVisible) {
        videoWidget->show();
        QCoreApplication::processEvents();
    }

    // 方法4：调整大小（最后再恢复）
    QSize originalSize = videoWidget->size();
    videoWidget->resize(originalSize.width() + 1, originalSize.height());
    QCoreApplication::processEvents();
    QThread::msleep(50);
    videoWidget->resize(originalSize);
    QCoreApplication::processEvents();

    qDebug() << "强制渲染完成";
}

void VideoPlayer::analyzeAndShowScreenshot(const QImage &screenshot)
{
    if (screenshot.isNull()) {
        qDebug() << "截图为空，无法分析";
        return;
    }

    // 创建分析对话框
    QDialog analysisDialog(this);
    analysisDialog.setWindowTitle("截图分析");
    analysisDialog.resize(600, 400);

    QVBoxLayout *layout = new QVBoxLayout(&analysisDialog);

    // 显示截图预览
    QLabel *imageLabel = new QLabel(&analysisDialog);
    QPixmap previewPixmap = QPixmap::fromImage(screenshot);
    previewPixmap = previewPixmap.scaled(400, 300, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    imageLabel->setPixmap(previewPixmap);
    imageLabel->setAlignment(Qt::AlignCenter);

    // 创建分析信息文本
    QTextEdit *analysisText = new QTextEdit(&analysisDialog);
    analysisText->setReadOnly(true);
    analysisText->setFont(QFont("Monospace", 10));

    QString analysis;
    analysis += "=== 截图分析报告 ===\n\n";
    analysis += QString("1. 基本信息:\n");
    analysis += QString("   尺寸: %1 × %2 像素\n").arg(screenshot.width()).arg(screenshot.height());
    analysis += QString("   格式: %1\n").arg(imageFormatToString(screenshot.format()));
    analysis += QString("   深度: %1 位\n").arg(screenshot.depth());

    analysis += QString("\n2. 颜色分析:\n");

    // 分析图像颜色
    QHash<QRgb, int> colorHistogram;
    int totalPixels = 0;
    int sampledPixels = 0;

    // 采样分析（避免处理整个大图像）
    int step = qMax(1, screenshot.width() / 100); // 每100像素采样一次
    for (int y = 0; y < screenshot.height(); y += step) {
        for (int x = 0; x < screenshot.width(); x += step) {
            if (x < screenshot.width() && y < screenshot.height()) {
                QRgb pixel = screenshot.pixel(x, y);
                colorHistogram[pixel]++;
                sampledPixels++;
            }
            totalPixels++;
        }
    }

    // 统计主要颜色
    QList<QRgb> topColors;
    for (auto it = colorHistogram.begin(); it != colorHistogram.end(); ++it) {
        topColors.append(it.key());
    }

    // 按出现频率排序
    std::sort(topColors.begin(), topColors.end(), [&colorHistogram](QRgb a, QRgb b) {
        return colorHistogram[a] > colorHistogram[b];
    });

    analysis += QString("   采样像素数: %1\n").arg(sampledPixels);
    analysis += QString("   独特颜色数: %1\n").arg(colorHistogram.size());

    // 显示前5种主要颜色
    analysis += QString("   主要颜色 (前5):\n");
    for (int i = 0; i < qMin(5, topColors.size()); i++) {
        QRgb color = topColors[i];
        int r = qRed(color);
        int g = qGreen(color);
        int b = qBlue(color);
        int count = colorHistogram[color];
        double percentage = (count * 100.0) / sampledPixels;

        analysis += QString("     %1. RGB(%2, %3, %4) - %5% (%6像素)\n")
                        .arg(i + 1)
                        .arg(r).arg(g).arg(b)
                        .arg(QString::number(percentage, 'f', 2))
                        .arg(count);
    }

    analysis += QString("\n3. 图像质量检查:\n");

    // 检查是否全黑
    int blackPixels = 0;
    int whitePixels = 0;
    int corruptPixels = 0;

    for (int i = 0; i < qMin(1000, sampledPixels); i++) {
        QRgb pixel = topColors[qMin(i, topColors.size() - 1)];
        int r = qRed(pixel);
        int g = qGreen(pixel);
        int b = qBlue(pixel);

        if (r == 0 && g == 0 && b == 0) blackPixels++;
        if (r == 255 && g == 255 && b == 255) whitePixels++;

        // 检查马赛克（极端颜色差异）
        if (abs(r - g) > 200 || abs(g - b) > 200 || abs(r - b) > 200) {
            corruptPixels++;
        }
    }

    analysis += QString("   全黑像素: %1\n").arg(blackPixels);
    analysis += QString("   全白像素: %1\n").arg(whitePixels);
    analysis += QString("   异常像素: %1\n").arg(corruptPixels);

    // 质量评估
    QString quality;
    if (blackPixels > sampledPixels * 0.8) {
        quality = "❌ 质量差（可能全黑）";
    } else if (whitePixels > sampledPixels * 0.8) {
        quality = "❌ 质量差（可能全白）";
    } else if (corruptPixels > sampledPixels * 0.3) {
        quality = "⚠️ 质量一般（可能有马赛克）";
    } else {
        quality = "✅ 质量良好";
    }

    analysis += QString("   质量评估: %1\n").arg(quality);

    analysis += QString("\n4. 建议:\n");
    if (quality.contains("❌")) {
        analysis += QString("   • 尝试使用软件解码模式截图\n");
        analysis += QString("   • 检查视频是否正在播放\n");
        analysis += QString("   • 确保视频窗口可见\n");
    } else if (quality.contains("⚠️")) {
        analysis += QString("   • 截图可能有马赛克，建议使用软件解码\n");
        analysis += QString("   • 尝试暂停视频后截图\n");
    } else {
        analysis += QString("   • 截图质量良好\n");
        analysis += QString("   • 可以继续使用当前设置\n");
    }

    analysisText->setText(analysis);

    // 添加按钮
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    QPushButton *saveButton = new QPushButton("保存截图", &analysisDialog);
    QPushButton *retryButton = new QPushButton("重新截图", &analysisDialog);
    QPushButton *closeButton = new QPushButton("关闭", &analysisDialog);

    connect(saveButton, &QPushButton::clicked, [this, &screenshot, &analysisDialog]() {
        QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss");
        QString defaultFileName = QString("screenshot_analysis_%1.png").arg(timestamp);

        QString fileName = QFileDialog::getSaveFileName(
            &analysisDialog,
            "保存截图",
            QStandardPaths::writableLocation(QStandardPaths::PicturesLocation) + "/" + defaultFileName,
            "PNG 图像 (*.png);;JPEG 图像 (*.jpg);;BMP 图像 (*.bmp)"
            );

        if (!fileName.isEmpty()) {
            if (screenshot.save(fileName)) {
                QMessageBox::information(&analysisDialog, "成功", QString("截图已保存到:\n%1").arg(fileName));
            } else {
                QMessageBox::warning(&analysisDialog, "错误", "保存失败");
            }
        }
    });

    connect(retryButton, &QPushButton::clicked, [&analysisDialog]() {
        analysisDialog.reject(); // 关闭对话框，让主程序重新截图
    });

    connect(closeButton, &QPushButton::clicked, &analysisDialog, &QDialog::accept);

    buttonLayout->addWidget(saveButton);
    buttonLayout->addWidget(retryButton);
    buttonLayout->addWidget(closeButton);

    // 添加到布局
    layout->addWidget(imageLabel);
    layout->addWidget(analysisText);
    layout->addLayout(buttonLayout);

    // 显示对话框
    analysisDialog.exec();
}

// 辅助函数：将QImage格式转换为字符串
QString VideoPlayer::imageFormatToString(QImage::Format format)
{
    switch (format) {
    case QImage::Format_Invalid:
        return "Invalid";
    case QImage::Format_Mono:
        return "Mono";
    case QImage::Format_MonoLSB:
        return "MonoLSB";
    case QImage::Format_Indexed8:
        return "Indexed8";
    case QImage::Format_RGB32:
        return "RGB32";
    case QImage::Format_ARGB32:
        return "ARGB32";
    case QImage::Format_ARGB32_Premultiplied:
        return "ARGB32_Premultiplied";
    case QImage::Format_RGB16:
        return "RGB16";
    case QImage::Format_RGB555:
        return "RGB555";
    case QImage::Format_RGB888:
        return "RGB888";
    case QImage::Format_RGBX8888:
        return "RGBX8888";
    case QImage::Format_RGBA8888:
        return "RGBA8888";
    case QImage::Format_RGBA8888_Premultiplied:
        return "RGBA8888_Premultiplied";
    case QImage::Format_Grayscale8:
        return "Grayscale8";
    case QImage::Format_Grayscale16:
        return "Grayscale16";
    default:
        return QString("Unknown (%1)").arg(static_cast<int>(format));
    }
}


void VideoPlayer::setupSpeedControls()
{
    if (!controlPanel) return;

    // 在控制面板的布局中找到合适位置添加速度控制
    QHBoxLayout *controlLayout = qobject_cast<QHBoxLayout*>(controlPanel->layout());
    if (!controlLayout) return;

    // 创建速度标签
    speedLabel = new QLabel("速度: 1.0x", this);
    speedLabel->setStyleSheet("color: white; font: 12px;");
    speedLabel->setFixedWidth(60);

    // 创建速度滑块
    speedSlider = new QSlider(Qt::Horizontal, this);
    speedSlider->setRange(10, 1000);  // 10% 到 10000%，//即0.10x到10.0x
    speedSlider->setValue(100);      // 默认100%，即1.0x
    speedSlider->setFixedWidth(35);

    // 设置滑块样式
    speedSlider->setStyleSheet(R"(
        QSlider::groove:horizontal {
            height: 12px;/* 修改速度滑块 */
            background: #555;
            border-radius: 3px;
        }
        QSlider::sub-page:horizontal {
            background: #FFA500;  /* 橙色进度条 */
            border-radius: 6px;
        }
        QSlider::add-page:horizontal {
            background: #555;
            border-radius: 6px;
        }
        QSlider::handle:horizontal {
            background: white;
            width: 12px;
            height: 12px;
            margin: -3px 0;
            border-radius: 6px;
        }
        QSlider::handle:horizontal:hover {
            background: #f0f0f0;
            width: 14px;
            height: 14px;
            margin: -4px 0;
            border-radius: 7px;
        }
    )");

    // 为速度滑块也设置最小高度
    speedSlider->setMinimumHeight(24);

    // 创建速度按钮（可选，用于快速预设）
    speedButton = new QToolButton(this);
    speedButton->setText("速度");
    speedButton->setStyleSheet("background-color: transparent; color: white; border: 1px solid #666;");
    speedButton->setFixedWidth(50);

    // 创建速度预设菜单
    speedMenu = new QMenu(this);
    for (float rate : speedPresets) {
        QAction *action = speedMenu->addAction(QString("%1x").arg(rate, 0, 'f', 2));
        connect(action, &QAction::triggered, this, [this, rate]() {
            setPlaybackRatePreset(rate);
        });
    }
    speedMenu->addSeparator();
    QAction *resetAction = speedMenu->addAction("重置为正常速度 (1.0x)");
    connect(resetAction, &QAction::triggered, this, &VideoPlayer::resetPlaybackRate);

    speedButton->setMenu(speedMenu);
    speedButton->setPopupMode(QToolButton::InstantPopup);

    // 连接滑块信号
    speedSlider->setFocusPolicy(Qt::NoFocus);

    // 在 speedSlider 创建后，添加释放连接
    connect(speedSlider, &QSlider::sliderReleased, this, [this]() {
        this->setFocus();
    });
    // 在控制面板布局中插入速度控制
    // 找到合适的位置，比如在音量控制之后，AB点信息之前
    int insertIndex = controlLayout->count() - 2; // 假设AB点信息在最后
    if (insertIndex < 0) insertIndex = controlLayout->count();

    // 创建速度控制容器
    QWidget *speedWidget = new QWidget(this);
    QHBoxLayout *speedLayout = new QHBoxLayout(speedWidget);
    speedLayout->setContentsMargins(0, 0, 0, 0);
    speedLayout->setSpacing(5);
    speedLayout->addWidget(speedLabel);
    speedLayout->addWidget(speedSlider);
    speedLayout->addWidget(speedButton);

    controlLayout->insertWidget(insertIndex, speedWidget);

    // 从设置加载保存的播放速度
    if (settings && settings->contains("playbackRate")) {
        float savedRate = settings->value("playbackRate").toFloat();
        if (savedRate >= 0.01f && savedRate <= 10.0f) {
            setPlaybackRatePreset(savedRate);
        }
    }
}


// 从滑块值设置播放速度
void VideoPlayer::setPlaybackRateFromSlider(int value)
{
    float rate = value / 100.0f;  // 转换为倍率，如100 -> 1.0x
    setPlaybackRate(rate);
    updateSpeedLabel(rate);
    currentPlaybackRate = rate;

    // 保存到设置
    if (settings) {
        settings->setValue("playbackRate", rate);
    }
}

// 设置预设播放速度
void VideoPlayer::setPlaybackRatePreset(float rate)
{
    if (rate < 0.1f || rate > 10.0f) {
        qDebug() << "播放速度超出范围:" << rate;
        return;
    }

    setPlaybackRate(rate);
    updateSpeedLabel(rate);
    currentPlaybackRate = rate;

    // 更新滑块位置
    if (speedSlider) {
        speedSlider->setValue(static_cast<int>(rate * 100));
    }

    // 保存到设置
    if (settings) {
        settings->setValue("playbackRate", rate);
    }

    qDebug() << "设置播放速度:" << rate << "x";
}

// 重置播放速度
void VideoPlayer::resetPlaybackRate()
{
    setPlaybackRatePreset(1.0f);
}

// 更新速度标签
void VideoPlayer::updateSpeedLabel(float rate)
{
    if (!speedLabel) return;

    QString speedText = QString("速度: %1x").arg(rate, 0, 'f', 2);
    speedLabel->setText(speedText);

    // 根据速度值改变标签颜色
    QString color = "white";
    if (rate > 1.0f) {
        color = "#FFA500";  // 橙色表示加速
    } else if (rate < 1.0f) {
        color = "#00BFFF";  // 浅蓝色表示减速
    }

    speedLabel->setStyleSheet(QString("color: %1; font: 12px; font-weight: bold;").arg(color));
}

// 显示/隐藏速度控制
void VideoPlayer::showSpeedControl(bool show)
{
    if (!speedLabel || !speedSlider || !speedButton) return;

    speedLabel->setVisible(show);
    speedSlider->setVisible(show);
    speedButton->setVisible(show);
}

// 切换速度控制显示
void VideoPlayer::toggleSpeedControl()
{
    if (!speedLabel) return;
    showSpeedControl(!speedLabel->isVisible());
}

// 更新速度控制显示
void VideoPlayer::updateSpeedControl()
{
    if (player && speedSlider) {
        float currentRate = player->playbackRate();
        speedSlider->setValue(static_cast<int>(currentRate * 100));
        updateSpeedLabel(currentRate);
    }
}

void VideoPlayer::exportABLoop()
{
    // 1. 检查是否已设置有效的 AB 点
    if (loopPointA == -1 || loopPointB == -1 || loopPointA == loopPointB) {
        QMessageBox::warning(this, "导出失败", "请先设置有效的 A 点和 B 点（A ≠ B）。");
        return;
    }

    // 2. 检查播放器是否有有效媒体源
    if (!player || player->source().isEmpty()) {
        QMessageBox::warning(this, "导出失败", "当前没有加载任何视频文件。");
        return;
    }

    // 3. 获取源文件本地路径
    QString sourcePath = player->source().toLocalFile();
    if (sourcePath.isEmpty() || !QFile::exists(sourcePath)) {
        QMessageBox::warning(this, "导出失败", "无法获取视频源文件路径，请确保视频是本地文件。");
        return;
    }

    // 4. 计算起始和结束时间（单位：秒）
    qint64 startMs = loopPointA;
    qint64 endMs = loopPointB;
    if (startMs > endMs) qSwap(startMs, endMs);   // 保证 A < B

    double startSec = startMs / 1000.0;
    double durationSec = (endMs - startMs) / 1000.0;

    // 5. 让用户选择保存位置
    QString defaultName = QFileInfo(sourcePath).baseName() + "_AB_loop.mp4";
    QString savePath = QFileDialog::getSaveFileName(
        this,
        "保存 AB 循环片段",
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/" + defaultName,
        "MP4 文件 (*.mp4)"
        );
    if (savePath.isEmpty()) return;

    // 6. 构建 FFmpeg 命令
    // 注意：ffmpeg 必须安装在系统中且可执行（或置于 PATH 下）
    QString program = "ffmpeg";
    QStringList args;
    args << "-i" << sourcePath
         << "-ss" << QString::number(startSec, 'f', 3)
         << "-t" << QString::number(durationSec, 'f', 3)
         << "-c:v" << "libx264"
         << "-preset" << "medium"
         << "-crf" << "23"
         << "-c:a" << "aac"
         << "-b:a" << "128k"
         << "-movflags" << "+faststart"
         << "-y"   // 覆盖已有文件
         << savePath;

    // 7. 执行 FFmpeg（用 QProcess）
    QProcess *ffmpeg = new QProcess(this);
    connect(ffmpeg, &QProcess::finished, this, [=](int exitCode, QProcess::ExitStatus status) {
        if (exitCode == 0 && status == QProcess::NormalExit) {
            QMessageBox::information(this, "导出成功", QString("片段已保存至：\n%1").arg(savePath));
        } else {
            QString error = ffmpeg->readAllStandardError();
            QMessageBox::critical(this, "导出失败", QString("FFmpeg 执行错误：\n%1").arg(error));
        }
        ffmpeg->deleteLater();
    });

    // 可选：显示进度对话框（简单起见仅显示等待光标）
    QApplication::setOverrideCursor(Qt::WaitCursor);
    ffmpeg->start(program, args);
    if (!ffmpeg->waitForStarted(3000)) {
        QApplication::restoreOverrideCursor();
        QMessageBox::critical(this, "导出失败", "无法启动 FFmpeg，请确认已安装 ffmpeg 并添加到 PATH。");
        ffmpeg->deleteLater();
        return;
    }
    // 等待结束后恢复光标
    connect(ffmpeg, &QProcess::finished, this, []() {
        QApplication::restoreOverrideCursor();
    });
}

bool VideoPlayer::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::MouseMove) {
        QMouseEvent *me = static_cast<QMouseEvent*>(event);
        QPoint globalPos = me->globalPosition().toPoint();
        // 检查鼠标是否在主窗口的客户区内
        if (this->rect().contains(this->mapFromGlobal(globalPos))) {
            showControlPanel();
        }
        // 注意：即使不在窗口内也不隐藏，隐藏由 Leave 事件处理
    }
    else if (event->type() == QEvent::Enter) {
        // 鼠标进入窗口（无论从哪个方向）都显示面板
        showControlPanel();
    }
    else if (event->type() == QEvent::Leave) {
        // 鼠标离开窗口时，延迟检查是否真的离开了所有区域
        QTimer::singleShot(150, this, [this]() {
            QPoint globalPos = QCursor::pos();
            bool inMain = this->rect().contains(this->mapFromGlobal(globalPos));
            bool inPanel = controlPanel && controlPanel->rect().contains(controlPanel->mapFromGlobal(globalPos));
            if (!inMain && !inPanel) {
                hideControlPanel();
            }
        });
    }

    return QMainWindow::eventFilter(obj, event);
}

void VideoPlayer::stopPlayback()
{
    // 禁用所有循环
    isLooping = false;
    isABLoopEnabled = false;

    if (loopAction) loopAction->setChecked(false);
    if (loopABAction) loopABAction->setChecked(false);

    // 取消原生循环（如果使用了）
    if (player) {
        player->setLoops(0);  // 如果是 Qt 6.5+
        player->stop();
    }

    if (playPauseButton) {
        playPauseButton->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
    }

    if (positionSlider) positionSlider->setValue(0);
    if (timeLabel) timeLabel->setText("00:00 / 00:00");

    qDebug() << "停止播放，循环已禁用";
}

void VideoPlayer::showControlPanel()
{
    //qDebug() << "showControlPanel called, visible=" << controlPanel->isVisible();
    if (!controlPanel) return;
    // 不要因为 controlPanelVisible 而返回（除非您有特殊需求）
    if (!controlPanel->isVisible()) {
        updateControlPanelPosition();
        controlPanel->show();
        controlPanel->raise();
        controlPanelVisible = true;
    }
    controlPanelHideTimer->start(2000);
}

void VideoPlayer::hideControlPanel()
{
    if (controlPanel && controlPanel->isVisible()) {
        controlPanel->hide();
        controlPanelVisible = false;
        controlPanelHideTimer->stop();
    }
}

