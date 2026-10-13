#!/usr/bin/env python3
"""Plays each two-console game between two test processes over a simulated radio
that loses 10% of the packets, and prints how each match ended. The four games
play at the same time (each pair on its own port) on a clock running 3 times
faster than real time.

usage: vs_check.py [directory with the vs_test program]
Runs on Linux, macOS and Windows."""
import os, re, subprocess, sys

HERE = os.path.dirname(os.path.abspath(__file__))
EXE = '.exe' if os.name == 'nt' else ''
BIN = os.path.abspath(sys.argv[1]) if len(sys.argv) > 1 else HERE
VS = os.path.join(BIN, 'vs_test' + EXE)
GAMES = ('pong', 'ttt', 'rps', 'c4')

pairs = []
for n, game in enumerate(GAMES):
    env = dict(os.environ, PB_SPEED='3', PB_AIR_PORT=str(42000 + 10 * n))
    host = subprocess.Popen([VS, game, 'host', '0', '10', '25'], stdout=subprocess.PIPE, text=True, env=env)
    join = subprocess.Popen([VS, game, 'join', '1', '10', '25'], stdout=subprocess.DEVNULL, env=env)
    pairs.append((game, host, join))

for game, host, join in pairs:
    host_log = host.communicate()[0]
    join.wait()
    print(f'{game} over a radio losing 10% of packets:')
    shown = 0
    for line in host_log.splitlines():
        if re.search(r'over|rematch', line):
            print(line)
            shown += 1
            if shown == 3:
                break
