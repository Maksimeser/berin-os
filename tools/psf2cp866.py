#!/usr/bin/env python3
"""PSF1/PSF2 font -> CP866 C array for VGA text mode."""

import sys

CP866 = [
    0x0410,0x0411,0x0412,0x0413,0x0414,0x0415,0x0416,0x0417,
    0x0418,0x0419,0x041A,0x041B,0x041C,0x041D,0x041E,0x041F,
    0x0420,0x0421,0x0422,0x0423,0x0424,0x0425,0x0426,0x0427,
    0x0428,0x0429,0x042A,0x042B,0x042C,0x042D,0x042E,0x042F,
    0x0430,0x0431,0x0432,0x0433,0x0434,0x0435,0x0436,0x0437,
    0x0438,0x0439,0x043A,0x043B,0x043C,0x043D,0x043E,0x043F,
    0x2591,0x2592,0x2593,0x2502,0x2524,0x2561,0x2562,0x2556,
    0x2555,0x2563,0x2551,0x2557,0x255D,0x255C,0x255B,0x2510,
    0x2514,0x2534,0x252C,0x251C,0x2500,0x253C,0x255E,0x255F,
    0x255A,0x2554,0x2569,0x2566,0x2560,0x2550,0x256C,0x2567,
    0x2568,0x2564,0x2565,0x2559,0x2558,0x2552,0x2553,0x256B,
    0x256A,0x2518,0x250C,0x2588,0x2584,0x258C,0x2590,0x2580,
    0x0440,0x0441,0x0442,0x0443,0x0444,0x0445,0x0446,0x0447,
    0x0448,0x0449,0x044A,0x044B,0x044C,0x044D,0x044E,0x044F,
    0x0401,0x0451,0x0404,0x0454,0x0407,0x0457,0x040E,0x045E,
    0x00B0,0x2219,0x00B7,0x221A,0x2116,0x00A4,0x25A0,0x00A0,
]


def _read_utf8(data, i):
    """Read one UTF-8 char from data[i:], return (codepoint, new_i)."""
    b = data[i]
    if b < 0x80:
        return b, i + 1
    if b < 0xE0:
        return ((b & 0x1F) << 6) | (data[i+1] & 0x3F), i + 2
    if b < 0xF0:
        return (((b & 0x0F) << 12)
                | ((data[i+1] & 0x3F) << 6)
                | (data[i+2] & 0x3F)), i + 3
    return 0, i + 4  # 4-byte, ignore


def parse_psf1(data):
    mode     = data[2]
    charsiz  = data[3]
    nglyph   = 512 if (mode & 0x01) else 256
    has_uni  = bool(mode & 0x02)
    g_start  = 4
    g_end    = g_start + nglyph * charsiz
    glyphs   = data[g_start:g_end]

    table = None
    if has_uni:
        i = g_end
        table = []
        for _ in range(nglyph):
            chars = []
            while data[i] != 0xFF:
                cp, i = _read_utf8(data, i)
                chars.append(cp)
            i += 1
            table.append(chars)
    return glyphs, charsiz, table


def parse_psf2(data):
    hs      = int.from_bytes(data[8:12],  'little')
    flags   = int.from_bytes(data[12:16], 'little')
    nglyph  = int.from_bytes(data[16:20], 'little')
    charsiz = int.from_bytes(data[20:24], 'little')
    glyphs  = data[hs:hs + nglyph * charsiz]

    table = None
    if flags & 1:
        i = hs + nglyph * charsiz
        table = []
        for _ in range(nglyph):
            chars = []
            while data[i] != 0xFF:
                cp, i = _read_utf8(data, i)
                chars.append(cp)
            i += 1
            table.append(chars)
    return glyphs, charsiz, table


def parse_psf(path):
    with open(path, 'rb') as f:
        data = f.read()
    if data[:2] == b'\x36\x04':
        return parse_psf1(data)
    if data[:4] == b'\x72\xb5\x4a\x86':
        return parse_psf2(data)
    raise ValueError("not a PSF1 or PSF2 font")


def main():
    if len(sys.argv) < 2:
        sys.exit("usage: psf2cp866.py <font.psfu> > cp866_8x16.h")

    glyphs, charsiz, table = parse_psf(sys.argv[1])
    if charsiz != 16:
        sys.exit(f"expected 16 bytes/glyph, got {charsiz}")

    print("#ifndef BERIN_CP866_8X16_H")
    print("#define BERIN_CP866_8X16_H")
    print()
    print("#include <stdint.h>")
    print()

    if not table:
        # No unicode table — assume font is already in CP866 order
        sys.stderr.write("# no unicode table: treating glyph index as CP866 code\n")
        nglyph = len(glyphs) // 16
        sys.stderr.write(f"# glyph count: {nglyph}\n")
        if nglyph < 256:
            sys.exit(f"only {nglyph} glyphs, need at least 256 for CP866")

        print("static const uint8_t cp866_8x16[256][16] = {")
        for code in range(256):
            row = glyphs[code * 16:(code + 1) * 16]
            print("    { " + ", ".join(f"0x{b:02X}" for b in row) + " },")
        print("};")
    else:
        # Has unicode table — reverse-map CP866 codepoints to glyph indices
        rev = {}
        for g, chars in enumerate(table):
            for cp in chars:
                if cp not in rev:
                    rev[cp] = g

        sys.stderr.write(f"# glyphs mapped: {len(rev)}\n")
        sys.stderr.write(f"# 'А' (U+0410) -> glyph {rev.get(0x0410, 'MISSING')}\n")
        sys.stderr.write(f"# 'я' (U+044F) -> glyph {rev.get(0x044F, 'MISSING')}\n")

        print("static const uint8_t cp866_8x16[256][16] = {")
        for code in range(256):
            cp = code if code < 0x80 else CP866[code - 0x80]
            g  = rev.get(cp, 0)
            row = glyphs[g * 16:(g + 1) * 16]
            print("    { " + ", ".join(f"0x{b:02X}" for b in row) + " },")
        print("};")

    print()
    print("#endif")


if __name__ == '__main__':
    main()
