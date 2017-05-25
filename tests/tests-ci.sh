#!/bin/bash

set -x

source /opt/qt*/bin/qt*-env.sh
/opt/qt*/bin/qmake linuxdeployqt.pro
make -j2

mkdir -p linuxdeployqt.AppDir/usr/bin/
cp /usr/local/bin/{appimagetool,mksquashfs,patchelf,zsyncmake} linuxdeployqt.AppDir/usr/bin/
find linuxdeployqt.AppDir/
export VERSION=continuous
cp ./bin/linuxdeployqt linuxdeployqt.AppDir/usr/bin/
./bin/linuxdeployqt linuxdeployqt.AppDir/linuxdeployqt.desktop -verbose=3 -appimage
ls -lh
find *.AppDir
xpra start :99

export DISPLAY=:99

until xset -q
do
        echo "Waiting for X server to start..."
        sleep 1;
done

bash -e tests/tests.sh
