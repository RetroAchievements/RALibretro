# Toolset setup
CC=gcc
CXX=g++
RC=windres
INCLUDES=-Isrc -I../RA_Integration/rapidjson/include
DEFINES=-DOUTSIDE_SPEEX -DRANDOM_PREFIX=speex -DEXPORT= -D_USE_SSE2 -DFIXED_POINT
#CCFLAGS=-Wall -O3 $(INCLUDES) $(DEFINES) `sdl2-config --cflags`
CCFLAGS=-Wall -O0 -g -m32 $(INCLUDES) $(DEFINES) `sdl2-config --cflags`
CXXFLAGS=$(CCFLAGS) -std=c++11
LDFLAGS=-m32

# main
LIBS=`sdl2-config --libs` -lopengl32 -lwinhttp
OBJS=\
	src/dynlib/dynlib.o \
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
	src/main.o \
	src/menu.res

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
