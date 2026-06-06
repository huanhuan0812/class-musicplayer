#include "musicplayer.h"
#include "ui_musicplayer.h"
#include <QFileDialog>
#include <QFileInfo>
#include <QDir>
#include <QTime>
#include <QRegularExpression>
#include <QListWidgetItem>
#include <QFontMetrics>
#include <QResizeEvent>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QMediaMetaData>
#include <QCoreApplication>

MusicPlayer::MusicPlayer(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MusicPlayer)
    , player(new QMediaPlayer(this))
    , audioOutput(new QAudioOutput(this))
{
    ui->setupUi(this);
    
    defaultFolder = QStandardPaths::writableLocation(QStandardPaths::MusicLocation);
    if (defaultFolder.isEmpty()) {
        defaultFolder = QDir::homePath();
    }
    
    loadConfig();
    setWindowTitle("音乐播放器");
    
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
    
    ui->listLyrics->setItemAlignment(Qt::AlignCenter);
    
    baseFont = ui->listLyrics->font();
    ui->listLyrics->setFont(baseFont);
    
    player->setAudioOutput(audioOutput);
    audioOutput->setVolume(0.5);
    ui->sliderVolume->setValue(50);
    
    connect(player, &QMediaPlayer::positionChanged, this, &MusicPlayer::updatePosition);
    connect(player, &QMediaPlayer::durationChanged, this, &MusicPlayer::updateDuration);
    
    connect(ui->btnSelect, &QPushButton::clicked, this, &MusicPlayer::selectMusic);
    connect(ui->btnPlay, &QPushButton::clicked, this, &MusicPlayer::togglePlay);
    connect(ui->btnStop, &QPushButton::clicked, this, &MusicPlayer::stop);
    
    connect(ui->sliderVolume, &QSlider::valueChanged, this, &MusicPlayer::updateVolume);
    connect(ui->sliderProgress, &QSlider::sliderMoved, this, &MusicPlayer::seekTo);
    
    connect(ui->listLyrics, &QListWidget::itemDoubleClicked, 
            this, &MusicPlayer::onLyricDoubleClicked);
}

MusicPlayer::~MusicPlayer()
{
    saveConfig();
    delete ui;
}

QString MusicPlayer::getConfigFilePath()
{
    QDir appDir(QCoreApplication::applicationDirPath());
    QString configDir = appDir.filePath("config");
    
    if (!QDir(configDir).exists()) {
        QDir().mkdir(configDir);
    }
    
    return configDir + "/musicplayer_config.json";
}

void MusicPlayer::loadConfig()
{
    QString configPath = getConfigFilePath();
    QFile file(configPath);
    
    if (file.open(QIODevice::ReadOnly)) {
        QByteArray data = file.readAll();
        QJsonDocument doc = QJsonDocument::fromJson(data);
        
        if (doc.isObject()) {
            QJsonObject obj = doc.object();
            lastFolderPath = obj.value("lastFolderPath").toString();
            if (lastFolderPath.isEmpty() || !QDir(lastFolderPath).exists()) {
                lastFolderPath = defaultFolder;
            }
            
            int volume = obj.value("volume").toInt(50);
            audioOutput->setVolume(volume / 100.0);
            ui->sliderVolume->setValue(volume);
            
            QString lastFile = obj.value("lastFile").toString();
            if (!lastFile.isEmpty() && QFile::exists(lastFile)) {
                player->setSource(QUrl::fromLocalFile(lastFile));
                ui->labelFile->setText(QFileInfo(lastFile).fileName());
                loadLyrics();
            }
        }
        file.close();
    } else {
        lastFolderPath = defaultFolder;
    }
}

void MusicPlayer::saveConfig()
{
    QString configPath = getConfigFilePath();
    QFile file(configPath);
    
    if (file.open(QIODevice::WriteOnly)) {
        QJsonObject obj;
        obj["lastFolderPath"] = lastFolderPath;
        obj["volume"] = ui->sliderVolume->value();
        
        if (!player->source().isEmpty()) {
            obj["lastFile"] = player->source().toLocalFile();
        }
        
        obj["windowWidth"] = this->width();
        obj["windowHeight"] = this->height();
        
        QJsonDocument doc(obj);
        file.write(doc.toJson());
        file.close();
    }
}

QString MusicPlayer::extractSongTitle(const QString &fileName)
{
    QString title = fileName;
    
    int lastDot = title.lastIndexOf('.');
    if (lastDot > 0) {
        title = title.left(lastDot);
    }
    
    QRegularExpression prefixRegex("^(\\d+[\\.\\-_\\s]+)|^(\\[.*?\\]\\s*)");
    title.remove(prefixRegex);
    title = title.trimmed();
    
    return title;
}

void MusicPlayer::extractMetadata(const QString &musicPath, QString &songName, QString &artistName)
{
    QFileInfo fileInfo(musicPath);
    songName = extractSongTitle(fileInfo.fileName());
    
    if (player->source() == QUrl::fromLocalFile(musicPath)) {
        QVariant titleMeta = player->metaData().value(QMediaMetaData::Title);
        QVariant artistMeta = player->metaData().value(QMediaMetaData::Author);
        
        if (!titleMeta.isNull() && !titleMeta.toString().isEmpty()) {
            songName = titleMeta.toString();
        }
        if (!artistMeta.isNull() && !artistMeta.toString().isEmpty()) {
            artistName = artistMeta.toString();
        }
    }
}

QStringList MusicPlayer::getAllLyricsFiles(const QString &directory)
{
    QStringList lyricsFiles;
    QDir dir(directory);
    
    QStringList filters;
    filters << "*.lrc" << "*.LRC" << "*.txt" << "*.TXT" << "*.lyric" << "*.LYRIC";
    
    QFileInfoList fileList = dir.entryInfoList(filters, QDir::Files);
    
    for (const QFileInfo &fileInfo : fileList) {
        lyricsFiles.append(fileInfo.absoluteFilePath());
    }
    
    return lyricsFiles;
}

int MusicPlayer::calculateMatchScore(const QString &fileName, const QString &songName, const QString &artistName)
{
    int score = 0;
    QString fileBaseName = QFileInfo(fileName).completeBaseName();
    QString fileBaseNameLower = fileBaseName.toLower();
    QString songNameLower = songName.toLower();
    QString artistNameLower = artistName.toLower();
    
    if (fileBaseNameLower == songNameLower) {
        score += 100;
    }
    
    if (fileBaseNameLower.contains(songNameLower) && songNameLower.length() > 3) {
        score += 50;
    }
    
    if (songNameLower.contains(fileBaseNameLower) && fileBaseNameLower.length() > 3) {
        score += 40;
    }
    
    QString cleanFileName = extractSongTitle(fileBaseName);
    if (cleanFileName.toLower() == songNameLower) {
        score += 80;
    } else if (cleanFileName.toLower().contains(songNameLower) || songNameLower.contains(cleanFileName.toLower())) {
        score += 30;
    }
    
    if (!artistName.isEmpty()) {
        if (fileBaseNameLower.contains(artistNameLower)) {
            score += 20;
        }
    }
    
    return score;
}

QString MusicPlayer::findLyricsFile(const QString &musicPath)
{
    QFileInfo musicInfo(musicPath);
    QString musicDir = musicInfo.absolutePath();
    QString musicBaseName = musicInfo.completeBaseName();
    
    QString songName, artistName;
    extractMetadata(musicPath, songName, artistName);
    
    QStringList extensions = {".lrc", ".LRC", ".txt", ".TXT", ".lyric", ".LYRIC"};
    for (const QString &ext : extensions) {
        QString exactPath = musicDir + "/" + musicBaseName + ext;
        if (QFile::exists(exactPath)) {
            return exactPath;
        }
    }
    
    QStringList allLyricsFiles = getAllLyricsFiles(musicDir);
    
    if (allLyricsFiles.isEmpty()) {
        return QString();
    }
    
    QMap<int, QString> scoredMatches;
    
    for (const QString &lyricsPath : allLyricsFiles) {
        QFileInfo lyricsInfo(lyricsPath);
        QString lyricsBaseName = lyricsInfo.completeBaseName();
        
        int score = calculateMatchScore(lyricsBaseName, songName, artistName);
        
        if (songName.isEmpty()) {
            if (lyricsBaseName.contains(musicBaseName) || musicBaseName.contains(lyricsBaseName)) {
                score = 30;
            }
        }
        
        if (score > 0) {
            scoredMatches[score] = lyricsPath;
        }
    }
    
    if (!scoredMatches.isEmpty()) {
        QString bestMatch = scoredMatches.last();
        int bestScore = scoredMatches.lastKey();
        
        if (bestScore >= 20) {
            return bestMatch;
        }
    }
    
    if (allLyricsFiles.size() == 1) {
        return allLyricsFiles.first();
    }
    
    return QString();
}

void MusicPlayer::selectMusic()
{
    QString startPath = lastFolderPath;
    if (startPath.isEmpty() || !QDir(startPath).exists()) {
        startPath = defaultFolder;
    }
    
    QString filePath = QFileDialog::getOpenFileName(this, "选择音乐文件", 
        startPath, "音频文件 (*.mp3 *.wav *.flac *.m4a *.ogg)");
    
    if (!filePath.isEmpty()) {
        QFileInfo fileInfo(filePath);
        lastFolderPath = fileInfo.absolutePath();
        
        player->setSource(QUrl::fromLocalFile(filePath));
        ui->labelFile->setText(fileInfo.fileName());
        loadLyrics();
        
        player->play();
        ui->btnPlay->setText("暂停");
        
        saveConfig();
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
    saveConfig();
}

void MusicPlayer::seekTo(int position)
{
    player->setPosition(position);
}

void MusicPlayer::loadLyrics()
{
    if (player->source().isEmpty()) return;
    
    QString musicPath = player->source().toLocalFile();
    QString lyricsPath = findLyricsFile(musicPath);
    
    lyricsMap.clear();
    timeStamps.clear();
    lyricTexts.clear();
    ui->listLyrics->clear();
    currentLyricIndex = -1;
    
    if (!lyricsPath.isEmpty()) {
        parseLyricsFile(lyricsPath);
        ui->labelStatus->setText("歌词已加载: " + QFileInfo(lyricsPath).fileName());
    } else {
        ui->listLyrics->addItem("未找到匹配的歌词文件");
        ui->labelStatus->setText("无歌词");
    }
    
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
    stream.setEncoding(QStringConverter::Utf8);
    
    QRegularExpression timeRegex("\\[(\\d+):(\\d+)(?:\\.(\\d+))?\\]");
    
    while (!stream.atEnd()) {
        QString line = stream.readLine().trimmed();
        if (line.isEmpty()) continue;
        
        QRegularExpressionMatchIterator it = timeRegex.globalMatch(line);
        QString lyricText = line;
        
        while (it.hasNext()) {
            QRegularExpressionMatch match = it.next();
            lyricText.remove(match.captured(0));
        }
        
        lyricText = lyricText.trimmed();
        if (lyricText.isEmpty()) continue;
        
        it = timeRegex.globalMatch(line);
        while (it.hasNext()) {
            QRegularExpressionMatch match = it.next();
            int minutes = match.captured(1).toInt();
            int seconds = match.captured(2).toInt();
            int milliseconds = match.captured(3).isEmpty() ? 0 : match.captured(3).toInt();
            
            qint64 time = (minutes * 60000) + (seconds * 1000) + (milliseconds * 10);
            
            if (!lyricsMap.contains(time) || lyricsMap[time].isEmpty()) {
                lyricsMap[time] = lyricText;
                timeStamps.append(time);
                lyricTexts.append(lyricText);
            }
        }
    }
    
    file.close();
    
    std::sort(timeStamps.begin(), timeStamps.end());
    
    lyricTexts.clear();
    for (qint64 time : timeStamps) {
        lyricTexts.append(lyricsMap[time]);
        QListWidgetItem *item = new QListWidgetItem(lyricsMap[time]);
        item->setTextAlignment(Qt::AlignCenter);
        ui->listLyrics->addItem(item);
    }
    
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
    
    for (int i = 0; i < itemCount; ++i) {
        QListWidgetItem *item = ui->listLyrics->item(i);
        QFont font = baseFont;
        
        if (i == currentLyricIndex) {
            font.setPointSize(currentFontSize);
            font.setBold(true);
            item->setForeground(QColor(30, 144, 255));
            item->setBackground(QColor(255, 255, 240));
        } else if (i == currentLyricIndex + 1 && currentLyricIndex + 1 < itemCount) {
            font.setPointSize(nextFontSize);
            font.setBold(true);
            item->setForeground(QColor(70, 130, 180));
        } else {
            font.setPointSize(normalFontSize);
            font.setBold(false);
            item->setForeground(QColor(120, 120, 120));
            item->setBackground(Qt::white);
        }
        
        item->setFont(font);
        
        QFontMetrics fm(font);
        int itemHeight = fm.height() + 25;
        
        if (i == currentLyricIndex) {
            itemHeight += 15;
        } else if (i == currentLyricIndex + 1) {
            itemHeight += 10;
        }
        item->setSizeHint(QSize(item->sizeHint().width(), itemHeight));
    }
    
    if (currentLyricIndex >= 0 && currentLyricIndex < itemCount) {
        QListWidgetItem *currentItem = ui->listLyrics->item(currentLyricIndex);
        ui->listLyrics->scrollToItem(currentItem, QAbstractItemView::PositionAtCenter);
    }
}

void MusicPlayer::onLyricDoubleClicked(QListWidgetItem *item)
{
    int row = ui->listLyrics->row(item);
    
    if (row >= 0 && row < timeStamps.size()) {
        qint64 time = timeStamps[row];
        player->setPosition(time);
        
        currentLyricIndex = row;
        updateLyricDisplay();
    }
}

void MusicPlayer::adjustLyricFontSizes()
{
    int itemCount = ui->listLyrics->count();
    if (itemCount == 0) return;
    
    int listHeight = ui->listLyrics->height();
    int listWidth = ui->listLyrics->width();
    
    double heightRatio = listHeight / 500.0;
    double widthRatio = listWidth / 400.0;
    double ratio = qMin(heightRatio, widthRatio);
    
    int baseCurrentSize = static_cast<int>(36 * ratio);
    int baseNextSize = static_cast<int>(28 * ratio);
    int baseNormalSize = static_cast<int>(18 * ratio);
    
    baseCurrentSize = qBound(24, baseCurrentSize, 48);
    baseNextSize = qBound(20, baseNextSize, 36);
    baseNormalSize = qBound(12, baseNormalSize, 24);
    
    if (itemCount > 30) {
        baseCurrentSize = qMax(20, baseCurrentSize - 4);
        baseNextSize = qMax(18, baseNextSize - 4);
        baseNormalSize = qMax(10, baseNormalSize - 4);
    } else if (itemCount < 15) {
        baseCurrentSize = qMin(48, baseCurrentSize + 6);
        baseNextSize = qMin(36, baseNextSize + 5);
        baseNormalSize = qMin(24, baseNormalSize + 4);
    }
    
    currentFontSize = baseCurrentSize;
    nextFontSize = baseNextSize;
    normalFontSize = baseNormalSize;
    
    updateLyricDisplay();
    
    ui->labelStatus->setText(QString("歌词: %1行 | 字体: %2/%3/%4 | 窗口: %5x%6")
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