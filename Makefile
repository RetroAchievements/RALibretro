# Toolset setup
CC=gcc
CXX=g++
RC=windres
INCLUDES=-Isrc -I../RA_Integration/rapidjson/include
DEFINES=-DOUTSIDE_SPEEX -DRANDOM_PREFIX=speex -DEXPORT= -D_USE_SSE2 -DFIXED_POINT
CCFLAGS=-Wall -m32 $(INCLUDES) $(DEFINES) `sdl2-config --cflags`
CXXFLAGS=$(CCFLAGS) -std=c++11
LDFLAGS=-m32

ifneq ($(DEBUG),)
  CFLAGS   += -O0 -g
  CXXFLAGS += -O0 -g
else
  CFLAGS   += -O3 -DNDEBUG
  CXXFLAGS += -O3 -DNDEBUG
endif

# main
LIBS=`sdl2-config --libs` -lopengl32 -lwinhttp
OBJS=\
	src/dynlib/dynlib.o \
	src/jsonsax/jsonsax.o \
	src/libretro/BareCore.o \
	src/libretro/Core.o \
	src/RA_Integration/RA_Implementation.o \
	src/RA_Integration/RA_Interface.o \
	src/SDLComponents/Audio.o \
	src/SDLComponents/Config.o \
	src/SDLComponents/Dialog.o \
	src/SDLComponents/Input.o \
	src/SDLComponents/Logger.o \
	src/SDLComponents/Video.o \
	src/speex/resample.o \
	src/Application.o \
	src/Fsm.o \
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

all: bin/RALibretro

bin/RALibretro: $(OBJS)
	$(CXX) $(LDFLAGS) -o $@ $+ $(LIBS)

clean:
	rm -f bin/RALibretro $(OBJS)

.PHONY: clean
