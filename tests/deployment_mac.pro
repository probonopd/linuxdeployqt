TEMPLATE = app
INCLUDEPATH += . ../shared/
TARGET=tst_deployment_mac
CONFIG += qtestlib

SOURCES += tst_deployment_mac.cpp ../shared/shared.cpp
HEADERS += ../shared/shared.h
