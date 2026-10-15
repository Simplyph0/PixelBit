#!/usr/bin/env python3
"""Makes docs/PixelBit-QuickStart.pdf: a one-page, black and white, printable quick start.

    python3 docs/make_quickstart.py

It needs LibreOffice (soffice) to turn the page into a PDF, and screenshots from the emulator in
docs/screens/ (see docs/screens/README.txt for how they were taken). Nothing else: no pip packages.
The page is written as a flat OpenDocument file (.fodt) with the screenshots inside it.
"""
import base64
import os
import re
import shutil
import subprocess
import sys
import tempfile
import zlib
from xml.sax.saxutils import escape

HERE = os.path.dirname(os.path.abspath(__file__))
SCREENS = os.path.join(HERE, 'screens')
OUT_PDF = os.path.join(HERE, 'PixelBit-QuickStart.pdf')
FONT_SCALE = 1.20        # everything is sized for 7.6 pt body text, times this


# ---- screenshots: read a PNG, print it dark on white ------------------------

def read_png(path):
    """(width, height, rows of brightness bytes) for the PNGs used here: grey, RGB(A) or palette, not interlaced."""
    data = open(path, 'rb').read()
    assert data[:8] == b'\x89PNG\r\n\x1a\n', path
    pos, idat, palette = 8, b'', []
    while pos < len(data):
        n = int.from_bytes(data[pos:pos + 4], 'big')
        kind = data[pos + 4:pos + 8]
        body = data[pos + 8:pos + 8 + n]
        if kind == b'IHDR':
            w, h, depth, ctype = int.from_bytes(body[:4], 'big'), int.from_bytes(body[4:8], 'big'), body[8], body[9]
            assert body[12] == 0 and depth <= 8, 'interlaced or 16-bit PNGs are not supported'
            ch = {0: 1, 2: 3, 3: 1, 4: 2, 6: 4}[ctype]
        elif kind == b'PLTE':
            palette = [max(body[i:i + 3]) for i in range(0, len(body), 3)]
        elif kind == b'IDAT':
            idat += body
        pos += 12 + n
    raw = zlib.decompress(idat)
    stride = (w * ch * depth + 7) // 8
    bpp = max(1, ch * depth // 8)
    rows, prev = [], bytearray(stride)
    for y in range(h):
        f = raw[y * (stride + 1)]
        line = bytearray(raw[y * (stride + 1) + 1:(y + 1) * (stride + 1)])
        for i in range(stride):
            a = line[i - bpp] if i >= bpp else 0
            b = prev[i]
            c = prev[i - bpp] if i >= bpp else 0
            if f == 1:
                line[i] = (line[i] + a) & 255
            elif f == 2:
                line[i] = (line[i] + b) & 255
            elif f == 3:
                line[i] = (line[i] + (a + b) // 2) & 255
            elif f == 4:
                pr = a + b - c
                pa, pb, pc = abs(pr - a), abs(pr - b), abs(pr - c)
                line[i] = (line[i] + (a if pa <= pb and pa <= pc else b if pb <= pc else c)) & 255
        rows.append(bytes(line))
        prev = line
    grey = []
    for line in rows:
        if ctype == 3:                          # palette: unpack the indices, look up the brightness
            per = 8 // depth
            idx = [(line[x // per] >> (8 - depth * (x % per + 1))) & ((1 << depth) - 1) for x in range(w)]
            grey.append(bytes(palette[i] if i < len(palette) else 0 for i in idx))
        else:
            grey.append(bytes(max(line[x * ch:x * ch + min(ch, 3)]) for x in range(w)))
    return w, h, grey


def print_ready(path):
    """The screen as black text on white paper, 1 bit per pixel at 4x, with a thin frame. Returns PNG bytes."""
    w, h, g = read_png(path)
    if (w, h) != (512, 256):                   # the old screenshots in images/ have a 10 pixel border
        ox, oy = (w - 512) // 2, (h - 256) // 2
        g = [row[ox:ox + 512] for row in g[oy:oy + 256]]
        w, h = 512, 256
    pad = 8
    out_w, out_h = w + 2 * pad, h + 2 * pad
    rows = []
    for y in range(out_h):
        row = bytearray([255] * out_w)
        if pad <= y < pad + h:
            src = g[y - pad]
            for x in range(w):
                if src[x] > 110:               # a lit pixel: draw it black
                    row[pad + x] = 0
        rows.append(row)
    for row in rows[:1] + rows[-1:]:           # the frame
        for x in range(out_w):
            row[x] = 0
    for row in rows:
        row[0] = row[-1] = 0
    raw = b''.join(b'\x00' + bytes(r) for r in rows)

    def chunk(kind, body):
        c = zlib.crc32(kind + body) & 0xffffffff
        return len(body).to_bytes(4, 'big') + kind + body + c.to_bytes(4, 'big')
    return (b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', out_w.to_bytes(4, 'big') + out_h.to_bytes(4, 'big') + bytes([8, 0, 0, 0, 0])) +
            chunk(b'IDAT', zlib.compress(raw, 9)) + chunk(b'IEND', b''))


# ---- the page ---------------------------------------------------------------

images = {}      # name -> png bytes


def shot(name):
    if name not in images:
        images[name] = print_ready(os.path.join(SCREENS, name + '.png'))
    return name


def inline(text):
    """**bold**, `mono` and [[key]] (a button name in a box) to ODF spans."""
    out = escape(text)
    out = re.sub(r'\*\*(.+?)\*\*', r'<text:span text:style-name="B">\1</text:span>', out)
    out = re.sub(r'`(.+?)`', r'<text:span text:style-name="M">\1</text:span>', out)
    return out


def p(text, style='Body'):
    return f'<text:p text:style-name="{style}">{inline(text)}</text:p>'


def h(text):
    return p(text, 'Head')


def bullet(text):
    return f'<text:p text:style-name="Bul">&#8226;<text:tab/>{inline(text)}</text:p>'


def step(n, text):
    return f'<text:p text:style-name="Step">{n}.<text:tab/>{inline(text)}</text:p>'


def frame(name, width_cm):
    height = width_cm * 272 / 528
    return (f'<draw:frame draw:style-name="Fr" draw:name="{name}" text:anchor-type="as-char" svg:width="{width_cm}cm" '
            f'svg:height="{height:.3f}cm"><draw:image><office:binary-data>{{{{IMG:{name}}}}}</office:binary-data></draw:image></draw:frame>')


def picture(name, width_cm, caption):
    shot(name)
    return (f'<text:p text:style-name="Pic">{frame(name, width_cm)}</text:p>' + p(caption, 'Cap'))


def cell(content, style='Cell'):
    return f'<table:table-cell table:style-name="{style}" office:value-type="string">{content}</table:table-cell>'


def table(name, columns, rows, style='Tbl'):
    cols = ''.join(f'<table:table-column table:style-name="Col{str(c).replace(".", "")}"/>' for c in columns)
    body = ''.join('<table:table-row>' + ''.join(r) + '</table:table-row>' for r in rows)
    return f'<table:table table:name="{name}" table:style-name="{style}">{cols}{body}</table:table>'


def with_picture(text_xml, name, width_cm, caption, text_w, pic_w):
    """Text on the left, a small screenshot on the right."""
    return table(f'tp_{name}', [text_w, pic_w], [[cell(text_xml), cell(picture(name, width_cm, caption), 'CellPic')]], 'TblIn')


def buttons_diagram():
    box = lambda t: cell(p(t, 'Key'), 'CellKey')
    empty = cell(p('', 'Key'), 'Cell')
    return table('keys', [18, 18, 18],
                 [[empty, box('UP'), empty], [box('LEFT'), box('DOWN'), box('RIGHT')]], 'TblKeys')


LEFT = [
    h('1.  THE FOUR BUTTONS'),
    table('buttons', [56, 35], [[cell(p('The console has four buttons. These are all you need:', 'Body') + buttons_diagram()),
                                 cell(picture('menu', 3.35, 'The menu: LEFT / RIGHT scroll'), 'CellPic')]], 'TblIn'),
    bullet('**Menu:** LEFT and RIGHT scroll through the games (hold to keep going). **UP** starts one.'),
    bullet('**DOWN** (tap) opens that game\'s settings. Games that have some show `vSET`. Hold DOWN for a second to replay the welcome tour.'),
    bullet('**In a game:** hold **UP + DOWN** together for a second to leave. On the result screen UP goes to the menu, RIGHT plays again.'),
    bullet('**In a list** (SETTINGS, VIDEOS, CHAT): UP and DOWN scroll, **RIGHT** picks, **LEFT** goes back.'),

    h('2.  PLAY A FRIEND (TWO CONSOLES)'),
    p('Pong, Tic-Tac-Toe, Rock Paper Scissors and Connect 4. No router and no Wi-Fi: the two consoles talk to each other directly by radio. Keep them within a few metres, both on the same firmware version.'),
    step(1, 'On **both** consoles pick the game, tap **DOWN** on the menu and choose **2 PLAYER**.'),
    step(2, 'One console picks **HOST GAME**, the other picks **JOIN GAME**.'),
    step(3, 'The joiner chooses the host\'s 5-letter ID with LEFT / RIGHT, then **UP**.'),
    step(4, 'The host presses **UP** to accept (DOWN says no). Play!'),
    table('lobby', [30, 31], [[cell(picture('lobby-role', 4.4, 'Host or join')), cell(picture('lobby-find', 4.4, 'Joiner picks a host'))]], 'TblIn'),

    h('3.  CHAT'),
    with_picture(
        p('The **CHAT** tile (after VIDEOS) sends ready-made messages to other consoles over the same radio: "LET\'S PLAY...", "REMATCH?", "GOOD GAME!" and more.', 'Body') +
        bullet('**NEW MESSAGE:** pick who (everyone, a saved contact or a console nearby), then the message. **DELIVERED** = it arrived, **NO REPLY** = nobody answered.') +
        bullet('**INBOX** keeps the last 8. RIGHT answers one.') +
        bullet('A **round dot flashing in the top right corner** means a message is waiting, even in a game or a video.') +
        bullet('Your 5-letter ID is on the CHAT screen. Messages are not secret. Turn listening off: SETTINGS > CHAT > OFF.'),
        'chat-screen', 3.35, 'The CHAT tile', 5.6, 3.5),
]

RIGHT = [
    h('4.  WI-FI PAGE (BACKUP AND CONTACTS)'),
    with_picture(
        p('The console can make its **own private Wi-Fi network**, so a phone or computer can talk to it. It is for backing up scores and saving chat contacts.') +
        step(1, '**SETTINGS** (the last tile) > **WI-FI PAGE**. The screen shows a network name (SSID) and a random password: new every time.') +
        step(2, 'Join that network with your phone or computer, then open `http://192.168.4.1`.') +
        step(3, 'On the page: download or restore your scores, add chat contacts (a 5-letter ID and a name), or erase all scores.') +
        step(4, 'Press **LEFT** on the console to switch it off. One device at a time.'),
        'wifi-page', 3.35, 'What the console shows', 5.6, 3.5),

    h('5.  VIDEOS (NO SOUND)'),
    p('The **VIDEOS** tile plays black and white clips. In the list RIGHT plays and LEFT leaves. While playing: **UP** pauses, LEFT / RIGHT jump 5 seconds, **DOWN** goes back to the list.'),
    p('**Add your own:** pick **+ UPLOAD VIDEOS** and press RIGHT. It makes a Wi-Fi network just like in 4: join it, open `192.168.4.1`, upload a `.pbv` file, then press LEFT on the console. Make a `.pbv` on a computer with `tools/video/pbv.py` (for example `python3 pbv.py movie.mp4 --pixel 2`). A bigger `--pixel` is easier to see and makes a smaller file.'),

    h('6.  SETTINGS (THE LAST TILE)'),
    with_picture(
        p('**BRIGHTNESS**, **SCREEN OFF** (after 1, 2 or 5 minutes; any button wakes it), **CHAT** on/off, **WI-FI PAGE**, **UPDATE FIRMWARE**, resets, **SHOW INTRO** and **ABOUT** (version and your ID).') +
        p('Resets ask you to **hold RIGHT** (2 seconds, 3 for a factory reset), so a long press on the list cannot erase anything.'),
        'settings-list', 3.35, 'SETTINGS', 5.6, 3.5),

    h('7.  UPDATE ONE CONSOLE FROM ANOTHER'),
    p('On both: **SETTINGS > UPDATE FIRMWARE**. One picks **RECEIVE**, the other **SEND** and finds it. The receiver sees what is coming: **UP** installs, **DOWN** says no. It takes about 10 seconds: do not switch off. If anything goes wrong the old firmware stays. Use two consoles wired the same way round (the button layout is copied too).'),

    h('8.  IF SOMETHING IS WRONG'),
    bullet('**Buttons feel mirrored:** you need the `-flipped` firmware file.'),
    bullet('**Blank screen:** check the display\'s two data wires (SDA and SCL).'),
    bullet('**Two consoles do not see each other:** get closer, host picks HOST GAME first, same firmware version.'),
    bullet('**An update sticks at 0%:** the receiving console has firmware older than v4.4: flash it once with a cable.'),
]

STYLES = '''
<style:style style:name="Default" style:family="paragraph"><style:text-properties style:font-name="Lato" fo:font-size="7.6pt" fo:color="#000000"/></style:style>
<style:style style:name="Body" style:family="paragraph"><style:paragraph-properties fo:margin-top="0cm" fo:margin-bottom="0.07cm" fo:line-height="104%"/><style:text-properties style:font-name="Lato" fo:font-size="7.6pt"/></style:style>
<style:style style:name="Head" style:family="paragraph"><style:paragraph-properties fo:margin-top="0.16cm" fo:margin-bottom="0.08cm" fo:keep-with-next="always" fo:border-bottom="0.9pt solid #000000" fo:padding-bottom="0.02cm"/><style:text-properties style:font-name="Lato" fo:font-size="9pt" fo:font-weight="bold" fo:letter-spacing="0.02cm"/></style:style>
<style:style style:name="Bul" style:family="paragraph"><style:paragraph-properties fo:margin-left="0.32cm" fo:text-indent="-0.32cm" fo:margin-bottom="0.05cm" fo:line-height="104%"><style:tab-stops><style:tab-stop style:position="0cm"/></style:tab-stops></style:paragraph-properties><style:text-properties style:font-name="Lato" fo:font-size="7.6pt"/></style:style>
<style:style style:name="Step" style:family="paragraph"><style:paragraph-properties fo:margin-left="0.42cm" fo:text-indent="-0.42cm" fo:margin-bottom="0.05cm" fo:line-height="104%"><style:tab-stops><style:tab-stop style:position="0cm"/></style:tab-stops></style:paragraph-properties><style:text-properties style:font-name="Lato" fo:font-size="7.6pt"/></style:style>
<style:style style:name="Cap" style:family="paragraph"><style:paragraph-properties fo:text-align="center" fo:margin-bottom="0.04cm"/><style:text-properties style:font-name="Lato" fo:font-size="6.4pt" fo:font-style="italic"/></style:style>
<style:style style:name="Pic" style:family="paragraph"><style:paragraph-properties fo:text-align="center" fo:margin-top="0.03cm" fo:margin-bottom="0.02cm"/></style:style>
<style:style style:name="Key" style:family="paragraph"><style:paragraph-properties fo:text-align="center"/><style:text-properties style:font-name="Lato" fo:font-size="8pt" fo:font-weight="bold"/></style:style>
<style:style style:name="Title" style:family="paragraph"><style:paragraph-properties fo:margin-bottom="0cm"/><style:text-properties style:font-name="Lato" fo:font-size="25pt" fo:font-weight="bold" fo:letter-spacing="0.06cm"/></style:style>
<style:style style:name="Sub" style:family="paragraph"><style:paragraph-properties fo:margin-bottom="0cm"/><style:text-properties style:font-name="Lato" fo:font-size="10pt"/></style:style>
<style:style style:name="Tag" style:family="paragraph"><style:paragraph-properties fo:text-align="end" fo:margin-bottom="0.03cm"/><style:text-properties style:font-name="Lato" fo:font-size="7.6pt"/></style:style>
<style:style style:name="Foot" style:family="paragraph"><style:paragraph-properties fo:text-align="center" fo:margin-top="0.12cm" fo:border-top="0.9pt solid #000000" fo:padding-top="0.05cm"/><style:text-properties style:font-name="Lato" fo:font-size="8pt" fo:font-weight="bold"/></style:style>
<style:style style:name="B" style:family="text"><style:text-properties fo:font-weight="bold"/></style:style>
<style:style style:name="M" style:family="text"><style:text-properties style:font-name="DejaVu Sans Mono" fo:font-size="6.9pt"/></style:style>
'''

AUTO = '''
<style:page-layout style:name="pm1"><style:page-layout-properties fo:page-width="21.59cm" fo:page-height="27.94cm" fo:margin-top="0.8cm" fo:margin-bottom="1.2cm" fo:margin-left="1.0cm" fo:margin-right="1.0cm"/></style:page-layout>
<style:style style:name="Tbl" style:family="table"><style:table-properties style:width="19.59cm" table:align="left"/></style:style>
<style:style style:name="TblIn" style:family="table"><style:table-properties style:width="9.1cm" table:align="left"/></style:style>
<style:style style:name="TblKeys" style:family="table"><style:table-properties style:width="5.25cm" table:align="center" fo:margin-top="0.15cm" fo:margin-bottom="0.1cm"/></style:style>
<style:style style:name="Col1" style:family="table-column"><style:table-column-properties style:column-width="9.5cm"/></style:style>
<style:style style:name="Col2" style:family="table-column"><style:table-column-properties style:column-width="0.59cm"/></style:style>
<style:style style:name="Col3" style:family="table-column"><style:table-column-properties style:column-width="9.5cm"/></style:style>
<style:style style:name="Col56" style:family="table-column"><style:table-column-properties style:column-width="5.6cm"/></style:style>
<style:style style:name="Col35" style:family="table-column"><style:table-column-properties style:column-width="3.5cm"/></style:style>
<style:style style:name="Col30" style:family="table-column"><style:table-column-properties style:column-width="4.55cm"/></style:style>
<style:style style:name="Col31" style:family="table-column"><style:table-column-properties style:column-width="4.55cm"/></style:style>
<style:style style:name="Col18" style:family="table-column"><style:table-column-properties style:column-width="1.75cm"/></style:style>
<style:style style:name="Cell" style:family="table-cell"><style:table-cell-properties fo:padding="0cm" fo:border="none"/></style:style>
<style:style style:name="CellPic" style:family="table-cell"><style:table-cell-properties fo:padding-left="0.15cm" fo:padding-top="0.03cm" fo:padding-bottom="0cm" fo:padding-right="0cm" fo:border="none"/></style:style>
<style:style style:name="CellKey" style:family="table-cell"><style:table-cell-properties fo:padding="0.07cm" fo:border="1.1pt solid #000000"/></style:style>
<style:style style:name="Fr" style:family="graphic"><style:graphic-properties style:wrap="none" style:vertical-pos="top" style:vertical-rel="baseline" fo:padding="0cm" fo:border="none"/></style:style>
'''


def build():
    left = ''.join(LEFT)
    right = ''.join(RIGHT)
    header = table('head', [1, 3],
                   [[cell(p('PIXELBIT', 'Title') + p('Pocket arcade: quick start', 'Sub')),
                     cell(p('14 games  +  a video player  +  chat', 'Tag') + p('No router and no computer needed to play.', 'Tag') +
                          p('Full guide: the README at github.com/Simplyph0/PixelBit', 'Tag'))]], 'Tbl')
    page = table('page', [1, 2, 3], [[cell(left), cell(''), cell(right)]], 'Tbl')
    foot = p('Stuck anywhere?  Hold  UP + DOWN  for a second to get out of a game, back to the menu.', 'Foot')
    return header, page, foot


def make_fodt():
    header, page, foot = build()
    body = header + page + foot
    styles = re.sub(r'fo:font-size="([0-9.]+)pt"', lambda m: 'fo:font-size="%.2fpt"' % (float(m.group(1)) * FONT_SCALE), STYLES)
    for name, png in images.items():
        body = body.replace('{{IMG:%s}}' % name, base64.b64encode(png).decode())
    ns = ('xmlns:office="urn:oasis:names:tc:opendocument:xmlns:office:1.0" xmlns:style="urn:oasis:names:tc:opendocument:xmlns:style:1.0" '
          'xmlns:text="urn:oasis:names:tc:opendocument:xmlns:text:1.0" xmlns:table="urn:oasis:names:tc:opendocument:xmlns:table:1.0" '
          'xmlns:draw="urn:oasis:names:tc:opendocument:xmlns:drawing:1.0" xmlns:fo="urn:oasis:names:tc:opendocument:xmlns:xsl-fo-compatible:1.0" '
          'xmlns:xlink="http://www.w3.org/1999/xlink" xmlns:svg="urn:oasis:names:tc:opendocument:xmlns:svg-compatible:1.0"')
    return (f'<?xml version="1.0" encoding="UTF-8"?>\n<office:document {ns} office:version="1.2" '
            f'office:mimetype="application/vnd.oasis.opendocument.text">'
            '<office:font-face-decls><style:font-face style:name="Lato" svg:font-family="Lato"/>'
            '<style:font-face style:name="DejaVu Sans Mono" svg:font-family="\'DejaVu Sans Mono\'"/></office:font-face-decls>'
            f'<office:styles>{styles}</office:styles><office:automatic-styles>{AUTO}</office:automatic-styles>'
            '<office:master-styles><style:master-page style:name="Standard" style:page-layout-name="pm1"/></office:master-styles>'
            f'<office:body><office:text>{body}</office:text></office:body></office:document>')


def main():
    soffice = shutil.which('soffice') or shutil.which('libreoffice')
    if not soffice:
        sys.exit('LibreOffice (soffice) is needed to make the PDF')
    work = tempfile.mkdtemp()
    fodt = os.path.join(work, 'PixelBit-QuickStart.fodt')
    xml = make_fodt()
    open(fodt, 'w', encoding='utf-8').write(xml)
    r = subprocess.run([soffice, '--headless', '--convert-to', 'pdf', '--outdir', work, fodt], capture_output=True, text=True, timeout=180)
    pdf = os.path.join(work, 'PixelBit-QuickStart.pdf')
    if not os.path.exists(pdf):
        sys.exit('conversion failed:\n' + r.stdout + r.stderr)
    shutil.copy(pdf, OUT_PDF)
    if '--keep-source' in sys.argv:
        shutil.copy(fodt, os.path.join(HERE, 'PixelBit-QuickStart.fodt'))
    print('wrote', OUT_PDF)


if __name__ == '__main__':
    main()
