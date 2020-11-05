# Building from source

If you just would like to bundle your application for x86_64 platforms, it is normally not necessary that you build `linuxdeployqt` yourself. Instead, download __linuxdeployqt-x86_64.AppImage__ from the [Releases](https://github.com/probonopd/linuxdeployqt/releases) page and `chmod a+x` it. This bundle, by the way, has been generated using `linuxdeployqt` itself as part of our Travis CI continuous build pipeline.

So, if you are on another platform (e.g. i686, ARM) or would like to compile from source, here are the steps:

* Get and build linuxdeployqt e.g., using Qt 5.7.0 (you could use this [Qt Creator AppImage](https://bintray.com/probono/AppImages/QtCreator#files) for this)

```
sudo apt-get -y install git g++ libgl1-mesa-dev
git clone https://github.com/probonopd/linuxdeployqt.git
# Then build in Qt Creator, or use
export PATH=$(readlink -f /tmp/.mount_QtCreator-*-x86_64/*/gcc_64/bin/):$PATH
cd linuxdeployqt
qmake
make
```

* Optional if you want to install `linuxdeployqt` into your Qt installation, and make it a part of your Qt just like any other tool (qmake, etc.)

```
sudo make install
```

* Build and install [patchelf](https://nixos.org/patchelf.html) (a small utility to modify the dynamic linker and RPATH of ELF executables; similar to `install_name_tool` on macOS). To learn more about this, see http://blog.qt.io/blog/2011/10/28/rpath-and-runpath/

```
wget https://nixos.org/releases/patchelf/patchelf-0.9/patchelf-0.9.tar.bz2
tar xf patchelf-0.9.tar.bz2
( cd patchelf-0.9/ && ./configure  && make && sudo make install )
```

* Optional if you want to generate AppImages: Download [appimagetool](https://github.com/AppImage/AppImageKit/releases) and put it into your $PATH, e.g., into `/usr/local/bin`. Make sure it is renamed to `appimagetool` and is `chmod a+x`

```
sudo wget -c "https://github.com/AppImage/AppImageKit/releases/download/continuous/appimagetool-x86_64.AppImage" -O /usr/local/bin/appimagetool
sudo chmod a+x /usr/local/bin/appimagetool
```
