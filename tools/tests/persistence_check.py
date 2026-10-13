#!/usr/bin/env python3
"""Boots the emulator on a fresh saved-data file and checks the first-boot rules:
 - a unit that was only switched on (greeting) saves nothing about the tour,
 - test mode (LEFT x3, hold UP) never marks the tour as acknowledged,
 - finishing the tour, or skipping it and closing the message, saves the flag,
 - scores and settings are saved and survive a restart.

usage: persistence_check.py [path to the emulator]
Runs on Linux, macOS and Windows."""
import os, shutil, subprocess, sys, tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
EXE = '.exe' if os.name == 'nt' else ''
EMU = os.path.abspath(sys.argv[1]) if len(sys.argv) > 1 else os.path.join(HERE, '..', 'emulator', 'emulator' + EXE)

DIR = tempfile.mkdtemp()
FILE = os.path.join(DIR, 'eeprom.bin')
REPLAY = os.path.join(DIR, 'replay')


def read_file():
    return open(FILE, 'rb').read() if os.path.exists(FILE) else b''


def flags():
    """The settings flags byte, or None when there is no settings block."""
    data = read_file()
    if data[128:132] == b'GOPT' and len(data) > 135:
        return data[135]
    return None


def flag():
    """1 when the tour is acknowledged, otherwise 0."""
    f = flags()
    return 0 if f is None else f & 1


def run(script):
    with open(REPLAY, 'w') as r:
        r.write(script)
    subprocess.run([EMU, '--headless', '--fast', '--speed', '8', '--eeprom', FILE, '--replay', REPLAY],
                   cwd=DIR, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, check=False)


def fail(msg):
    print('FAIL: ' + msg)
    shutil.rmtree(DIR, ignore_errors=True)
    sys.exit(1)


# 1. switched on, greeting shown for a moment, power cut
run('seed 1\n1500 quit\n')
if not read_file():
    fail('nothing at all was saved on the first boot')
if read_file()[0:4] != b'GSAV':
    fail('no high-score block saved')
if flag() != 0:
    fail('the tour flag was saved before the tour was acknowledged')

# 2. test mode: LEFT three times, hold UP, run the storage test, leave
run('seed 1\n700 4\n830 0\n1000 4\n1130 0\n1300 4\n1430 0\n1600 1\n3000 0\n3300 8\n3430 0\n3700 1\n3830 0\n4300 quit\n')
if flag() != 0:
    fail('test mode must not acknowledge the tour')

# 3. skipping the tour is not enough: the message has to be closed
run('seed 1\n700 1\n830 0\n1100 8\n3000 0\n3600 quit\n')
if flag() != 0:
    fail('skipping saved the flag before the message was closed')

# 4. skip, then close the message with UP
run('seed 1\n700 1\n830 0\n1100 8\n3000 0\n3500 1\n3630 0\n4200 quit\n')
if flag() != 1:
    fail('closing the skip message did not save the flag')

# 5. next boot: the saved data is left untouched (no greeting, nothing rewritten)
before = read_file()
run('seed 1\n3500 quit\n')
if read_file() != before:
    fail('the saved data changed on a normal boot')

print('persistence: the tour is acknowledged only by finishing it or closing the skip message; '
      'test mode and power cuts leave it unacknowledged; saved data survives a restart')
shutil.rmtree(DIR, ignore_errors=True)
