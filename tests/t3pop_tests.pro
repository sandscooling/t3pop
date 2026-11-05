QT += core gui testlib
CONFIG += c++17 testcase
TARGET = t3pop_tests
TEMPLATE = app

INCLUDEPATH += ../src

HEADERS += ../src/backend.h
SOURCES += t3pop_tests.cpp ../src/backend.cpp
