QT += core gui widgets qml quick quickcontrols2 network

CONFIG += c++17 release
TARGET = t3pop
TEMPLATE = app

HEADERS += src/backend.h
SOURCES += src/main.cpp src/backend.cpp
RESOURCES += src/resources.qrc
