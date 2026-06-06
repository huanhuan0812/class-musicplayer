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
    void onLyricDoubleClicked(QListWidgetItem *item);
    void adjustLyricFontSizes();
    void loadConfig();
    void saveConfig();

private:
    Ui::MusicPlayer *ui;
    QMediaPlayer *player;
    QAudioOutput *audioOutput;
    
    QMap<qint64, QString> lyricsMap;
    QList<qint64> timeStamps;
    QList<QString> lyricTexts;
    int currentLyricIndex = -1;
    
    QFont baseFont;
    int baseFontSize = 12;
    int currentFontSize = 20;
    int nextFontSize = 16;
    int normalFontSize = 12;

    QString lastFolderPath;
    QString defaultFolder;
    
    void parseLyricsFile(const QString &filePath);
    void updateLyricDisplay();
    QString getConfigFilePath();
    QString findLyricsFile(const QString &musicPath);
    QString extractSongTitle(const QString &fileName);
    QStringList getAllLyricsFiles(const QString &directory);
    int calculateMatchScore(const QString &fileName, const QString &songName, const QString &artistName);
    void extractMetadata(const QString &musicPath, QString &songName, QString &artistName);
};

#endif // MUSICPLAYER_H