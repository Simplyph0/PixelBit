#!/usr/bin/env python3
"""Converts a video into a .pbv file for the PixelBit video player: black and
white, no sound, small. Works on Windows, macOS and Linux; needs Python 3 and
ffmpeg (on the PATH, or `pip install imageio-ffmpeg`). Nothing else.

  python3 pbv.py movie.mp4 --pixel 2 --fps 12          # movie.pbv, upload it from the console's video page
  python3 pbv.py movie.mp4 --pixel 2 --embed ../../GameSystem/demo_clip.h   # the clip every console ships with
  python3 pbv.py --demo -o demo.pbv                    # a test pattern, no video needed

DETAIL: --pixel N sets how big one video pixel is on the screen, from 1 to 4.
  --pixel 1  128x64 pixels: most detail, biggest file, harder to read on a 0.96" screen
  --pixel 2   64x32 pixels: half the detail each way, a quarter of the data
  --pixel 3   42x21 pixels
  --pixel 4   32x16 pixels: chunky and easy to see, tiny file
The format is described in GameSystem/pbvcodec.h."""
import argparse
import operator
import os
import shutil
import struct
import subprocess
import sys

SCREEN_W, SCREEN_H = 128, 64
STORAGE_KB = 900            # about what the console's file system holds
EMBED_WARN_KB = 200         # a clip in the firmware makes every firmware update that much longer


# ---- reading the video ------------------------------------------------------

def find_ffmpeg():
    exe = shutil.which('ffmpeg')
    if exe:
        return exe
    try:
        import imageio_ffmpeg
        return imageio_ffmpeg.get_ffmpeg_exe()
    except ImportError:
        sys.exit('ffmpeg was not found. Install it (https://ffmpeg.org/download.html) and put it on the PATH, '
                 'or run: pip install imageio-ffmpeg')


def fit_filter(fit, w, h):
    if fit == 'stretch':
        return f'scale={w}:{h}:flags=area'
    if fit == 'crop':
        return f'scale={w}:{h}:force_original_aspect_ratio=increase:flags=area,crop={w}:{h}'
    return (f'scale={w}:{h}:force_original_aspect_ratio=decrease:flags=area,'
            f'pad={w}:{h}:(ow-iw)/2:(oh-ih)/2:color=black')


def video_frames(args, w, h):
    """Yields grey frames, w*h bytes each (0 black .. 255 white)."""
    cmd = [find_ffmpeg(), '-v', 'error', '-nostdin']
    if args.start:
        cmd += ['-ss', str(args.start)]
    cmd += ['-i', args.input]
    if args.duration:
        cmd += ['-t', str(args.duration)]
    cmd += ['-an', '-vf', f'fps={args.fps},{fit_filter(args.fit, w, h)}', '-f', 'rawvideo', '-pix_fmt', 'gray', '-']
    proc = subprocess.Popen(cmd, stdout=subprocess.PIPE)
    size = w * h
    try:
        while True:
            data = b''
            while len(data) < size:
                chunk = proc.stdout.read(size - len(data))
                if not chunk:
                    break
                data += chunk
            if len(data) < size:
                break
            yield data
    finally:
        proc.stdout.close()
        if proc.wait() != 0:
            sys.exit('ffmpeg failed (see its message above)')


def demo_frames(w, h, seconds, fps):
    """A test pattern: moving stripes and a bouncing ball, so the player can be tried without a video."""
    n = int(seconds * fps)
    r = max(3, min(w, h) // 5)
    for t in range(n):
        bx = r + abs((t * 3) % (2 * (w - 2 * r)) - (w - 2 * r))
        by = r + abs((t * 2) % (2 * (h - 2 * r)) - (h - 2 * r))
        frame = bytearray(w * h)
        for y in range(h):
            for x in range(w):
                d2 = (x - bx) ** 2 + (y - by) ** 2
                if d2 <= r * r:
                    v = 255
                elif d2 <= (r + 2) ** 2:
                    v = 0
                else:
                    v = 110 if ((x + y - t * 2) // 4) % 2 else 0
                frame[y * w + x] = v
        yield bytes(frame)


# ---- making black and white -------------------------------------------------

BAYER4 = [[0, 8, 2, 10], [12, 4, 14, 6], [3, 11, 1, 9], [15, 7, 13, 5]]


def tone_table(contrast, brightness):
    table = bytearray(256)
    for v in range(256):
        x = (v - 128) * contrast + 128 + brightness * 255
        table[v] = max(0, min(255, int(round(x))))
    return bytes(table)


def threshold_bits(grey, t):
    table = bytes(1 if v >= t else 0 for v in range(256))
    return grey.translate(table)


def bayer_bits(grey, w, h, t):
    shift = (128 - t) / 128 * 64                     # moves the whole pattern with --threshold
    levels = bytes(max(1, min(255, int((BAYER4[y % 4][x % 4] + 0.5) / 16 * 255 - shift)))
                   for y in range(h) for x in range(w))
    return bytes(map(operator.ge, grey, levels))


def floyd_bits(grey, w, h, t):
    cur = list(grey)
    out = bytearray(w * h)
    for y in range(h):
        row = y * w
        for x in range(w):
            i = row + x
            old = cur[i]
            new = 255 if old >= t else 0
            out[i] = 1 if new else 0
            err = old - new
            if err:
                if x + 1 < w:
                    cur[i + 1] += (err * 7) >> 4
                if y + 1 < h:
                    if x > 0:
                        cur[i + w - 1] += (err * 3) >> 4
                    cur[i + w] += (err * 5) >> 4
                    if x + 1 < w:
                        cur[i + w + 1] += err >> 4
    return bytes(out)


def to_bits(grey, w, h, args, tone):
    if tone:
        grey = grey.translate(tone)
    if args.invert:
        grey = grey.translate(bytes(255 - v for v in range(256)))
    if args.dither == 'floyd':
        return floyd_bits(grey, w, h, args.threshold)
    if args.dither == 'threshold':
        return threshold_bits(grey, args.threshold)
    return bayer_bits(grey, w, h, args.threshold)


# ---- the .pbv format --------------------------------------------------------

def pack_pages(bits, w, h):
    """0/1 per pixel -> the OLED layout: pages of 8 rows, w bytes per page, bit 0 at the top."""
    out = bytearray()
    for p in range((h + 7) // 8):
        rows = [bits[(p * 8 + k) * w:(p * 8 + k + 1) * w] if p * 8 + k < h else bytes(w) for k in range(8)]
        out += bytes(map(lambda a, b, c, d, e, f, g, i: a | b << 1 | c << 2 | d << 3 | e << 4 | f << 5 | g << 6 | i << 7, *rows))
    return bytes(out)


def rle(data):
    """PackBits: a byte below 128 = that many + 1 literal bytes follow; from 128 up = repeat the next byte (b - 128 + 2) times."""
    out = bytearray()
    lit = bytearray()

    def flush():
        while lit:
            part = lit[:128]
            del lit[:128]
            out.append(len(part) - 1)
            out.extend(part)

    i, n = 0, len(data)
    while i < n:
        j = i + 1
        while j < n and data[j] == data[i] and j - i < 129:
            j += 1
        if j - i >= 3:
            flush()
            out.append(128 + (j - i) - 2)
            out.append(data[i])
            i = j
        else:
            lit.append(data[i])
            i += 1
    flush()
    return bytes(out)


def xor_bytes(a, b):
    return (int.from_bytes(a, 'little') ^ int.from_bytes(b, 'little')).to_bytes(len(a), 'little')


def build(frames_iter, w, h, scale, fps, key_every, raw_out=None):
    records = []
    prev = None
    for i, grey in enumerate(frames_iter):
        packed = grey
        if i % key_every == 0:
            payload = rle(packed)
        else:
            payload = rle(xor_bytes(prev, packed))
        prev = packed
        records.append(struct.pack('<H', len(payload)) + payload)
        if raw_out:
            raw_out.write(packed)
        if len(records) > 65535:
            sys.exit('too many frames (65535 at most): use --duration or a lower --fps')
    if not records:
        sys.exit('no frames were read from the video')
    frames = len(records)
    key_count = (frames + key_every - 1) // key_every
    offset = 12 + 4 * key_count
    table = bytearray()
    for i, rec in enumerate(records):
        if i % key_every == 0:
            table += struct.pack('<I', offset)
        offset += len(rec)
    header = b'PBV1' + struct.pack('<BBBBHH', w, h, scale, fps, frames, key_every)
    return header + bytes(table) + b''.join(records), frames


def write_header_file(path, data, name):
    lines = []
    for i in range(0, len(data), 16):
        lines.append('  ' + ', '.join(f'0x{b:02X}' for b in data[i:i + 16]) + ',')
    with open(path, 'w', newline='\n') as f:
        f.write('// The clip every console ships with. Made by tools/video/pbv.py --embed; do not edit by hand.\n'
                '#ifndef DEMO_CLIP_H\n#define DEMO_CLIP_H\n\n#include <stdint.h>\n\n'
                f'#define DEMO_CLIP_NAME "{name}"\n#define DEMO_CLIP_SIZE {len(data)}UL\n\n'
                'static const uint8_t DEMO_CLIP[] PROGMEM = {\n' + '\n'.join(lines) + '\n};\n\n#endif\n')


# ---- main -------------------------------------------------------------------

def main():
    ap = argparse.ArgumentParser(description='Convert a video to a black and white .pbv for the PixelBit video player.',
                                 formatter_class=argparse.RawDescriptionHelpFormatter, epilog='DETAIL:' + __doc__.split('DETAIL:')[1])
    ap.add_argument('input', nargs='?', help='the video file (anything ffmpeg reads)')
    ap.add_argument('-o', '--output', help='the .pbv file to write (default: the input name with .pbv)')
    ap.add_argument('--pixel', type=int, choices=[1, 2, 3, 4], default=1,
                    help='size of one video pixel on the screen: 1 = most detail, 4 = chunkiest and smallest (default 1)')
    ap.add_argument('--fps', type=int, default=15, help='frames per second, 1 to 30 (default 15; the console shows about 20 at most)')
    ap.add_argument('--dither', choices=['bayer', 'threshold', 'floyd'], default='bayer',
                    help='how greys become black and white: bayer = fixed pattern (default), threshold = hard edges '
                         '(best for line art), floyd = error diffusion (finest, largest files)')
    ap.add_argument('--threshold', type=int, default=128, help='brightness that counts as white, 1 to 254 (default 128)')
    ap.add_argument('--contrast', type=float, default=1.0, help='1.0 = unchanged; more = harder contrast')
    ap.add_argument('--brightness', type=float, default=0.0, help='-1.0 to 1.0, 0 = unchanged')
    ap.add_argument('--invert', action='store_true', help='swap black and white')
    ap.add_argument('--fit', choices=['letterbox', 'crop', 'stretch'], default='letterbox',
                    help='letterbox = black bars, keep the shape (default); crop = fill the screen; stretch = fill, distorted')
    ap.add_argument('--start', type=float, default=0, help='start this many seconds in')
    ap.add_argument('--duration', type=float, default=0, help='only this many seconds (default: to the end)')
    ap.add_argument('--key-seconds', type=float, default=2.0, help='a full picture this often, for seeking (default 2)')
    ap.add_argument('--embed', metavar='HEADER.h', help='also write a C header with the clip, for GameSystem/demo_clip.h')
    ap.add_argument('--name', help='name for the embedded clip (default: the input file name)')
    ap.add_argument('--demo', action='store_true', help='make a test pattern instead of reading a video')
    ap.add_argument('--demo-seconds', type=float, default=6)
    ap.add_argument('--dump-raw', metavar='FILE', help='(for the tests) also write every decoded frame, one after the other')
    args = ap.parse_args()

    if not args.demo and not args.input:
        ap.error('give a video file, or --demo')
    if not 1 <= args.fps <= 30:
        ap.error('--fps must be 1 to 30')
    if not 1 <= args.threshold <= 254:
        ap.error('--threshold must be 1 to 254')
    if args.fps > 20:
        print('note: the console shows about 20 frames per second at most; more is wasted data', file=sys.stderr)

    w, h = SCREEN_W // args.pixel, SCREEN_H // args.pixel
    key_every = max(1, round(args.key_seconds * args.fps))
    tone = tone_table(args.contrast, args.brightness) if (args.contrast != 1.0 or args.brightness != 0.0) else None

    source = demo_frames(w, h, args.demo_seconds, args.fps) if args.demo else video_frames(args, w, h)
    bits = (pack_pages(to_bits(g, w, h, args, tone), w, h) for g in source)
    raw = open(args.dump_raw, 'wb') if args.dump_raw else None
    data, frames = build(bits, w, h, args.pixel, args.fps, key_every, raw)
    if raw:
        raw.close()

    base = 'demo' if args.demo else os.path.splitext(os.path.basename(args.input))[0]
    out = args.output or (None if args.embed else base + '.pbv')
    if out:
        with open(out, 'wb') as f:
            f.write(data)
    if args.embed:
        name = (args.name or base).upper()[:12]
        write_header_file(args.embed, data, name)

    kb = len(data) / 1024
    print(f'{frames} frames, {frames / args.fps:.1f} s at {args.fps} fps, {w}x{h} pixels drawn {args.pixel}x{args.pixel} '
          f'({w * args.pixel}x{h * args.pixel} on screen), {kb:.0f} KB ({len(data) / frames:.0f} bytes per frame)')
    if out:
        print(f'wrote {out}')
    if args.embed:
        print(f'wrote {args.embed}')
        if kb > EMBED_WARN_KB:
            print(f'warning: {kb:.0f} KB inside the firmware makes every console-to-console update that much longer '
                  f'(and the .bin that much bigger). Try a higher --pixel or a lower --fps.', file=sys.stderr)
    if kb > STORAGE_KB:
        print(f'warning: {kb:.0f} KB will not fit in the console\'s ~{STORAGE_KB} KB of storage. '
              f'Try a higher --pixel, a lower --fps or a shorter --duration.', file=sys.stderr)


if __name__ == '__main__':
    main()
