# Building RALibretro with MSYS2

## 1. install msys2

1. Go to [http://www.msys2.org/](http://www.msys2.org/) and download the 32 bit version (not mandatory, but recommended).

2. Follow the installation instructions on the site [http://www.msys2.org/](http://www.msys2.org/).

3. When on the prompt for the first time, perform (**NOTE**: at the end of this command it will ask you to close the terminal window without going back to the prompt.): `pacman -Syu`

4. Launch the msys2 terminal again and: `pacman -Su`

5. Done! msys2 is ready!


## 2. installing the toolchain

```
pacman -S make git zip mingw-w64-i686-gcc mingw-w64-i686-SDL2 mingw-w64-i686-gcc-libs
```


## 3. cloning the repo

```
git clone --recursive --depth 1 https://github.com/leiradel/RASuite.git
```


## 4. building

```
cd /path/to/RASuite/RALibretro
make
```

**NOTE**: use `make` for a release build or `make DEBUG=1` for a debug build.
And don't forget to run `make clean` first if switching between a release build and a debug build.


# Building RALibretro with Visual Studio

1. download SDL2-devel-XXXX-VC.xip from https://www.libsdl.org/download-2.0.php

2. extract to SDL2 subdirectory in the RALibretro folder of your checkout
