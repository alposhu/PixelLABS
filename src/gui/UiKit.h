#pragma once

// Sprite and font drawing for the editor, built on the baked Dark Ages UI art.
// Included by PixelLabsView.cpp only (UiAssets.inc holds the data tables).

#include "PixelFont5x7.h"
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <vector>
#include <algorithm>

namespace PixelLabs {

#include "UiAssets.inc"

namespace ui {

using namespace UiAssets;

// ---------------------------------------------------------------------------
// Atlas + recolouring
// ---------------------------------------------------------------------------

inline const uint8_t* atlas() {
    static const std::vector<uint8_t> pixels = [] {
        std::vector<uint8_t> out;
        out.reserve(static_cast<size_t>(kAtlasW) * kAtlasH);
        size_t i = 0;
        const size_t n = sizeof(kAtlasRle);
        while (i < n) {
            const uint8_t h = kAtlasRle[i++];
            if (h & 0x80) {
                const int run = (h & 0x7F) + 1;
                const uint8_t v = i < n ? kAtlasRle[i++] : 0;
                out.insert(out.end(), run, v);
            } else {
                const int lit = h + 1;
                for (int k = 0; k < lit && i < n; ++k) out.push_back(kAtlasRle[i++]);
            }
        }
        out.resize(static_cast<size_t>(kAtlasW) * kAtlasH, 0);
        return out;
    }();
    return pixels.data();
}

// One colour per atlas palette index for the current skin. Index 0 stays transparent.
struct Skin {
    uint32_t lut[kNumAtlasColors] = {};
};

inline uint32_t shadeColor(uint32_t base, int k) {
    int r = (base >> 16) & 0xFF, g = (base >> 8) & 0xFF, b = base & 0xFF;
    if (k < 0) {
        const int f = 127 + k;
        r = r * f / 127; g = g * f / 127; b = b * f / 127;
    } else if (k > 0) {
        r += (255 - r) * k / 127; g += (255 - g) * k / 127; b += (255 - b) * k / 127;
    }
    return 0xFF000000u | (static_cast<uint32_t>(r) << 16) | (static_cast<uint32_t>(g) << 8) | static_cast<uint32_t>(b);
}

// roles == nullptr keeps the art's own colours.
inline void buildSkin(Skin& s, const uint32_t* roles) {
    for (int i = 0; i < kNumAtlasColors; ++i) {
        if (i == 0) { s.lut[i] = 0; continue; }
        if (!roles) { s.lut[i] = kAtlasColors[i]; continue; }
        const int role = kAtlasRole[i];
        s.lut[i] = role == kRoleBlack ? roles[role] : shadeColor(roles[role], kAtlasShade[i]);
    }
}

// Blits a rectangle of the atlas (sx,sy,sw,sh) to (dx,dy).
inline void blit(PixelCanvas& c, const Skin& s, int sx, int sy, int sw, int sh, int dx, int dy) {
    const uint8_t* a = atlas();
    for (int y = 0; y < sh; ++y) {
        const int py = dy + y;
        if (py < 0 || py >= c.height) continue;
        const uint8_t* row = a + (sy + y) * kAtlasW + sx;
        uint32_t* dst = c.pixels + py * c.width;
        for (int x = 0; x < sw; ++x) {
            const uint8_t idx = row[x];
            const int px = dx + x;
            if (idx && px >= 0 && px < c.width) dst[px] = s.lut[idx];
        }
    }
}

// Same, but every opaque pixel in one colour (icons, shadows).
inline void blitTint(PixelCanvas& c, int sx, int sy, int sw, int sh, int dx, int dy, uint32_t col) {
    const uint8_t* a = atlas();
    for (int y = 0; y < sh; ++y)
        for (int x = 0; x < sw; ++x)
            if (a[(sy + y) * kAtlasW + sx + x]) c.setPixel(dx + x, dy + y, col);
}

// Stretches one source column / row across a span.
inline void stretchCols(PixelCanvas& c, const Skin& s, int sx, int sy, int sh, int dx, int dy, int dw) {
    for (int x = 0; x < dw; ++x) blit(c, s, sx, sy, 1, sh, dx + x, dy);
}
inline void stretchRows(PixelCanvas& c, const Skin& s, int sx, int sy, int sw, int dx, int dy, int dh) {
    for (int y = 0; y < dh; ++y) blit(c, s, sx, sy, sw, 1, dx, dy + y);
}

inline const SpriteRect& rect(int id) { return kSprites[id]; }

inline void sprite(PixelCanvas& c, const Skin& s, int id, int x, int y) {
    const SpriteRect& r = rect(id);
    blit(c, s, r.x, r.y, r.w, r.h, x, y);
}
inline void spriteTint(PixelCanvas& c, int id, int x, int y, uint32_t col) {
    const SpriteRect& r = rect(id);
    blitTint(c, r.x, r.y, r.w, r.h, x, y, col);
}

// Horizontal 3-slice: fixed caps, middle column stretched. Drawn at the sprite's own height.
inline void hbar(PixelCanvas& c, const Skin& s, int id, int x, int y, int w, int cap) {
    const SpriteRect& r = rect(id);
    cap = std::min(cap, w / 2);
    blit(c, s, r.x, r.y, cap, r.h, x, y);
    stretchCols(c, s, r.x + r.w / 2, r.y, r.h, x + cap, y, w - 2 * cap);
    blit(c, s, r.x + r.w - cap, r.y, cap, r.h, x + w - cap, y);
}

// Vertical 3-slice, drawn at the sprite's own width.
inline void vbar(PixelCanvas& c, const Skin& s, int id, int x, int y, int h, int cap) {
    const SpriteRect& r = rect(id);
    cap = std::min(cap, h / 2);
    blit(c, s, r.x, r.y, r.w, cap, x, y);
    stretchRows(c, s, r.x, r.y + r.h / 2, r.w, x, y + cap, h - 2 * cap);
    blit(c, s, r.x, r.y + r.h - cap, r.w, cap, x, y + h - cap);
}

// 9-slice. Each edge repeats a `period`-long run that starts right after the corner,
// so knotwork continues out of the corner without a seam (0 = the whole middle).
inline void tileH(PixelCanvas& c, const Skin& s, int sx, int sy, int sw, int sh, int dx, int dy, int dw) {
    if (sw <= 0) return;
    for (int o = 0; o < dw; o += sw) blit(c, s, sx, sy, std::min(sw, dw - o), sh, dx + o, dy);
}
inline void tileV(PixelCanvas& c, const Skin& s, int sx, int sy, int sw, int sh, int dx, int dy, int dh) {
    if (sh <= 0) return;
    for (int o = 0; o < dh; o += sh) blit(c, s, sx, sy, sw, std::min(sh, dh - o), dx, dy + o);
}

inline void panel9(PixelCanvas& c, const Skin& s, int id, int x, int y, int w, int h, int corner, int period = 0) {
    const SpriteRect& r = rect(id);
    const int k = std::min(corner, std::min(w, h) / 2);
    const int midW = r.w - 2 * k, midH = r.h - 2 * k;
    const int pw = period > 0 ? std::min(period, midW) : midW;
    const int ph = period > 0 ? std::min(period, midH) : midH;
    // centre: flat fill from the sprite's middle pixel
    const uint8_t mid = atlas()[(r.y + r.h / 2) * kAtlasW + r.x + r.w / 2];
    if (mid) c.fillRect(x + k, y + k, w - 2 * k, h - 2 * k, s.lut[mid]);
    // edges
    tileH(c, s, r.x + k, r.y, pw, k, x + k, y, w - 2 * k);
    tileH(c, s, r.x + k, r.y + r.h - k, pw, k, x + k, y + h - k, w - 2 * k);
    tileV(c, s, r.x, r.y + k, k, ph, x, y + k, h - 2 * k);
    tileV(c, s, r.x + r.w - k, r.y + k, k, ph, x + w - k, y + k, h - 2 * k);
    // corners
    blit(c, s, r.x, r.y, k, k, x, y);
    blit(c, s, r.x + r.w - k, r.y, k, k, x + w - k, y);
    blit(c, s, r.x, r.y + r.h - k, k, k, x, y + h - k);
    blit(c, s, r.x + r.w - k, r.y + r.h - k, k, k, x + w - k, y + h - k);
}

// ---------------------------------------------------------------------------
// Text
// ---------------------------------------------------------------------------

// Tiny is the original 5x7 face (uppercase ASCII), kept for key labels and fine print.
enum Font { kFontTiny = 0, kFontBody, kFontTitle, kFontLight, kFontChunky };

inline const FontDef* fontDef(int f) {
    switch (f) {
        case kFontBody:   return &kFontDefBody;
        case kFontTitle:  return &kFontDefTitle;
        case kFontLight:  return &kFontDefLight;
        case kFontChunky: return &kFontDefChunky;
        default:          return nullptr;
    }
}

inline int capHeight(int f) {
    const FontDef* d = fontDef(f);
    return d ? d->capHeight : 7;
}

// Next code point from UTF-8 (bad bytes come back as '?').
inline uint32_t nextCodepoint(const char*& p) {
    const uint8_t c = static_cast<uint8_t>(*p++);
    if (c < 0x80) return c;
    int extra = (c >= 0xF0) ? 3 : (c >= 0xE0) ? 2 : (c >= 0xC0) ? 1 : -1;
    if (extra < 0) return '?';
    uint32_t cp = c & (0x3F >> extra);
    for (int i = 0; i < extra; ++i) {
        const uint8_t n = static_cast<uint8_t>(*p);
        if ((n & 0xC0) != 0x80) return '?';
        cp = (cp << 6) | (n & 0x3F);
        ++p;
    }
    return cp;
}

inline const GlyphDef* findGlyph(const FontDef* d, uint32_t cp) {
    int lo = 0, hi = d->count - 1;
    while (lo <= hi) {
        const int m = (lo + hi) / 2;
        if (d->glyphs[m].cp == cp) return &d->glyphs[m];
        if (d->glyphs[m].cp < cp) lo = m + 1; else hi = m - 1;
    }
    return nullptr;
}

// Glyph for cp in font f, falling back to the body face (it has the accented letters).
inline const GlyphDef* glyphFor(int f, uint32_t cp, const FontDef*& owner) {
    owner = fontDef(f);
    const GlyphDef* g = findGlyph(owner, cp);
    if (!g && owner != &kFontDefBody) { owner = &kFontDefBody; g = findGlyph(owner, cp); }
    if (!g) { owner = fontDef(f); g = findGlyph(owner, '?'); }
    return g;
}

inline int textWidth(int f, const char* s, int scale = 1) {
    if (!s || !*s) return 0;
    if (f == kFontTiny) return measurePixelText(s, scale);
    int w = 0, last = 0;
    for (const char* p = s; *p;) {
        const FontDef* owner = nullptr;
        const GlyphDef* g = glyphFor(f, nextCodepoint(p), owner);
        if (!g) continue;
        w += g->adv;
        last = g->w > 0 ? g->adv - (g->xoff + g->w) : 0; // trailing side bearing of the last glyph
    }
    return (w - std::max(0, last)) * scale;
}

// y is the top of the capital letters.
inline void text(PixelCanvas& c, int f, int x, int y, const char* s, uint32_t col, int scale = 1) {
    if (!s) return;
    if (f == kFontTiny) { drawPixelText(c, x, y, s, col, scale); return; }
    const int baseline = y + capHeight(f) * scale;
    int pen = x;
    for (const char* p = s; *p;) {
        const FontDef* owner = nullptr;
        const GlyphDef* g = glyphFor(f, nextCodepoint(p), owner);
        if (!g) continue;
        for (int row = 0; row < g->h; ++row) {
            const uint16_t bits = owner->rows[g->row + row];
            if (!bits) continue;
            const int py = baseline + (g->yoff + row) * scale;
            for (int bx = 0; bx < g->w; ++bx)
                if (bits & (1u << bx)) c.fillRect(pen + (g->xoff + bx) * scale, py, scale, scale, col);
        }
        pen += g->adv * scale;
    }
}

inline void textCentered(PixelCanvas& c, int f, int cx, int y, const char* s, uint32_t col, int scale = 1) {
    text(c, f, cx - textWidth(f, s, scale) / 2, y, s, col, scale);
}
inline void textRight(PixelCanvas& c, int f, int rx, int y, const char* s, uint32_t col, int scale = 1) {
    text(c, f, rx - textWidth(f, s, scale), y, s, col, scale);
}

// Drop shadow one pixel down-right (per scale step).
inline void textShadow(PixelCanvas& c, int f, int x, int y, const char* s, uint32_t col, uint32_t shadow, int scale = 1) {
    text(c, f, x + scale, y + scale, s, shadow, scale);
    text(c, f, x, y, s, col, scale);
}

// Shortens with ".." when wider than maxW. Returns the drawn width.
inline int textFit(PixelCanvas& c, int f, int x, int y, int maxW, const char* s, uint32_t col, int scale = 1) {
    char buf[128];
    std::snprintf(buf, sizeof(buf), "%s", s ? s : "");
    if (textWidth(f, buf, scale) > maxW) {
        size_t n = std::strlen(buf);
        while (n > 1) {
            // drop a whole UTF-8 character
            do { --n; } while (n > 0 && (static_cast<uint8_t>(buf[n]) & 0xC0) == 0x80);
            char tmp[132];
            std::snprintf(tmp, sizeof(tmp), "%.*s..", static_cast<int>(n), buf);
            while (n > 0 && buf[n - 1] == ' ') { --n; std::snprintf(tmp, sizeof(tmp), "%.*s..", static_cast<int>(n), buf); }
            if (textWidth(f, tmp, scale) <= maxW) { std::snprintf(buf, sizeof(buf), "%s", tmp); break; }
        }
    }
    text(c, f, x, y, buf, col, scale);
    return textWidth(f, buf, scale);
}

} // namespace ui
} // namespace PixelLabs
