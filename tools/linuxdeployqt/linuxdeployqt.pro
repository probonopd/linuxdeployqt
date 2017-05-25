option(host_build)

QT = core
CONFIG += console

TARGET = linuxdeployqt
VERSION = $$MODULE_VERSION

DEFINES += BUILD_LINUXDEPLOYQT

load(qt_tool)

HEADERS += ../shared/shared.h
SOURCES += main.cpp \
	../shared/shared.cpp

DEFINES -= QT_USE_QSTRINGBUILDER
