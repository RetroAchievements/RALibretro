# supported parameters
#  ARCH           architecture - "x86" or "x64" [detected if not set]
#  DEBUG          if set to anything, builds with DEBUG symbols

include Makefile.common

ifeq ($(ARCH), arm64)
  $(error Cannot build for arm64)
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
endif

# compile flags
INCLUDES += -I./src/RAInterface -I./src/SDL2/include
DEFINES=-DOUTSIDE_SPEEX -DRANDOM_PREFIX=speex -DEXPORT= -D_USE_SSE2 -DFIXED_POINT -D_WINDOWS
CFLAGS += $(DEFINES)
CXXFLAGS += $(DEFINES)

ifeq ($(ARCH), x86)
  SDLLIBDIR=src/SDL2/lib/x86
else
  SDLLIBDIR=src/SDL2/lib/x64
endif

ifneq ($(DEBUG),)
  CFLAGS   += -DDEBUG_FSM -DLOG_TO_FILE
  CXXFLAGS += -DDEBUG_FSM -DLOG_TO_FILE
else
  CFLAGS   += -DLOG_TO_FILE
  CXXFLAGS += -DLOG_TO_FILE
endif

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
	src/components/Microphone.o \
	src/components/Video.o \
	src/components/VideoContext.o \
	src/libmincrypt/sha256.o \
	src/miniz/miniz.o \
	src/miniz/miniz_tdef.o \
	src/miniz/miniz_tinfl.o \
	src/miniz/miniz_zip.o \
	src/rcheevos/src/rcheevos/consoleinfo.o \
	src/rcheevos/src/rc_libretro.o \
	src/rcheevos/src/rhash/aes.o \
	src/rcheevos/src/rhash/cdreader.o \
	src/rcheevos/src/rhash/md5.o \
	src/rcheevos/src/rhash/hash.o \
	src/rcheevos/src/rhash/hash_disc.o \
	src/rcheevos/src/rhash/hash_encrypted.o \
	src/rcheevos/src/rhash/hash_rom.o \
	src/rcheevos/src/rhash/hash_zip.o \
	src/speex/resample.o \
	src/About.o \
	src/Application.o \
	src/CdRom.o \
	src/Emulator.o \
	src/Fsm.o \
	src/Gl.o \
	src/GlUtil.o \
	src/Hash.o \
	src/Hash3DS.o \
	src/KeyBinds.o \
	src/main.o \
	src/Memory.o \
	src/menu.res \
	src/States.o \
	src/Util.o

ifdef HAVE_CHD
  OBJS += $(CHD_OBJS) \
          src/HashCHD.o
endif

all: $(OUTDIR)/RALibretro.exe $(OUTDIR)/SDL2.dll $(OUTDIR)/libwinpthread-1.dll

src/rcheevos/src/rc_libretro.o: CFLAGS += -I./src/libretro

src/components/Config.o: CFLAGS += -I./src/libretro

src/Memory.o: CFLAGS += -I./src/libretro

src/Hash.o: CFLAGS += -I./src/libretro

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

src/RA_BuildVer.h: .git/HEAD
	src/RAInterface/MakeBuildVer.sh src/RA_BuildVer.h "" RA_LIBRETRO

src/RA_Implementation.o: src/RA_Implementation.cpp src/RA_BuildVer.h
	$(CXX) $(CXXFLAGS) -c $< -o $@

%.res: %.rc
	$(RC) $< -O coff -o $@

$(OUTDIR)/SDL2.dll: $(SDLLIBDIR)/SDL2.dll
	cp -f $< $@

$(OUTDIR)/libwinpthread-1.dll: $(MINGWLIBDIR)/libwinpthread-1.dll
	cp -f $< $@

$(OUTDIR)/RALibretro.exe: $(OBJS)
	mkdir -p $(OUTDIR)
	$(CXX) -o $@ $+ $(LDFLAGS)

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
