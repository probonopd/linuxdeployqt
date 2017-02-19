#!/bin/bash

# Build the sample Qt Widgets Application that comes with Qt Creator

cd tests/QtWidgetsApplication/
if [ -e build/ ] ; then
  rm -rf build/
fi
mkdir build
cd build/
qmake ../QtWidgetsApplication.pro
make -j2
rm *.o *.cpp *.h Makefile
mkdir -p nonfhs fhs/usr/bin

cp QtWidgetsApplication nonfhs/
../../../linuxdeployqt-*-x86_64.AppImage nonfhs/QtWidgetsApplication
find nonfhs/

cp QtWidgetsApplication fhs/usr/bin/
../../../linuxdeployqt-*-x86_64.AppImage fhs/usr/bin/QtWidgetsApplication
find fhs/

cd ../../../
