# Logic tests for the firmware, run on the PC (no hardware, no screen).
#   make test      game rules, AI strength, and the two-console games over a simulated radio
# Needs the Adafruit GFX library and the emulator's stubs.
GFX ?= $(HOME)/Arduino/libraries/Adafruit_GFX_Library
FW  := ../../GameSystem
EM  := ../emulator
CXX_FLAGS := -std=gnu++17 -O1 -w -DARDUINO=100 -I $(EM)/stubs -I $(GFX) -I $(FW)
CORE := hostenv.cpp $(GFX)/Adafruit_GFX.cpp $(FW)/display.cpp $(FW)/input.cpp $(FW)/ui.cpp $(FW)/fx.cpp
LINK := $(FW)/lobby.cpp $(FW)/gamelink.cpp $(FW)/crypto.cpp
GAMES := $(addsuffix .cpp,$(addprefix $(FW)/,snake tetris flappy game2048 breakout frogger helicopter pacman pong invaders dino))

test: logic_test ttt_test rps_test vs_test settings_test
	./logic_test
	./settings_test
	$(MAKE) -C $(EM) --no-print-directory > /dev/null && ./persistence_check.sh
	./ttt_test
	./rps_test
	@for g in pong ttt rps c4; do \
	  ./vs_test $$g host 0 10 25 > /tmp/vs_h.log & ./vs_test $$g join 1 10 25 > /tmp/vs_j.log; wait; \
	  echo "$$g over a radio losing 10% of packets:"; grep -E "over|rematch" /tmp/vs_h.log | head -3; done

logic_test: logic_test.cpp $(CORE) $(LINK) $(GAMES)
	g++ $(CXX_FLAGS) -o $@ logic_test.cpp $(CORE) $(LINK) $(GAMES)

ttt_test: ttt_test.cpp $(CORE) $(FW)/tictactoe.cpp $(LINK)
	g++ $(CXX_FLAGS) -o $@ ttt_test.cpp $(CORE) $(FW)/tictactoe.cpp $(LINK)

rps_test: rps_test.cpp $(CORE) $(FW)/rps.cpp $(LINK)
	g++ $(CXX_FLAGS) -o $@ rps_test.cpp $(CORE) $(FW)/rps.cpp $(LINK)

vs_test: vs_test.cpp air.cpp $(CORE) $(LINK) $(FW)/pong.cpp $(FW)/tictactoe.cpp $(FW)/rps.cpp $(FW)/connect4.cpp
	g++ $(CXX_FLAGS) -DUSE_AIR -o $@ vs_test.cpp air.cpp $(CORE) $(LINK) $(FW)/pong.cpp $(FW)/tictactoe.cpp $(FW)/rps.cpp $(FW)/connect4.cpp

settings_test: settings_test.cpp $(CORE) $(FW)/settings.cpp
	g++ $(CXX_FLAGS) -o $@ settings_test.cpp $(CORE) $(FW)/settings.cpp

# firmware update test: three builds with different version numbers talk to each other
update_test: update_test.cpp air.cpp $(CORE) $(LINK) $(FW)/fwupdate.cpp
	g++ $(CXX_FLAGS) -DUSE_AIR -o update_test update_test.cpp air.cpp $(CORE) $(LINK) $(FW)/fwupdate.cpp
	g++ $(CXX_FLAGS) -DUSE_AIR '-DOS_VERSION="v4.3"' -DOS_VERSION_NUM=403 -o update_test_new update_test.cpp air.cpp $(CORE) $(LINK) $(FW)/fwupdate.cpp
	g++ $(CXX_FLAGS) -DUSE_AIR '-DOS_VERSION="v4.0"' -DOS_VERSION_NUM=400 -o update_test_old update_test.cpp air.cpp $(CORE) $(LINK) $(FW)/fwupdate.cpp

clean:
	rm -f update_test update_test_new update_test_old logic_test ttt_test rps_test vs_test c4_test settings_test

c4_test: c4_test.cpp air.cpp $(CORE) $(LINK) $(FW)/connect4.cpp
	g++ $(CXX_FLAGS) -DUSE_AIR -o $@ c4_test.cpp air.cpp $(CORE) $(LINK) $(FW)/connect4.cpp
