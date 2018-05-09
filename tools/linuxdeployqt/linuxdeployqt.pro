option(host_build)

QT = core
CONFIG += console

TARGET = linuxdeployqt
VERSION = $$MODULE_VERSION

DEFINES += BUILD_LINUXDEPLOYQT

load(qt_tool)

HEADERS += shared.h
SOURCES += main.cpp \
    shared.cpp

DEFINES -= QT_USE_QSTRINGBUILDER #leads to compile errors if not disabled

# versioning
# don't break the quotes -- at the moment, the shell commands are injected into the Makefile to have them run on every
# build and not during configure time

DEFINES += LINUXDEPLOYQT_GIT_COMMIT="'\"$(shell cd $$PWD && git rev-parse --short HEAD)\"'"

DEFINES += BUILD_DATE="'\"$(shell env LC_ALL=C date -u '+%Y-%m-%d %H:%M:%S %Z')\"'"

_BUILD_NUMBER = $$(TRAVIS_BUILD_NUMBER)

isEmpty(_BUILD_NUMBER) {
    message("Not building on Travis CI, tagging build as local dev build")
    DEFINES += BUILD_NUMBER="'\"<local dev build>\"'"
} else {
    message("Building on Travis CI build, build number $$_BUILD_NUMBER")
    DEFINES += BUILD_NUMBER="'\"$$_BUILD_NUMBER\"'"
}

DEFINES += LINUXDEPLOYQT_VERSION="'\"$(shell cd $$PWD && git describe --tags $(shell cd $$PWD && git rev-list --tags --skip=1 --max-count=1) --abbrev=0)\"'"
contains(DEFINES, EXCLUDELIST.*) {
    message("EXCLUDELIST specified, to use the most recent exclude list, please run qmake without EXCLUDELIST definition and with internet.")
} else {
    message("Updating exclude list...")

    # check whether command _would_ run successfully
    EXCLUDELIST_GENERATION_WORKS = FALSE
    system($$_PRO_FILE_PWD_/../generate-excludelist.sh): EXCLUDELIST_GENERATION_WORKS = TRUE
    isEqual(EXCLUDELIST_GENERATION_WORKS, FALSE) {
        warning("Updating excludelist failed, using outdated copy")
    }
}
