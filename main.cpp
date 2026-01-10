#include "musicplayer.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    
    // 设置应用程序信息
    app.setApplicationName("MusicPlayer");
    app.setOrganizationName("SimplePlayer");
    
    MusicPlayer window;
    window.show();
    
    return app.exec();
}