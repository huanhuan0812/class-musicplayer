cmake_minimum_required(VERSION 3.16)
project(MusicPlayer VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets Multimedia MultimediaWidgets)

qt_add_executable(MusicPlayer
    main.cpp
    musicplayer.cpp
    musicplayer.h
    musicplayer.ui
)

target_link_libraries(MusicPlayer PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::Multimedia
    Qt6::MultimediaWidgets
)