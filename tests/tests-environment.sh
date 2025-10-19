#!/bin/bash

set -e

sudo apt-get update -qq

# wget https://ftp.fau.de/debian/pool/main/p/patchelf/patchelf_0.8-2_amd64.deb
# echo "5d506507df7c02766ae6c3ca0d15b4234f4cb79a80799190ded9d3ca0ac28c0c  patchelf_0.8-2_amd64.deb" | sha256sum -c
if [ "$(uname -m)" = "aarch64" ]; then
  wget http://old-releases.ubuntu.com/ubuntu/pool/universe/p/patchelf/patchelf_0.8-2_arm64.deb;
  echo "3236b6aa89e891a9ab2da4f4574784b29ebd418b55d30a332ed8e2feff537d3c  patchelf_0.8-2_arm64.deb" | sha256sum -c;
  tool_arch="aarch64";
else
  wget http://old-releases.ubuntu.com/ubuntu/pool/universe/p/patchelf/patchelf_0.8-2_amd64.deb;
  echo "b3b69f346afb16ab9e925b91afc4c77179a70b676c2eb58a2821f8664fd4cae6  patchelf_0.8-2_amd64.deb" | sha256sum -c;
  tool_arch="x86_64";
fi

sudo dpkg -i patchelf_0.8-2_*.deb
# We want a newer patchelf since the one above is missing e.g., '--add-needed' which our users might want to use
# However, the newer patchelf versions are broken and cripple, e.g., libQt5Core; see https://github.com/NixOS/patchelf/issues/124
# git clone -o e1e39f3 https://github.com/NixOS/patchelf
# cd patchelf
# bash ./bootstrap.sh
# ./configure --prefix=/usr
# make -j$(nproc)
# sudo make install

cd /tmp/
# wget -c https://artifacts.assassinate-you.net/artifactory/AppImageKit/travis-2052/appimagetool-x86_64.AppImage # branch last-good, https://travis-ci.org/AppImage/AppImageKit/jobs/507462541
# wget -c "https://github.com/AppImage/AppImageKit/releases/download/continuous/appimagetool-x86_64.AppImage" # See #542
# wget -c "https://github.com/AppImage/AppImageKit/releases/download/13/appimagetool-$tool_arch.AppImage" # Workaround for #542
wget -c "https://github.com/AppImage/appimagetool/releases/download/continuous/appimagetool-$tool_arch.AppImage"
chmod +x appimagetool*AppImage
./appimagetool*AppImage --appimage-extract
sudo cp squashfs-root/usr/bin/* /usr/local/bin/
# sudo cp -r squashfs-root/usr/lib/appimagekit /usr/local/lib/
# sudo chmod +rx /usr/local/lib/appimagekit
cd -

sudo apt-get -y install qt5-default qtbase5-dev qtdeclarative5-dev qtwebengine5-dev qttranslations5-l10n binutils xpra zsync desktop-file-utils gcc g++ make libgl1-mesa-dev fuse psmisc
