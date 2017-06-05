include(commons.pri)

QT += core
QT -= gui

CONFIG += c++11

CONFIG += console
CONFIG -= app_bundle

TEMPLATE = app

HEADERS = \
    src/shared.h

SOURCES = \
    src/main.cpp \
    src/shared.cpp

DESTDIR = $${OUT_PWD}
TARGET = $${COMMONS_TARGET}

INSTALLS += target
target.path = $${BINDIR}

RESOURCES += \
    linuxdeployqt.qrc
