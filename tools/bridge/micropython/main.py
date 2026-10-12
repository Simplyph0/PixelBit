# ESP-NOW <-> USB serial bridge in MicroPython (ESP32-C3 SuperMini and other ESP32 boards).
# Same job and the same serial frames as ../bridge/bridge.ino; see that file for the
# frame format. Copy this file to the board as main.py so it starts on power-up.
import sys
import select
import time
import micropython
import network
import espnow

CHANNEL = 1  # must match LINK_CHANNEL in GameSystem/config.h

micropython.kbd_intr(-1)  # frames are binary: a 0x03 byte must not act as Ctrl-C

sta = network.WLAN(network.STA_IF)
sta.active(True)
sta.disconnect()
sta.config(channel=CHANNEL)

now = espnow.ESPNow()
now.active(True)
BROADCAST = b"\xff" * 6
now.add_peer(BROADCAST)

stdin = sys.stdin.buffer
stdout = sys.stdout.buffer
poll = select.poll()
poll.register(sys.stdin, select.POLLIN)


def write_frame(payload):
    ck = len(payload)
    for b in payload:
        ck ^= b
    stdout.write(b"\xa5" + bytes([len(payload)]) + payload + bytes([ck]))


def handle(payload):
    if payload[0:1] == b"M":
        write_frame(b"M" + sta.config("mac"))
    elif payload[0:1] == b"T" and len(payload) >= 8:
        mac = bytes(payload[1:7])
        try:
            now.add_peer(mac)
        except OSError:
            pass  # already known
        try:
            now.send(mac, bytes(payload[7:]))
        except OSError:
            pass


buf = bytearray()
while True:
    while poll.poll(0):
        buf.extend(stdin.read(1))
        # look for 0xA5, length, payload, checksum
        while True:
            start = buf.find(b"\xa5")
            if start < 0:
                buf = bytearray()
                break
            if start:
                buf = buf[start:]
            if len(buf) < 2:
                break
            n = buf[1]
            if len(buf) < n + 3:
                break
            ck = n
            for b in buf[2:2 + n]:
                ck ^= b
            if ck == buf[2 + n]:
                handle(buf[2:2 + n])
                buf = buf[n + 3:]
            else:
                buf = buf[1:]  # damaged: look for the next frame start

    mac, msg = now.irecv(0)
    while mac is not None:
        write_frame(b"R" + bytes(mac) + bytes(msg))
        mac, msg = now.irecv(0)

    time.sleep_ms(1)
