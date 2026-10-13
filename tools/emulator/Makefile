# Builds the PC emulator from the firmware sources (Linux, macOS, and Windows
# with MSYS2/MinGW-w64). CMake does the same job: see ../CMakeLists.txt.
# Needs the Adafruit GFX library (the one the Arduino IDE installed, or set GFX=).
# With SDL2 installed it also gets a real window; without, it runs in the terminal only.
GFX ?= $(firstword $(wildcard $(HOME)/Arduino/libraries/Adafruit_GFX_Library \
                              $(HOME)/Documents/Arduino/libraries/Adafruit_GFX_Library \
                              $(USERPROFILE)/Documents/Arduino/libraries/Adafruit_GFX_Library))
FW  := ../../GameSystem

ifeq ($(OS),Windows_NT)
  PLATFORM := platform_win.cpp
  EXE := .exe
  SYSLIBS := -lws2_32 -lwinmm
else
  PLATFORM := platform_posix.cpp
  EXE :=
  SYSLIBS :=
endif

SDL_CFLAGS := $(shell pkg-config --cflags sdl2 2>/dev/null)
SDL_LIBS   := $(shell pkg-config --libs sdl2 2>/dev/null)
ifneq ($(SDL_LIBS),)
  SDL_DEFS := -DUSE_SDL
endif

emulator$(EXE): host.cpp capture.h platform.h platform_common.cpp $(PLATFORM) $(wildcard $(FW)/*.cpp) $(GFX)/Adafruit_GFX.cpp
	g++ -std=gnu++17 -O1 -w -DARDUINO=100 $(SDL_DEFS) $(SDL_CFLAGS) -I stubs -I . -I $(GFX) -I $(FW) -o $@ host.cpp platform_common.cpp $(PLATFORM) $(wildcard $(FW)/*.cpp) $(GFX)/Adafruit_GFX.cpp $(SDL_LIBS) $(SYSLIBS)

clean:
	rm -f emulator emulator.exe
