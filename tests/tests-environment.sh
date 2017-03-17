#!/bin/bash

set -e

sudo add-apt-repository --yes ppa:beineri/opt-qt58-trusty
sudo apt-get update -qq

git clone https://github.com/NixOS/patchelf.git
cd patchelf
bash ./bootstrap.sh
./configure
make -j2
sudo make install

cd -

sudo wget -c "https://github.com/probonopd/AppImageKit/releases/download/continuous/appimagetool-x86_64.AppImage" -O /usr/local/bin/appimagetool
sudo chmod a+x /usr/local/bin/appimagetool

sudo apt-get -y install qt58base qt58declarative qt58webengine binutils xpra
