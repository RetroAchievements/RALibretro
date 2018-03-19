Installing MSys2:
1) install MSys2: http://www.msys2.org/
2) in a terminal, run "pacman -Syu"
3) close terminal as instructed
4) in a new MinGW 32-bit terminal, run "pacman -Su"
5) install dev tools "pacman -S make git zip mingw-w64-i686-gcc mingw-w64-i686-gcc-libs"
6) install Windows SDL2 "pacman -S mingw-w64-i686-SDL2"

Builing ralibretro:
1) open a MinGW 32-bit terminal
2) change to the code directory. Paths are linux-style with the root directory being the drive:
   "cd /e/RetroAchievements/RASuiteLibRetro/RALibretro"
3) make sure the bin directory exists: "mkdir bin"
4) run make: "make" for a release build or "make DEBUG=y" for a debug build.
   run "make clean" first if switching between a release build and a debug build.
   
   
Visual Studio:
1) download SDL2-devel-XXXX-VC.xip from https://www.libsdl.org/download-2.0.php
2) extract to SDL2 subdirectory in the RALibretro folder of your checkout