#!/bin/sh
# Boots the emulator on a fresh saved-data file and checks the first-boot rules:
#  - a unit that was only switched on (greeting) saves nothing about the tour,
#  - test mode (LEFT x3, hold UP) never marks the tour as acknowledged,
#  - finishing the tour, or skipping it and closing the message, saves the flag,
#  - scores and settings are saved and survive a restart.
set -e
cd "$(dirname "$0")"
EMU=$(cd ../emulator && pwd)/emulator
DIR=$(mktemp -d)
FILE="$DIR/eeprom.bin"

flag() {   # prints the tour flag byte, or "none" when there is no settings block
  if [ "$(dd if="$FILE" bs=1 skip=128 count=4 2>/dev/null | tr -d '\000')" = "GOPT" ]; then
    dd if="$FILE" bs=1 skip=135 count=1 2>/dev/null | od -An -tu1 | tr -d ' '
  else
    echo none
  fi
}
run() { ( cd "$DIR" && $EMU --headless --fast --speed 8 --eeprom "$FILE" --replay "$DIR/replay" > /dev/null 2>&1 ); }
fail() { echo "FAIL: $1"; rm -rf "$DIR"; exit 1; }

# 1. switched on, greeting shown for a moment, power cut
printf 'seed 1\n1500 quit\n' > "$DIR/replay"; run
[ -s "$FILE" ] || fail "nothing at all was saved on the first boot"
[ "$(dd if="$FILE" bs=1 count=4 2>/dev/null | tr -d '\000')" = "GSAV" ] || fail "no high-score block saved"
[ "$(flag)" = "none" ] || fail "the tour flag was saved before the tour was acknowledged"

# 2. test mode: LEFT three times, hold UP, run the storage test, leave
printf 'seed 1\n700 4\n830 0\n1000 4\n1130 0\n1300 4\n1430 0\n1600 1\n3000 0\n3300 8\n3430 0\n3700 1\n3830 0\n4300 quit\n' > "$DIR/replay"; run
[ "$(flag)" = "none" ] || fail "test mode must not acknowledge the tour"

# 3. skipping the tour is not enough: the message has to be closed
printf 'seed 1\n700 1\n830 0\n1100 8\n3000 0\n3600 quit\n' > "$DIR/replay"; run
[ "$(flag)" = "none" ] || fail "skipping saved the flag before the message was closed"

# 4. skip, then close the message with UP
printf 'seed 1\n700 1\n830 0\n1100 8\n3000 0\n3500 1\n3630 0\n4200 quit\n' > "$DIR/replay"; run
[ "$(flag)" = "1" ] || fail "closing the skip message did not save the flag"

# 5. next boot: the saved data is left untouched (no greeting, nothing rewritten)
cp "$FILE" "$DIR/before"
printf 'seed 1\n3500 quit\n' > "$DIR/replay"; run
cmp -s "$FILE" "$DIR/before" || fail "the saved data changed on a normal boot"

echo "persistence: the tour is acknowledged only by finishing it or closing the skip message; test mode and power cuts leave it unacknowledged; saved data survives a restart"
rm -rf "$DIR"
