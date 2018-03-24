# Building RALibretro with MSYS2

## Install MSYS2

1. Go to [http://www.msys2.org/](http://www.msys2.org/) and download the 32 bit version.
1. Follow the installation instructions on the site [http://www.msys2.org/](http://www.msys2.org/).
1. When on the prompt for the first time, run `pacman -Syu` (**NOTE**: at the end of this command it will ask you to close the terminal window **without** going back to the prompt.)
1. Launch the MSYS2 terminal again and run `pacman -Su`

## Install the toolchain

```
$ pacman -S make git zip mingw-w64-i686-gcc mingw-w64-i686-SDL2 mingw-w64-i686-gcc-libs
```

## Clone the repo

```
$ git clone --recursive --depth 1 https://github.com/leiradel/RASuite.git
```

## Build

```
$ cd /path/to/RASuite/RALibretro
$ make
```

**NOTE**: use `make` for a release build or `make DEBUG=1` for a debug build. Don't forget to run `make clean` first if switching between a release build and a debug build.

# Building RALibretro with Visual Studio

1. Download SDL2-devel-XXXX-VC.xip from https://www.libsdl.org/download-2.0.php
1. Extract to SDL2 subdirectory in the RALibretro folder of your checkout
1. Load `RALibretro.sln` in Visual Studio
