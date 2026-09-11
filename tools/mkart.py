#!/usr/bin/env python3
"""Generate the Pocket core art: Cores/<id>/icon.bin and Platforms/_images/<p>.bin.

Both files are raw greyscale bitmaps, two bytes per pixel, brightness in the
upper byte and the lower byte zero (verified against the SDK's own art:
dist/sdk/Cores/ThinkElastic.openfpgaOS/icon.bin is 36x36x2 and
dist/sdk/Platforms/_images/openfpgaos.bin is 521x165x2, both using only
0xFF00 and 0x0000). Rows run top to bottom, pixels left to right.

Everything drawn here is original: a blocky 5x7 font defined below, and our
own rendition of the "image failed to load" motif (a frame with a torn
corner and three shapes inside). No game artwork is used.

    python3 tools/mkart.py --preview     # ASCII preview, writes nothing
    python3 tools/mkart.py --write       # write both .bin files into dist/
"""

import argparse
import os

ICON_W, ICON_H = 36, 36
BANNER_W, BANNER_H = 521, 165

WHITE, BLACK = 0xFF, 0x00
GREY_LIGHT, GREY_MID, GREY_DARK = 0xC0, 0x88, 0x50

# ---------------------------------------------------------------- 5x7 font ---
# One string per row, '#' is ink. Only the glyphs the captions need.
FONT = {
    "A": (".###.", "#...#", "#...#", "#####", "#...#", "#...#", "#...#"),
    "B": ("####.", "#...#", "#...#", "####.", "#...#", "#...#", "####."),
    "C": (".###.", "#...#", "#....", "#....", "#....", "#...#", ".###."),
    "E": ("#####", "#....", "#....", "####.", "#....", "#....", "#####"),
    "F": ("#####", "#....", "#....", "####.", "#....", "#....", "#...."),
    "G": (".###.", "#...#", "#....", "#..##", "#...#", "#...#", ".###."),
    "I": ("#####", "..#..", "..#..", "..#..", "..#..", "..#..", "#####"),
    "J": ("#####", "...#.", "...#.", "...#.", "...#.", "#..#.", ".##.."),
    "K": ("#...#", "#..#.", "#.#..", "##...", "#.#..", "#..#.", "#...#"),
    "L": ("#....", "#....", "#....", "#....", "#....", "#....", "#####"),
    "N": ("#...#", "##..#", "##..#", "#.#.#", "#..##", "#..##", "#...#"),
    "O": (".###.", "#...#", "#...#", "#...#", "#...#", "#...#", ".###."),
    "P": ("####.", "#...#", "#...#", "####.", "#....", "#....", "#...."),
    "R": ("####.", "#...#", "#...#", "####.", "#.#..", "#..#.", "#...#"),
    "T": ("#####", "..#..", "..#..", "..#..", "..#..", "..#..", "..#.."),
    "U": ("#...#", "#...#", "#...#", "#...#", "#...#", "#...#", ".###."),
    "Z": ("#####", "....#", "...#.", "..#..", ".#...", "#....", "#####"),
    " ": (".....", ".....", ".....", ".....", ".....", ".....", "....."),
}
GLYPH_W, GLYPH_H = 5, 7


class Canvas:
    def __init__(self, w, h, fill=WHITE):
        self.w, self.h = w, h
        self.px = [[fill] * w for _ in range(h)]

    def set(self, x, y, v):
        if 0 <= x < self.w and 0 <= y < self.h:
            self.px[y][x] = v

    def rect(self, x, y, w, h, v):
        for yy in range(y, y + h):
            for xx in range(x, x + w):
                self.set(xx, yy, v)

    def frame(self, x, y, w, h, t, v):
        self.rect(x, y, w, t, v)
        self.rect(x, y + h - t, w, t, v)
        self.rect(x, y, t, h, v)
        self.rect(x + w - t, y, t, h, v)

    def text(self, x, y, s, scale, v):
        """Draw s with the 5x7 font scaled by an integer factor."""
        cx = x
        for ch in s.upper():
            glyph = FONT.get(ch, FONT[" "])
            for gy, row in enumerate(glyph):
                for gx, cell in enumerate(row):
                    if cell == "#":
                        self.rect(cx + gx * scale, y + gy * scale, scale, scale, v)
            cx += (GLYPH_W + 1) * scale
        return cx - x

    def to_bin(self, transpose=False, rotate_ccw=False):
        """Two bytes per pixel, brightness first, low byte zero.

        The core icon is stored as it reads: 36 rows of 36 pixels. The
        platform banner is stored ROTATED -- the file is 165 pixels wide and
        521 tall. That is not a guess: the SDK's own Platforms/_images art
        only resolves into legible text when read at 165x521, and reading it
        at 521x165 gives noise, which is exactly what a banner written the
        naive way looks like on the Pocket.
        """
        # The Pocket draws these images inverted: a byte of 0xFF shows as
        # black on screen. Confirmed on hardware -- art written straight
        # through came back with every tone reversed. So invert on the way
        # out and keep the PNGs the right way round, which is what anyone
        # editing them expects.
        rows = [[0xFF - v for v in row] for row in self.px]
        if transpose:
            # Rotate rather than merely transpose: a plain transpose put the
            # banner on screen mirrored left to right, so the source column is
            # read from the far edge.
            src = rows
            rows = [[src[y][self.w - 1 - x] for y in range(self.h)]
                    for x in range(self.w)]
        if rotate_ccw:
            # The icon is stored rotated too: written straight through it
            # appears a quarter turn clockwise on the device, so store it
            # turned the other way.
            src = rows
            h, w = len(src), len(src[0])
            rows = [[src[x][w - 1 - y] for x in range(h)] for y in range(w)]
        out = bytearray()
        for row in rows:
            for v in row:
                out += bytes((v & 0xFF, 0x00))
        return bytes(out)

    def preview(self, cols=None):
        """Block-minimum downsample, so thin strokes survive the shrink."""
        ramp = " .:-=+*#%@"
        cols = cols or self.w
        sx = max(1, self.w // cols)
        sy = sx * 2
        lines = []
        for y in range(0, self.h, sy):
            line = []
            for x in range(0, self.w, sx):
                block = [self.px[yy][xx]
                         for yy in range(y, min(y + sy, self.h))
                         for xx in range(x, min(x + sx, self.w))]
                v = min(block)
                line.append(ramp[(255 - v) * (len(ramp) - 1) // 255])
            lines.append("".join(line))
        return "\n".join(lines)


def png_write(path, rows):
    """Minimal 8-bit greyscale PNG writer (no filtering, one IDAT)."""
    import struct, zlib
    h, w = len(rows), len(rows[0])
    raw = b"".join(b"\x00" + bytes(r) for r in rows)

    def chunk(tag, data):
        return (struct.pack(">I", len(data)) + tag + data
                + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF))

    ihdr = struct.pack(">IIBBBBB", w, h, 8, 0, 0, 0, 0)   # 8-bit greyscale
    with open(path, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n")
        f.write(chunk(b"IHDR", ihdr))
        f.write(chunk(b"IDAT", zlib.compress(raw, 9)))
        f.write(chunk(b"IEND", b""))


def png_read_grey(path):
    """Read an 8-bit PNG (greyscale, RGB or RGBA, non-interlaced) as greys."""
    import struct, zlib
    data = open(path, "rb").read()
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise SystemExit("%s is not a PNG" % path)
    pos, idat, hdr = 8, bytearray(), None
    while pos < len(data):
        (ln,) = struct.unpack(">I", data[pos:pos + 4])
        tag = data[pos + 4:pos + 8]
        body = data[pos + 8:pos + 8 + ln]
        if tag == b"IHDR":
            hdr = struct.unpack(">IIBBBBB", body)
        elif tag == b"IDAT":
            idat += body
        elif tag == b"IEND":
            break
        pos += 12 + ln
    w, h, depth, ctype, _comp, _filt, interlace = hdr
    if depth != 8 or interlace or ctype not in (0, 2, 6):
        raise SystemExit("need an 8-bit, non-interlaced greyscale/RGB/RGBA PNG "
                         "(got depth %d, colour type %d, interlace %d)"
                         % (depth, ctype, interlace))
    nch = {0: 1, 2: 3, 6: 4}[ctype]
    raw = zlib.decompress(bytes(idat))
    stride = w * nch
    rows, prev = [], bytearray(stride)
    o = 0
    for _ in range(h):
        ft = raw[o]; o += 1
        line = bytearray(raw[o:o + stride]); o += stride
        for i in range(stride):
            a = line[i - nch] if i >= nch else 0
            b = prev[i]
            c = prev[i - nch] if i >= nch else 0
            if ft == 1: line[i] = (line[i] + a) & 0xFF
            elif ft == 2: line[i] = (line[i] + b) & 0xFF
            elif ft == 3: line[i] = (line[i] + ((a + b) >> 1)) & 0xFF
            elif ft == 4:
                pa, pb, pc = abs(b - c), abs(a - c), abs(a + b - 2 * c)
                pred = a if (pa <= pb and pa <= pc) else (b if pb <= pc else c)
                line[i] = (line[i] + pred) & 0xFF
            elif ft != 0:
                raise SystemExit("unknown PNG row filter %d" % ft)
        prev = line
        if nch == 1:
            rows.append(list(line))
        else:
            rows.append([(line[i] * 299 + line[i + 1] * 587 + line[i + 2] * 114) // 1000
                         for i in range(0, stride, nch)])
    return rows


def downscale(rows, w, h):
    """Average square blocks down to w x h. The source must be an exact
    integer multiple, so hand-drawn art can be made at a comfortable size."""
    sh, sw = len(rows), len(rows[0])
    if sw == w and sh == h:
        return rows
    if sw % w or sh % h or sw // w != sh // h:
        raise SystemExit("image is %dx%d; need %dx%d or an exact integer "
                         "multiple of it" % (sw, sh, w, h))
    n = sw // w
    out = []
    for y in range(h):
        row = []
        for x in range(w):
            block = [rows[y * n + dy][x * n + dx] for dy in range(n) for dx in range(n)]
            row.append(sum(block) // len(block))
        out.append(row)
    return out


def text_width(s, scale):
    return len(s) * (GLYPH_W + 1) * scale - scale


def draw_broken_image(c, x, y, size, thick):
    """Our own take on the classic "image did not load" placeholder.

    The frame is deliberately interrupted along the top-right corner: the
    tear leaves real gaps in the outline rather than a neatly closed
    staircase. The three shapes inside sit on plain white, each in its own
    grey so they stay distinct without colour.
    """
    bite = size // 3

    # Frame, with the torn corner left open: the top edge stops short of it
    # and the right edge starts below it.
    c.rect(x, y, size - bite, thick, BLACK)                     # top
    c.rect(x, y + size - thick, size, thick, BLACK)             # bottom
    c.rect(x, y, thick, size, BLACK)                            # left
    c.rect(x + size - thick, y + bite, thick, size - bite, BLACK)  # right

    # The tear itself: short dashes stepping down to the right, with gaps
    # between them, so the outline reads as broken rather than merely bent.
    step = max(thick, bite // 3)
    for i in range(3):
        sx = x + size - bite + i * step
        sy = y + i * step
        c.rect(sx, sy, step - thick, thick, BLACK)              # horizontal dash
        if i < 2:
            c.rect(sx + step - thick, sy, thick, step - thick, BLACK)  # riser

    # Three shapes on white, three greys.
    unit = max(2, size // 9)
    inset = thick + unit // 2
    c.rect(x + size // 3, y + inset + unit, unit * 2, unit * 2, GREY_DARK)
    sx, sy = x + inset, y + size - inset - unit * 3
    for i in range(3):
        c.rect(sx, sy + i * unit, unit * (3 - i), unit, GREY_MID)
    c.rect(x + size - inset - unit * 3, y + size // 2, unit * 2, unit * 2, GREY_LIGHT)


def build_icon():
    c = Canvas(ICON_W, ICON_H, WHITE)
    draw_broken_image(c, 2, 2, ICON_W - 4, 2)
    return c


def build_banner():
    c = Canvas(BANNER_W, BANNER_H, WHITE)

    title, sub1, sub2 = "OPENJAZZ", "FOR JAZZ JACKRABBIT", "ANALOGUE POCKET"
    ts, ss = 6, 2
    left = 22
    ty = 34
    c.text(left, ty, title, ts, BLACK)
    # A rule under the title, as wide as the title itself.
    c.rect(left, ty + GLYPH_H * ts + 10, text_width(title, ts), 3, BLACK)
    c.text(left, ty + GLYPH_H * ts + 26, sub1, ss, BLACK)
    c.text(left, ty + GLYPH_H * ts + 26 + GLYPH_H * ss + 8, sub2, ss, GREY_MID)

    # The joke: on the right, the picture that did not load.
    side = 108
    draw_broken_image(c, BANNER_W - side - 34, (BANNER_H - side) // 2, side, 4)
    return c


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--write", action="store_true", help="write the .bin files")
    ap.add_argument("--preview", action="store_true", help="print ASCII previews")
    ap.add_argument("--core-dir", default="dist/openjazz/Cores/negenii.OpenJazz")
    ap.add_argument("--platform-image", default="dist/openjazz/Platforms/_images/openjazz.bin")
    ap.add_argument("--export-icon-png", metavar="PATH",
                    help="write the generated icon as an editable PNG")
    ap.add_argument("--export-banner-png", metavar="PATH",
                    help="write the generated banner as an editable PNG "
                         "(521x165, the way it looks on screen)")
    ap.add_argument("--import-banner-png", metavar="PATH",
                    help="build the platform image from a PNG (521x165, or an "
                         "exact integer multiple); the rotation the format "
                         "wants is applied on the way out")
    ap.add_argument("--import-icon-png", metavar="PATH",
                    help="build icon.bin from a PNG (36x36, or an exact "
                         "integer multiple) instead of generating it")
    a = ap.parse_args()

    icon, banner = build_icon(), build_banner()

    if a.export_icon_png:
        png_write(a.export_icon_png, icon.px)
        print("wrote %s (%dx%d, 8-bit greyscale)" % (a.export_icon_png, ICON_W, ICON_H))

    if a.export_banner_png:
        png_write(a.export_banner_png, banner.px)
        print("wrote %s (%dx%d, 8-bit greyscale)"
              % (a.export_banner_png, BANNER_W, BANNER_H))

    if a.import_banner_png:
        banner.px = downscale(png_read_grey(a.import_banner_png), BANNER_W, BANNER_H)
        print("banner taken from %s" % a.import_banner_png)

    if a.import_icon_png:
        icon.px = downscale(png_read_grey(a.import_icon_png), ICON_W, ICON_H)
        print("icon taken from %s" % a.import_icon_png)

    if a.preview or not a.write:
        print("icon 36x36:")
        print(icon.preview())
        print("\nbanner 521x165:")
        print(banner.preview(cols=130))

    if a.write:
        icon_path = os.path.join(a.core_dir, "icon.bin")
        os.makedirs(a.core_dir, exist_ok=True)
        os.makedirs(os.path.dirname(a.platform_image), exist_ok=True)
        with open(icon_path, "wb") as f:
            f.write(icon.to_bin(rotate_ccw=True))
        with open(a.platform_image, "wb") as f:
            f.write(banner.to_bin(transpose=True))
        print("wrote %s (%d bytes)" % (icon_path, ICON_W * ICON_H * 2))
        print("wrote %s (%d bytes)" % (a.platform_image, BANNER_W * BANNER_H * 2))


if __name__ == "__main__":
    main()
