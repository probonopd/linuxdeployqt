#!/bin/bash

set -x

source /opt/qt*/bin/qt*-env.sh
/opt/qt*/bin/qmake CONFIG+=debug linuxdeployqt.pro
make -j

mkdir -p linuxdeployqt.AppDir/usr/bin/
cp /usr/local/bin/{appimagetool,mksquashfs,patchelf,zsyncmake} linuxdeployqt.AppDir/usr/bin/
find linuxdeployqt.AppDir/
export VERSION=continuous
cp ./linuxdeployqt/linuxdeployqt linuxdeployqt.AppDir/usr/bin/
./linuxdeployqt/linuxdeployqt linuxdeployqt.AppDir/linuxdeployqt.desktop -verbose=3 -appimage
ls -lh
find *.AppDir
xpra start :99

export DISPLAY=:99

until xset -q
do
        echo "Waiting for X server to start..."
        sleep 1;
done

# enable core dumps
echo "/tmp/coredump" | sudo tee /proc/sys/kernel/core_pattern

ulimit -c unlimited
ulimit -a -S
ulimit -a -H

bash -e tests/tests.sh || RESULT=$?

if [ $RESULT -ne 0 ]; then
  echo "FAILURE: linuxdeployqt CRASHED -- uploading files for debugging to transfer.sh"
  set -v
  [ -e /tmp/coredump ] && curl --upload-file /tmp/coredump https://transfer.sh/coredump
  curl --upload-file linuxdeployqt-*-x86_64.AppImage https://transfer.sh/linuxdeployqt-x86_64.AppImage
  exit $RESULT
fi
