#ifndef MUSICPLAYER_H
#define MUSICPLAYER_H

#include <QMainWindow>
#include <QMediaPlayer>
#include <QAudioOutput>
#include <QMap>
#include <QList>
#include <QListWidgetItem>

QT_BEGIN_NAMESPACE
namespace Ui {
class MusicPlayer;
}
QT_END_NAMESPACE

class MusicPlayer : public QMainWindow
{
    Q_OBJECT

public:
    MusicPlayer(QWidget *parent = nullptr);
    ~MusicPlayer();

protected:
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void selectMusic();
    void togglePlay();
    void stop();
    void updatePosition(qint64 position);
    void updateDuration(qint64 duration);
    void updateVolume(int volume);
    void seekTo(int position);
    void loadLyrics();
    void highlightCurrentLyric();
    
    // 新增：双击歌词跳转
    void onLyricDoubleClicked(QListWidgetItem *item);
    // 新增：调整字体大小
    void adjustLyricFontSizes();

    void loadConfig();
    void saveConfig();

private:
    Ui::MusicPlayer *ui;
    QMediaPlayer *player;
    QAudioOutput *audioOutput;
    
    QMap<qint64, QString> lyricsMap;  // 时间戳 -> 歌词文本（纯文本）
    QList<qint64> timeStamps;         // 时间戳列表
    QList<QString> lyricTexts;        // 歌词文本列表（纯文本）
    int currentLyricIndex = -1;       // 当前歌词索引
    
    // 新增：字体相关
    QFont baseFont;
    int baseFontSize = 12;
    int currentFontSize = 20;         // 当前行字体大小
    int nextFontSize = 16;            // 下一行字体大小
    int normalFontSize = 12;          // 普通行字体大小

    QString lastFolderPath;
    
    void parseLyricsFile(const QString &filePath);
    void updateLyricDisplay();
};
#endif // MUSICPLAYER_H