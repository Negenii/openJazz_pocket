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

    def to_bin(self):
        out = bytearray()
        for row in self.px:
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


def text_width(s, scale):
    return len(s) * (GLYPH_W + 1) * scale - scale


def draw_broken_image(c, x, y, size, thick):
    """Our own take on the classic 'image did not load' placeholder: a frame
    with a stepped bite out of the top-right corner, three shapes inside."""
    c.frame(x, y, size, size, thick, BLACK)

    # Stepped tear across the top-right corner: erase the frame there, then
    # draw the staircase that reads as a torn edge.
    bite = size // 3
    c.rect(x + size - bite, y, bite, thick, WHITE)
    c.rect(x + size - thick, y, thick, bite, WHITE)
    steps = 4
    sw = max(thick, bite // steps)
    for i in range(steps):
        sx = x + size - bite + i * sw
        sy = y + i * sw
        c.rect(sx, sy, sw + thick, thick, BLACK)
        c.rect(sx + sw, sy, thick, sw + thick, BLACK)

    # Three shapes, in three greys so they stay distinct without colour.
    unit = max(2, size // 9)
    # top: a plus/blob
    bx, by = x + size // 3, y + size // 4
    c.rect(bx, by, unit * 2, unit * 2, GREY_DARK)
    c.rect(bx - unit, by + unit // 2, unit, unit, GREY_DARK)
    # bottom-left: a staircase
    sx, sy = x + unit, y + size - unit * 4
    for i in range(3):
        c.rect(sx, sy + i * unit, unit * (3 - i), unit, GREY_MID)
    # right: a square
    c.rect(x + size - unit * 4, y + size // 2, unit * 2, unit * 2, GREY_LIGHT)


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
    a = ap.parse_args()

    icon, banner = build_icon(), build_banner()

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
            f.write(icon.to_bin())
        with open(a.platform_image, "wb") as f:
            f.write(banner.to_bin())
        print("wrote %s (%d bytes)" % (icon_path, ICON_W * ICON_H * 2))
        print("wrote %s (%d bytes)" % (a.platform_image, BANNER_W * BANNER_H * 2))


if __name__ == "__main__":
    main()
