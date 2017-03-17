#!/bin/bash

set -x

source /opt/qt*/bin/qt*-env.sh
/opt/qt*/bin/qmake linuxdeployqt.pro
make -j2

mkdir -p linuxdeployqt.AppDir/usr/bin/
cp /usr/local/bin/patchelf linuxdeployqt.AppDir/usr/bin/
cp /usr/local/bin/appimagetool linuxdeployqt.AppDir/usr/bin/
find linuxdeployqt.AppDir/
export VERSION=continuous
cp ./linuxdeployqt/linuxdeployqt linuxdeployqt.AppDir/usr/bin/
./linuxdeployqt/linuxdeployqt linuxdeployqt.AppDir/linuxdeployqt.desktop -verbose=3 -appimage
ls -lh
find *.AppDir
xpra start :99
bash -e tests/tests.sh
