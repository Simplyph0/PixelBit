#!/usr/bin/env python3
"""Plays chess on two consoles over a simulated radio that loses 10% of the packets, through the real
button flow: a scripted fool's mate (checkmate, the result, a rematch with the sides swapped), then random
moves. Both consoles must end in the same position. Then chess against the computer at the easier levels
must play legal moves to the end of a short game.

usage: chess_check.py <directory with chess_vs_test, or the program itself>
Runs on Linux, macOS and Windows."""
import os, re, subprocess, sys

HERE = os.path.dirname(os.path.abspath(__file__))
EXE = '.exe' if os.name == 'nt' else ''
arg = os.path.abspath(sys.argv[1]) if len(sys.argv) > 1 else HERE
PROG = arg if os.path.isfile(arg) else os.path.join(arg, 'chess_vs_test' + EXE)
fails = 0


def check(ok, msg):
    global fails
    print(('  ok   ' if ok else '  FAIL ') + msg)
    if not ok:
        fails += 1


def final(log):
    m = re.search(r'final game=(\d+) plies=(\d+) hash=([0-9a-f]+) mates=(\d+) rematches=(\d+) over=(\d)', log)
    return tuple(m.groups()) if m else None


# Everything runs at the same time: the two consoles and the three games against the computer
env = dict(os.environ, PB_SPEED='3', PB_AIR_PORT='44000')
host = subprocess.Popen([PROG, 'host', '0', '10', '30'], stdout=subprocess.PIPE, text=True, env=env)
join = subprocess.Popen([PROG, 'join', '1', '10', '30'], stdout=subprocess.PIPE, text=True, env=env)
ai_env = dict(os.environ, PB_SPEED='4')
ais = {level: subprocess.Popen([PROG, 'ai', str(level), '70'], stdout=subprocess.PIPE, text=True, env=ai_env) for level in (0, 1, 2)}

print('two consoles, 10% of the packets lost')
host_log = host.communicate()[0]
join_log = join.communicate()[0]
print(host_log.rstrip())
print(join_log.rstrip())
fh, fj = final(host_log), final(join_log)
check(fh is not None and fj is not None, 'both consoles finished and reported')
if fh and fj:
    check(fh[:3] == fj[:3], f'both consoles end in the same game, with the same moves and the same position ({fh[1]} plies)')
    check(int(fh[1]) >= 20, 'a good number of moves was played (not stuck)')
    check(fh[4] == '1' and fj[4] == '1', 'both consoles saw the rematch')
check('game 0 over: result 1, winner 1' in host_log and 'game 0 over: result 1, winner 1' in join_log,
      "the fool's mate ended the first game as a checkmate, won by Black, on both consoles")
check('rematch: game 1, I am Black' in host_log and 'rematch: game 1, I am White' in join_log,
      'in the rematch the sides are swapped')

print('against the computer')
for level, proc in ais.items():
    out = proc.communicate()[0]
    f = final(out)
    check(proc.returncode == 0 and f is not None, f'level {level}: plays a game without a crash or an illegal move ({f[1] if f else "?"} plies)')
    check(f is not None and int(f[1]) >= 10, f'level {level}: the game gets going')

print('\n' + ('all chess play checks passed' if not fails else f'{fails} CHECKS FAILED'))
sys.exit(1 if fails else 0)
