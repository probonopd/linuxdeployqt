# linuxdeployqt

This Linux Deployment Tool for Qt, `linuxdeployqt`, takes an application as input and makes it self-contained by copying in the Qt libraries and plugins that the application uses into a bundle. This can optionally be put into an [AppImage](http://appimage.org/).

## Differences to macdeployqt
This tool is conceptually based on the [Mac Deployment Tool](http://doc.qt.io/qt-5/osx-deployment.html), `macdeployqt` in the tools applications of the Qt Toolkit, but has been changed to a slightly different logic and other tools needed for Linux.

* Instead of an `.app` bundle for macOS, this produces an [AppDir](http://rox.sourceforge.net/desktop/AppDirs.html) for Linux
* Instead of a `.dmg` disk image for macOS, this produces an [AppImage](http://appimage.org/) for Linux which is quite similar to a dmg but executes the contained application rather than just opening a window on the desktop from where the application can be launched

## Known issues

* __This may not be fully working yet.__ Use with care, run with maximum verbosity, submit issues and pull requests. Help is appreciated
* Some functions may still refer to macOS specifics. These need to be converted over to their Linux counterparts or deleted
* Scan for QML imports has not been tested yet

## Installation

* Open in Qt Creator and build
* Build and install [patchelf](https://nixos.org/patchelf.html) (a small utility to modify the dynamic linker and RPATH of ELF executables; similar to `install_name_tool` on macOS). To learn more about this, see http://blog.qt.io/blog/2011/10/28/rpath-and-runpath/
* Download [AppImageAssistant](https://github.com/probonopd/AppImagaeKit/releases) and put it into your $PATH, e.g., into `/usr/local/bin`. Make sure it is renamed to `AppImageAssistant` and is `chmod a+x`

## Usage

```
Usage: linuxdeployqt app-binary [options]

Options:
   -verbose=<0-3>     : 0 = no output, 1 = error/warning (default), 2 = normal, 3 = debug
   -no-plugins        : Skip plugin deployment
   -appimage          : Create an AppImage
   -no-strip          : Don't run 'strip' on the binaries
   -executable=<path> : Let the given executable use the deployed libraries too
   -qmldir=<path>     : Scan for QML imports in the given path
   -always-overwrite  : Copy files even if the target file exists
   -libpath=<path>    : Add the given path to the library search path

linuxdeployqt takes an application as input and makes it
self-contained by copying in the Qt libraries and plugins that
the application uses.
```
