# Logic tests for the firmware, run on the PC (no hardware, no screen).
#   make test      game rules, AI strength, and the two-console games over a simulated radio
# Needs the Adafruit GFX library and the emulator's stubs.
GFX ?= $(firstword $(wildcard $(HOME)/Arduino/libraries/Adafruit_GFX_Library \
                              $(HOME)/Documents/Arduino/libraries/Adafruit_GFX_Library \
                              $(USERPROFILE)/Documents/Arduino/libraries/Adafruit_GFX_Library))
FW  := ../../GameSystem
EM  := ../emulator
ifeq ($(OS),Windows_NT)
  PLATFORM := $(EM)/platform_win.cpp
  EXE := .exe
  SYSLIBS := -lws2_32 -lwinmm
else
  PLATFORM := $(EM)/platform_posix.cpp
  EXE :=
  SYSLIBS :=
endif
PY ?= python3
CXX_FLAGS := -std=gnu++17 -O1 -w -DARDUINO=100 -I $(EM)/stubs -I $(EM) -I $(GFX) -I $(FW)
CORE := hostenv.cpp $(EM)/platform_common.cpp $(PLATFORM) $(GFX)/Adafruit_GFX.cpp $(FW)/display.cpp $(FW)/input.cpp $(FW)/ui.cpp $(FW)/fx.cpp
LINK := $(FW)/lobby.cpp $(FW)/gamelink.cpp $(FW)/crypto.cpp
GAMES := $(addsuffix .cpp,$(addprefix $(FW)/,snake tetris flappy game2048 breakout frogger helicopter pacman pong invaders dino))

PROGRAMS := logic_test ttt_test rps_test vs_test settings_test pbv_test chess_test chess_vs_test checkers_test checkers_vs_test mines_test chat_test

test: $(addsuffix $(EXE),$(PROGRAMS))
	./logic_test$(EXE)
	./settings_test$(EXE)
	$(MAKE) -C $(EM) --no-print-directory > /dev/null && $(PY) persistence_check.py $(EM)/emulator$(EXE)
	./ttt_test$(EXE)
	./chess_test$(EXE)
	./checkers_test$(EXE)
	./mines_test$(EXE)
	$(PY) board_check.py chess .
	$(PY) board_check.py checkers .
	./rps_test$(EXE)
	$(MAKE) -C $(EM) --no-print-directory > /dev/null && $(PY) video_check.py ./pbv_test$(EXE) $(EM)/emulator$(EXE)
	./chat_test$(EXE) unit
	$(PY) chat_check.py .
	$(PY) vs_check.py .

logic_test$(EXE): logic_test.cpp $(CORE) $(LINK) $(GAMES)
	g++ $(CXX_FLAGS) -o $@ logic_test.cpp $(CORE) $(LINK) $(GAMES) $(SYSLIBS)

ttt_test$(EXE): ttt_test.cpp $(CORE) $(FW)/tictactoe.cpp $(LINK)
	g++ $(CXX_FLAGS) -o $@ ttt_test.cpp $(CORE) $(FW)/tictactoe.cpp $(LINK) $(SYSLIBS)

rps_test$(EXE): rps_test.cpp $(CORE) $(FW)/rps.cpp $(LINK)
	g++ $(CXX_FLAGS) -o $@ rps_test.cpp $(CORE) $(FW)/rps.cpp $(LINK) $(SYSLIBS)

vs_test$(EXE): vs_test.cpp air.cpp $(CORE) $(LINK) $(FW)/pong.cpp $(FW)/tictactoe.cpp $(FW)/rps.cpp $(FW)/connect4.cpp
	g++ $(CXX_FLAGS) -DUSE_AIR -o $@ vs_test.cpp air.cpp $(CORE) $(LINK) $(FW)/pong.cpp $(FW)/tictactoe.cpp $(FW)/rps.cpp $(FW)/connect4.cpp $(SYSLIBS)

pbv_test$(EXE): pbv_test.cpp $(FW)/pbvcodec.cpp
	g++ $(CXX_FLAGS) -o $@ pbv_test.cpp $(FW)/pbvcodec.cpp

chat_test$(EXE): chat_test.cpp air.cpp $(CORE) $(LINK) $(FW)/chat.cpp $(FW)/contacts.cpp $(FW)/device.cpp
	g++ $(CXX_FLAGS) -DUSE_AIR -o $@ chat_test.cpp air.cpp $(CORE) $(LINK) $(FW)/chat.cpp $(FW)/contacts.cpp $(FW)/device.cpp $(SYSLIBS)

chess_test$(EXE): chess_test.cpp $(FW)/chessengine.cpp
	g++ $(CXX_FLAGS) -o $@ chess_test.cpp $(FW)/chessengine.cpp $(SYSLIBS)

checkers_test$(EXE): checkers_test.cpp $(FW)/checkersengine.cpp
	g++ $(CXX_FLAGS) -o $@ checkers_test.cpp $(FW)/checkersengine.cpp $(SYSLIBS)

checkers_vs_test$(EXE): checkers_vs_test.cpp air.cpp $(CORE) $(LINK) $(FW)/checkers.cpp $(FW)/checkersengine.cpp
	g++ $(CXX_FLAGS) -DUSE_AIR -o $@ checkers_vs_test.cpp air.cpp $(CORE) $(LINK) $(FW)/checkers.cpp $(FW)/checkersengine.cpp $(SYSLIBS)

mines_test$(EXE): mines_test.cpp $(FW)/mines.cpp
	g++ $(CXX_FLAGS) -o $@ mines_test.cpp $(FW)/mines.cpp $(SYSLIBS)

chess_vs_test$(EXE): chess_vs_test.cpp air.cpp $(CORE) $(LINK) $(FW)/chess.cpp $(FW)/chessengine.cpp
	g++ $(CXX_FLAGS) -DUSE_AIR -o $@ chess_vs_test.cpp air.cpp $(CORE) $(LINK) $(FW)/chess.cpp $(FW)/chessengine.cpp $(SYSLIBS)

settings_test$(EXE): settings_test.cpp $(CORE) $(FW)/settings.cpp
	g++ $(CXX_FLAGS) -o $@ settings_test.cpp $(CORE) $(FW)/settings.cpp $(SYSLIBS)

# firmware update test: three builds with different, fixed version numbers talk to each other
# (fixed, so raising OS_VERSION in config.h never changes what the test means)
# (then: python3 update_check.py)
ifneq ($(EXE),)
update_test: update_test$(EXE)
endif
update_test$(EXE): update_test.cpp air.cpp $(CORE) $(LINK) $(FW)/fwupdate.cpp
	g++ $(CXX_FLAGS) -DUSE_AIR '-DOS_VERSION="v4.2"' -DOS_VERSION_NUM=402 -o update_test$(EXE) update_test.cpp air.cpp $(CORE) $(LINK) $(FW)/fwupdate.cpp $(SYSLIBS)
	g++ $(CXX_FLAGS) -DUSE_AIR '-DOS_VERSION="v4.3"' -DOS_VERSION_NUM=403 -o update_test_new$(EXE) update_test.cpp air.cpp $(CORE) $(LINK) $(FW)/fwupdate.cpp $(SYSLIBS)
	g++ $(CXX_FLAGS) -DUSE_AIR '-DOS_VERSION="v4.0"' -DOS_VERSION_NUM=400 -o update_test_old$(EXE) update_test.cpp air.cpp $(CORE) $(LINK) $(FW)/fwupdate.cpp $(SYSLIBS)

clean:
	rm -f chat_test update_test update_test_new update_test_old logic_test ttt_test rps_test vs_test settings_test *.exe

c4_test$(EXE): c4_test.cpp air.cpp $(CORE) $(LINK) $(FW)/connect4.cpp
	g++ $(CXX_FLAGS) -DUSE_AIR -o $@ c4_test.cpp air.cpp $(CORE) $(LINK) $(FW)/connect4.cpp $(SYSLIBS)
