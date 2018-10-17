# RALibretro

RALibretro is a multi-emulator that can be used to develop RetroAchievements and, of course, earn them.

The "multi-emulation" feature is only possible because it uses [libretro](https://github.com/libretro/) cores to do the actual emulation. What RALibretro does is to connect the emulation to the tools used to create RetroAchievements.

The integration with RetroAchievements.org site is done by the [RAIntegration](https://github.com/RetroAchievements/RAIntegration).


## Building RALibretro with MSYS2/Makefile

### Install MSYS2

1. Go to [http://www.msys2.org/](http://www.msys2.org/) and download the 32 bit version.
2. Follow the installation instructions on the site [http://www.msys2.org/](http://www.msys2.org/).
3. When on the prompt for the first time, run `pacman -Syu` (**NOTE**: at the end of this command it will ask you to close the terminal window **without** going back to the prompt.)
4. Launch the MSYS2 terminal again and run `pacman -Su`

### Install the toolchain

```
$ pacman -S make git zip mingw-w64-i686-gcc mingw-w64-i686-SDL2 mingw-w64-i686-gcc-libs
```

### Clone the repo

```
$ git clone --recursive --depth 1 https://github.com/RetroAchievements/RALibretro.git
```

### Fetch dependencies
```
$ cd RALibretro
$ ./dl-cores.sh all
```

### Build

```
$ make
```

**NOTE**: use `make` for a release build or `make DEBUG=1` for a debug build. Don't forget to run `make clean` first if switching between a release build and a debug build.

## Building RALibretro with Visual Studio

### Clone the repo

```
> git clone --recursive --depth 1 https://github.com/RetroAchievements/RALibretro.git
```

### Fetch dependencies
The core fetch script requires the GNU `wget` and `unzip` utilities (e.g. [GnuWin32](http://gnuwin32.sourceforge.net/)) to be accessible through the command line.

```
> .\dl-cores.bat all
```

Other dependencies: `SDL2.dll`, `libgcc_s_dw2-1.dll`, `libgcc_s_sjlj-1.dll`, `libstdc++-6.dll`, `libwinpthread-1.dll`

### Build

Load `RALibretro.sln` in Visual Studio and build it.
