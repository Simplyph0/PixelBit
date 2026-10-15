#!/usr/bin/env python3
"""Checks the video player two ways.

1. The converter and the console's decoder agree: pbv.py makes test clips with
   different detail settings and dithers, and pbv_test decodes each one frame by
   frame against what the converter encoded. A bigger --pixel must also mean a
   smaller file.
2. The real player, run in the emulator: the clip built into the firmware is
   copied to the file system on the first start, the VIDEOS entry opens, a clip
   plays (the picture changes), DOWN goes back to the list, DOWN again leaves.

usage: video_check.py <pbv_test program> <emulator program>
Runs on Linux, macOS and Windows."""
import os, shutil, subprocess, sys, tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
PBV = os.path.join(HERE, '..', 'video', 'pbv.py')
test_bin, emulator = os.path.abspath(sys.argv[1]), os.path.abspath(sys.argv[2])
tmp = tempfile.mkdtemp()
fails = 0


def check(ok, msg):
    global fails
    print(('  ok   ' if ok else '  FAIL ') + msg)
    if not ok:
        fails += 1


def run(cmd, **kw):
    return subprocess.run(cmd, capture_output=True, text=True, **kw)


print('converter and decoder')
sizes = {}
for pixel in (1, 2, 3, 4):
    for dither in ('bayer', 'threshold', 'floyd'):
        if dither == 'floyd' and pixel == 1:
            continue                      # the slow one; the smaller sizes cover it
        clip, raw = f'{tmp}/c{pixel}{dither}.pbv', f'{tmp}/c{pixel}{dither}.raw'
        r = run([sys.executable, PBV, '--demo', '--demo-seconds', '3', '--pixel', str(pixel), '--dither', dither,
                 '--fps', '10', '--key-seconds', '1', '-o', clip, '--dump-raw', raw])
        check(r.returncode == 0, f'--pixel {pixel} --dither {dither}: the converter runs')
        if r.returncode:
            print(r.stderr)
            continue
        d = run([test_bin, clip, raw])
        check(d.returncode == 0, f'--pixel {pixel} --dither {dither}: the decoder reads every frame back ({d.stdout.strip().splitlines()[-1] if d.stdout.strip() else d.stderr.strip()})')
        if dither == 'bayer':
            sizes[pixel] = os.path.getsize(clip)
check(sizes.get(1, 0) > sizes.get(2, 1) > sizes.get(3, 2) > sizes.get(4, 3), f'a bigger --pixel makes a smaller file ({sizes})')

# the converter refuses nonsense
check(run([sys.executable, PBV, '--demo', '--fps', '99', '-o', f'{tmp}/x.pbv']).returncode != 0, 'the converter refuses 99 fps')
check(run([sys.executable, PBV, '--demo', '--pixel', '5', '-o', f'{tmp}/x.pbv']).returncode != 0, 'the converter refuses --pixel 5')

print('the player in the emulator')
work = os.path.join(tmp, 'emu')
os.makedirs(work)
fsdir = os.path.join(work, 'fs')
replay = os.path.join(work, 'replay')
with open(replay, 'w') as f:
    # Buttons are held 300 ms and the clock runs only 4x, so a slow machine (a CI
    # runner) cannot have a press and its release fall inside one frame
    f.write('seed 1\n'
            '600 4\n900 0\n'            # LEFT from the first game: the last tile, SETTINGS
            '1600 4\n1900 0\n'          # LEFT again: CHAT
            '2600 4\n2900 0\n'          # and again: VIDEOS (a full second between presses: the carousel is still sliding)
            '3800 shot menu\n'
            '4200 1\n4500 0\n'          # UP: open it
            '5200 shot list\n'
            '5500 8\n5800 0\n'          # RIGHT: play the first clip
            '6500 shot play1\n'
            '7500 shot play2\n'
            '7600 2\n7900 0\n'          # DOWN: back to the list
            '8600 shot list2\n'
            '8700 4\n9000 0\n'          # LEFT: leave the player
            '9700 shot menu2\n'
            '10200 quit\n')
env = dict(os.environ, PB_FS=fsdir)
r = run([emulator, '--headless', '--fast', '--speed', '4', '--no-tutorial', '--replay', replay], cwd=work, env=env)
check(r.returncode == 0, 'the emulator ran the script')
demo = os.path.join(fsdir, 'clips', 'DEMO.pbv')
check(os.path.exists(demo) and open(demo, 'rb').read(4) == b'PBV1', 'the clip built into the firmware was copied to the file system')
check(os.path.exists(os.path.join(fsdir, 'seeded')), 'and marked as done, so deleting it later sticks')


def shot(name):
    p = os.path.join(work, 'shots', name + '.png')
    return open(p, 'rb').read() if os.path.exists(p) else None


s = {n: shot(n) for n in ('menu', 'list', 'play1', 'play2', 'list2', 'menu2')}
check(all(s.values()), 'every screenshot was taken')
if all(s.values()):
    check(s['menu'] != s['list'], 'opening VIDEOS shows the clip list')
    check(s['list'] != s['play1'], 'RIGHT on a clip starts the video')
    check(s['play1'] != s['play2'], 'the picture moves while it plays')
    check(s['list2'] == s['list'], 'DOWN while playing goes back to the same list')
    check(s['menu2'] != s['list2'], 'LEFT on the list leaves the player')

print('\n' + ('all video checks passed' if not fails else f'{fails} CHECKS FAILED'))
shutil.rmtree(tmp, ignore_errors=True)
sys.exit(1 if fails else 0)
