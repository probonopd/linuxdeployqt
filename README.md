# linuxdeployqt [![Build Status](https://travis-ci.org/probonopd/linuxdeployqt.svg?branch=master)](https://travis-ci.org/probonopd/linuxdeployqt) ![Downloads](https://img.shields.io/github/downloads/probonopd/linuxdeployqt/total.svg) [![Codacy Badge](https://api.codacy.com/project/badge/Grade/93b4a359057e412b8a7673b4b61d7cb7)](https://www.codacy.com/app/probonopd/linuxdeployqt?utm_source=github.com&amp;utm_medium=referral&amp;utm_content=probonopd/linuxdeployqt&amp;utm_campaign=Badge_Grade) [![discourse](https://img.shields.io/badge/forum-discourse-orange.svg)](http://discourse.appimage.org/t/linuxdeployqt-new-linux-deployment-tool-for-qt/57) [![Gitter](https://badges.gitter.im/Join%20Chat.svg)](https://gitter.im/probonopd/AppImageKit?utm_source=badge&utm_medium=badge&utm_campaign=pr-badge) [![irc](https://img.shields.io/badge/IRC-%23AppImage%20on%20freenode-blue.svg)](https://webchat.freenode.net/?channels=AppImage)

This Linux Deployment Tool, `linuxdeployqt`, takes an application as input and makes it self-contained by copying in the resources that the application uses (like libraries, graphics, and plugins) into a bundle. The resulting bundle can be distributed as an AppDir or as an [AppImage](https://appimage.org/) to users, or can be put into cross-distribution packages. It can be used as part of the build process to deploy applications written in C, C++, and other compiled languages with systems like `CMake`, `qmake`, and `make`. When used on Qt-based applications, it can bundle a specific minimal subset of Qt required to run the application.

![](https://user-images.githubusercontent.com/2480569/34471167-d44bd55e-ef41-11e7-941e-e091a83cae38.png)

## Differences to macdeployqt
This tool is conceptually based on the [Mac Deployment Tool](https://doc-snapshots.qt.io/qt5-5.9/osx-deployment.html#the-mac-deployment-tool), `macdeployqt` in the tools applications of the Qt Toolkit, but has been changed to a slightly different logic and other tools needed for Linux.

* Instead of an `.app` bundle for macOS, this produces an [AppDir](http://rox.sourceforge.net/desktop/AppDirs.html) for Linux
* Instead of a `.dmg` disk image for macOS, this produces an [AppImage](http://appimage.org/) for Linux which is quite similar to a dmg but executes the contained application rather than just opening a window on the desktop from where the application can be launched

## A note on binary compatibility

__To produce binaries that are compatible with many target systems, build on the oldest still-supported build system.__ The oldest still-supported release of Ubuntu is currently targeted, tested and supported by the team. 

We recommend to target the oldest still-supported Ubuntu LTS release and build your applications on that. If you do this, the resulting binaries should be able to run on newer (but not older) systems (Ubuntu and other distributions).

We do not support linuxdeployqt on systems newer than the oldest Ubuntu LTS release, because we want to encourage developers to build applications in a way that makes them possible to run on all still-supported distribution releases. For an overview about the support cycles of Ubuntu LTS releases, please see https://wiki.ubuntu.com/Releases.

## Installation

Please download __linuxdeployqt-x86_64.AppImage__ from the [Releases](https://github.com/probonopd/linuxdeployqt/releases) page and `chmod a+x` it. If you would like to build `linuxdeployqt` from source instead, see [BUILDING.md](https://github.com/probonopd/linuxdeployqt/blob/master/BUILDING.md).

## Usage

```
Usage: linuxdeployqt <app-binary|desktop file> [options]

Options:
   -always-overwrite        : Copy files even if the target file exists.
   -appimage                : Create an AppImage (implies -bundle-non-qt-libs).
   -bundle-non-qt-libs      : Also bundle non-core, non-Qt libraries.
   -exclude-libs=<list>     : List of libraries which should be excluded,
                              separated by comma.
   -ignore-glob=<glob>      : Glob pattern relative to appdir to ignore when
                              searching for libraries.
   -executable=<path>       : Let the given executable use the deployed libraries
                              too
   -extra-plugins=<list>    : List of extra plugins which should be deployed,
                              separated by comma.
   -no-copy-copyright-files : Skip deployment of copyright files.
   -no-plugins              : Skip plugin deployment.
   -no-strip                : Don't run 'strip' on the binaries.
   -no-translations         : Skip deployment of translations.
   -qmake=<path>            : The qmake executable to use.
   -qmldir=<path>           : Scan for QML imports in the given path.
   -qmlimport=<path>        : Add the given path to QML module search locations.
   -show-exclude-libs       : Print exclude libraries list.
   -verbose=<0-3>           : 0 = no output, 1 = error/warning (default),
                              2 = normal, 3 = debug.
   -updateinformation=<update string>        : Embed update information STRING; if zsyncmake is installed, generate zsync file
   -version                 : Print version statement and exit.

linuxdeployqt takes an application as input and makes it
self-contained by copying in the Qt libraries and plugins that
the application uses.

By default it deploys the Qt instance that qmake on the $PATH points to.
The '-qmake' option can be used to point to the qmake executable
to be used instead.

Plugins related to a Qt library are copied in with the library.

See the "Deploying Applications on Linux" topic in the
documentation for more information about deployment on Linux.
```

#### Simplest example

You'll need to provide the basic structure of an `AppDir` which should look something like this:
```
└── usr
    ├── bin
    │   └── your_app
    ├── lib
    └── share
        ├── applications
        │   └── your_app.desktop
        └── icons
            └── <theme>
                └── <resolution> 
                    └── apps 
                        └── your_app.png
```
Replace `<theme>` and `<resolution>` with (for example) `hicolor` and `256x256` respectively; see [icon theme spec](https://specifications.freedesktop.org/icon-theme-spec/icon-theme-spec-latest.html) for more details.


Using the desktop file `linuxdeployqt` can determine the parameters of the build.

Where your desktop file would look something like:
```
[Desktop Entry]
Type=Application
Name=Amazing Qt App
Comment=The best Qt Application Ever
Exec=your_app
Icon=your_app
Categories=Office;
```

* Notice that both `Exec` and `Icon` only have file names.
* Also Notice that the `Icon` entry does not include an extension.

Read more about desktop files in the [Desktop Entry Specification 1.0](https://standards.freedesktop.org/desktop-entry-spec/1.0/).

Now you can say: `linuxdeployqt-continuous-x86_64.AppImage path/to/AppDir/usr/share/applications/your_app.desktop`

For a more detailed example, see "Using linuxdeployqt with Travis CI" below.

#### Checking library inclusion

Open in Qt Creator and build your application. Run it from the command line and inspect it with `ldd` to make sure the correct libraries from the correct locations are getting loaded, as `linuxdeployqt` will use `ldd` internally to determine from where to copy libraries into the bundle.

#### QMake configuration

__Important:__ By default, `linuxdeployqt` deploys the Qt instance that qmake on the $PATH points to, so make sure that it is the correct one. Verify that qmake finds the correct Qt instance like this before running the `linuxdeployqt` tool:

```
qmake -v

QMake version 3.0
Using Qt version 5.7.0 in /tmp/.mount_QtCreator-5.7.0-x86_64/5.7/gcc_64/lib
```

If this does not show the correct path to your Qt instance that you want to be bundled, then adjust your `$PATH` to find the correct `qmake`.

Alternatively, use the `-qmake` command line option to point the tool directly to the qmake executable to be used.

#### Remove unnecessary files

Before running linuxdeployqt it may be wise to delete unneeded files that you do not wish to distribute from the build directory. These may be autogenerated during the build. You can delete them like so:

```
find $HOME/build-*-*_Qt_* \( -name "moc_*" -or -name "*.o" -or -name "qrc_*" -or -name "Makefile*" -or -name "*.a" \) -exec rm {} \;
```

Alternatively, you could use `$DESTDIR`.

#### Adding icon and icon theme support

To enable icon and icon theme support you must add `iconengines` as an extra Qt plugin while running `linuxdeployqt`. In order for your application to locate the system theme icons, the `libqgtk3.so` platform theme must also be added:

`-extra-plugins=iconengines,platformthemes/libqgtk3.so`

#### Adding extra Qt plugins 

If you want aditional plugins which the tool doesn't deploy, for a variety of reasons, you can use the -extra-plugins argument and include a list of plugins separated by a comma.  
The plugins deployed are from the Qt installation pointed out by `qmake -v`.  
You can deploy entire plugin directories, a specific directory or a mix of both.

Usage examples: 

1. `-extra-plugins=sqldrivers/libqmsql.so,iconengines/libqsvgicon.so`  
2. `-extra-plugins=sqldrivers,iconengines/libqsvgicon.so`  
3. `-extra-plugins=sqldrivers,iconengines,mediaservice,gamepads`

## Using linuxdeployqt with Travis CI

A common use case for `linuxdeployqt` is to use it on Travis CI after the `make` command. The following example illustrates how to use `linuxdeployqt` with Travis CI. Create a `.travis.yml` file similar to this one (be sure to customize it, e.g., change `APPNAME` to the name of your application as it is spelled in the `Name=` entry of the `.desktop` file):

```
language: cpp
compiler: gcc
sudo: require
dist: trusty

before_install:
  - sudo add-apt-repository ppa:beineri/opt-qt-5.10.1-trusty -y
  - sudo apt-get update -qq

install:
  - sudo apt-get -y install qt510base libgl1-mesa-dev
  - source /opt/qt*/bin/qt*-env.sh

script:
  - qmake CONFIG+=release PREFIX=/usr
  - make -j$(nproc)
  - make INSTALL_ROOT=appdir -j$(nproc) install ; find appdir/
  - wget -c -nv "https://github.com/probonopd/linuxdeployqt/releases/download/continuous/linuxdeployqt-continuous-x86_64.AppImage"
  - chmod a+x linuxdeployqt-continuous-x86_64.AppImage
  # export VERSION=... # linuxdeployqt uses this for naming the file
  - ./linuxdeployqt-continuous-x86_64.AppImage appdir/usr/share/applications/*.desktop -appimage

after_success:
  # find appdir -executable -type f -exec ldd {} \; | grep " => /usr" | cut -d " " -f 2-3 | sort | uniq # for debugging
  # curl --upload-file APPNAME*.AppImage https://transfer.sh/APPNAME-git.$(git rev-parse --short HEAD)-x86_64.AppImage
  - wget -c https://github.com/probonopd/uploadtool/raw/master/upload.sh
  - bash upload.sh APPNAME*.AppImage*
  
branches:
  except:
    - # Do not build tags that we create when we upload to GitHub Releases
    - /^(?i:continuous)/
```

When you save your change, then Travis CI should build and upload an AppImage for you. More likely than not, some fine-tuning will still be required.

For this to work, you need to set up `GITHUB_TOKEN` in Travis CI; please see https://github.com/probonopd/uploadtool.

By default, qmake `.pro` files generated by Qt Creator unfortunately don't support `make install` out of the box. In this case you will get

```
make: Nothing to be done for `install'.
find: `appdir/': No such file or directory
```
### Fix for "make: Nothing to be done for 'install'"

If `qmake` does not allow for `make install` or does not install the desktop file and icon, then you need to change your `.pro` file it similar to https://github.com/probonopd/FeedTheMonkey/blob/master/FeedTheMonkey.pro.

[Here](https://github.com/ilia3101/MLV-App/issues/17#issuecomment-393507203) is another simple example.

It is common on Unix to also use the build tool to install applications and libraries; for example, by invoking `make install`. For this reason, `qmake` has the concept of an install set, an object which contains instructions about the way a part of a project is to be installed.

Please see the section "Installing Files" on http://doc.qt.io/qt-5/qmake-advanced-usage.html.

### For projects that use CMake, autotools, or meson with ninja instead of qmake

```
  - make INSTALL_ROOT=appdir install ; find appdir/
```

__CMake__ wants `DESTDIR` instead:

```
  - cmake . -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr
  - make -j$(nproc)
  - make DESTDIR=appdir -j$(nproc) install ; find appdir/
```

Some applications have the bad habit of relying on CMake versions newer than what comes with the oldest still-supported distributions. In this case, install a newer CMake with

```
  - sudo rm -rf /usr/bin/cmake /usr/local/cmake-* /usr/local/bin/cmake || true # Needed on Travis CI; don't do this on other systems!
  - wget "https://github.com/Kitware/CMake/releases/download/v3.13.2/cmake-3.13.2-Linux-x86_64.tar.gz" ; sudo tar xf  cmake*.tar.gz --strip-components=1 -C /usr
```

Under some circumstances it may also be required to add `-DCMAKE_INSTALL_LIBDIR=/usr/lib` to the `cmake` call.

__autotools__ (the dinosaur that spends precious minutes "checking...") wants `DESTDIR` too but insists on an absolute link which we can feed it using readlink:

```
  - ./configure --prefix=/usr
  - make -j$(nproc)
  - make install DESTDIR=$(readlink -f appdir) ; find appdir/
```

Caution if you encounter

```
qmake PREFIX=/usr CONFIG+=use_qt_paths
```

Here, `CONFIG+=use_qt_paths` needs to be removed, otherwise it will install everything under the Qt installation paths in `/opt/qt58` when using the beineri ppa.

The exception is that you are building Qt libraries that _should_ be installed to the same location where Qt resides on your system, from where it will be picked up by `linuxdeployqt`.

__meson with ninja__ [apparently](https://github.com/openAVproductions/openAV-Luppp/pull/270/commits/fa160a46d908e57b2c5b7bdb47cb5a089e08c212) wants

```
  - meson --prefix /usr build
  - ninja -C build
  - DESTDIR=./appdir ninja -C build install ; find build/appdir
```

### When using Qt from distribution packages

On Ubuntu 14.04, you will need to pass in `-qmake=/usr/lib/x86_64-linux-gnu/qt5/bin/qmake` when using distribution packages.

### A note on DESTDIR

According to https://dwheeler.com/essays/automating-destdir.html, 

> Automating DESTDIR can be a pain, so it’s best if the program supports it to start with; my package Auto-DESTDIR can automatically support DESTDIR in some cases if the program installation does not support it to begin with.

Also see https://www.gnu.org/prep/standards/html_node/DESTDIR.html for more information.

### Sending Pull Requests on GitHub

`linuxdeployqt` is great for upstream application projects that want to release their software in binary form to Linux users quickly and without much overhead. If you would like to see a particular application use `linuxdeployqt`, then sending a Pull Request may be an option to get the upstream application project to consider it. You can use the following template text for Pull Requests but make sure to customize it to the project in question.

```
This PR, when merged, will compile this application on [Travis CI](https://travis-ci.org/) upon each `git push`, and upload an [AppImage](http://appimage.org/) to your GitHub Releases page.

Providing an [AppImage](http://appimage.org/) would have, among others, these advantages:
- Applications packaged as an AppImage can run on many distributions (including Ubuntu, Fedora, openSUSE, CentOS, elementaryOS, Linux Mint, and others)
- One app = one file = super simple for users: just download one AppImage file, [make it executable](http://discourse.appimage.org/t/how-to-make-an-appimage-executable/80), and run
- No unpacking or installation necessary
- No root needed
- No system libraries changed
- Works out of the box, no installation of runtimes needed
- Optional desktop integration with `appimaged`
- Optional binary delta updates, e.g., for continuous builds (only download the binary diff) using AppImageUpdate
- Can optionally GPG2-sign your AppImages (inside the file)
- Works on Live ISOs
- Can use the same AppImages when dual-booting multiple distributions
- Can be listed in the [AppImageHub](https://appimage.github.io/) central directory of available AppImages
- Can double as a self-extracting compressed archive with the `--appimage-extract` parameter
- No repositories needed. Suitable/optimized for air-gapped (offline) machines
- Decentralized

[Here is an overview](https://appimage.github.io/apps) of projects that are already distributing upstream-provided, official AppImages.

__PLEASE NOTE:__ For this to work, you need to set up `GITHUB_TOKEN` in Travis CI for this to work; please see https://github.com/probonopd/uploadtool.
If you would like to see only one entry for the Pull Request in your project's history, then please enable [this GitHub functionality](https://help.github.com/articles/configuring-commit-squashing-for-pull-requests/) on your repo. It allows you to squash (combine) the commits when merging.

If you have questions, AppImage developers are on #AppImage on irc.freenode.net.
```

## Projects using linuxdeployqt

These projects are already using [Travis CI](http://travis-ci.org/) and linuxdeployqt to provide AppImages of their builds:
- https://github.com/probonopd/ImageMagick
- https://github.com/Subsurface-divelog/subsurface/
- https://github.com/jimevins/glabels-qt
- https://travis-ci.org/NeoTheFox/RepRaptor
- https://github.com/electronpass/electronpass-desktop
- https://github.com/lirios/browser
- https://github.com/jeena/FeedTheMonkey
- https://github.com/labsquare/fastQt/
- https://github.com/sqlitebrowser/sqlitebrowser/
- https://github.com/neuro-sys/tumblr-downloader-client
- https://github.com/LongSoft/UEFITool
- https://github.com/dannagle/PacketSender
- https://github.com/nuttyartist/notes
- https://github.com/leozide/leocad/
- https://github.com/Blinkinlabs/PatternPaint
- https://github.com/fathomssen/redtimer
- https://github.com/coryo/amphetype2
- https://github.com/chkmue/MyQtTravisTemplateProject
- https://github.com/chkmue/qttravisCI_1
- https://github.com/eteran/edb-debugger
- https://github.com/crapp/labpowerqt/
- https://github.com/probonopd/linuxdeployqt/ obviously ;-)
- https://github.com/xdgurl/xdgurl
- https://github.com/QNapi/qnapi
- https://github.com/m-o-s-t-a-f-a/dana
- https://github.com/patrickelectric/mini-qml
- https://github.com/vaibhavpandeyvpz/apkstudio

These projects are using GitHub Actions and linuxdeployqt to provide AppImages of their builds:
- https://github.com/pbek/QOwnNotes ([build-release.yml](https://github.com/pbek/QOwnNotes/blob/develop/.github/workflows/build-release.yml))
- https://github.com/bjorn/tiled/ ([workflow](https://github.com/bjorn/tiled/blob/master/.github/workflows/packages.yml))

This project is already using linuxdeployqt in a custom Jenkins workflow:
- https://github.com/appimage-packages/

This GitHub template invokes linuxdeployqt during a GitHub CI Action:
- https://github.com/219-design/qt-qml-project-template-with-ci

These projects are already using linuxdeployqt:
- Autodesk EAGLE for Linux http://www.autodesk.com/products/eagle/free-download
- https://github.com/evpo/EncryptPad
- https://github.com/grahamrow/Muview2
- https://github.com/freemountain/quark/
- https://github.com/Mr0815/geraetepruefung/

This project on GitLab uses linuxdeployqt:

- https://gitlab.com/rpdev/opentodolist

This project can be bundled successfully using linuxdeployqt:

- https://gitlab.com/rpdev/opentodolist/issues/96


## Contributing

One great way to contribute is to send Pull Requests to the application projects you'd like to see use linuxdeployqt, as described above. You are also welcome to contribute to linuxdeployqt development itself. Please discuss in the [forum](http://discourse.appimage.org/t/linuxdeployqt-new-linux-deployment-tool-for-qt/57) or using GitHub issues and Pull Requests.

## Contact

The developers are in the channel #AppImage on irc.freenode.net
