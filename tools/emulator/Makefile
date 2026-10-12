# Builds the PC emulator from the firmware sources.
# Needs the Adafruit GFX library (the one the Arduino IDE installed). With SDL2
# installed it also gets a real window; without, it runs in the terminal only.
GFX ?= $(HOME)/Arduino/libraries/Adafruit_GFX_Library
FW  := ../../GameSystem

SDL_CFLAGS := $(shell pkg-config --cflags sdl2 2>/dev/null)
SDL_LIBS   := $(shell pkg-config --libs sdl2 2>/dev/null)
ifneq ($(SDL_LIBS),)
  SDL_DEFS := -DUSE_SDL
endif

emulator: host.cpp capture.h $(wildcard $(FW)/*.cpp) $(GFX)/Adafruit_GFX.cpp
	g++ -std=gnu++17 -O1 -w -DARDUINO=100 $(SDL_DEFS) $(SDL_CFLAGS) -I stubs -I $(GFX) -I $(FW) -o $@ host.cpp $(wildcard $(FW)/*.cpp) $(GFX)/Adafruit_GFX.cpp $(SDL_LIBS)

clean:
	rm -f emulator
