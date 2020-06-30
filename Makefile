# supported parameters
#  ARCH           architecture - "x86" or "x64" [detected if not set]
#  DEBUG          if set to anything, builds with DEBUG symbols

SDL2CONFIG=sdl2-config

# default parameter values
ifeq ($(ARCH),)
  UNAME := $(shell uname -s)
  ifeq ($(findstring MINGW64, $(UNAME)), MINGW64)
    ARCH=x64
  else ifeq ($(findstring MINGW32, $(UNAME)), MINGW32)
    ARCH=x86
  else
    $(error Could not determine ARCH)
  endif
endif

# Toolset setup
ifeq ($(OS),Windows_NT)
  CC=gcc
  CXX=g++
  RC=windres
  
  ifeq ($(ARCH), x86)
    MINGWLIBDIR=/mingw32/bin
  else
    MINGWLIBDIR=/mingw64/bin
  endif

else ifeq ($(shell uname -s),Linux)
  MINGW=x86_64-w64-mingw32
  ifeq ($(ARCH), x86)
    MINGW=i686-w64-mingw32
  endif

  CC=$(MINGW)-gcc
  CXX=$(MINGW)-g++
  RC=$(MINGW)-windres
  
  MINGWLIBDIR=/usr/$(MINGW)/lib

  # make sure to use the mingw-32bit flavor of sdl2-config
  ifneq (,$(wildcard /usr/lib/mxe/usr/i686-w64-mingw32.static/bin/sdl2-config))
    SDL2CONFIG=/usr/lib/mxe/usr/i686-w64-mingw32.static/bin/sdl2-config
  endif
endif

# compile flags
INCLUDES=-Isrc -I./src/RAInterface -I./src/miniz -I./src/rcheevos/include
DEFINES=-DOUTSIDE_SPEEX -DRANDOM_PREFIX=speex -DEXPORT= -D_USE_SSE2 -DFIXED_POINT -D_WINDOWS
CCFLAGS=-Wall $(INCLUDES) $(DEFINES) `$(SDL2CONFIG) --cflags`
CXXFLAGS=$(CCFLAGS) -std=c++11

ifeq ($(ARCH), x86)
  CFLAGS += -m32
  CXXFLAGS += -m32
  LDFLAGS += -m32
  OUTDIR=bin
  SDLLIBDIR=src/SDL2/lib/x86
else ifeq ($(ARCH), x64)
  CFLAGS += -m64
  CXXFLAGS += -m64
  LDFLAGS += -m64
  OUTDIR=bin64
  SDLLIBDIR=src/SDL2/lib/x64
else
  $(error unknown ARCH "$(ARCH)")
endif

ifneq ($(DEBUG),)
  CFLAGS   += -O0 -g -DDEBUG_FSM -DLOG_TO_FILE
  CXXFLAGS += -O0 -g -DDEBUG_FSM -DLOG_TO_FILE
else
  CFLAGS   += -O3 -DNDEBUG -DLOG_TO_FILE
  CXXFLAGS += -O3 -DNDEBUG -DLOG_TO_FILE
endif

LDFLAGS += -static-libgcc -static-libstdc++
LDFLAGS += -mwindows -lmingw32 -lopengl32 -lwinhttp -lgdi32 -limm32 -lcomdlg32
LDFLAGS += -L${SDLLIBDIR} -lSDL2main -lSDL2

# main
OBJS=\
	src/dynlib/dynlib.o \
	src/jsonsax/jsonsax.o \
	src/libretro/BareCore.o \
	src/libretro/Core.o \
	src/RA_Implementation.o \
	src/RAInterface/RA_Interface.o \
	src/components/Audio.o \
	src/components/Config.o \
	src/components/Dialog.o \
	src/components/Input.o \
	src/components/Logger.o \
	src/components/Video.o \
	src/components/VideoContext.o \
	src/miniz/miniz.o \
	src/miniz/miniz_tdef.o \
	src/miniz/miniz_tinfl.o \
	src/miniz/miniz_zip.o \
	src/rcheevos/src/rcheevos/consoleinfo.o \
	src/rcheevos/src/rhash/cdreader.o \
	src/rcheevos/src/rhash/md5.o \
	src/rcheevos/src/rhash/hash.o \
	src/speex/resample.o \
	src/About.o \
	src/Application.o \
	src/CdRom.o \
	src/Emulator.o \
	src/Fsm.o \
	src/Git.o \
	src/Gl.o \
	src/GlUtil.o \
	src/Hash.o \
	src/KeyBinds.o \
	src/main.o \
	src/Memory.o \
	src/menu.res \
	src/States.o \
	src/Util.o

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

%.o: %.c
	$(CC) $(CCFLAGS) -c $< -o $@

%.res: %.rc
	$(RC) $< -O coff -o $@

all: $(OUTDIR)/RALibretro.exe $(OUTDIR)/SDL2.dll $(OUTDIR)/libwinpthread-1.dll

$(OUTDIR)/SDL2.dll: $(SDLLIBDIR)/SDL2.dll
	cp -f $< $@

$(OUTDIR)/libwinpthread-1.dll: $(MINGWLIBDIR)/libwinpthread-1.dll
	cp -f $< $@

$(OUTDIR)/RALibretro.exe: $(OBJS)
	mkdir -p $(OUTDIR)
	$(CXX) -o $@ $+ $(LDFLAGS) $(LIBS)

src/Git.cpp: etc/Git.cpp.template FORCE
	cat $< | sed s/GITFULLHASH/`git rev-parse HEAD | tr -d "\n"`/g | sed s/GITMINIHASH/`git rev-parse HEAD | tr -d "\n" | cut -c 1-7`/g | sed s/GITRELEASE/`git describe --tags | sed s/\-.*//g | tr -d "\n"`/g > $@

zip:
	rm -f $(OUTDIR)/RALibretro-*.zip RALibretro-*.zip
	zip -9 RALibretro-`git describe | tr -d "\n"`.zip $(OUTDIR)/RALibretro.exe

clean:
	rm -f $(OUTDIR)/RALibretro.exe $(OBJS) $(OUTDIR)/RALibretro-*.zip RALibretro-*.zip

pack:
ifeq ("", "$(wildcard $(OUTDIR)/RALibretro.exe)")
	echo '"$(OUTDIR)/RALibretro.exe" not found!'
else
	rm -f $(OUTDIR)/RALibretro-*.zip RALibretro-*.zip
	zip -9r RALibretro-pack-`git describe | tr -d "\n"`.zip bin
endif

.PHONY: clean FORCE
