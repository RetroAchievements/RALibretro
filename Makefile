# IMPORTANT NOTE FOR CROSS-COMPILING:
# you need to change your PATH to invoke the mingw-32bit flavor of sdl2-config

# Toolset setup
ifeq ($(OS),Windows_NT)
    CC=gcc
    CXX=g++
    RC=windres
else ifeq ($(shell uname -s),Linux)
    CC=i686-w64-mingw32-gcc
    CXX=i686-w64-mingw32-g++
    RC=i686-w64-mingw32-windres
    STATICLIBS=-static-libstdc++ 
endif

INCLUDES=-Isrc -I./src/RAInterface -I./src/miniz
DEFINES=-DOUTSIDE_SPEEX -DRANDOM_PREFIX=speex -DEXPORT= -D_USE_SSE2 -DFIXED_POINT
CCFLAGS=-Wall -m32 $(INCLUDES) $(DEFINES) `sdl2-config --cflags`
CXXFLAGS=$(CCFLAGS) -std=c++11
LDFLAGS=-m32


ifneq ($(DEBUG),)
  CFLAGS   += -O0 -g -DDEBUG_FSM -DLOG_TO_FILE
  CXXFLAGS += -O0 -g -DDEBUG_FSM -DLOG_TO_FILE
else
  CFLAGS   += -O3 -DNDEBUG -DLOG_TO_FILE
  CXXFLAGS += -O3 -DNDEBUG -DLOG_TO_FILE
endif

# main
LIBS=`sdl2-config --static-libs` $(STATICLIBS) -lopengl32 -lwinhttp
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
	src/miniz/miniz.o \
	src/miniz/miniz_tdef.o \
	src/miniz/miniz_tinfl.o \
	src/miniz/miniz_zip.o \
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
	src/menu.res \
	src/Util.o

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

%.o: %.c
	$(CC) $(CCFLAGS) -c $< -o $@

%.res: %.rc
	$(RC) $< -O coff -o $@

all: bin/RALibretro.exe

bin/RALibretro.exe: $(OBJS)
	mkdir -p bin
	$(CXX) $(LDFLAGS) -o $@ $+ $(LIBS)

src/Git.cpp: etc/Git.cpp.template FORCE
	cat $< | sed s/GITFULLHASH/`git rev-parse HEAD | tr -d "\n"`/g | sed s/GITMINIHASH/`git rev-parse HEAD | tr -d "\n" | cut -c 1-7`/g | sed s/GITRELEASE/`git describe --tags | sed s/\-.*//g | tr -d "\n"`/g > $@

zip:
	rm -f bin/RALibretro-*.zip RALibretro-*.zip
	zip -9 RALibretro-`git describe | tr -d "\n"`.zip bin/RALibretro.exe

clean:
	rm -f bin/RALibretro $(OBJS) bin/RALibretro-*.zip RALibretro-*.zip

pack:
ifeq ("", "$(wildcard bin/RALibretro.exe)")
	echo '"bin/RALibretro.exe" not found!'
else
	rm -f bin/RALibretro-*.zip RALibretro-*.zip
	zip -9r RALibretro-pack-`git describe | tr -d "\n"`.zip bin
endif

.PHONY: clean FORCE
