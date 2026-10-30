#!/usr/bin/env python3
"""Two test consoles chat over a simulated radio, with and without lost packets, and the
result is checked from what each of them prints. usage: chat_check.py [directory with chat_test]"""
import os, subprocess, sys

HERE = os.path.dirname(os.path.abspath(__file__))
EXE = '.exe' if os.name == 'nt' else ''
BIN = os.path.abspath(sys.argv[1]) if len(sys.argv) > 1 else HERE
CT = os.path.join(BIN, 'chat_test' + EXE)
fails = 0

def check(cond, msg):
    global fails
    print(('  ok   ' if cond else '  FAIL ') + msg)
    if not cond: fails += 1

def start(port, loss, secs, *extra):   # a pair of consoles on its own ports, so the three runs go at the same time
    env = dict(os.environ, PB_SPEED='4', PB_AIR_PORT=str(port))
    rx = subprocess.Popen([CT, 'rx', '0', str(loss), secs, *extra], stdout=subprocess.PIPE, text=True, env=env)
    tx = subprocess.Popen([CT, 'tx', '1', str(loss), secs, *extra], stdout=subprocess.PIPE, text=True, env=env)
    return tx, rx

def finish(pair):
    return pair[0].communicate()[0], pair[1].communicate()[0]

runs = {loss: start(43000 + loss, loss, '60') for loss in (0, 20)}
handoff = start(43500, 0, '90', 'handoff')

for loss in (0, 20):
    print(f'two consoles, {loss}% of packets lost')
    tx, rx = finish(runs[loss])
    check('[tx] nearby 1' in tx and 'matches rx: 1' in tx, 'the ping finds the other console')
    check('[tx] to rx: DELIVERED' in tx, 'a message to one console is acknowledged')
    check('[tx] to all: SENT' in tx, 'a message to everyone ends as SENT')
    check('[tx] to nobody: NO REPLY' in tx, 'a message to a console that is not there ends as NO REPLY')
    lines = [l for l in rx.splitlines() if '[rx] inbox' in l]
    check(len(lines) == 2, f'the receiver kept exactly two messages, repeats and the one for nobody left out ({len(lines)})')
    check(any('from SIS: BEAT MY SCORE IN TETRIS: 4321' in l for l in lines), 'the first has the alias, the game and the score')
    check(any('HELLO!' in l for l in lines), 'the one to everyone arrived too')
    check(all('unread' in l for l in lines) and lines and 'unread 2' in lines[-1], 'both are unread')

print('chat and a two-console session share the radio')
tx, rx = finish(handoff)
check('[tx] 1st: DELIVERED' in tx, 'chat works before the session')
check('session up, chat tag now 5' in tx and 'session up' in rx, 'a session can take the radio from chat')
check('[tx] 2nd (in session): DELIVERED' in tx, 'a chat message gets through during the session')
check('[tx] 3rd (after the session): DELIVERED' in tx, 'chat takes the radio back by itself when the session ends')
check(len([l for l in rx.splitlines() if '[rx] inbox' in l]) == 3, 'the receiver kept all three messages')

print('\nALL CHAT CHECKS PASSED' if not fails else f'\n{fails} CHAT CHECKS FAILED')
sys.exit(1 if fails else 0)
