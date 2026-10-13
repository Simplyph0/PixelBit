#!/usr/bin/env python3
"""Runs two simulated consoles against each other for the firmware update, in
several situations, and checks what ended up "installed" on the receiver."""
import re, os, random, subprocess, sys, tempfile, hashlib, time

HERE = os.path.dirname(os.path.abspath(__file__))
EXE = '.exe' if os.name == 'nt' else ''
SPEED = os.environ.get('PB_SPEED', '4')   # simulated clock speed: the radio is a loopback, so it keeps up
BIN = os.path.abspath(sys.argv[1]) if len(sys.argv) > 1 else HERE   # directory with update_test, update_test_new, update_test_old
tmp = tempfile.mkdtemp()
random.seed(7)

def image(path, header, size):
    data = bytearray(random.getrandbits(8) for _ in range(size))
    data[0:4] = header
    open(path, 'wb').write(data)
    return bytes(data)

sender_img = image(f'{tmp}/sender.bin', bytes([0xE9, 4, 0x03, 0x20]), 360 * 1024 + 123)   # last chunk is short
receiver_img = image(f'{tmp}/receiver.bin', bytes([0xE9, 4, 0x02, 0x40]), 300 * 1024)

def run(name, tx, rx, loss=0, env_tx=None, env_rx=None, secs=200):   # secs: simulated seconds before a run gives up; runs end by themselves when done, so a slow machine only needs headroom
    out = f'{tmp}/out-{name}.bin'
    for p in (out, out + '.partial'):
        if os.path.exists(p): os.remove(p)
    e_rx = dict(os.environ, PB_SPEED=SPEED, FW_IMAGE=f'{tmp}/receiver.bin', FW_OUT=out, **(env_rx or {}))
    e_tx = dict(os.environ, PB_SPEED=SPEED, FW_IMAGE=f'{tmp}/sender.bin', FW_OUT=out + '.tx', **(env_tx or {}))
    t0 = time.time()
    prx = subprocess.Popen([os.path.join(BIN, rx + EXE), 'rx', '0', str(loss), str(secs)], env=e_rx, stdout=subprocess.PIPE, text=True)
    ptx = subprocess.Popen([os.path.join(BIN, tx + EXE), 'tx', '1', str(loss), str(secs)], env=e_tx, stdout=subprocess.PIPE, text=True)
    log_tx = ptx.communicate()[0]; log_rx = prx.communicate()[0]
    installed = open(out, 'rb').read() if os.path.exists(out) else None
    return installed, log_rx, log_tx, time.time() - t0

fails = 0
def check(cond, msg):
    global fails
    print(('  ok   ' if cond else '  FAIL ') + msg)
    if not cond: fails += 1

def failed(log, title):
    return re.search(r'phase FAIL t=\d+ : ' + title, log) is not None

def expected():
    e = bytearray(sender_img); e[2] = receiver_img[2]; e[3] = receiver_img[3]   # the receiver keeps its own flash settings
    return bytes(e)

print('newer version, 10% of packets lost')
inst, lrx, ltx, secs = run('newer', 'update_test_new', 'update_test', loss=10)
check(inst == expected(), f'the installed image is the sender\'s, with the receiver\'s own flash header bytes ({secs:.0f} s)')
check('RESTART requested' in lrx, 'the receiver restarts after installing')
check('newer' in lrx and 'v4.3' in lrx, 'the receiver was told it is a newer version')
check('phase TX_DONE' in ltx, 'the sender reports success')

print('same version number')
inst, lrx, ltx, secs = run('same', 'update_test', 'update_test', loss=5)
check('same version' in lrx, 'the receiver is told the version number is the same')
check(inst == expected(), 'it can still be installed')

print('older version')
inst, lrx, ltx, secs = run('older', 'update_test_old', 'update_test', secs=60)
check(inst is None, 'nothing is installed')
check(failed(lrx, 'REFUSED') and failed(ltx, 'REFUSED'), 'both sides show that the older version was refused')

print('the receiver says no')
inst, lrx, ltx, secs = run('deny', 'update_test_new', 'update_test', env_rx={'UT_DENY': '1'}, secs=60)
check(inst is None, 'nothing is installed')
check(failed(ltx, 'DENIED'), 'the sender is told it was denied')

print('wrong checksum announced')
inst, lrx, ltx, secs = run('hash', 'update_test_new', 'update_test', env_rx={'UT_CORRUPT': '1'}, secs=60)
check(inst is None, 'the image is not installed when its SHA-256 does not match')
check('RESTART requested' not in lrx, 'the receiver does not restart')
check(failed(lrx, 'UPDATE FAILED'), 'the receiver reports the failure')

print('not enough room on the receiver')
inst, lrx, ltx, secs = run('space', 'update_test_new', 'update_test', env_rx={'FREE_SPACE': '100000'}, secs=60)
check(inst is None and failed(lrx, 'REFUSED'), 'refused, nothing installed')

print('the sender disappears half way')
inst, lrx, ltx, secs = run('kill', 'update_test_new', 'update_test', env_tx={'UT_KILL': '50'}, secs=80)
check(inst is None, 'a half-received image is never installed')
check('RESTART requested' not in lrx, 'the receiver keeps running its old firmware')

print('a bad radio: 15% of packets lost')
inst, lrx, ltx, secs = run('lossy', 'update_test_new', 'update_test', loss=15, secs=300)
check(inst == expected(), f'it still completes, byte for byte ({secs:.0f} s)')

print('\n' + ('all firmware update checks passed' if not fails else f'{fails} CHECKS FAILED'))
sys.exit(1 if fails else 0)
