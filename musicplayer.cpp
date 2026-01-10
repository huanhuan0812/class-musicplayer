#include "musicplayer.h"
#include "ui_musicplayer.h"
#include <QFileDialog>
#include <QFileInfo>
#include <QDir>
#include <QTime>
#include <QRegularExpression>
#include <QListWidgetItem>
#include <QFontMetrics>
#include <QScrollBar>
#include <QTimer>
#include <QStringConverter>
#include <QResizeEvent>
#include <QDebug>
#include <QSettings>
#include <QString>

MusicPlayer::MusicPlayer(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MusicPlayer)
    , player(new QMediaPlayer(this))
    , audioOutput(new QAudioOutput(this))
{
    ui->setupUi(this);
    loadConfig();
    setWindowTitle("音乐播放器");
    
    // 设置歌词列表的样式
    ui->listLyrics->setStyleSheet(R"(
        QListWidget {
            background-color: #f0f0f0;
            border: 1px solid #ccc;
            border-radius: 5px;
        }
        QListWidget::item {
            padding: 10px;
            border-bottom: 1px solid #ddd;
        }
        QListWidget::item:last {
            border-bottom: none;
        }
    )");
    
    // 设置列表项对齐方式
    ui->listLyrics->setItemAlignment(Qt::AlignCenter);
    
    // 初始化字体
    baseFont = ui->listLyrics->font();
    ui->listLyrics->setFont(baseFont);
    
    // 初始化播放器
    player->setAudioOutput(audioOutput);
    audioOutput->setVolume(0.5);
    ui->sliderVolume->setValue(50);
    
    // 连接信号槽
    connect(player, &QMediaPlayer::positionChanged, this, &MusicPlayer::updatePosition);
    connect(player, &QMediaPlayer::durationChanged, this, &MusicPlayer::updateDuration);
    
    // 连接按钮
    connect(ui->btnSelect, &QPushButton::clicked, this, &MusicPlayer::selectMusic);
    connect(ui->btnPlay, &QPushButton::clicked, this, &MusicPlayer::togglePlay);
    connect(ui->btnStop, &QPushButton::clicked, this, &MusicPlayer::stop);
    
    // 连接滑块
    connect(ui->sliderVolume, &QSlider::valueChanged, this, &MusicPlayer::updateVolume);
    connect(ui->sliderProgress, &QSlider::sliderMoved, this, &MusicPlayer::seekTo);
    
    // 连接歌词列表双击事件
    connect(ui->listLyrics, &QListWidget::itemDoubleClicked, 
            this, &MusicPlayer::onLyricDoubleClicked);
}

MusicPlayer::~MusicPlayer()
{
    saveConfig();
    delete ui;
}

void MusicPlayer::selectMusic()
{
    

    QString filePath = QFileDialog::getOpenFileName(this, "选择音乐文件", 
        lastFolderPath, "音频文件 (*.mp3 *.wav *.flac *.m4a *.ogg)");
    
    if (!filePath.isEmpty()) {
        player->setSource(QUrl::fromLocalFile(filePath));
        ui->labelFile->setText(QFileInfo(filePath).fileName());
        loadLyrics();
        
        // 自动开始播放
        player->play();
        ui->btnPlay->setText("暂停");
    }
}

void MusicPlayer::togglePlay()
{
    if (player->playbackState() == QMediaPlayer::PlayingState) {
        player->pause();
        ui->btnPlay->setText("播放");
    } else {
        player->play();
        ui->btnPlay->setText("暂停");
    }
}

void MusicPlayer::stop()
{
    player->stop();
    ui->btnPlay->setText("播放");
    ui->sliderProgress->setValue(0);
    ui->labelCurrent->setText("00:00");
    
    // 重置歌词高亮
    currentLyricIndex = -1;
    updateLyricDisplay();
}

void MusicPlayer::updatePosition(qint64 position)
{
    if (!ui->sliderProgress->isSliderDown()) {
        ui->sliderProgress->setValue(position);
    }
    
    QTime currentTime(0, 0);
    currentTime = currentTime.addMSecs(position);
    ui->labelCurrent->setText(currentTime.toString("mm:ss"));
    
    highlightCurrentLyric();
}

void MusicPlayer::updateDuration(qint64 duration)
{
    ui->sliderProgress->setRange(0, duration);
    
    QTime totalTime(0, 0);
    totalTime = totalTime.addMSecs(duration);
    ui->labelTotal->setText(totalTime.toString("mm:ss"));
}

void MusicPlayer::updateVolume(int volume)
{
    audioOutput->setVolume(volume / 100.0);
}

void MusicPlayer::seekTo(int position)
{
    player->setPosition(position);
}

void MusicPlayer::loadLyrics()
{
    if (player->source().isEmpty()) return;
    
    QString musicPath = player->source().toLocalFile();
    QFileInfo musicInfo(musicPath);
    QString baseName = musicInfo.completeBaseName();
    QString dirPath = musicInfo.absolutePath();
    
    // 查找歌词文件
    QStringList lrcExtensions = {".lrc", ".txt", ".lyric"};
    QString lyricsPath;
    
    for (const QString &ext : lrcExtensions) {
        QString path = dirPath + "/" + baseName + ext;
        if (QFile::exists(path)) {
            lyricsPath = path;
            break;
        }
    }
    
    // 清空之前的歌词
    lyricsMap.clear();
    timeStamps.clear();
    lyricTexts.clear();
    ui->listLyrics->clear();
    currentLyricIndex = -1;
    
    if (!lyricsPath.isEmpty()) {
        parseLyricsFile(lyricsPath);
        ui->labelStatus->setText("歌词已加载");
    } else {
        ui->listLyrics->addItem("未找到歌词文件");
        ui->labelStatus->setText("无歌词");
    }
    
    // 调整字体大小
    adjustLyricFontSizes();
}

void MusicPlayer::parseLyricsFile(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        ui->listLyrics->addItem("无法打开歌词文件");
        return;
    }
    
    QTextStream stream(&file);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    stream.setCodec("UTF-8");
#else
    stream.setEncoding(QStringConverter::Utf8);
#endif
    
    QRegularExpression timeRegex("\\[(\\d+):(\\d+)(?:\\.(\\d+))?\\]");
    
    while (!stream.atEnd()) {
        QString line = stream.readLine().trimmed();
        if (line.isEmpty()) continue;
        
        QRegularExpressionMatchIterator it = timeRegex.globalMatch(line);
        QString lyricText = line;
        
        // 移除所有时间标签，只保留纯文本歌词
        while (it.hasNext()) {
            QRegularExpressionMatch match = it.next();
            lyricText.remove(match.captured(0));
        }
        
        lyricText = lyricText.trimmed();
        if (lyricText.isEmpty()) continue;
        
        // 重新获取迭代器
        it = timeRegex.globalMatch(line);
        while (it.hasNext()) {
            QRegularExpressionMatch match = it.next();
            int minutes = match.captured(1).toInt();
            int seconds = match.captured(2).toInt();
            int milliseconds = match.captured(3).isEmpty() ? 0 : match.captured(3).toInt();
            
            qint64 time = (minutes * 60000) + (seconds * 1000) + (milliseconds * 10);
            
            if (!lyricsMap.contains(time) || lyricsMap[time].isEmpty()) {
                // 存储纯文本歌词
                lyricsMap[time] = lyricText;
                timeStamps.append(time);
                lyricTexts.append(lyricText);
            }
        }
    }
    
    file.close();
    
    // 按时间排序
    std::sort(timeStamps.begin(), timeStamps.end());
    
    // 重新整理歌词文本列表以匹配时间戳顺序
    lyricTexts.clear();
    for (qint64 time : timeStamps) {
        lyricTexts.append(lyricsMap[time]);
        // 只添加纯文本歌词，不包含时间标签
        QListWidgetItem *item = new QListWidgetItem(lyricsMap[time]);
        item->setTextAlignment(Qt::AlignCenter);
        ui->listLyrics->addItem(item);
    }
    
    // 添加结束标记
    if (!timeStamps.isEmpty()) {
        QListWidgetItem *endItem = new QListWidgetItem("♪");
        endItem->setTextAlignment(Qt::AlignCenter);
        ui->listLyrics->addItem(endItem);
    }
}

void MusicPlayer::highlightCurrentLyric()
{
    if (timeStamps.isEmpty() || player->playbackState() != QMediaPlayer::PlayingState) {
        return;
    }
    
    qint64 position = player->position();
    int newIndex = -1;
    
    // 找到当前时间对应的歌词索引
    for (int i = 0; i < timeStamps.size(); ++i) {
        if (position >= timeStamps[i]) {
            newIndex = i;
        } else {
            break;
        }
    }
    
    if (newIndex != currentLyricIndex) {
        currentLyricIndex = newIndex;
        updateLyricDisplay();
    }
}

void MusicPlayer::updateLyricDisplay()
{
    int itemCount = ui->listLyrics->count();
    if (itemCount <= 0) return;
    
    // 首先重置所有项目的样式和字体
    for (int i = 0; i < itemCount; ++i) {
        QListWidgetItem *item = ui->listLyrics->item(i);
        QFont font = baseFont;
        
        // 根据位置设置字体大小和样式
        if (i == currentLyricIndex) {
            // 当前行：超大字体，蓝色，加粗
            font.setPointSize(currentFontSize);
            font.setBold(true);
            item->setForeground(QColor(30, 144, 255));  // 蓝色
            
            // 为当前行添加发光效果
            item->setBackground(QColor(255, 255, 240));  // 浅黄色背景
        } else if (i == currentLyricIndex + 1 && currentLyricIndex + 1 < itemCount) {
            // 下一行：大字体，深蓝色
            font.setPointSize(nextFontSize);
            font.setBold(true);
            item->setForeground(QColor(70, 130, 180));  // 深蓝色
        } else {
            // 其他行：正常字体，灰色
            font.setPointSize(normalFontSize);
            font.setBold(false);
            item->setForeground(QColor(120, 120, 120));  // 灰色
            item->setBackground(Qt::white);  // 白色背景
        }
        
        item->setFont(font);
        
        // 调整项目高度以适应字体大小
        QFontMetrics fm(font);
        int itemHeight = fm.height() + 25;  // 增加更多padding
        
        // 当前行和下一行高度更大
        if (i == currentLyricIndex) {
            itemHeight += 15;  // 当前行额外增加高度
        } else if (i == currentLyricIndex + 1) {
            itemHeight += 10;  // 下一行额外增加高度
        }
        item->setSizeHint(QSize(item->sizeHint().width(), itemHeight));
    }
    
    // 滚动到当前歌词位置
    if (currentLyricIndex >= 0 && currentLyricIndex < itemCount) {
        QListWidgetItem *currentItem = ui->listLyrics->item(currentLyricIndex);
        ui->listLyrics->scrollToItem(currentItem, QAbstractItemView::PositionAtCenter);
    }
}

void MusicPlayer::onLyricDoubleClicked(QListWidgetItem *item)
{
    int row = ui->listLyrics->row(item);
    
    // 查找对应的时间戳
    if (row >= 0 && row < timeStamps.size()) {
        qint64 time = timeStamps[row];
        player->setPosition(time);
        
        // 更新当前歌词索引
        currentLyricIndex = row;
        updateLyricDisplay();
    }
}

void MusicPlayer::adjustLyricFontSizes()
{
    // 根据窗口大小和歌词数量自适应调整字体大小
    int itemCount = ui->listLyrics->count();
    if (itemCount == 0) return;
    
    // 获取歌词列表的可用高度和宽度
    int listHeight = ui->listLyrics->height();
    int listWidth = ui->listLyrics->width();
    
    // 计算歌词区域的比例
    double heightRatio = listHeight / 500.0;  // 以500像素为基准
    double widthRatio = listWidth / 400.0;    // 以400像素为基准
    
    // 综合比例（取高度和宽度的较小值，确保在小窗口上也能正常显示）
    double ratio = qMin(heightRatio, widthRatio);
    
    // 基础字体大小（根据比例调整）
    int baseCurrentSize = static_cast<int>(36 * ratio);
    int baseNextSize = static_cast<int>(28 * ratio);
    int baseNormalSize = static_cast<int>(18 * ratio);
    
    // 确保基础字体大小在合理范围内
    baseCurrentSize = qBound(24, baseCurrentSize, 48);  // 当前行: 24-48
    baseNextSize = qBound(20, baseNextSize, 36);       // 下一行: 20-36
    baseNormalSize = qBound(12, baseNormalSize, 24);   // 其他行: 12-24
    
    // 根据歌词数量进一步调整
    if (itemCount > 30) {
        // 歌词很多，减小字体
        baseCurrentSize = qMax(20, baseCurrentSize - 4);
        baseNextSize = qMax(18, baseNextSize - 4);
        baseNormalSize = qMax(10, baseNormalSize - 4);
    } else if (itemCount < 15) {
        // 歌词很少，增大字体
        baseCurrentSize = qMin(48, baseCurrentSize + 6);
        baseNextSize = qMin(36, baseNextSize + 5);
        baseNormalSize = qMin(24, baseNormalSize + 4);
    }
    
    // 根据当前歌词索引位置调整（如果靠近末尾，可以适当减小字体）
    if (currentLyricIndex > 0 && currentLyricIndex > itemCount * 0.7) {
        // 歌曲播放到70%以后，稍微减小字体
        baseCurrentSize = qMax(20, baseCurrentSize - 2);
        baseNextSize = qMax(18, baseNextSize - 2);
    }
    
    // 设置最终字体大小
    currentFontSize = baseCurrentSize;
    nextFontSize = baseNextSize;
    normalFontSize = baseNormalSize;
    
    // 更新显示
    updateLyricDisplay();
    
    // 更新状态栏提示
    ui->labelStatus->setText(QString("歌词: %1行 | 字体大小: %2/%3/%4 | 窗口: %5x%6")
        .arg(itemCount)
        .arg(currentFontSize)
        .arg(nextFontSize)
        .arg(normalFontSize)
        .arg(listWidth)
        .arg(listHeight));
}

void MusicPlayer::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    adjustLyricFontSizes();
}

void MusicPlayer::loadConfig()
{
    QSettings settings("config.ini", QSettings::IniFormat);
    lastFolderPath = settings.value("lastFolderPath", QDir::homePath()).toString();

    settings.sync();
}

void MusicPlayer::saveConfig()
{
    QSettings settings("config.ini", QSettings::IniFormat);
    settings.setValue("lastFolderPath", lastFolderPath);

    settings.sync();
}