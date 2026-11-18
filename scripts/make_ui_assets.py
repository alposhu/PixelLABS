"""Bakes the editor's art into C++: the Dark Ages UI tilesheet and four pixel fonts.

Reads the packs straight from assets/ (zips are opened in memory) and writes
src/gui/UiAssets.inc. Run it again after changing an asset or a sprite rect:

    python scripts/make_ui_assets.py

Needs Pillow and fontTools.
"""
import io
import os
import zipfile

from PIL import Image, ImageDraw, ImageFont
from fontTools.ttLib import TTFont

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ASSETS = os.path.join(ROOT, 'assets')
OUT = os.path.join(ROOT, 'src', 'gui', 'UiAssets.inc')


def read_zip_member(zip_path, suffix):
    with zipfile.ZipFile(zip_path) as z:
        for name in z.namelist():
            if name.lower().endswith(suffix.lower()):
                return z.read(name)
    raise SystemExit('%s not found in %s' % (suffix, zip_path))


# ----------------------------------------------------------------------------
# Tilesheet
# ----------------------------------------------------------------------------

sheet = Image.open(io.BytesIO(read_zip_member(os.path.join(ASSETS, 'UI', 'DarkAgesUi_v1.0.zip'),
                                              '32x32-Tilesheet.png'))).convert('RGBA')
SW, SH = sheet.size
px = sheet.load()

# Palette: index 0 is transparent
colors = [None]
index_of = {}
indices = bytearray(SW * SH)
for y in range(SH):
    for x in range(SW):
        r, g, b, a = px[x, y]
        if a < 128:
            continue
        key = (r, g, b)
        if key not in index_of:
            index_of[key] = len(colors)
            colors.append(key)
        indices[y * SW + x] = index_of[key]
assert len(colors) < 256

# Recolour roles. Every sheet colour is a shade of one of these; a skin swaps the
# role colours and keeps each pixel's shade (darker / lighter than its role).
ROLES = [
    ('gold',     (220, 196, 124)),
    ('tan',      (159, 136, 115)),
    ('camel',    (193, 145, 73)),
    ('charcoal', (46, 50, 42)),
    ('blue',     (64, 108, 145)),
    ('navy',     (21, 32, 47)),
    ('red',      (201, 46, 52)),
    ('green',    (96, 191, 91)),
    ('sky',      (49, 161, 227)),
    ('black',    (0, 0, 0)),
]
# Hand-picked where hue alone guesses wrong
ROLE_OVERRIDE = {
    (22, 29, 29): 'charcoal', (112, 98, 85): 'tan', (155, 131, 85): 'tan',
    (108, 69, 31): 'camel', (86, 33, 12): 'camel', (121, 93, 54): 'camel', (163, 126, 55): 'camel',
    (155, 101, 38): 'camel', (191, 163, 89): 'gold', (251, 214, 147): 'gold', (201, 156, 97): 'camel',
    (65, 83, 108): 'blue', (69, 76, 103): 'navy', (0, 3, 45): 'navy', (56, 48, 95): 'navy',
    (116, 7, 49): 'red', (85, 3, 49): 'red', (255, 113, 91): 'red',
    (51, 127, 89): 'green', (0, 93, 89): 'green',
    (0, 0, 0): 'black',
}


def luma(c):
    return 0.299 * c[0] + 0.587 * c[1] + 0.114 * c[2]


def nearest_role(c):
    if c in ROLE_OVERRIDE:
        return [n for n, _ in ROLES].index(ROLE_OVERRIDE[c])
    best, bi = None, 0
    for i, (_, rc) in enumerate(ROLES):
        d = sum((a - b) ** 2 for a, b in zip(c, rc))
        if best is None or d < best:
            best, bi = d, i
    return bi


role_of = [0]
shade_of = [0]
for c in colors[1:]:
    ri = nearest_role(c)
    base = ROLES[ri][1]
    yc, yb = luma(c), luma(base)
    if ROLES[ri][0] == 'black':
        k = 0.0
    elif yc <= yb:
        k = (yc / yb - 1.0) if yb > 0 else 0.0           # -1..0: towards black
    else:
        k = (yc - yb) / (255.0 - yb) if yb < 255 else 0.0  # 0..1: towards white
    role_of.append(ri)
    shade_of.append(int(round(max(-1.0, min(1.0, k)) * 127)))


def bbox_in(region):
    x0, y0, x1, y1 = region
    bx0, by0, bx1, by1 = x1, y1, x0 - 1, y0 - 1
    for y in range(y0, y1):
        for x in range(x0, x1):
            if indices[y * SW + x]:
                bx0, by0, bx1, by1 = min(bx0, x), min(by0, y), max(bx1, x), max(by1, y)
    assert bx1 >= bx0, region
    return (bx0, by0, bx1 - bx0 + 1, by1 - by0 + 1)


def opaque_width(y, x0, x1):
    return sum(1 for x in range(x0, x1) if indices[y * SW + x])


def split_crest(rect):
    """Gilded pieces carry a small crest on top. Split it off so the body can stretch."""
    x, y, w, h = rect
    body_top = y
    for yy in range(y, y + h):
        if opaque_width(yy, x, x + w) > w * 0.7:
            body_top = yy
            break
    crest = bbox_in((x, y, x + w, body_top))
    body = (x, body_top, w, y + h - body_top)
    return body, crest


sprites = []  # (enum name, rect, comment)


def add(name, rect, note=''):
    sprites.append((name, rect, note))


add('kSprFrame', bbox_in((0, 0, 96, 96)), 'gold knotwork window, 9-slice 24')
add('kSprParchment', bbox_in((96, 0, 192, 96)), 'parchment, 9-slice 8')
add('kSprKnotDark', bbox_in((192, 0, 288, 96)), 'dark knot panel, 9-slice 14')
add('kSprKnotMid', bbox_in((288, 0, 384, 96)), 'charcoal knot panel, 9-slice 14')
add('kSprBarDark', bbox_in((0, 96, 64, 128)), 'scroll-end bar, 3-slice 9')
add('kSprBarMid', bbox_in((64, 96, 128, 128)), 'scroll-end bar, lighter')
body, crest = split_crest(bbox_in((128, 96, 192, 128)))
add('kSprBarGilt', body, 'gilded blue bar, 3-slice 9')
add('kSprBarGiltCrest', crest)
add('kSprSlotDark', bbox_in((192, 96, 224, 128)), 'square slot, 9-slice 7')
add('kSprSlotMid', bbox_in((224, 96, 256, 128)))
body, crest = split_crest(bbox_in((256, 96, 288, 128)))
add('kSprSlotGilt', body, 'gilded blue slot, 9-slice 7')
add('kSprSlotGiltCrest', crest)
add('kSprFlatDark', bbox_in((288, 96, 320, 128)), 'flat tab, 3-slice 4')
add('kSprFlatMid', bbox_in((320, 96, 352, 128)))
body, crest = split_crest(bbox_in((352, 96, 384, 128)))
add('kSprFlatGilt', body)
add('kSprFlatGiltCrest', crest)
add('kSprVBarDark', bbox_in((0, 128, 32, 224)), 'vertical gauge, 3-slice 10')
add('kSprVBarMid', bbox_in((32, 128, 64, 224)))
add('kSprVBarNavy', bbox_in((64, 128, 96, 224)))
add('kSprHBarDark', bbox_in((96, 128, 192, 160)), 'horizontal gauge, 3-slice 10')
add('kSprHBarMid', bbox_in((96, 160, 192, 192)))
add('kSprHBarNavy', bbox_in((96, 192, 192, 224)))
body, crest = split_crest(bbox_in((192, 128, 288, 160)))
add('kSprGiltGauge', body, 'gilded gauge, 3-slice 6')
add('kSprGiltGaugeCrest', crest)
add('kSprGemBlue', bbox_in((288, 128, 384, 160)), 'gem fill bars, 3-slice 3')
add('kSprGemRed', bbox_in((192, 160, 288, 192)))
add('kSprGemGreen', bbox_in((288, 160, 384, 192)))
add('kSprStickV', bbox_in((0, 224, 32, 256)))
add('kSprStickH', bbox_in((32, 224, 64, 256)))
add('kSprCross', bbox_in((64, 224, 96, 256)))
add('kSprWarning', bbox_in((96, 224, 128, 256)))
for i in range(4):
    add('kSprRuneGold%d' % i, bbox_in((i * 32, 256, i * 32 + 32, 288)))
for i in range(4):
    add('kSprRuneGrey%d' % i, bbox_in((i * 32, 288, i * 32 + 32, 320)))
add('kSprChainV', bbox_in((128, 224, 160, 320)))
add('kSprChainH', bbox_in((192, 256, 288, 288)))
add('kSprDividerLight', bbox_in((288, 256, 384, 288)))
add('kSprDividerDark', bbox_in((288, 288, 384, 320)))
add('kSprFlourishLight', bbox_in((320, 192, 384, 224)))
add('kSprFlourishDark', bbox_in((320, 224, 384, 256)))
add('kSprParchmentSmall', bbox_in((0, 320, 64, 352)))


def packbits(data):
    out = bytearray()
    i, n = 0, len(data)
    while i < n:
        run = 1
        while i + run < n and run < 128 and data[i + run] == data[i]:
            run += 1
        if run >= 3:
            out += bytes([0x80 | (run - 1), data[i]])
            i += run
            continue
        j = i
        lit = bytearray()
        while j < n and len(lit) < 128:
            if j + 2 < n and data[j] == data[j + 1] == data[j + 2]:
                break
            lit.append(data[j])
            j += 1
        out += bytes([len(lit) - 1]) + lit
        i = j
    return out


rle = packbits(indices)

# ----------------------------------------------------------------------------
# Fonts (all four are drawn on a 16 px em grid: one font unit = one pixel)
# ----------------------------------------------------------------------------

S = io.BytesIO
FONTS = [
    ('Body', 'BoldPixels', open(os.path.join(ASSETS, 'fonts', 'BoldPixels.ttf'), 'rb').read()),
    ('Title', 'Abaddon Bold', read_zip_member(os.path.join(ASSETS, 'fonts', 'Abaddon_Fonts_v1.2.zip'), 'Abaddon Bold.ttf')),
    ('Light', 'Abaddon Light', read_zip_member(os.path.join(ASSETS, 'fonts', 'Abaddon_Fonts_v1.2.zip'), 'Abaddon Light.ttf')),
    ('Chunky', 'Fibberish', read_zip_member(os.path.join(ASSETS, 'fonts', 'Fibberish v1.0.zip'), 'fibberish.ttf')),
]
# ASCII, Latin-1 letters and the Turkish letters from Latin Extended-A
WANTED = list(range(32, 127)) + list(range(0xC0, 0x100)) + [0x11E, 0x11F, 0x130, 0x131, 0x15E, 0x15F]

font_blocks = []
for key, label, data in FONTS:
    tt = TTFont(S(data))
    cmap = tt.getBestCmap()
    font = ImageFont.truetype(S(data), 16)
    ascent, descent = font.getmetrics()
    glyphs = []
    rows = []
    for cp in WANTED:
        if cp not in cmap:
            continue
        ch = chr(cp)
        adv = int(round(font.getlength(ch)))
        if cp == 32:
            glyphs.append((cp, 0, 0, 0, 0, adv, len(rows)))
            continue
        img = Image.new('1', (64, 64), 0)
        ImageDraw.Draw(img).text((16, 40), ch, font=font, fill=1, anchor='ls')
        bb = img.getbbox()
        if not bb:
            glyphs.append((cp, 0, 0, 0, 0, adv, len(rows)))
            continue
        x0, y0, x1, y1 = bb
        w, h = x1 - x0, y1 - y0
        assert w <= 16, (label, ch, w)
        start = len(rows)
        gp = img.load()
        for yy in range(y0, y1):
            bits = 0
            for xx in range(x0, x1):
                if gp[xx, yy]:
                    bits |= 1 << (xx - x0)
            rows.append(bits)
        glyphs.append((cp, x0 - 16, y0 - 40, w, h, adv, start))
    cap = font.getbbox('H', anchor='ls')
    font_blocks.append((key, label, ascent, descent, -cap[1], glyphs, rows))


# ----------------------------------------------------------------------------
# Emit
# ----------------------------------------------------------------------------

def wrap(values, per=24):
    vals = [str(v) for v in values]
    return '\n'.join('    ' + ','.join(vals[i:i + per]) + ',' for i in range(0, len(vals), per))


lines = []
w = lines.append
w('// Generated by scripts/make_ui_assets.py - do not edit by hand.')
w('// Art: "Dark Ages UI" by Hypnobius (hypnobius.itch.io).')
w('// Fonts: BoldPixels by YukiPixels (CC BY-SA 4.0); Abaddon and Fibberish by Nathan Scott.')
w('')
w('namespace UiAssets {')
w('')
w('static const int kAtlasW = %d, kAtlasH = %d;' % (SW, SH))
w('static const int kNumAtlasColors = %d;' % len(colors))
w('static const uint32_t kAtlasColors[kNumAtlasColors] = {')
w(wrap(['0x00000000u'] + ['0xFF%02X%02X%02Xu' % c for c in colors[1:]], 8))
w('};')
w('enum RecolorRole { %s, kNumRoles };' % ', '.join('kRole' + n.capitalize() for n, _ in ROLES))
w('static const uint32_t kRoleBase[kNumRoles] = { %s };' % ', '.join('0xFF%02X%02X%02Xu' % c for _, c in ROLES))
w('static const uint8_t kAtlasRole[kNumAtlasColors] = {')
w(wrap(role_of))
w('};')
w('// Shade relative to the role colour: <0 darker (x/127 towards black), >0 lighter (towards white)')
w('static const int8_t kAtlasShade[kNumAtlasColors] = {')
w(wrap(shade_of))
w('};')
w('// Palette indices, PackBits: n<128 -> n+1 literal bytes follow; n>=128 -> next byte repeated n-127 times')
w('static const uint8_t kAtlasRle[%d] = {' % len(rle))
w(wrap(list(rle), 32))
w('};')
w('')
w('enum SpriteId {')
for name, rect, note in sprites:
    w('    %s,%s' % (name, ('  // ' + note) if note else ''))
w('    kNumSprites')
w('};')
w('struct SpriteRect { int16_t x, y, w, h; };')
w('static const SpriteRect kSprites[kNumSprites] = {')
for name, (x, y, ww, hh), _ in sprites:
    w('    { %d, %d, %d, %d }, // %s' % (x, y, ww, hh, name))
w('};')
w('')
w('struct GlyphDef { uint16_t cp; int8_t xoff, yoff; uint8_t w, h, adv; uint16_t row; };')
w('struct FontDef { const GlyphDef* glyphs; int count; const uint16_t* rows; int ascent, descent, capHeight; };')
for key, label, ascent, descent, cap, glyphs, rows in font_blocks:
    w('')
    w('// %s: %d glyphs, ascent %d, descent %d, cap height %d' % (label, len(glyphs), ascent, descent, cap))
    w('static const uint16_t kRows%s[%d] = {' % (key, max(1, len(rows))))
    w(wrap(rows or [0], 24))
    w('};')
    w('static const GlyphDef kGlyphs%s[%d] = {' % (key, len(glyphs)))
    for g in glyphs:
        w('    { %d, %d, %d, %d, %d, %d, %d },' % g)
    w('};')
    w('static const FontDef kFontDef%s = { kGlyphs%s, %d, kRows%s, %d, %d, %d };'
      % (key, key, len(glyphs), key, ascent, descent, cap))
w('')
w('} // namespace UiAssets')
w('')

with open(OUT, 'w', encoding='utf-8', newline='\n') as f:
    f.write('\n'.join(lines))

print('wrote %s: %d colours, %d sprites, atlas %d bytes RLE (%d raw)'
      % (os.path.relpath(OUT, ROOT), len(colors), len(sprites), len(rle), len(indices)))
for key, label, ascent, descent, cap, glyphs, rows in font_blocks:
    print('  %-14s %3d glyphs, asc %d desc %d cap %d' % (label, len(glyphs), ascent, descent, cap))
for name, rect, note in sprites:
    print('  %-22s %s' % (name, rect))
