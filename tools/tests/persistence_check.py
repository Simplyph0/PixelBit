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
run('seed 1\n700 4\n830 0\n1000 4\n1130 0\n1300 4\n1430 0\n1600 1\n3000 0\n3300 2\n3430 0\n3500 2\n3630 0\n3700 8\n3830 0\n4300 quit\n')
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

def taps(*codes, start=3000, gap=400):
    """Replay lines for a sequence of button taps (bits: 1 up, 2 down, 4 left, 8 right)."""
    t, out = start, ''
    for c in codes:
        out += f'{t} {c}\n{t + 130} 0\n'
        t += gap
    return out, t


# 6. SETTINGS > SHOW INTRO clears the tour flag, so the next boot greets again (the flag is 1 from step 4)
if flag() != 1:
    fail('the tour flag should be set before the SHOW INTRO check')
body, t = taps(4, 1, 2, 2, 2, 2, 2, 2, 2, 8)   # SETTINGS tile, open it, down 7 times to SHOW INTRO, select
run('seed 1\n' + body + f'{t + 800} quit\n')
if flags() is None or flag() != 0:
    fail('SHOW INTRO did not clear the tour flag')

run('seed 1\n700 1\n830 0\n1100 8\n3000 0\n3500 1\n3630 0\n4200 quit\n')   # greet and acknowledge again (as in step 4)
if flag() != 1:
    fail('could not acknowledge the tour again for the factory reset check')

# 7. SETTINGS keeps brightness; FACTORY RESET puts everything back (and the console restarts)
def device():
    d = read_file()
    return (d[200:204], d[204], d[205], d[206]) if len(d) > 207 else (b'', None, None, None)

def contact_record(alias):
    slots = [bytes([1, 2, 3]) + alias.encode().ljust(11, b'\0')] + [bytes(14)] * 7
    body = b'CONT' + b''.join(slots)
    sum_ = 0x5A
    for b in body:
        sum_ = (sum_ * 31 + b) & 0xFF
    return body + bytes([sum_])

data = bytearray(read_file().ljust(512, b'\0'))
rec = contact_record('DAD')
data[220:220 + len(rec)] = rec
open(FILE, 'wb').write(bytes(data))
body, t = taps(4, 1, 8)    # SETTINGS, open, BRIGHTNESS: HIGH -> MAX
run('seed 1\n' + body + f'{t + 800} quit\n')
tag, bright, sleep, chat = device()
if tag != b'DEVS' or bright != 4:
    fail('the brightness was not saved')
if read_file()[220:224] != b'CONT' or read_file()[224:227] != bytes([1, 2, 3]):
    fail('the chat contact should still be there before the factory reset')
body, t = taps(4, 1, 2, 2, 2, 2, 2, 2, 2, 2)   # SETTINGS, open, down 8 times: FACTORY RESET
run('seed 1\n' + body + f'{t} 8\n{t + 3400} 0\n' + f'{t + 3600} quit\n')
if device()[1] != 4 or read_file()[224:227] != bytes([1, 2, 3]):
    fail('holding RIGHT through the opening of the factory reset screen must not erase anything')
run('seed 1\n' + body + f'{t} 8\n{t + 130} 0\n{t + 600} 8\n{t + 4000} 0\n' + f'{t + 7000} quit\n')
tag, bright, sleep, chat = device()
if tag != b'DEVS' or bright != 3 or sleep != 0 or chat != 1:
    fail(f'the factory reset did not restore the screen and chat settings ({device()})')
if read_file()[224:227] != bytes(3):
    fail('the factory reset did not clear the chat contacts')
if flag() != 0:
    fail('the factory reset did not make the tour new again')

print('persistence: the tour is acknowledged only by finishing it or closing the skip message; '
      'test mode and power cuts leave it unacknowledged; saved data survives a restart; SHOW INTRO and FACTORY RESET work')
shutil.rmtree(DIR, ignore_errors=True)
