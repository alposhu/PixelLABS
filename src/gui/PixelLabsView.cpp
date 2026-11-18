#include "PixelLabsView.h"
#include "UiKit.h"
#include "../presets/FactoryPresets.h"
#include "../plugin/ParamNames.h"
#include "../plugin/Version.h"
#include "../dsp/RetroSynthEngine.h"
#include <windowsx.h>
#include <cstdio>
#include <cmath>
#include <cstring>
#include <algorithm>

namespace PixelLabs {

static const wchar_t* kWndClassName = L"PixelLabsVST3View2";
static constexpr UINT_PTR kTimerId = 1001;

// =============================================================================
// Skins
// =============================================================================

// Every skin draws the same Dark Ages art. `roles` recolours it (gold, tan, camel,
// charcoal, blue, navy, red, green, sky, black); all zero keeps the art's own colours.
struct Palette {
    const char* name;
    uint32_t bg, panel, inset, border, shadow;
    uint32_t ink, muted, accent, accent2, accent3;
    uint32_t keyWhite, keyWhiteShade, keyWhiteInk, keyBlack, keyBlackInk, keyHit, keyHitInk;
    uint8_t scanAlpha;
    uint32_t roles[ui::kNumRoles];
};

static const char* kThemeNames[kNumThemes] = { "AUTO", "NES", "DMG", "NEON", "CODEX", "ARCADE", "PAPER", "GILDED" };

static const Palette kPalettes[kNumThemes] = {
    // 0 AUTO placeholder (resolved per era)
    { "AUTO", 0,0,0,0,0, 0,0,0,0,0, 0,0,0,0,0,0,0, 0, {} },
    // 1 NES - console grey, red and sky, orange-gold trim
    { "NES",
      0xFF111116, 0xFF1D1D25, 0xFF0A0A0E, 0xFF3D3D4C, 0xFF050507,
      0xFFF4F4F4, 0xFF8E8EA0, 0xFFE8382F, 0xFF3CBCFC, 0xFFFCB040,
      0xFFEDEDED, 0xFFB8B8C2, 0xFF2A2A33, 0xFF1A1A20, 0xFF9A9AAA, 0xFFE8382F, 0xFFFFFFFF, 40,
      { 0xFFFCB040, 0xFF8E8EA0, 0xFFC84C0C, 0xFF1D1D25, 0xFFE8382F, 0xFF0A0A0E, 0xFFE8382F, 0xFF58D854, 0xFF3CBCFC, 0xFF050507 } },
    // 2 DMG - four shades of handheld green
    { "DMG",
      0xFF0F380F, 0xFF1C4A1C, 0xFF0A2A0A, 0xFF4E7A1E, 0xFF061F06,
      0xFF9BBC0F, 0xFF6E9420, 0xFFC6E25A, 0xFF8BAC0F, 0xFFE0F08A,
      0xFF9BBC0F, 0xFF6E9420, 0xFF0F380F, 0xFF0F380F, 0xFF8BAC0F, 0xFF306230, 0xFFC6E25A, 50,
      { 0xFFC6E25A, 0xFF6E9420, 0xFF8BAC0F, 0xFF1C4A1C, 0xFF306230, 0xFF0F380F, 0xFFC6E25A, 0xFF9BBC0F, 0xFFE0F08A, 0xFF061F06 } },
    // 3 NEON - synthwave night, cyan trim
    { "NEON",
      0xFF0D0221, 0xFF1A0B3B, 0xFF07011A, 0xFF46207F, 0xFF04000E,
      0xFFF7EEFF, 0xFFA28BCB, 0xFFFF2E97, 0xFF00F0FF, 0xFFFFD319,
      0xFFEDE6FF, 0xFFB7A6E0, 0xFF2A1250, 0xFF170732, 0xFFB08CFF, 0xFFFF2E97, 0xFFFFFFFF, 50,
      { 0xFF00F0FF, 0xFF7A5CB0, 0xFF0090B0, 0xFF1A0B3B, 0xFFFF2E97, 0xFF07011A, 0xFFFF2E97, 0xFF00F0FF, 0xFFFFD319, 0xFF04000E } },
    // 4 CODEX - illuminated manuscript, rubric red
    { "CODEX",
      0xFF1A120B, 0xFF2C1F13, 0xFF120C07, 0xFF6B4E2A, 0xFF0A0603,
      0xFFF3E6C8, 0xFFB0946A, 0xFFE0AE3C, 0xFFC8362B, 0xFF4F8CC4,
      0xFFEFE1C0, 0xFFC7B48C, 0xFF3A2A16, 0xFF24170C, 0xFFC7A46A, 0xFFE0AE3C, 0xFF1A120B, 36,
      { 0xFFE0AE3C, 0xFFB0946A, 0xFF9A6B2A, 0xFF2C1F13, 0xFFC8362B, 0xFF120C07, 0xFFC8362B, 0xFF6E9A3C, 0xFF4F8CC4, 0xFF0A0603 } },
    // 5 ARCADE - black cabinet, maze blue, marquee yellow
    { "ARCADE",
      0xFF000000, 0xFF0B0B18, 0xFF000000, 0xFF2626E0, 0xFF000000,
      0xFFFFFFFF, 0xFF8A8AB0, 0xFFFFE600, 0xFF00E5FF, 0xFFFF4FB0,
      0xFFF2F2F2, 0xFFB0B0C8, 0xFF101018, 0xFF12121E, 0xFF8A8AB0, 0xFFFFE600, 0xFF000000, 60,
      { 0xFFFFE600, 0xFF5A5AE8, 0xFFC0A800, 0xFF0B0B18, 0xFF2626E0, 0xFF000000, 0xFFFF4FB0, 0xFF00E5FF, 0xFF00E5FF, 0xFF000000 } },
    // 6 PAPER - light studio skin, ink-drawn trim
    { "PAPER",
      0xFFEAE3D2, 0xFFF8F4EA, 0xFFE1D7C2, 0xFF6A6252, 0xFFBDB29B,
      0xFF1C1C1C, 0xFF6A6252, 0xFFE0502A, 0xFF2479A3, 0xFFB0702A,
      0xFFFFFFFF, 0xFFD8D0BE, 0xFF1C1C1C, 0xFF222222, 0xFFBDB6A6, 0xFFE0502A, 0xFFFFFFFF, 0,
      { 0xFF4A3F2E, 0xFF8A7F6A, 0xFF7A5A2A, 0xFFF8F4EA, 0xFFE0502A, 0xFFE1D7C2, 0xFFE0502A, 0xFF5E8A2C, 0xFF2479A3, 0xFFBDB29B } },
    // 7 GILDED - the Dark Ages art in its own gold, charcoal and blue
    { "GILDED",
      0xFF1A1D18, 0xFF2E322A, 0xFF161D1D, 0xFF706255, 0xFF080A07,
      0xFFF3E7C4, 0xFF9F8873, 0xFFDCC47C, 0xFF6FB2DE, 0xFFFBD693,
      0xFFE9DDBA, 0xFFC4B188, 0xFF2E322A, 0xFF161D1D, 0xFF9F8873, 0xFFDCC47C, 0xFF15202F, 24,
      {} }
};

static const uint32_t kEraColors[kNumEras] = {
    0xFFE8382F, // 80s chip
    0xFF8BC34A, // 90s chip
    0xFFFF2E97, // synth 80s
    0xFFE0AE3C, // medieval
    0xFF00E5FF  // sfx + kit
};

static int autoThemeForEra(int era) {
    switch (era) {
        case kEraChip90:   return kThemeDmg;
        case kEraSynth:    return kThemeNeon;
        case kEraMedieval: return kThemeGilded;
        case kEraSfx:      return kThemeArcade;
        default:           return kThemeNes;
    }
}

// A role colour as this skin draws it (the art's own colour for GILDED).
static uint32_t roleColor(const Palette& p, int role) {
    return p.roles[0] ? p.roles[role] : ui::kRoleBase[role];
}

// =============================================================================
// Static tables
// =============================================================================

static const char* kMacroNames[kNumMacros] = {
    "REVERB", "DELAY", "CHORUS", "PHASER", "TREMOLO", "TAPE WOW",
    "DRIVE", "PIXEL CRUSH", "GB SPEAKER", "STEREO WIDTH", "CONTOUR", "VIBRATO"
};
static const char* kMacroShort[kNumMacros] = {
    "VERB", "DELAY", "CHORUS", "PHASER", "TREM", "TAPE",
    "DRIVE", "CRUSH", "SPEAKR", "WIDTH", "SHAPE", "VIBRA"
};

struct StepperDef { int param; int count; const char* label; };
static const StepperDef kSteppers[4] = {
    { kParamWaveform,   kNumWaveforms,    "OSC"   },
    { kParamHarmony,    kNumHarmonyModes, "HARM"  },
    { kParamReverbType, kNumReverbTypes,  "ROOM"  },
    { kParamDelayType,  kNumDelayTypes,   "ECHO"  }
};

struct SliderDef { int param; const char* label; };
static const SliderDef kSliders[16] = {
    { kParamDynamics,  "TIMBRE"  }, { kParamCutoff,       "CUTOFF"  },
    { kParamResonance, "RESO"    }, { kParamFilterEnv,    "F.ENV"   },
    { kParamSubMix,    "SUB OSC" }, { kParamNoise,        "NOISE"   },
    { kParamDetune,    "DETUNE"  }, { kParamGlide,        "GLIDE"   },
    { kParamAttack,    "ATTACK"  }, { kParamDecay,        "DECAY"   },
    { kParamSustainLevel, "SUSTAIN" }, { kParamRelease,   "RELEASE" },
    { kParamSweep,     "SWEEP"   }, { kParamSweepTime,    "SWP TIME"},
    { kParamArpRate,   "ARP RATE"}, { kParamExpression,   "VOLUME"  }
};

static const char* kDrumNames[kNumDrumKits][12] = {
    { "KICK", "RIM", "SNARE", "CLAP", "TIGHT SN", "LO TOM", "C.HAT", "MID TOM", "O.HAT", "HI TOM", "CRASH", "COWBELL" },
    { "808 KICK", "RIM", "SNARE", "CLAP", "TIGHT SN", "LO TOM", "C.HAT", "MID TOM", "O.HAT", "HI TOM", "CYMBAL", "COWBELL" },
    { "TRI KICK", "CLICK", "SNARE", "METAL", "HISS SN", "LO TOM", "C.HAT", "MID TOM", "O.HAT", "HI TOM", "CRASH", "BLIP" },
    { "KICK", "RIM", "SNARE", "CLAP", "TIGHT SN", "LO TOM", "C.HAT", "MID TOM", "O.HAT", "HI TOM", "CRASH", "COWBELL" },
    { "KICK", "CLICK", "SNARE", "CLAP", "GHOST SN", "LO TOM", "C.HAT", "MID TOM", "O.HAT", "HI TOM", "RIDE", "SHAKER" },
    { "FRAME DRUM", "WOOD BLOCK", "TABOR", "CLAP", "TEK", "DOUM", "TAMB SHAKE", "LOW TABOR", "TAMBOURINE", "NAKERS", "ZILLS", "HAND BELL" },
    { "GREAT DRUM", "RIM", "FIELD SN", "CLAP", "WAR DRUM", "LO TIMP", "CHAINS", "MID TIMP", "GONG", "HI TIMP", "CRASH", "ANVIL" },
    { "LO CONGA", "CLAVES", "SLAP", "SNAP", "OPEN CONGA", "DJEMBE", "SHAKER", "LO BONGO", "LONG SHAKE", "HI BONGO", "CABASA", "AGOGO" }
};
static const char* kDrumKeyLabels[kNumDrumKits][12] = {
    { "KIK", "RM", "SNR", "CP", "TSN", "LTM", "CH", "MTM", "OH", "HTM", "CR", "BEL" },
    { "KIK", "RM", "SNR", "CP", "TSN", "LTM", "CH", "MTM", "OH", "HTM", "CY", "BEL" },
    { "KIK", "CLK", "SNR", "MTL", "HSN", "LTM", "CH", "MTM", "OH", "HTM", "CR", "BLP" },
    { "KIK", "RM", "SNR", "CP", "TSN", "LTM", "CH", "MTM", "OH", "HTM", "CR", "BEL" },
    { "KIK", "CLK", "SNR", "CP", "GSN", "LTM", "CH", "MTM", "OH", "HTM", "RD", "SHK" },
    { "FRM", "WD", "TAB", "CP", "TEK", "DUM", "TMS", "LTB", "TMB", "NAK", "ZIL", "BEL" },
    { "GRT", "RM", "SNR", "CP", "WAR", "LTP", "CHN", "MTP", "GNG", "HTP", "CR", "ANV" },
    { "LCG", "CLV", "SLP", "SNP", "OCG", "DJB", "SHK", "LBG", "LSH", "HBG", "CAB", "AGO" }
};

static int countBits(int m) { int n = 0; while (m) { n += m & 1; m >>= 1; } return n; }

static bool isBlackKey(int note) {
    const int pc = ((note % 12) + 12) % 12;
    return pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10;
}

// Index of the white key at or just left of `note`, counted from kFirstKey (a C).
static int whiteIndex(int note) {
    static const int kWhiteOf[12] = { 0, 0, 1, 1, 2, 3, 3, 4, 4, 5, 5, 6 };
    const int rel = note - PixelLabsView::kFirstKey;
    return (rel / 12) * 7 + kWhiteOf[rel % 12];
}

// =============================================================================
// Drawing helpers
// =============================================================================

// Recolour table of the skin being drawn. Rebuilt at the start of every frame
// (editors all draw on the host's UI thread).
static ui::Skin g_skin;

static constexpr int kBarH = 19;   // height of the scroll-end bars (buttons, tabs)
static constexpr int kFlatH = 15;  // height of the flat bars (steppers, fields)

static void blendRect(PixelCanvas& c, int x, int y, int w, int h, uint32_t color, uint8_t alpha) {
    const int x0 = std::max(0, x), y0 = std::max(0, y);
    const int x1 = std::min(c.width, x + w), y1 = std::min(c.height, y + h);
    for (int py = y0; py < y1; ++py)
        for (int px = x0; px < x1; ++px) c.blendPixel(px, py, color, alpha);
}

static void scanlines(PixelCanvas& c, int x, int y, int w, int h, uint8_t alpha) {
    if (alpha == 0) return;
    for (int py = y + 1; py < y + h; py += 2) blendRect(c, x, py, w, 1, 0xFF000000, alpha);
}

// Gold knotwork window: the three main panels and every dialog.
static void framePanel(PixelCanvas& c, int x, int y, int w, int h) {
    blendRect(c, x + 4, y + 5, w, h, 0xFF000000, 120);
    ui::panel9(c, g_skin, ui::kSprFrame, x, y, w, h, 24, 8);
}

// Dark knotwork inset (scope, notes strip).
static void knotPanel(PixelCanvas& c, int x, int y, int w, int h) {
    ui::panel9(c, g_skin, ui::kSprKnotDark, x, y, w, h, 14, 4);
}

static void parchment(PixelCanvas& c, int x, int y, int w, int h) {
    ui::panel9(c, g_skin, ui::kSprParchment, x, y, w, h, 10);
}

// Plain recessed well for lists.
static void well(PixelCanvas& c, const Palette& p, int x, int y, int w, int h) {
    c.fillRect(x, y, w, h, p.inset);
    c.fillRect(x, y, w, 1, p.shadow);
    c.fillRect(x, y, 1, h, p.shadow);
    c.fillRect(x, y + h - 1, w, 1, p.border);
    c.fillRect(x + w - 1, y, 1, h, p.border);
}

static void triangle(PixelCanvas& c, int cx, int cy, int size, int dir, uint32_t col) {
    // dir: -1 points left, +1 points right
    for (int i = 0; i < size; ++i) {
        const int h = (dir < 0) ? i * 2 + 1 : (size - i) * 2 - 1; // tip on the pointing side
        c.fillRect(cx - size / 2 + i, cy - h / 2, 1, h, col);
    }
}

// Arrow with a one-pixel drop shadow so it reads on gold and on charcoal.
static void arrow(PixelCanvas& c, const Palette& p, int cx, int cy, int size, int dir, uint32_t col) {
    triangle(c, cx + 1, cy + 1, size, dir, p.shadow);
    triangle(c, cx, cy, size, dir, col);
}

// Small faceted gem: era markers, LEDs.
static void gem(PixelCanvas& c, int cx, int cy, int r, uint32_t col, uint32_t outline) {
    for (int dy = -(r + 1); dy <= r + 1; ++dy) {
        const int half = r + 1 - std::abs(dy);
        c.fillRect(cx - half, cy + dy, 2 * half + 1, 1, outline);
    }
    for (int dy = -r; dy <= r; ++dy) {
        const int half = r - std::abs(dy);
        c.fillRect(cx - half, cy + dy, 2 * half + 1, 1, col);
    }
    if (r >= 2) c.blendPixel(cx - 1, cy - r + 1, 0xFFFFFFFF, 150);
}

// ---- Text that stays readable on any skin's recoloured surfaces ----
static int luma(uint32_t c) {
    return static_cast<int>((((c >> 16) & 0xFF) * 299 + ((c >> 8) & 0xFF) * 587 + (c & 0xFF) * 114) / 1000);
}
static uint32_t mix(uint32_t a, uint32_t b, int t) { // t = 0..255 towards b
    auto ch = [&](int sh) { return ((((a >> sh) & 0xFFu) * (255u - t) + ((b >> sh) & 0xFFu) * t) / 255u) << sh; };
    return 0xFF000000u | ch(16) | ch(8) | ch(0);
}
// The colour a sprite shows in its middle (a button's face) in the current skin.
static uint32_t faceColor(int id) {
    const ui::SpriteRect& r = ui::rect(id);
    return g_skin.lut[ui::atlas()[(r.y + r.h / 2) * ui::kAtlasW + r.x + r.w / 2]];
}
// The skin's ink when it reads on the face, otherwise its most contrasting colour.
static uint32_t inkOn(const Palette& p, uint32_t face) {
    int bestD = std::abs(luma(p.ink) - luma(face));
    if (bestD >= 100) return p.ink;
    uint32_t best = p.ink;
    for (uint32_t cand : { p.keyWhite, p.shadow, p.bg, p.keyWhiteInk }) {
        const int d = std::abs(luma(cand) - luma(face));
        if (d > bestD) { bestD = d; best = cand; }
    }
    return best;
}
static uint32_t accentOn(const Palette& p, uint32_t face, uint32_t want) {
    return std::abs(luma(want) - luma(face)) >= 70 ? want : inkOn(p, face);
}
static uint32_t mutedOn(const Palette& p, uint32_t face) { return mix(inkOn(p, face), face, 100); }

// Scroll-end bar, drawn at its own height and centred in the h-tall box. Returns its face colour.
static int barY(int y, int h) { return y + (h - kBarH) / 2; }
static uint32_t bar(PixelCanvas& c, int x, int y, int w, int h, bool active, bool hover, bool crest = false) {
    const int by = barY(y, h);
    const int id = active ? ui::kSprBarGilt : (hover ? ui::kSprBarMid : ui::kSprBarDark);
    ui::hbar(c, g_skin, id, x, by, w, 9);
    if (active && crest) {
        const ui::SpriteRect& r = ui::rect(ui::kSprBarGiltCrest);
        ui::sprite(c, g_skin, ui::kSprBarGiltCrest, x + (w - r.w) / 2, by - r.h);
    }
    return faceColor(id);
}

static void button(PixelCanvas& c, const Palette& p, int x, int y, int w, int h,
                   const char* label, bool active, bool hover) {
    const uint32_t face = bar(c, x, y, w, h, active, hover);
    const uint32_t col = (hover && !active) ? accentOn(p, face, p.accent) : inkOn(p, face);
    ui::textCentered(c, ui::kFontTitle, x + w / 2, barY(y, h) + (kBarH - ui::capHeight(ui::kFontTitle)) / 2, label, col);
}

// Flat bar for steppers and small readouts. Returns its face colour.
static int flatY(int y, int h) { return y + (h - kFlatH) / 2; }
static uint32_t flat(PixelCanvas& c, int x, int y, int w, int h, bool lit = false) {
    const int id = lit ? ui::kSprFlatMid : ui::kSprFlatDark;
    ui::hbar(c, g_skin, id, x, flatY(y, h), w, 4);
    return faceColor(id);
}

// Square slot: macro buttons, keycaps. state 0 idle, 1 hover, 2 selected. Returns its face colour.
static uint32_t slot(PixelCanvas& c, int x, int y, int w, int h, int state) {
    const int id = state == 2 ? ui::kSprSlotGilt : state == 1 ? ui::kSprSlotMid : ui::kSprSlotDark;
    ui::panel9(c, g_skin, id, x, y, w, h, 7);
    return faceColor(id);
}

// Thin gem bar filled to v (0..1) on a dark track.
static void gauge(PixelCanvas& c, const Palette& p, int x, int y, int w, float v, int gemId = ui::kSprGemBlue) {
    c.fillRect(x, y, w, 4, p.inset);
    const int fw = static_cast<int>(std::lround(std::clamp(v, 0.0f, 1.0f) * w));
    if (fw >= 2) ui::hbar(c, g_skin, gemId, x, y, fw, std::min(3, fw / 2));
}

static void divider(PixelCanvas& c, int cx, int y) {
    const ui::SpriteRect& r = ui::rect(ui::kSprDividerLight);
    ui::sprite(c, g_skin, ui::kSprDividerLight, cx - r.w / 2, y);
}

// 7x7 pixel icons, one byte per row, bit 6 = leftmost pixel
static const uint8_t kIconStar[7]   = { 0x08, 0x1C, 0x7F, 0x3E, 0x1C, 0x36, 0x22 };
static const uint8_t kIconDisk[7]   = { 0x7E, 0x5D, 0x5D, 0x7F, 0x41, 0x41, 0x7F };
static const uint8_t kIconFolder[7] = { 0x70, 0x7F, 0x41, 0x41, 0x41, 0x41, 0x7F };
static const uint8_t kIconPencil[7] = { 0x03, 0x07, 0x0E, 0x1C, 0x38, 0x50, 0x60 };
static const uint8_t kIconCross[7]  = { 0x41, 0x22, 0x14, 0x08, 0x14, 0x22, 0x41 };
static const uint8_t kIconMinus[7]  = { 0x00, 0x00, 0x00, 0x7F, 0x00, 0x00, 0x00 };
static const uint8_t kIconZoom[7]   = { 0x38, 0x44, 0x44, 0x44, 0x3C, 0x06, 0x03 };

static void drawIcon(PixelCanvas& c, int x, int y, const uint8_t rows[7], uint32_t col, int scale = 1) {
    for (int r = 0; r < 7; ++r)
        for (int b = 0; b < 7; ++b)
            if (rows[r] & (1 << (6 - b))) c.fillRect(x + b * scale, y + r * scale, scale, scale, col);
}

// Icon with a shadow pixel under every lit one.
static void iconShadow(PixelCanvas& c, const Palette& p, int x, int y, const uint8_t rows[7], uint32_t col, int scale = 1) {
    drawIcon(c, x + scale, y + scale, rows, p.shadow, scale);
    drawIcon(c, x, y, rows, col, scale);
}

// SoundKeys travel through Hit::a: factory slot as is, user ids with bit 30 set
static int packKey(const SoundKey& k) {
    return k.kind == kSoundUser ? static_cast<int>(0x40000000u | (k.id & 0x3FFFFFFFu)) : static_cast<int>(k.id);
}
static SoundKey unpackKey(int a) {
    return (a & 0x40000000) ? SoundKey::user(static_cast<uint32_t>(a) & 0x3FFFFFFFu) : SoundKey::factory(a);
}

static std::string upper(const char* s) {
    std::string out = s ? s : "";
    for (char& ch : out) if (ch >= 'a' && ch <= 'z') ch = static_cast<char>(ch - 'a' + 'A');
    return out;
}

static const char* kTouchShort[kNumTouchCurves] = { "OFF", "LIGHT", "NORMAL", "HEAVY" };

// =============================================================================
// Construction / IPlugView plumbing
// =============================================================================

int PixelLabsView::zoomPercent(int scaleIdx) {
    static const int kZoom[kNumUiScales] = { 100, 125, 150, 200 };
    return kZoom[std::clamp(scaleIdx, 0, kNumUiScales - 1)];
}

int PixelLabsView::zoom() const {
    return zoomPercent(telemetry_ ? telemetry_->uiScale.load(std::memory_order_relaxed) : 0);
}

PixelLabsView::PixelLabsView(SharedUITelemetry* telemetry, IEditorBridge* bridge)
    : telemetry_(telemetry), bridge_(bridge) {
    hits_.reserve(256);
    lib_.revision = 0;
    createOffscreenBuffer();
}

PixelLabsView::~PixelLabsView() {
    removed();
    destroyOffscreenBuffer();
}

Steinberg::tresult PLUGIN_API PixelLabsView::queryInterface(const Steinberg::TUID _iid, void** obj) {
    if (Steinberg::iidEqual(_iid, Steinberg::FUnknown::iid) ||
        Steinberg::iidEqual(_iid, Steinberg::IPlugView::iid)) {
        addRef();
        *obj = static_cast<Steinberg::IPlugView*>(this);
        return Steinberg::kResultOk;
    }
    *obj = nullptr;
    return Steinberg::kNoInterface;
}

Steinberg::uint32 PLUGIN_API PixelLabsView::addRef() { return ++refCount_; }

Steinberg::uint32 PLUGIN_API PixelLabsView::release() {
    uint32_t r = --refCount_;
    if (r == 0) delete this;
    return r;
}

Steinberg::tresult PLUGIN_API PixelLabsView::isPlatformTypeSupported(Steinberg::FIDString type) {
    return (type && std::strcmp(type, "HWND") == 0) ? Steinberg::kResultTrue : Steinberg::kResultFalse;
}

void PixelLabsView::createOffscreenBuffer() {
    if (dibBitmap_) return;
    HDC screenDC = GetDC(nullptr);
    memDC_ = CreateCompatibleDC(screenDC);
    ReleaseDC(nullptr, screenDC);

    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = kBaseWidth;
    bmi.bmiHeader.biHeight = -kBaseHeight; // top-down
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    dibBitmap_ = CreateDIBSection(memDC_, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (dibBitmap_) {
        SelectObject(memDC_, dibBitmap_);
        pixelBuffer_ = static_cast<uint32_t*>(bits);
    }
}

void PixelLabsView::destroyOffscreenBuffer() {
    if (dibBitmap_) { DeleteObject(dibBitmap_); dibBitmap_ = nullptr; pixelBuffer_ = nullptr; }
    if (memDC_) { DeleteDC(memDC_); memDC_ = nullptr; }
}

Steinberg::tresult PLUGIN_API PixelLabsView::attached(void* parent, Steinberg::FIDString type) {
    if (isPlatformTypeSupported(type) != Steinberg::kResultTrue || !parent) {
        return Steinberg::kInvalidArgument;
    }
    // Register the class against this DLL (not the host exe) so a reloaded plugin never
    // inherits a stale window procedure.
    HINSTANCE hInst = nullptr;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       reinterpret_cast<LPCWSTR>(&PixelLabsView::WndProc), &hInst);
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    if (!GetClassInfoExW(hInst, kWndClassName, &wc)) {
        wc.style = CS_DBLCLKS | CS_OWNDC;
        wc.lpfnWndProc = &PixelLabsView::WndProc;
        wc.hInstance = hInst;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.lpszClassName = kWndClassName;
        RegisterClassExW(&wc);
    }

    const int z = zoom();
    hwnd_ = CreateWindowExW(0, kWndClassName, L"Pixel LABS",
                            WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_TABSTOP,
                            0, 0, kBaseWidth * z / 100, kBaseHeight * z / 100,
                            static_cast<HWND>(parent), nullptr, hInst, this);
    if (hwnd_) {
        SetTimer(hwnd_, kTimerId, 30, nullptr);
        renderUI();
        InvalidateRect(hwnd_, nullptr, FALSE);
    }
    return Steinberg::kResultOk;
}

Steinberg::tresult PLUGIN_API PixelLabsView::removed() {
    if (telemetry_) {
        telemetry_->demoActive.store(0, std::memory_order_release);
        if (mouseNote_ >= 0) playGuiNote(mouseNote_, false);
        for (int vk = 0; vk < 256; ++vk) {
            if (kbdHeld_[vk]) { playGuiNote(kbdHeld_[vk] - 1, false); kbdHeld_[vk] = 0; }
        }
    }
    mouseNote_ = -1;
    if (hwnd_) {
        KillTimer(hwnd_, kTimerId);
        SetWindowLongPtrW(hwnd_, GWLP_USERDATA, 0);
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
    }
    return Steinberg::kResultOk;
}

Steinberg::tresult PLUGIN_API PixelLabsView::onWheel(float distance) {
    handleWheel(hoverX_, hoverY_, distance);
    return Steinberg::kResultTrue;
}

static int vkFromChar(Steinberg::char16 key, Steinberg::int16 keyCode) {
    if (key >= 'a' && key <= 'z') return key - 'a' + 'A';
    if ((key >= 'A' && key <= 'Z') || (key >= '0' && key <= '9')) return key;
    if (key == ';') return VK_OEM_1;
    return keyCode;
}

// A host may hand us a keystroke through onKeyDown, as a window message, or both.
// While a dialog takes text, whichever path arrives first wins and the echo is dropped.
static constexpr uint64_t kKeyEchoMs = 25;

Steinberg::tresult PLUGIN_API PixelLabsView::onKeyDown(Steinberg::char16 key, Steinberg::int16 keyCode, Steinberg::int16) {
    if (modal_.kind != kModalNone) {
        const uint64_t now = GetTickCount64();
        if (now - lastNativeKeyMs_ <= kKeyEchoMs) return Steinberg::kResultTrue;
        lastHostKeyMs_ = now;
        // VST3 virtual key codes: 1 back, 4 return, 6 escape, 7 space, 19 enter, 22 delete
        int vk = 0;
        if (key == 8 || keyCode == 1) vk = VK_BACK;
        else if (key == 13 || keyCode == 4 || keyCode == 19) vk = VK_RETURN;
        else if (key == 27 || keyCode == 6) vk = VK_ESCAPE;
        else if (keyCode == 22) vk = VK_DELETE;
        if (vk) handleModalKey(vk);
        else if (key >= 32) handleChar(key, true);
        else if (keyCode == 7) handleChar(' ', true);
        return Steinberg::kResultTrue;
    }
    return handleKey(vkFromChar(key, keyCode), true) ? Steinberg::kResultTrue : Steinberg::kResultFalse;
}

Steinberg::tresult PLUGIN_API PixelLabsView::onKeyUp(Steinberg::char16 key, Steinberg::int16 keyCode, Steinberg::int16) {
    if (modal_.kind != kModalNone) return Steinberg::kResultTrue;
    return handleKey(vkFromChar(key, keyCode), false) ? Steinberg::kResultTrue : Steinberg::kResultFalse;
}

Steinberg::tresult PLUGIN_API PixelLabsView::getSize(Steinberg::ViewRect* size) {
    if (!size) return Steinberg::kInvalidArgument;
    const int z = zoom();
    size->left = 0;
    size->top = 0;
    size->right = kBaseWidth * z / 100;
    size->bottom = kBaseHeight * z / 100;
    return Steinberg::kResultOk;
}

Steinberg::tresult PLUGIN_API PixelLabsView::onSize(Steinberg::ViewRect* newSize) {
    if (hwnd_ && newSize) {
        SetWindowPos(hwnd_, nullptr, 0, 0, newSize->getWidth(), newSize->getHeight(),
                     SWP_NOZORDER | SWP_NOMOVE | SWP_NOACTIVATE);
        InvalidateRect(hwnd_, nullptr, FALSE);
    }
    return Steinberg::kResultOk;
}

Steinberg::tresult PLUGIN_API PixelLabsView::onFocus(Steinberg::TBool) { return Steinberg::kResultOk; }

Steinberg::tresult PLUGIN_API PixelLabsView::setFrame(Steinberg::IPlugFrame* frame) {
    plugFrame_ = frame;
    return Steinberg::kResultOk;
}

Steinberg::tresult PLUGIN_API PixelLabsView::canResize() { return Steinberg::kResultFalse; }

Steinberg::tresult PLUGIN_API PixelLabsView::checkSizeConstraint(Steinberg::ViewRect* rect) {
    if (!rect) return Steinberg::kInvalidArgument;
    const int z = zoom();
    rect->right = rect->left + kBaseWidth * z / 100;
    rect->bottom = rect->top + kBaseHeight * z / 100;
    return Steinberg::kResultOk;
}

void PixelLabsView::requestResize() {
    Steinberg::ViewRect r;
    getSize(&r);
    if (plugFrame_) {
        plugFrame_->resizeView(this, &r);
    }
    if (hwnd_) {
        SetWindowPos(hwnd_, nullptr, 0, 0, r.getWidth(), r.getHeight(), SWP_NOZORDER | SWP_NOMOVE | SWP_NOACTIVATE);
        InvalidateRect(hwnd_, nullptr, FALSE);
    }
}

// =============================================================================
// State helpers
// =============================================================================

float PixelLabsView::param(int id) const {
    return telemetry_ ? telemetry_->params[id].load(std::memory_order_relaxed) : 0.0f;
}

void PixelLabsView::setParam(int id, float v) {
    v = std::clamp(v, 0.0f, 1.0f);
    if (bridge_) bridge_->editorSetParam(id, v);
}

void PixelLabsView::stepList(int paramId, int count, int dir) {
    int idx = normToList(param(paramId), count);
    idx = (idx + dir + count) % count;
    setParam(paramId, listToNorm(idx, count));
}

int PixelLabsView::browserTab() const {
    return telemetry_ ? std::clamp(telemetry_->eraTab.load(std::memory_order_relaxed), 0, kNumBrowserTabs - 1) : 0;
}

int PixelLabsView::currentEra() const {
    return std::clamp(cur_.era, 0, kNumEras - 1);
}

void PixelLabsView::playGuiNote(int note, bool on, int velocity) {
    if (!telemetry_ || note < 0 || note > 127) return;
    telemetry_->pushGuiNote(note, on ? std::clamp(velocity, 1, 127) : 0);
}

void PixelLabsView::releaseHeldNotes() {
    if (mouseNote_ >= 0) playGuiNote(mouseNote_, false);
    mouseNote_ = -1;
    for (int vk = 0; vk < 256; ++vk) {
        if (kbdHeld_[vk]) { playGuiNote(kbdHeld_[vk] - 1, false); kbdHeld_[vk] = 0; }
    }
}

void PixelLabsView::showToast(const std::string& text) {
    toast_ = text;
    toastFrames_ = 75;
}

// =============================================================================
// Library: browsing, loading, saving
// =============================================================================

void PixelLabsView::refreshState() {
    SoundLibrary& lib = SoundLibrary::instance();
    const uint64_t rev = lib.revision();
    if (rev != lib_.revision) lib_ = lib.snapshot();
    if (bridge_) {
        cur_ = bridge_->editorCurrentSound();
        if (cur_.key.kind == kSoundUser && !lib_.findSound(cur_.key.id)) {
            cur_.key.kind = kSoundDetached; // deleted from another instance's editor
            bridge_->editorSetCurrentSound(cur_);
        }
    }
    modified_ = computeModified();
    if (openBankId_ && !lib_.findBank(openBankId_)) openBankId_ = 0;
}

bool PixelLabsView::computeModified() const {
    if (!telemetry_) return false;
    for (int i = 0; i < kNumParams; ++i) {
        if (isPerformanceParam(i)) continue;
        if (std::fabs(param(i) - cur_.ref[i]) > 0.002f) return true;
    }
    return false;
}

void PixelLabsView::currentValues(float* out) const {
    for (int i = 0; i < kNumParams; ++i) out[i] = param(i);
}

bool PixelLabsView::soundExists(const SoundKey& k) const {
    if (k.kind == kSoundFactory) return k.id < static_cast<uint32_t>(kNumFactoryPresets);
    if (k.kind == kSoundUser) return lib_.findSound(k.id) != nullptr;
    return false;
}

std::string PixelLabsView::soundName(const SoundKey& k) const {
    if (k == cur_.key) return cur_.name;
    if (k.kind == kSoundFactory) return factoryPreset(static_cast<int>(k.id)).name;
    if (const UserSound* s = lib_.findSound(k.id)) return s->name;
    return "?";
}

int PixelLabsView::soundEra(const SoundKey& k) const {
    if (k.kind == kSoundFactory) return factoryPreset(static_cast<int>(k.id)).era;
    if (const UserSound* s = lib_.findSound(k.id)) return std::clamp(s->era, 0, kNumEras - 1);
    return cur_.era;
}

std::vector<SoundKey> PixelLabsView::browseList() const {
    std::vector<SoundKey> out;
    const int tab = browserTab();
    if (tab == kTabFavorites) {
        for (const auto& k : lib_.favorites) if (soundExists(k)) out.push_back(k);
        return out;
    }
    if (tab == kTabBanks) {
        if (const SoundBank* b = lib_.findBank(openBankId_))
            for (const auto& k : b->items) if (soundExists(k)) out.push_back(k);
        return out;
    }
    if (tab != kTabUser) {
        for (int i = 0; i < kNumFactoryPresets; ++i)
            if (tab == kTabAll || factoryPreset(i).era == tab - 1) out.push_back(SoundKey::factory(i));
    }
    for (const auto& s : lib_.sounds)
        if (tab == kTabAll || tab == kTabUser || s.era == tab - 1) out.push_back(SoundKey::user(s.id));
    return out;
}

void PixelLabsView::stepPreset(int dir) {
    if (!telemetry_ || modal_.kind != kModalNone) return;
    std::vector<SoundKey> list = browseList();
    if (list.empty()) { // bank list or an empty view: step through everything
        for (int i = 0; i < kNumFactoryPresets; ++i) list.push_back(SoundKey::factory(i));
        for (const auto& s : lib_.sounds) list.push_back(SoundKey::user(s.id));
    }
    const int n = static_cast<int>(list.size());
    auto it = std::find(list.begin(), list.end(), cur_.key);
    SoundKey target = dir > 0 ? list.front() : list.back();
    if (it != list.end()) target = list[(static_cast<int>(it - list.begin()) + dir + n) % n];
    requestLoad(target);
}

void PixelLabsView::requestLoad(const SoundKey& key) {
    if (modal_.kind != kModalNone || !soundExists(key)) return;
    refreshState();
    if (modified_) {
        // Edited sound: ask before throwing the edits away
        modal_ = Modal();
        modal_.hasPending = true;
        modal_.pending = key;
        const bool isUser = cur_.key.kind == kSoundUser && lib_.findSound(cur_.key.id);
        std::vector<int> buttons = isUser ? std::vector<int>{ kBtnSave, kBtnSaveAs, kBtnDiscard, kBtnCancel }
                                          : std::vector<int>{ kBtnSaveAs, kBtnDiscard, kBtnCancel };
        openConfirm(kConfirmUnsaved, "UNSAVED CHANGES", "YOU CHANGED " + cur_.name + ".",
                    isUser ? "SAVE THE CHANGES, OR KEEP THEM AS A NEW SOUND?"
                           : "SAVE IT AS A NEW SOUND BEFORE SWITCHING?", buttons);
        return;
    }
    doLoad(key);
}

void PixelLabsView::doLoad(const SoundKey& key) {
    if (!bridge_) return;
    if (key.kind == kSoundFactory) {
        bridge_->editorLoadFactory(static_cast<int>(key.id));
    } else if (key.kind == kSoundUser) {
        if (!bridge_->editorLoadUser(key.id)) showToast("THAT SOUND IS NO LONGER IN THE LIBRARY");
    }
    refreshState();
}

void PixelLabsView::saveOverwrite() {
    SoundLibrary& lib = SoundLibrary::instance();
    if (cur_.key.kind == kSoundUser && lib.hasSound(cur_.key.id)) {
        float vals[kNumParams];
        currentValues(vals);
        lib.updateSound(cur_.key.id, vals);
        CurrentSoundInfo info = cur_;
        for (int i = 0; i < kNumParams; ++i) info.ref[i] = vals[i];
        if (bridge_) bridge_->editorSetCurrentSound(info);
        showToast(lib.lastSaveFailed() ? "COULD NOT WRITE THE LIBRARY FILE" : "SAVED " + cur_.name);
        refreshState();
        return;
    }
    openSaveAs(kAfterNothing);
}

void PixelLabsView::openSaveAs(int after) {
    const bool hadPending = modal_.hasPending;
    const SoundKey pending = modal_.pending;
    SoundLibrary& lib = SoundLibrary::instance();
    const std::string suggestion = lib.uniqueSoundName(cur_.key.kind == kSoundFactory ? cur_.name + " 2" : cur_.name);
    std::string sub = "MADE FROM " + upper(factoryPreset(cur_.basePreset).name);
    if (after == kAfterBankPicker) sub = "SAVE YOUR EDITS FIRST, THEN PICK A BANK";
    else if (after == kAfterLoadPending) sub = "SAVED SOUNDS LIVE IN MY SOUNDS";
    modal_ = Modal();
    modal_.hasPending = hadPending && after == kAfterLoadPending;
    modal_.pending = pending;
    openNameDialog(kNameSaveSound, "SAVE SOUND AS", sub, suggestion, 0);
    modal_.after = after;
}

void PixelLabsView::openNameDialog(int purpose, const std::string& title, const std::string& subtitle,
                                   const std::string& text, uint32_t targetId) {
    releaseHeldNotes();
    modal_.kind = kModalName;
    modal_.namePurpose = purpose;
    modal_.title = title;
    modal_.subtitle = subtitle;
    modal_.text = SoundLibrary::sanitizeName(text);
    modal_.targetId = targetId;
    if (hwnd_) SetFocus(hwnd_);
}

void PixelLabsView::openConfirm(int purpose, const std::string& title, const std::string& l1, const std::string& l2,
                                std::vector<int> buttons) {
    releaseHeldNotes();
    modal_.kind = kModalConfirm;
    modal_.confirmPurpose = purpose;
    modal_.title = title;
    modal_.line1 = l1;
    modal_.line2 = l2;
    modal_.buttons = std::move(buttons);
}

void PixelLabsView::openBankPicker(const SoundKey& key) {
    releaseHeldNotes();
    modal_ = Modal();
    modal_.kind = kModalBankPicker;
    modal_.pickerKey = key;
}

void PixelLabsView::openSongPicker() {
    releaseHeldNotes();
    modal_ = Modal();
    modal_.kind = kModalSongPicker;
    const bool playing = telemetry_->demoActive.load(std::memory_order_relaxed) != 0;
    modal_.songGroup = demoSongGroup(playing ? telemetry_->demoSong.load(std::memory_order_relaxed) : nextDemoSong());
}

int PixelLabsView::nextDemoSong() const {
    const int choice = telemetry_->demoChoice.load(std::memory_order_relaxed);
    return (choice >= 0 && choice < kNumDemoSongs) ? choice : demoSongForEra(cur_.era);
}

void PixelLabsView::playDemoSong(int index) {
    telemetry_->demoChoice.store(std::clamp(index, 0, kNumDemoSongs - 1), std::memory_order_relaxed);
    telemetry_->demoActive.store(1, std::memory_order_relaxed);
    telemetry_->demoRestart.fetch_add(1, std::memory_order_release);
}

void PixelLabsView::closeModal() {
    modal_ = Modal();
}

void PixelLabsView::nameInput(int ch) {
    if (modal_.kind != kModalName) return;
    std::string& t = modal_.text;
    if (ch == 8) { if (!t.empty()) t.pop_back(); return; }
    if (ch == 127) { t.clear(); return; }
    if (ch >= 'a' && ch <= 'z') ch = ch - 'a' + 'A';
    if (ch < 32 || ch > 126 || !SoundLibrary::isNameChar(static_cast<char>(ch))) return;
    if (t.size() >= SoundLibrary::kMaxNameLen) return;
    if (ch == ' ' && (t.empty() || t.back() == ' ')) return;
    t.push_back(static_cast<char>(ch));
}

void PixelLabsView::handleChar(int ch, bool) {
    if (modal_.kind != kModalName) return;
    // The pixel font has no Turkish letters, so type them as their plain twins
    switch (ch) {
        case 0x131: case 0x130: ch = 'I'; break; // i dotless / I dotted
        case 0x15F: case 0x15E: ch = 'S'; break;
        case 0x11F: case 0x11E: ch = 'G'; break;
        case 0xFC: case 0xDC:   ch = 'U'; break;
        case 0xF6: case 0xD6:   ch = 'O'; break;
        case 0xE7: case 0xC7:   ch = 'C'; break;
        default: break;
    }
    nameInput(ch);
}

void PixelLabsView::commitName() {
    SoundLibrary& lib = SoundLibrary::instance();
    const std::string name = SoundLibrary::sanitizeName(modal_.text);
    if (name.empty()) return; // the dialog stays open until there is a name
    switch (modal_.namePurpose) {
        case kNameSaveSound: {
            const std::string finalName = lib.uniqueSoundName(name);
            float vals[kNumParams];
            currentValues(vals);
            const uint32_t id = lib.addSound(finalName, cur_.category, cur_.era, cur_.basePreset, vals);
            CurrentSoundInfo info = cur_;
            info.key = SoundKey::user(id);
            info.name = finalName;
            for (int i = 0; i < kNumParams; ++i) info.ref[i] = vals[i];
            if (bridge_) bridge_->editorSetCurrentSound(info);
            showToast(lib.lastSaveFailed() ? "COULD NOT WRITE THE LIBRARY FILE" : "SAVED " + finalName + " TO MY SOUNDS");
            const int after = modal_.after;
            const bool hasPending = modal_.hasPending;
            const SoundKey pending = modal_.pending;
            closeModal();
            refreshState();
            if (after == kAfterLoadPending && hasPending) doLoad(pending);
            else if (after == kAfterBankPicker) openBankPicker(SoundKey::user(id));
            break;
        }
        case kNameNewBank: {
            const uint32_t id = lib.createBank(lib.uniqueBankName(name));
            if (modal_.backToPicker) {
                const SoundKey k = modal_.pickerKey;
                lib.addToBank(id, k);
                refreshState();
                openBankPicker(k);
                showToast("ADDED " + soundName(k) + " TO THE NEW BANK");
            } else {
                closeModal();
                if (telemetry_) telemetry_->eraTab.store(kTabBanks, std::memory_order_relaxed);
                openBankId_ = id;
                listScroll_ = 0;
                lastTabSeen_ = -1;
                refreshState();
                showToast("BANK CREATED");
            }
            break;
        }
        case kNameRenameSound: {
            const uint32_t id = modal_.targetId;
            const std::string finalName = lib.uniqueSoundName(name, id);
            lib.renameSound(id, finalName);
            if (cur_.key == SoundKey::user(id) && bridge_) {
                CurrentSoundInfo info = cur_;
                info.name = finalName;
                bridge_->editorSetCurrentSound(info);
            }
            closeModal();
            refreshState();
            break;
        }
        case kNameRenameBank:
            lib.renameBank(modal_.targetId, lib.uniqueBankName(name, modal_.targetId));
            closeModal();
            refreshState();
            break;
        default:
            closeModal();
            break;
    }
}

void PixelLabsView::pressDialogButton(int btn) {
    SoundLibrary& lib = SoundLibrary::instance();
    if (modal_.kind == kModalName) {
        if (btn == kBtnOk) { commitName(); return; }
        if (modal_.backToPicker) { openBankPicker(modal_.pickerKey); return; }
        closeModal();
        return;
    }
    if (modal_.kind == kModalBankPicker || modal_.kind == kModalSongPicker) { closeModal(); return; }
    if (modal_.kind != kModalConfirm) return;

    switch (modal_.confirmPurpose) {
        case kConfirmUnsaved: {
            const SoundKey pending = modal_.pending;
            if (btn == kBtnSave) { closeModal(); saveOverwrite(); doLoad(pending); }
            else if (btn == kBtnSaveAs) openSaveAs(kAfterLoadPending);
            else if (btn == kBtnDiscard) { closeModal(); doLoad(pending); }
            else closeModal();
            break;
        }
        case kConfirmDeleteSound: {
            const uint32_t id = modal_.targetId;
            closeModal();
            if (btn != kBtnDelete) break;
            const std::string name = soundName(SoundKey::user(id));
            lib.deleteSound(id);
            if (cur_.key == SoundKey::user(id) && bridge_) {
                CurrentSoundInfo info = cur_;   // keep playing it, but it is no longer in the library
                info.key.kind = kSoundDetached;
                bridge_->editorSetCurrentSound(info);
            }
            refreshState();
            showToast("DELETED " + name);
            break;
        }
        case kConfirmDeleteBank: {
            const uint32_t id = modal_.targetId;
            closeModal();
            if (btn != kBtnDelete) break;
            lib.deleteBank(id);
            if (openBankId_ == id) openBankId_ = 0;
            refreshState();
            showToast("BANK DELETED");
            break;
        }
        default:
            closeModal();
            break;
    }
}

void PixelLabsView::addHit(int x, int y, int w, int h, int kind, int a) {
    hits_.push_back({ x, y, w, h, kind, a });
}

const PixelLabsView::Hit* PixelLabsView::hitAt(int x, int y) const {
    // Last added wins (drawn on top).
    for (auto it = hits_.rbegin(); it != hits_.rend(); ++it) {
        if (x >= it->x && x < it->x + it->w && y >= it->y && y < it->y + it->h) return &*it;
    }
    return nullptr;
}

bool PixelLabsView::hovered(int x, int y, int w, int h) const {
    if (modal_.kind != kModalNone && !drawingModal_) return false; // the dialog owns the mouse
    return hoverX_ >= x && hoverX_ < x + w && hoverY_ >= y && hoverY_ < y + h && dragKind_ == kHitNone;
}

// =============================================================================
// Rendering
// =============================================================================

void PixelLabsView::renderUI() {
    if (!pixelBuffer_ || !telemetry_) return;
    ++frame_;
    refreshState();
    if (toastFrames_ > 0) --toastFrames_;
    prevHits_.swap(hits_);
    hits_.clear();

    PixelCanvas c;
    c.pixels = pixelBuffer_;
    c.width = kBaseWidth;
    c.height = kBaseHeight;

    int theme = std::clamp(telemetry_->theme.load(std::memory_order_relaxed), 0, kNumThemes - 1);
    if (theme == 0) theme = autoThemeForEra(currentEra());
    const Palette& p = kPalettes[theme];
    ui::buildSkin(g_skin, p.roles[0] ? p.roles : nullptr);

    // Woven backdrop: a faint 4-pixel stitch
    c.fillRect(0, 0, kBaseWidth, kBaseHeight, p.bg);
    for (int y = 1; y < kBaseHeight; y += 4)
        for (int x = ((y / 4) & 1) * 2 + 1; x < kBaseWidth; x += 4) c.blendPixel(x, y, p.panel, 160);

    drawHeader(c, p);
    drawEraTabs(c, p);
    drawPresetList(c, p);
    drawHero(c, p);
    drawSoundPanel(c, p);
    drawStatusStrip(c, p);
    drawKeyboard(c, p);
    if (modal_.kind != kModalNone) drawModal(c, p);
}

void PixelLabsView::drawHeader(PixelCanvas& c, const Palette& p) {
    // Logo
    ui::textShadow(c, ui::kFontTitle, 16, 9, "PIXEL", p.accent, p.shadow, 2);
    const int lx = 16 + ui::textWidth(ui::kFontTitle, "PIXEL ", 2);
    ui::textShadow(c, ui::kFontTitle, lx, 9, "LABS", p.ink, p.shadow, 2);
    ui::text(c, ui::kFontLight, 17, 36, "RETRO INSTRUMENT LAB  V" PIXELLABS_VERSION_SHORT, p.muted);

    // Sound plaque: gilded blue slot with its crest
    const int bx = 236, by = 8, bw = 392, bh = 40;
    blendRect(c, bx + 3, by + 4, bw, bh, 0xFF000000, 110);
    ui::panel9(c, g_skin, ui::kSprSlotGilt, bx, by, bw, bh, 7);
    {
        const ui::SpriteRect& cr = ui::rect(ui::kSprSlotGiltCrest);
        ui::sprite(c, g_skin, ui::kSprSlotGiltCrest, bx + (bw - cr.w) / 2, by - cr.h + 1);
    }
    const int era = currentEra();
    const uint32_t face = faceColor(ui::kSprSlotGilt);
    const uint32_t plaqueInk = inkOn(p, face);
    const uint32_t arrowCol = accentOn(p, face, roleColor(p, ui::kRoleGold));

    const bool hPrev = hovered(bx, by, 30, bh), hNext = hovered(bx + bw - 30, by, 30, bh);
    arrow(c, p, bx + 16, by + bh / 2, 7, -1, hPrev ? plaqueInk : arrowCol);
    arrow(c, p, bx + bw - 16, by + bh / 2, 7, 1, hNext ? plaqueInk : arrowCol);
    addHit(bx, by, 30, bh, kHitPresetPrev);
    addHit(bx + bw - 30, by, 30, bh, kHitPresetNext);

    // Favourite star for the loaded sound
    {
        const bool canFav = cur_.key.kind != kSoundDetached;
        const bool fav = canFav && lib_.isFavorite(cur_.key);
        const bool hv = hovered(kHeaderStarX - 9, kHeaderStarY - 9, 18, 18);
        const uint32_t col = fav ? accentOn(p, face, p.accent3) : (hv && canFav ? plaqueInk : mix(face, p.shadow, 110));
        iconShadow(c, p, kHeaderStarX - 7, kHeaderStarY - 7, kIconStar, col, 2);
        addHit(kHeaderStarX - 9, kHeaderStarY - 9, 18, 18, kHitHeaderStar);
    }

    const int nameX = bx + bw / 2 + 8;
    ui::textFit(c, ui::kFontTitle, nameX - std::min(ui::textWidth(ui::kFontTitle, cur_.name.c_str()), bw - 120) / 2,
                by + 8, bw - 120, cur_.name.c_str(), plaqueInk);
    // (textFit draws from the left; centre it by its fitted width)

    char sub[96];
    if (cur_.key.kind == kSoundFactory)
        std::snprintf(sub, sizeof(sub), "%02u/%02d  %s  /  %s", cur_.key.id + 1, kNumFactoryPresets, kEraNames[era], cur_.category.c_str());
    else
        std::snprintf(sub, sizeof(sub), "%s  %s  /  %s", cur_.key.kind == kSoundUser ? "MY SOUND" : "NOT SAVED",
                      kEraNames[era], cur_.category.c_str());
    const char* edited = modified_ ? "   EDITED" : "";
    const int subW = ui::textWidth(ui::kFontBody, sub) + ui::textWidth(ui::kFontBody, edited);
    const int sx = nameX + 6 - subW / 2;
    gem(c, sx - 10, by + 28, 3, kEraColors[era], p.shadow);
    ui::text(c, ui::kFontBody, sx, by + 24, sub, mix(plaqueInk, face, 50));
    if (modified_) ui::text(c, ui::kFontBody, sx + ui::textWidth(ui::kFontBody, sub), by + 24, edited, accentOn(p, face, p.accent3));

    // Demo button: play / stop on the left, the song list on the right
    const bool demo = telemetry_->demoActive.load(std::memory_order_relaxed) != 0;
    {
        const int x = 640, y = 10, w = 104, h = 34;
        const int listX = 712, mainW = listX - x;
        const bool hvMain = hovered(x, y, mainW, h), hvList = hovered(listX, y, x + w - listX, h);
        const bool hv = hvMain;
        const uint32_t f = bar(c, x, y, w, h, demo, hvMain || hvList);
        const int ty = barY(y, h) + (kBarH - ui::capHeight(ui::kFontTitle)) / 2;
        const int tw = 12 + ui::textWidth(ui::kFontTitle, "DEMO");
        const int tx = x + 4 + (mainW - 4 - tw) / 2;
        // divider and the list arrow
        const int barTop = barY(y, h);
        c.fillRect(listX, barTop + 4, 1, kBarH - 8, ui::shadeColor(f, -70));
        c.fillRect(listX + 1, barTop + 4, 1, kBarH - 8, ui::shadeColor(f, 40));
        {
            const uint32_t ac = accentOn(p, f, hvList ? p.accent3 : p.accent);
            const int ax = listX + (x + w - listX) / 2 - 1, ay = barTop + kBarH / 2 - 2;
            for (int i = 0; i < 4; ++i) c.fillRect(ax - 3 + i + 1, ay + i + 1, 7 - 2 * i, 1, p.shadow);
            for (int i = 0; i < 4; ++i) c.fillRect(ax - 3 + i, ay + i, 7 - 2 * i, 1, ac);
        }
        addHit(listX, y, x + w - listX, h, kHitDemoList);
        if (demo) {
            const bool blink = (frame_ / 10) & 1;
            c.fillRect(tx, ty + 1, 8, 8, blink ? accentOn(p, f, p.accent3) : inkOn(p, f));
        } else {
            arrow(c, p, tx + 3, ty + 5, 6, 1, accentOn(p, f, hv ? p.accent3 : p.accent));
        }
        ui::text(c, ui::kFontTitle, tx + 12, ty, "DEMO", (hv && !demo) ? accentOn(p, f, p.accent) : inkOn(p, f));
        addHit(x, y, mainW, h, kHitDemo);
    }
    // Skin button
    {
        const int x = 752, y = 10, w = 112, h = 34;
        const bool hv = hovered(x, y, w, h);
        const int theme = std::clamp(telemetry_->theme.load(std::memory_order_relaxed), 0, kNumThemes - 1);
        const uint32_t f = bar(c, x, y, w, h, false, hv);
        const int ty = barY(y, h) + (kBarH - ui::capHeight(ui::kFontTitle)) / 2;
        const int tw = ui::textWidth(ui::kFontTiny, "SKIN", 1) + 5 + ui::textWidth(ui::kFontTitle, kThemeNames[theme]);
        const int tx = x + (w - tw) / 2;
        ui::text(c, ui::kFontTiny, tx, ty + 2, "SKIN", mutedOn(p, f), 1);
        ui::text(c, ui::kFontTitle, tx + ui::textWidth(ui::kFontTiny, "SKIN", 1) + 5, ty, kThemeNames[theme],
                 hv ? accentOn(p, f, p.accent) : inkOn(p, f));
        addHit(x, y, w, h, kHitTheme);
    }
    // Zoom button
    {
        const int x = 872, y = 10, w = 72, h = 34;
        const bool hv = hovered(x, y, w, h);
        const uint32_t f = bar(c, x, y, w, h, false, hv);
        char buf[16];
        std::snprintf(buf, sizeof(buf), "%d%%", zoom());
        const int ty = barY(y, h) + (kBarH - ui::capHeight(ui::kFontTitle)) / 2;
        const int tw = 11 + ui::textWidth(ui::kFontTitle, buf);
        const int tx = x + (w - tw) / 2;
        drawIcon(c, tx, ty + 2, kIconZoom, mutedOn(p, f));
        ui::text(c, ui::kFontTitle, tx + 11, ty, buf, hv ? accentOn(p, f, p.accent) : inkOn(p, f));
        addHit(x, y, w, h, kHitZoom);
    }
}

void PixelLabsView::drawEraTabs(PixelCanvas& c, const Palette& p) {
    static const char* kTabNames[kNumBrowserTabs] = {
        "ALL", "80S CHIP", "90S CHIP", "SYNTH 80S", "MEDIEVAL", "SFX + KIT", "MY SOUNDS", "FAVORITES", "BANKS"
    };
    const int tab = browserTab();
    const int y = kTabY, h = kTabH;
    for (int t = 0; t < kNumBrowserTabs; ++t) {
        const int x = tabX(t), w = tabW(t);
        const bool active = (t == tab);
        const bool hv = hovered(x, y, w, h);
        const uint32_t f = bar(c, x, y, w, h, active, hv, true);
        const int ty = barY(y, h) + (kBarH - ui::capHeight(ui::kFontTitle)) / 2;
        const int iconW = t == 0 ? 0 : 11;
        const int tw = iconW + ui::textWidth(ui::kFontTitle, kTabNames[t]);
        const int tx = x + (w - tw) / 2;
        const uint32_t iconCol = active ? inkOn(p, f) : accentOn(p, f, p.accent);
        if (t >= 1 && t <= kNumEras) gem(c, tx + 3, ty + 5, 3, kEraColors[t - 1], p.shadow);
        else if (t == kTabUser) drawIcon(c, tx, ty + 1, kIconDisk, iconCol);
        else if (t == kTabFavorites) drawIcon(c, tx, ty + 1, kIconStar, active ? inkOn(p, f) : accentOn(p, f, p.accent3));
        else if (t == kTabBanks) drawIcon(c, tx, ty + 1, kIconFolder, iconCol);
        ui::text(c, ui::kFontTitle, tx + iconW, ty, kTabNames[t],
                 active ? inkOn(p, f) : (hv ? accentOn(p, f, p.accent) : mutedOn(p, f)));
        addHit(x, y, w, h, kHitEraTab, t);
    }
}

void PixelLabsView::drawPresetList(PixelCanvas& c, const Palette& p) {
    const int x = kListX, y = kListY, w = kListW, h = kListH;
    framePanel(c, x, y, w, h);
    addHit(x, y, w, h, kHitPresetList);
    const int tab = browserTab();
    well(c, p, kRowX - 3, kRowsTop - 3, kListX + kListW - 12 - (kRowX - 3), kVisibleRows * kRowH + 5);
    const int ty = y + 15;

    if (tab == kTabBanks && openBankId_ == 0) {
        drawBankRows(c, p);
    } else {
        const std::vector<SoundKey> list = browseList();
        char title[64];
        if (tab == kTabBanks) {
            const SoundBank* b = lib_.findBank(openBankId_);
            const bool hv = hovered(x + 20, y + 10, w - 40, 18);
            arrow(c, p, x + 28, ty + 4, 5, -1, hv ? p.accent3 : p.accent);
            std::snprintf(title, sizeof(title), "%s  %d", b ? b->name.c_str() : "", static_cast<int>(list.size()));
            ui::textFit(c, ui::kFontTitle, x + 36, ty - 1, w - 64, title, hv ? p.accent3 : p.accent);
            addHit(x + 20, y + 10, w - 40, 18, kHitBankBack);
        } else {
            const char* label = tab == kTabAll ? "ALL SOUNDS" : tab == kTabUser ? "MY SOUNDS"
                              : tab == kTabFavorites ? "FAVORITES" : kEraNames[tab - 1];
            ui::text(c, ui::kFontTitle, x + 26, ty - 1, label, p.accent);
            char cnt[16];
            std::snprintf(cnt, sizeof(cnt), "%d", static_cast<int>(list.size()));
            ui::text(c, ui::kFontBody, x + 32 + ui::textWidth(ui::kFontTitle, label), ty, cnt, p.muted);
            if (static_cast<int>(list.size()) > kVisibleRows)
                ui::textRight(c, ui::kFontTiny, x + w - 26, ty + 1, "WHEEL", p.border, 1);
        }
        drawSoundRows(c, p, list, tab);
    }
    drawActionBar(c, p);
}

static void drawScrollbar(PixelCanvas& c, const Palette& p, int sx, int sy, int sh, int visible, int n, int scroll) {
    const int maxScroll = std::max(0, n - visible);
    if (maxScroll <= 0) return;
    ui::vbar(c, g_skin, ui::kSprVBarNavy, sx - 2, sy, sh, 10);
    const int inner = sh - 20;
    const int th = std::max(10, inner * visible / n);
    const int ty = sy + 10 + (inner - th) * scroll / maxScroll;
    c.fillRect(sx, ty, 3, th, p.accent);
    c.fillRect(sx, ty, 1, th, p.accent3);
}

static void drawHintLines(PixelCanvas& c, const Palette& p, int cx, int y, std::initializer_list<const char*> lines) {
    int ly = y;
    for (const char* l : lines) { ui::textCentered(c, ui::kFontLight, cx, ly, l, p.muted); ly += 14; }
}

void PixelLabsView::drawSoundRows(PixelCanvas& c, const Palette& p, const std::vector<SoundKey>& list, int tab) {
    const int n = static_cast<int>(list.size());
    const int maxScroll = std::max(0, n - kVisibleRows);
    const int viewTag = tab * 64 + static_cast<int>(openBankId_ & 63);

    // Keep the loaded sound in view when it changes or the view changes
    if (cur_.key != lastKeySeen_ || viewTag != lastTabSeen_) {
        auto it = std::find(list.begin(), list.end(), cur_.key);
        if (it != list.end()) {
            const int pos = static_cast<int>(it - list.begin());
            if (pos < listScroll_) listScroll_ = pos;
            if (pos >= listScroll_ + kVisibleRows) listScroll_ = pos - kVisibleRows + 1;
        }
        lastKeySeen_ = cur_.key;
        lastTabSeen_ = viewTag;
    }
    listScroll_ = std::clamp(listScroll_, 0, maxScroll);

    if (n == 0) {
        const int cx = kRowX + kRowW / 2, cy = kRowsTop + 80;
        if (tab == kTabUser) drawHintLines(c, p, cx, cy, { "NO SAVED SOUNDS YET", "", "CHANGE A SOUND, THEN", "PRESS SAVE AS BELOW" });
        else if (tab == kTabFavorites) drawHintLines(c, p, cx, cy, { "NO FAVORITES YET", "", "CLICK THE STAR NEXT", "TO ANY SOUND" });
        else if (tab == kTabBanks) drawHintLines(c, p, cx, cy, { "THIS BANK IS EMPTY", "", "LOAD A SOUND, THEN", "PRESS + BANK BELOW" });
        return;
    }

    for (int r = 0; r < kVisibleRows && listScroll_ + r < n; ++r) {
        const SoundKey key = list[listScroll_ + r];
        const int ry = kRowsTop + r * kRowH;
        const int rx = kRowX, rw = kRowW, rh = kRowH - 1;
        const bool sel = (key == cur_.key);
        const bool hv = hovered(rx, ry, rw, rh);
        const bool user = key.kind == kSoundUser;
        uint32_t face = p.inset;
        if (sel) { ui::hbar(c, g_skin, ui::kSprFlatGilt, rx, ry + (rh - 16) / 2, rw, 4); face = faceColor(ui::kSprFlatGilt); }
        else if (hv) { ui::hbar(c, g_skin, ui::kSprFlatMid, rx, ry + (rh - kFlatH) / 2, rw, 4); face = faceColor(ui::kSprFlatMid); }
        const int ty = ry + (rh - ui::capHeight(ui::kFontBody)) / 2;
        gem(c, rx + 7, ry + rh / 2, 2, kEraColors[soundEra(key)], p.shadow);
        if (user) {
            drawIcon(c, rx + 13, ty, kIconDisk, accentOn(p, face, sel ? p.accent3 : p.accent));
        } else {
            char num[8];
            std::snprintf(num, sizeof(num), "%02u", key.id + 1);
            ui::text(c, ui::kFontTiny, rx + 13, ty + 1, num, sel ? accentOn(p, face, p.accent3) : mutedOn(p, face), 1);
        }
        addHit(rx, ry, rw, rh, kHitSoundRow, packKey(key));

        // Row tools: star always, rename / delete / remove-from-bank while hovered
        const bool fav = lib_.isFavorite(key);
        const bool starHv = hovered(rx + rw - 15, ry, 15, rh);
        const uint32_t starCol = fav ? accentOn(p, face, p.accent3) : (starHv ? accentOn(p, face, p.accent) : mix(face, inkOn(p, face), 90));
        drawIcon(c, rx + rw - 12, ty, kIconStar, starCol);
        addHit(rx + rw - 15, ry, 15, rh, kHitStar, packKey(key));

        int tools = 0;
        if (hv || starHv || hovered(rx + rw - 45, ry, 30, rh)) {
            const uint32_t toolCol = inkOn(p, face);
            if (tab == kTabUser && user) {
                const bool h1 = hovered(rx + rw - 29, ry, 14, rh), h2 = hovered(rx + rw - 43, ry, 14, rh);
                drawIcon(c, rx + rw - 26, ty, kIconCross, h1 ? accentOn(p, face, p.accent3) : toolCol);
                drawIcon(c, rx + rw - 40, ty, kIconPencil, h2 ? accentOn(p, face, p.accent3) : toolCol);
                addHit(rx + rw - 29, ry, 14, rh, kHitSoundDelete, static_cast<int>(key.id));
                addHit(rx + rw - 43, ry, 14, rh, kHitSoundRename, static_cast<int>(key.id));
                tools = 2;
            } else if (tab == kTabBanks) {
                const bool h1 = hovered(rx + rw - 29, ry, 14, rh);
                drawIcon(c, rx + rw - 26, ty, kIconMinus, h1 ? accentOn(p, face, p.accent3) : toolCol);
                addHit(rx + rw - 29, ry, 14, rh, kHitBankRemoveItem, packKey(key));
                tools = 1;
            }
        }
        ui::textFit(c, ui::kFontBody, rx + 28, ty, rw - 28 - 17 - tools * 14, soundName(key).c_str(),
                    (hv && !sel) ? accentOn(p, face, p.accent) : inkOn(p, face));
    }
    drawScrollbar(c, p, kListX + kListW - 18, kRowsTop - 2, kVisibleRows * kRowH + 2, kVisibleRows, n, listScroll_);
}

void PixelLabsView::drawBankRows(PixelCanvas& c, const Palette& p) {
    const int x = kListX, y = kListY;
    const int nBanks = static_cast<int>(lib_.banks.size());
    const int n = nBanks + 1; // + NEW BANK
    ui::text(c, ui::kFontTitle, x + 26, y + 14, "BANKS", p.accent);
    {
        char cnt[16];
        std::snprintf(cnt, sizeof(cnt), "%d", nBanks);
        ui::text(c, ui::kFontBody, x + 32 + ui::textWidth(ui::kFontTitle, "BANKS"), y + 15, cnt, p.muted);
    }
    if (lastTabSeen_ != -2) { lastTabSeen_ = -2; lastKeySeen_ = { -1, 0 }; }
    listScroll_ = std::clamp(listScroll_, 0, std::max(0, n - kVisibleRows));

    for (int r = 0; r < kVisibleRows && listScroll_ + r < n; ++r) {
        const int idx = listScroll_ + r;
        const int ry = kRowsTop + r * kRowH;
        const int rx = kRowX, rw = kRowW, rh = kRowH - 1;
        const bool hv = hovered(rx, ry, rw, rh);
        const int ty = ry + (rh - ui::capHeight(ui::kFontBody)) / 2;
        uint32_t face = p.inset;
        if (hv) { ui::hbar(c, g_skin, ui::kSprFlatMid, rx, ry + (rh - kFlatH) / 2, rw, 4); face = faceColor(ui::kSprFlatMid); }
        if (idx == nBanks) {
            ui::text(c, ui::kFontBody, rx + 9, ty, "+ NEW BANK", accentOn(p, face, hv ? p.accent3 : p.accent2));
            addHit(rx, ry, rw, rh, kHitBankNew);
            continue;
        }
        const SoundBank& b = lib_.banks[idx];
        drawIcon(c, rx + 8, ty, kIconFolder, accentOn(p, face, p.accent));
        addHit(rx, ry, rw, rh, kHitBankRow, static_cast<int>(b.id));
        int right = rx + rw - 5;
        if (hv || hovered(rx + rw - 30, ry, 30, rh)) {
            const bool h1 = hovered(rx + rw - 15, ry, 15, rh), h2 = hovered(rx + rw - 29, ry, 14, rh);
            drawIcon(c, rx + rw - 12, ty, kIconCross, h1 ? accentOn(p, face, p.accent3) : inkOn(p, face));
            drawIcon(c, rx + rw - 26, ty, kIconPencil, h2 ? accentOn(p, face, p.accent3) : inkOn(p, face));
            addHit(rx + rw - 15, ry, 15, rh, kHitBankDelete, static_cast<int>(b.id));
            addHit(rx + rw - 29, ry, 14, rh, kHitBankRename, static_cast<int>(b.id));
            right = rx + rw - 32;
        }
        char cnt[16];
        std::snprintf(cnt, sizeof(cnt), "%d", static_cast<int>(b.items.size()));
        const int cw = ui::textWidth(ui::kFontBody, cnt);
        ui::text(c, ui::kFontBody, right - cw, ty, cnt, mutedOn(p, face));
        ui::textFit(c, ui::kFontBody, rx + 22, ty, right - cw - 8 - (rx + 22), b.name.c_str(), hv ? accentOn(p, face, p.accent) : inkOn(p, face));
    }
    if (nBanks == 0) {
        drawHintLines(c, p, kRowX + kRowW / 2, kRowsTop + 60,
                      { "A BANK IS YOUR OWN SET", "OF SOUNDS. MAKE AS MANY", "AS YOU LIKE AND NAME", "THEM ANYTHING." });
    }
    drawScrollbar(c, p, kListX + kListW - 18, kRowsTop - 2, kVisibleRows * kRowH + 2, kVisibleRows, n, listScroll_);
}

void PixelLabsView::drawActionBar(PixelCanvas& c, const Palette& p) {
    const int y = kActionY, h = kActionH;
    button(c, p, kSaveBtnX, y, kSaveBtnW, h, "SAVE", modified_, hovered(kSaveBtnX, y, kSaveBtnW, h));
    button(c, p, kSaveAsBtnX, y, kSaveAsBtnW, h, "SAVE AS", false, hovered(kSaveAsBtnX, y, kSaveAsBtnW, h));
    button(c, p, kBankBtnX, y, kBankBtnW, h, "+ BANK", false, hovered(kBankBtnX, y, kBankBtnW, h));
    addHit(kSaveBtnX, y, kSaveBtnW, h, kHitSave);
    addHit(kSaveAsBtnX, y, kSaveAsBtnW, h, kHitSaveAs);
    addHit(kBankBtnX, y, kBankBtnW, h, kHitAddToBank);
}

void PixelLabsView::drawHero(PixelCanvas& c, const Palette& p) {
    const int x = kHeroX, y = kHeroY, w = kHeroW, h = kHeroH;
    framePanel(c, x, y, w, h);

    // ---- Oscilloscope ----
    const int sx = x + 24, sy = y + 12, sw = w - 48, sh = 62;
    knotPanel(c, sx, sy, sw, sh);
    const int ix = sx + 7, iy = sy + 7, iw = sw - 14, ih = sh - 14;
    for (int gx = ix + 8; gx < ix + iw; gx += 16) c.blendPixel(gx, iy + ih / 2, p.border, 200);
    for (int gy = iy + 4; gy < iy + ih; gy += 8) c.blendPixel(ix + iw / 2, gy, p.border, 200);
    {
        float snap[256];
        const uint32_t wIdx = telemetry_->scopeWriteIdx.load(std::memory_order_relaxed);
        for (int i = 0; i < 256; ++i) snap[i] = telemetry_->scopeSamples[(wIdx + i) & 255];
        int start = 0;
        for (int i = 1; i < 128; ++i) {
            if (snap[i - 1] <= 0.0f && snap[i] > 0.0f) { start = i; break; }
        }
        int prevX = ix, prevY = iy + ih / 2;
        for (int i = 0; i < 128; ++i) {
            const float v = std::clamp(snap[start + i] * 1.6f, -1.0f, 1.0f);
            const int px = ix + 2 + i * (iw - 4) / 127;
            const int py = iy + ih / 2 - static_cast<int>(v * (ih / 2 - 3));
            if (i > 0) c.drawLine(prevX, prevY, px, py, p.accent2, 2);
            prevX = px;
            prevY = py;
        }
    }
    ui::text(c, ui::kFontTiny, ix + 4, iy + 3, "SCOPE", p.muted, 1);
    {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%.0f BPM", telemetry_->tempoBpm.load(std::memory_order_relaxed));
        ui::textRight(c, ui::kFontTiny, ix + iw - 4, iy + 3, buf, p.muted, 1);
    }
    scanlines(c, ix, iy, iw, ih, p.scanAlpha);

    // ---- Macro dial: a ring of gems around a gold bezel ----
    const int macro = std::clamp(telemetry_->activeMacro.load(std::memory_order_relaxed), 0, kNumMacros - 1);
    const int macroParam = kMacroParamIds[macro];
    const float mv = param(macroParam);
    const int cx = x + w / 2, cy = y + 140, r = 52;
    const bool dialHover = hovered(cx - 66, cy - 66, 132, 132) || dragKind_ == kHitBigDial;
    const uint32_t gold = roleColor(p, ui::kRoleGold), camel = roleColor(p, ui::kRoleCamel);
    c.fillCircle(cx, cy, r + 10, p.inset, 1);
    c.drawPixelCircle(cx, cy, r + 11, ui::shadeColor(camel, -40), 2, 1);
    c.drawPixelCircle(cx, cy, r + 10, dialHover ? p.accent3 : gold, 1, 1);
    const int segs = 27;
    const int lit = static_cast<int>(std::lround(mv * (segs - 1)));
    for (int i = 0; i < segs; ++i) {
        const double ang = (225.0 - i * (270.0 / (segs - 1))) * 3.14159265358979 / 180.0;
        const int px = cx + static_cast<int>(std::lround(std::cos(ang) * (r - 3)));
        const int py = cy - static_cast<int>(std::lround(std::sin(ang) * (r - 3)));
        const bool on = (mv > 0.001f) && i <= lit;
        if (on) blendRect(c, px - 5, py - 5, 11, 11, p.accent, 40);
        gem(c, px, py, 3, on ? (i == lit ? p.accent3 : p.accent) : p.panel, on ? p.shadow : p.border);
    }
    // Inner bezel
    c.fillCircle(cx, cy, r - 15, p.panel, 1);
    c.drawPixelCircle(cx, cy, r - 15, gold, 1, 1);
    c.drawPixelCircle(cx, cy, r - 17, ui::shadeColor(camel, -50), 1, 1);
    char valBuf[32];
    formatParamValue(macroParam, mv, valBuf, sizeof(valBuf));
    const int vScale = ui::textWidth(ui::kFontChunky, valBuf, 2) <= 2 * (r - 22) ? 2 : 1;
    ui::textShadow(c, ui::kFontChunky, cx - ui::textWidth(ui::kFontChunky, valBuf, vScale) / 2,
                   cy - (ui::capHeight(ui::kFontChunky) * vScale) / 2, valBuf, p.ink, p.shadow, vScale);
    addHit(cx - 66, cy - 66, 132, 132, kHitBigDial);

    char mname[48];
    if (macroParam == kParamReverb)
        std::snprintf(mname, sizeof(mname), "%s / %s", kMacroNames[macro], reverbTypeName(normToList(param(kParamReverbType), kNumReverbTypes)));
    else if (macroParam == kParamDelay)
        std::snprintf(mname, sizeof(mname), "%s / %s", kMacroNames[macro], delayTypeName(normToList(param(kParamDelayType), kNumDelayTypes)));
    else
        std::snprintf(mname, sizeof(mname), "%s", kMacroNames[macro]);
    ui::textShadow(c, ui::kFontTitle, cx - ui::textWidth(ui::kFontTitle, mname) / 2, cy + r + 15, mname, p.accent, p.shadow);

    // ---- Output meters (left of the dial) ----
    auto meter = [&](int mx, float peak, const char* label) {
        const int my = cy - 58, mh = 112, nSeg = 14;
        ui::vbar(c, g_skin, ui::kSprVBarDark, mx, my, mh, 10);
        const float db = 20.0f * std::log10(std::max(peak, 1e-5f));
        const int on = std::clamp(static_cast<int>((db + 48.0f) / 48.0f * nSeg + 0.5f), 0, nSeg);
        const int top = my + 10, span = mh - 20;
        for (int i = 0; i < nSeg; ++i) {
            const int segY = top + span - (i + 1) * span / nSeg;
            uint32_t col = p.inset;
            if (i < on) col = (i >= nSeg - 2) ? roleColor(p, ui::kRoleRed) : (i >= nSeg - 5 ? p.accent : p.accent2);
            c.fillRect(mx + 2, segY + 1, 3, span / nSeg - 1, col);
        }
        ui::textCentered(c, ui::kFontTiny, mx + 4, my + mh + 4, label, p.muted, 1);
    };
    meter(x + 24, telemetry_->peakL.load(std::memory_order_relaxed), "L");
    meter(x + 38, telemetry_->peakR.load(std::memory_order_relaxed), "R");

    // ---- Voices, pedal and bend (right of the dial) ----
    {
        const int vx = x + w - 62, vy = cy - 58;
        const int voices = telemetry_->activeVoices.load(std::memory_order_relaxed);
        ui::text(c, ui::kFontTiny, vx, vy, "VOICE", p.muted, 1);
        for (int i = 0; i < 16; ++i) {
            const int col = i % 4, row = i / 4;
            gem(c, vx + 3 + col * 10, vy + 16 + row * 10, 2, i < voices ? p.accent2 : p.panel, i < voices ? p.shadow : p.border);
        }
        char buf[16];
        std::snprintf(buf, sizeof(buf), "%02d/16", voices);
        ui::text(c, ui::kFontBody, vx, vy + 54, buf, p.ink);
        const bool sus = param(kParamSustain) >= 0.5f;
        gem(c, vx + 3, vy + 74, 3, sus ? p.accent3 : p.panel, sus ? p.shadow : p.border);
        ui::text(c, ui::kFontTiny, vx + 10, vy + 71, "PEDAL", sus ? p.ink : p.muted, 1);
        const float bend = (param(kParamPitchBend) - 0.5f) * 4.0f;
        ui::hbar(c, g_skin, ui::kSprHBarNavy, vx - 2, vy + 86, 46, 10);
        c.fillRect(vx + 20 + static_cast<int>(bend * 9.0f), vy + 84, 3, 11, p.accent3);
        ui::text(c, ui::kFontTiny, vx, vy + 98, "BEND", p.muted, 1);
    }

    // ---- 12 macro buttons ----
    const int bw = 54, bh = 30, gap = 4;
    const int bx0 = x + (w - (6 * bw + 5 * gap)) / 2;
    for (int m = 0; m < kNumMacros; ++m) {
        const int bx = bx0 + (m % 6) * (bw + gap);
        const int by = y + 226 + (m / 6) * (bh + gap);
        const bool sel = (m == macro);
        const bool hv = hovered(bx, by, bw, bh);
        const uint32_t f = slot(c, bx, by, bw, bh, sel ? 2 : (hv ? 1 : 0));
        ui::textCentered(c, ui::kFontChunky, bx + bw / 2, by + 6, kMacroShort[m], (hv && !sel) ? accentOn(p, f, p.accent) : inkOn(p, f));
        gauge(c, p, bx + 7, by + 20, bw - 14, param(kMacroParamIds[m]), sel ? ui::kSprGemRed : ui::kSprGemBlue);
        addHit(bx, by, bw, bh, kHitMacroBtn, m);
    }
}

void PixelLabsView::drawSoundPanel(PixelCanvas& c, const Palette& p) {
    const int x = kSoundX, y = kSoundY, w = kSoundW, h = kSoundH;
    framePanel(c, x, y, w, h);
    ui::text(c, ui::kFontTitle, x + 26, y + 14, "SOUND", p.accent);
    ui::textRight(c, ui::kFontTiny, x + w - 26, y + 16, "DBL-CLICK = RESET", p.border, 1);

    // List steppers
    const bool drumOsc = normToList(param(kParamWaveform), kNumWaveforms) == kWaveDrums;
    for (int i = 0; i < 4; ++i) {
        const StepperDef& s = kSteppers[i];
        const bool kitRow = drumOsc && s.param == kParamHarmony;
        const int ry = y + 30 + i * 21, rh = 19;
        ui::text(c, ui::kFontBody, x + 16, ry + (rh - ui::capHeight(ui::kFontBody)) / 2, kitRow ? "KIT" : s.label,
                 kitRow ? p.accent : p.muted);
        const int bx = x + 58, bw = w - 74;
        const bool hp = hovered(bx, ry, bw / 2, rh), hn = hovered(bx + bw / 2, ry, bw - bw / 2, rh);
        const uint32_t f = flat(c, bx, ry, bw, rh, hp || hn);
        arrow(c, p, bx + 10, ry + rh / 2, 5, -1, accentOn(p, f, hp ? p.accent3 : p.accent));
        arrow(c, p, bx + bw - 10, ry + rh / 2, 5, 1, accentOn(p, f, hn ? p.accent3 : p.accent));
        char buf[48];
        if (kitRow) std::snprintf(buf, sizeof(buf), "%s", drumKitName(normToList(param(s.param), kNumDrumKits)));
        else formatParamValue(s.param, param(s.param), buf, sizeof(buf));
        const int idx = normToList(param(s.param), s.count);
        const int ty = ry + (rh - ui::capHeight(ui::kFontBody)) / 2;
        ui::textCentered(c, ui::kFontBody, bx + bw / 2 + 8, ty, buf, inkOn(p, f));
        char cnt[16];
        std::snprintf(cnt, sizeof(cnt), "%d/%d", idx + 1, s.count);
        ui::text(c, ui::kFontTiny, bx + 20, ty + 1, cnt, mutedOn(p, f), 1);
        addHit(bx, ry, bw / 2, rh, kHitStepPrev, s.param);
        addHit(bx + bw / 2, ry, bw - bw / 2, rh, kHitStepNext, s.param);
    }

    // 16 sliders in two columns
    for (int i = 0; i < 16; ++i) {
        const SliderDef& s = kSliders[i];
        const int col = i / 8, row = i % 8;
        const int sx = x + 16 + col * 142, sy = y + 118 + row * 21, sw = 130;
        const float v = param(s.param);
        const bool active = (dragKind_ == kHitSlider && dragParam_ == s.param);
        const bool hv = hovered(sx, sy - 2, sw, 21) || active;
        ui::text(c, ui::kFontBody, sx, sy, s.label, hv ? p.ink : p.muted);
        char buf[32];
        formatParamValue(s.param, v, buf, sizeof(buf));
        ui::textRight(c, ui::kFontBody, sx + sw, sy, buf, hv ? p.accent3 : p.ink);
        const int by = sy + 11;
        ui::hbar(c, g_skin, ui::kSprHBarDark, sx, by, sw, 10);
        const int tx0 = sx + 9, tw = sw - 18; // channel between the gold points
        const bool bipolar = (s.param == kParamSweep);
        const uint32_t fillCol = hv ? p.accent3 : p.accent;
        if (bipolar) {
            const int mid = tx0 + tw / 2;
            const int px = tx0 + static_cast<int>((tw - 1) * v);
            c.fillRect(std::min(mid, px), by + 2, std::abs(px - mid) + 1, 3, fillCol);
            c.fillRect(mid, by, 1, 7, p.muted);
        } else {
            const int fill = static_cast<int>((tw - 1) * v);
            if (fill > 0) {
                c.fillRect(tx0, by + 2, fill, 3, fillCol);
                c.fillRect(tx0, by + 2, fill, 1, ui::shadeColor(fillCol, 60));
            }
        }
        // Stored-sound reference tick
        const int rx = tx0 + static_cast<int>((tw - 1) * cur_.ref[s.param]);
        c.fillRect(rx, by + 7, 1, 2, p.muted);
        // Jewel handle
        const int hx = tx0 + static_cast<int>((tw - 1) * v);
        const ui::SpriteRect& rr = ui::rect(ui::kSprRuneGold0);
        ui::sprite(c, g_skin, (hv ? ui::kSprRuneGold0 : ui::kSprRuneGold3), hx - rr.w / 2, by + 3 - rr.h / 2);
        addHit(tx0, sy - 2, tw, 21, kHitSlider, s.param); // the channel, so a click lands where the handle goes
    }
}

void PixelLabsView::drawStatusStrip(PixelCanvas& c, const Palette& p) {
    const int x = 16, y = kStripY, w = 928, h = 30;
    knotPanel(c, x, y, w, h);

    const uint64_t lo = telemetry_->activeNotesLow.load(std::memory_order_relaxed);
    const uint64_t hi = telemetry_->activeNotesHigh.load(std::memory_order_relaxed);
    int notes[16];
    int count = 0, total = 0;
    int pcMask = 0, lowest = -1;
    for (int n = 0; n < 128; ++n) {
        const bool on = n < 64 ? ((lo >> n) & 1ull) : ((hi >> (n - 64)) & 1ull);
        if (!on) continue;
        ++total;
        if (count < 16) notes[count++] = n;
        pcMask |= 1 << (n % 12);
        if (lowest < 0) lowest = n;
    }
    const bool drums = normToList(param(kParamWaveform), kNumWaveforms) == kWaveDrums;
    const int kit = normToList(param(kParamHarmony), kNumDrumKits);
    const int notesRight = kTouchX - 8;

    // Hover readout (from last frame's hit map: later sections have not added theirs yet)
    const Hit* hh = nullptr;
    if (hoverX_ >= 0 && modal_.kind == kModalNone) {
        for (auto it = prevHits_.rbegin(); it != prevHits_.rend(); ++it) {
            if (hoverX_ >= it->x && hoverX_ < it->x + it->w && hoverY_ >= it->y && hoverY_ < it->y + it->h) { hh = &*it; break; }
        }
    }
    if (dragKind_ == kHitSlider || dragKind_ == kHitBigDial) hh = &dragHit_;
    char info[96] = {};
    if (hh) {
        int pid = -1;
        if (hh->kind == kHitSlider || hh->kind == kHitStepPrev || hh->kind == kHitStepNext) pid = hh->a;
        else if (hh->kind == kHitBigDial) pid = kMacroParamIds[std::clamp(telemetry_->activeMacro.load(), 0, kNumMacros - 1)];
        else if (hh->kind == kHitMacroBtn) pid = kMacroParamIds[hh->a];
        if (pid == kParamVelSens) {
            std::snprintf(info, sizeof(info), "VELOCITY SENS: %.0f%%  LAST HIT %d", param(pid) * 100.0f,
                          telemetry_->lastVelocity.load(std::memory_order_relaxed));
        } else if (pid >= 0) {
            char v[32];
            formatParamValue(pid, param(pid), v, sizeof(v));
            const char* label = "";
            for (const auto& s : kSliders) if (s.param == pid) label = s.label;
            for (const auto& s : kSteppers) if (s.param == pid) label = s.label;
            for (int m = 0; m < kNumMacros; ++m) if (kMacroParamIds[m] == pid) label = kMacroNames[m];
            if (pid == kParamHarmony && drums) {
                label = "DRUM KIT";
                std::snprintf(v, sizeof(v), "%s", drumKitName(kit));
            }
            std::snprintf(info, sizeof(info), "%s: %s", label, v);
        } else if (hh->kind == kHitKeyboard) {
            const int k = keyAt(hoverX_, hoverY_);
            if (k >= 0) std::snprintf(info, sizeof(info), "KEY %s%d  (MIDI %d)  VEL %d", noteName(k), noteOctave(k), k, velocityAt(k, hoverY_));
        } else if (hh->kind == kHitTouchPrev || hh->kind == kHitTouchNext) {
            static const char* kTouchHelp[kNumTouchCurves] = {
                "TOUCH OFF: EVERY KEY PLAYS AT FULL LEVEL", "LIGHT: SOFT PLAYING ALREADY SOUNDS LOUD",
                "NORMAL: LEVEL FOLLOWS HOW HARD YOU PLAY", "HEAVY: PLAY HARD TO GET FULL LEVEL" };
            std::snprintf(info, sizeof(info), "%s", kTouchHelp[normToList(param(kParamTouch), kNumTouchCurves)]);
        } else if (hh->kind == kHitStar) {
            std::snprintf(info, sizeof(info), "%s", lib_.isFavorite(unpackKey(hh->a)) ? "REMOVE FROM FAVORITES" : "ADD TO FAVORITES");
        } else if (hh->kind == kHitHeaderStar) {
            std::snprintf(info, sizeof(info), "%s", cur_.key.kind == kSoundDetached ? "SAVE THE SOUND TO MAKE IT A FAVORITE"
                          : lib_.isFavorite(cur_.key) ? "REMOVE FROM FAVORITES" : "ADD TO FAVORITES");
        } else if (hh->kind == kHitSave) {
            std::snprintf(info, sizeof(info), "%s", cur_.key.kind == kSoundUser ? "SAVE CHANGES INTO THIS SOUND"
                                                                               : "SAVE AS A NEW SOUND IN MY SOUNDS");
        } else if (hh->kind == kHitSaveAs) {
            std::snprintf(info, sizeof(info), "SAVE A COPY UNDER A NEW NAME");
        } else if (hh->kind == kHitAddToBank) {
            std::snprintf(info, sizeof(info), "PUT THIS SOUND IN ONE OF YOUR BANKS");
        } else if (hh->kind == kHitSoundRename || hh->kind == kHitBankRename) {
            std::snprintf(info, sizeof(info), "RENAME");
        } else if (hh->kind == kHitSoundDelete || hh->kind == kHitBankDelete) {
            std::snprintf(info, sizeof(info), "DELETE");
        } else if (hh->kind == kHitBankRemoveItem) {
            std::snprintf(info, sizeof(info), "TAKE OUT OF THIS BANK");
        } else if (hh->kind == kHitBankBack) {
            std::snprintf(info, sizeof(info), "BACK TO ALL BANKS");
        } else if (hh->kind == kHitTheme) {
            std::snprintf(info, sizeof(info), "SKIN: CLICK FOR THE NEXT ONE  (AUTO FOLLOWS THE SOUND'S ERA)");
        } else if (hh->kind == kHitZoom) {
            std::snprintf(info, sizeof(info), "WINDOW SIZE");
        } else if (hh->kind == kHitDemo) {
            if (telemetry_->demoActive.load(std::memory_order_relaxed)) std::snprintf(info, sizeof(info), "STOP THE DEMO SONG");
            else std::snprintf(info, sizeof(info), "PLAY DEMO: %s", demoSongName(nextDemoSong()));
        } else if (hh->kind == kHitDemoList) {
            std::snprintf(info, sizeof(info), "CHOOSE ONE OF THE %d DEMO SONGS", kNumDemoSongs);
        }
    }

    const int textY = y + (h - ui::capHeight(ui::kFontBody)) / 2;
    ui::text(c, ui::kFontTiny, x + 14, y + 12, "NOTES", p.border, 1);
    int tx = x + 54;
    if (count == 0) {
        if (toastFrames_ > 0) {
            const int tw = std::min(ui::textWidth(ui::kFontBody, toast_.c_str()) + 20, notesRight - tx);
            ui::hbar(c, g_skin, ui::kSprFlatGilt, tx - 6, y + 7, tw, 4);
            ui::textFit(c, ui::kFontBody, tx + 4, textY, tw - 16, toast_.c_str(), inkOn(p, faceColor(ui::kSprFlatGilt)));
        } else if (info[0]) {
            ui::textFit(c, ui::kFontBody, tx, textY, notesRight - tx, info, p.accent2);
        } else {
            std::string desc;
            if (cur_.key.kind == kSoundFactory) desc = factoryPreset(static_cast<int>(cur_.key.id)).description;
            else if (cur_.key.kind == kSoundUser) desc = "YOUR SOUND, MADE FROM " + upper(factoryPreset(cur_.basePreset).name);
            else desc = "NOT IN YOUR LIBRARY - PRESS SAVE AS TO KEEP IT";
            ui::textFit(c, ui::kFontLight, tx, y + (h - ui::capHeight(ui::kFontLight)) / 2, notesRight - tx, desc.c_str(), p.muted);
        }
    } else {
        const int noteY = y + (h - ui::capHeight(ui::kFontTitle)) / 2;
        if (telemetry_->demoActive.load(std::memory_order_relaxed)) {
            const char* song = demoSongName(telemetry_->demoSong.load(std::memory_order_relaxed));
            const int sw = std::min(ui::textWidth(ui::kFontBody, song) + 20, 200);
            ui::hbar(c, g_skin, ui::kSprFlatMid, tx - 6, y + 7, sw, 4);
            ui::textFit(c, ui::kFontBody, tx + 4, textY, sw - 16, song, accentOn(p, faceColor(ui::kSprFlatMid), p.accent3));
            tx += sw + 6;
        }
        for (int i = 0; i < count && i < 8 && tx < notesRight - 60; ++i) {
            char nb[16];
            if (drums) std::snprintf(nb, sizeof(nb), "%s", kDrumNames[kit][notes[i] % 12]);
            else std::snprintf(nb, sizeof(nb), "%s%d", noteName(notes[i]), noteOctave(notes[i]));
            ui::textShadow(c, ui::kFontTitle, tx, noteY, nb, p.ink, p.shadow);
            tx += ui::textWidth(ui::kFontTitle, nb) + 10;
        }
        if (total > 8) {
            char more[8];
            std::snprintf(more, sizeof(more), "+%d", total - 8);
            ui::text(c, ui::kFontBody, tx, textY, more, p.muted);
            tx += 24;
        }
        // Chord name
        if (!drums && countBits(pcMask) >= 2) {
            struct ChordShape { int mask; const char* name; };
            static const ChordShape kShapes[] = {
                { 0x091, "MAJ" }, { 0x089, "MIN" }, { 0x049, "DIM" }, { 0x111, "AUG" },
                { 0x085, "SUS2" }, { 0x0A1, "SUS4" }, { 0x491, "7" }, { 0x891, "MAJ7" },
                { 0x489, "MIN7" }, { 0x449, "M7B5" }, { 0x081, "5" }, { 0x011, "3RD" }, { 0x009, "MIN3" }
            };
            const char* found = nullptr;
            int rootPc = -1;
            for (int pass = 0; pass < 12 && !found; ++pass) {
                const int root = (lowest + pass) % 12; // prefer the bass note as root
                if (!(pcMask & (1 << root))) continue;
                const int rel = ((pcMask >> root) | (pcMask << (12 - root))) & 0xFFF;
                for (const auto& s : kShapes) {
                    if (rel == s.mask) { found = s.name; rootPc = root; break; }
                }
            }
            if (found) {
                char cb[24];
                std::snprintf(cb, sizeof(cb), "%s %s", noteName(rootPc), found);
                const int cw = ui::textWidth(ui::kFontTitle, cb) + 20;
                ui::hbar(c, g_skin, ui::kSprFlatGilt, tx + 2, y + 7, cw, 4);
                ui::text(c, ui::kFontTitle, tx + 12, noteY, cb, inkOn(p, faceColor(ui::kSprFlatGilt)));
                tx += cw + 10;
            }
        }
        if (info[0]) {
            const int iw = ui::textWidth(ui::kFontBody, info);
            if (notesRight - iw > tx + 8) ui::text(c, ui::kFontBody, notesRight - iw, textY, info, p.accent2);
        }
    }

    // Keyboard touch: curve stepper + velocity sensitivity
    {
        const int ty = y + 6, th = 18;
        const int touch = normToList(param(kParamTouch), kNumTouchCurves);
        const bool hp = hovered(kTouchX, ty, kTouchW / 2, th), hn = hovered(kTouchX + kTouchW / 2, ty, kTouchW - kTouchW / 2, th);
        const uint32_t f = flat(c, kTouchX, ty, kTouchW, th, hp || hn);
        arrow(c, p, kTouchX + 9, ty + th / 2, 5, -1, accentOn(p, f, hp ? p.accent3 : p.accent));
        arrow(c, p, kTouchX + kTouchW - 9, ty + th / 2, 5, 1, accentOn(p, f, hn ? p.accent3 : p.accent));
        char buf[32];
        std::snprintf(buf, sizeof(buf), "TOUCH %s", kTouchShort[touch]);
        ui::textCentered(c, ui::kFontBody, kTouchX + kTouchW / 2, ty + (th - ui::capHeight(ui::kFontBody)) / 2, buf, inkOn(p, f));
        addHit(kTouchX, ty, kTouchW / 2, th, kHitTouchPrev);
        addHit(kTouchX + kTouchW / 2, ty, kTouchW - kTouchW / 2, th, kHitTouchNext);

        const float vs = param(kParamVelSens);
        const bool off = touch == kTouchOff;
        const bool hv = hovered(kVelX, ty, kVelW, th) || (dragKind_ == kHitSlider && dragParam_ == kParamVelSens);
        const uint32_t vf = flat(c, kVelX, ty, kVelW, th, hv);
        const int fy = flatY(ty, th);
        const int fill = static_cast<int>((kVelW - 8) * vs);
        if (fill > 0) blendRect(c, kVelX + 4, fy + 3, fill, kFlatH - 6, off ? p.border : p.accent2, hv ? 110 : 80);
        std::snprintf(buf, sizeof(buf), "VEL SENS %.0f%%", vs * 100.0f);
        ui::textCentered(c, ui::kFontBody, kVelX + kVelW / 2, ty + (th - ui::capHeight(ui::kFontBody)) / 2, buf,
                         off ? mutedOn(p, vf) : (hv ? accentOn(p, vf, p.accent3) : inkOn(p, vf)));
        addHit(kVelX, ty, kVelW, th, kHitSlider, kParamVelSens);
    }

    // Computer keyboard octave control
    {
        const int ox = x + w - 182, oy = y + 6, ow = 168, oh = 18;
        char buf[32];
        std::snprintf(buf, sizeof(buf), "PC KEYS A=C%d", kbdOctave_);
        const bool hd = hovered(ox, oy, 22, oh), hu = hovered(ox + ow - 22, oy, 22, oh);
        const uint32_t f = flat(c, ox, oy, ow, oh, hd || hu);
        arrow(c, p, ox + 9, oy + oh / 2, 5, -1, accentOn(p, f, hd ? p.accent3 : p.accent));
        arrow(c, p, ox + ow - 9, oy + oh / 2, 5, 1, accentOn(p, f, hu ? p.accent3 : p.accent));
        ui::textCentered(c, ui::kFontBody, ox + ow / 2, oy + (oh - ui::capHeight(ui::kFontBody)) / 2, buf, inkOn(p, f));
        addHit(ox, oy, 22, oh, kHitOctDown);
        addHit(ox + ow - 22, oy, 22, oh, kHitOctUp);
    }
}

void PixelLabsView::drawKeyboard(PixelCanvas& c, const Palette& p) {
    const int top = kKeyboardTop, bottom = kKeyboardBottom;
    const int left = kKeyboardLeft, ww = kWhiteKeyW;
    const int nWhite = whiteIndex(kLastKey) + 1;
    const int whiteH = bottom - top;
    const int blackH = 88, blackW = 16;

    // Knotwork rail along the top (the window frame's band)
    {
        const ui::SpriteRect& fr = ui::rect(ui::kSprFrame);
        c.fillRect(left - 4, top - 2, nWhite * ww + 8, whiteH + 6, p.shadow);
        ui::tileH(c, g_skin, fr.x + 24, fr.y + 2, 8, 8, left - 4, top - 9, nWhite * ww + 8);
    }

    const uint64_t lo = telemetry_->activeNotesLow.load(std::memory_order_relaxed);
    const uint64_t hi = telemetry_->activeNotesHigh.load(std::memory_order_relaxed);
    auto isOn = [&](int n) { return n < 64 ? ((lo >> n) & 1ull) != 0 : ((hi >> (n - 64)) & 1ull) != 0; };
    const bool drums = normToList(param(kParamWaveform), kNumWaveforms) == kWaveDrums;
    const int kit = normToList(param(kParamHarmony), kNumDrumKits);
    const int hoverKey = (dragKind_ == kHitNone && hoverY_ >= top && hoverY_ < bottom) ? keyAt(hoverX_, hoverY_) : -1;
    const int pcBase = (kbdOctave_ + 1) * 12;

    addHit(left, top, nWhite * ww, whiteH, kHitKeyboard);

    // White keys
    for (int n = kFirstKey; n <= kLastKey; ++n) {
        if (isBlackKey(n)) continue;
        const int kx = left + whiteIndex(n) * ww;
        const bool on = isOn(n) || n == mouseNote_;
        const uint32_t body = on ? p.keyHit : p.keyWhite;
        c.fillRect(kx, top, ww - 2, whiteH, body);
        if (!on) {
            c.fillRect(kx, top, ww - 2, 2, p.keyWhiteShade);              // shade under the rail
            c.fillRect(kx, bottom - 6, ww - 2, 6, p.keyWhiteShade);
            c.fillRect(kx + ww - 3, top, 1, whiteH - 6, ui::shadeColor(p.keyWhite, -12));
            if (n == hoverKey) blendRect(c, kx, top, ww - 2, whiteH - 6, p.accent, 60);
        } else {
            c.fillRect(kx, top, ww - 2, 3, p.keyHitInk);
        }
        const uint32_t ink = on ? p.keyHitInk : p.keyWhiteInk;
        // Letter name on every white key, octave number on C
        char letter[2] = { noteName(n)[0], 0 };
        ui::textCentered(c, ui::kFontTitle, kx + (ww - 2) / 2, bottom - 22, letter, ink);
        if (n % 12 == 0) {
            char oct[4];
            std::snprintf(oct, sizeof(oct), "%d", noteOctave(n));
            ui::textCentered(c, ui::kFontTiny, kx + (ww - 2) / 2, bottom - 34, oct, on ? ink : p.accent, 1);
        }
        if (drums) ui::textCentered(c, ui::kFontTiny, kx + (ww - 2) / 2, top + blackH + 8, kDrumKeyLabels[kit][n % 12], on ? ink : p.muted, 1);
        // PC-keyboard range marker
        if (n >= pcBase && n <= pcBase + 16) c.fillRect(kx + 4, bottom - 4, ww - 10, 2, p.accent2);
    }

    // Black keys
    for (int n = kFirstKey; n <= kLastKey; ++n) {
        if (!isBlackKey(n)) continue;
        const int kx = left + (whiteIndex(n) + 1) * ww - blackW / 2 - 1;
        const bool on = isOn(n) || n == mouseNote_;
        c.fillRect(kx - 1, top, blackW + 2, blackH + 1, p.shadow);
        c.fillRect(kx, top, blackW, blackH, on ? p.keyHit : p.keyBlack);
        if (!on) {
            c.fillRect(kx + 2, top, 2, blackH - 6, p.border);
            c.fillRect(kx, top + blackH - 6, blackW, 6, p.inset);
            c.fillRect(kx + 1, top + blackH - 6, blackW - 2, 1, roleColor(p, ui::kRoleCamel));
            if (n == hoverKey) blendRect(c, kx, top, blackW, blackH, p.accent, 70);
        }
        const char* label = drums ? kDrumKeyLabels[kit][n % 12] : noteName(n);
        ui::textCentered(c, ui::kFontTiny, kx + blackW / 2, top + blackH - 18, label, on ? p.keyHitInk : p.keyBlackInk, 1);
    }
}

// =============================================================================
// Dialogs
// =============================================================================

void PixelLabsView::drawModal(PixelCanvas& c, const Palette& p) {
    blendRect(c, 0, 0, kBaseWidth, kBaseHeight, 0xFF000000, 150);
    addHit(0, 0, kBaseWidth, kBaseHeight, kHitModalBlock);
    drawingModal_ = true;
    switch (modal_.kind) {
        case kModalName:       drawNameDialog(c, p); break;
        case kModalConfirm:    drawConfirmDialog(c, p); break;
        case kModalBankPicker: drawBankPicker(c, p); break;
        case kModalSongPicker: drawSongPicker(c, p); break;
        default: break;
    }
    drawingModal_ = false;
}

static const char* dialogButtonLabel(int b) {
    switch (b) {
        case 0: return "OK";
        case 1: return "CANCEL";
        case 2: return "SAVE";
        case 3: return "SAVE AS...";
        case 4: return "DON'T SAVE";
        case 5: return "DELETE";
        case 6: return "DONE";
        default: return "?";
    }
}

void PixelLabsView::drawNameDialog(PixelCanvas& c, const Palette& p) {
    const int x = kNameDlgX, y = kNameDlgY, w = kNameDlgW, h = kNameDlgH;
    framePanel(c, x, y, w, h);
    ui::textShadow(c, ui::kFontTitle, x + 26, y + 16, modal_.title.c_str(), p.accent, p.shadow, 1);
    ui::textFit(c, ui::kFontLight, x + 26 + ui::textWidth(ui::kFontTitle, modal_.title.c_str()) + 10, y + 16,
                w - 60 - ui::textWidth(ui::kFontTitle, modal_.title.c_str()), modal_.subtitle.c_str(), p.muted);

    // Text field on parchment, with a blinking block cursor
    const uint32_t inkOnParch = roleColor(p, ui::kRoleNavy);
    const uint32_t faintOnParch = ui::shadeColor(roleColor(p, ui::kRoleCamel), -10);
    const std::string& t = modal_.text;
    const int fx = x + 20, fy = y + 36, fw = w - 40, fh = 44;
    parchment(c, fx, fy, fw, fh);
    const int ty = fy + (fh - 2 * ui::capHeight(ui::kFontBody)) / 2;
    if (t.empty()) ui::text(c, ui::kFontBody, fx + 14, ty, "TYPE A NAME", faintOnParch, 2);
    else ui::text(c, ui::kFontBody, fx + 14, ty, t.c_str(), inkOnParch, 2);
    const int cx = fx + 14 + (t.empty() ? 0 : ui::textWidth(ui::kFontBody, t.c_str(), 2) + 3);
    if ((frame_ / 8) & 1) c.fillRect(cx, ty - 1, 10, 2 * ui::capHeight(ui::kFontBody) + 2, inkOnParch);
    char cnt[16];
    std::snprintf(cnt, sizeof(cnt), "%d/%d", static_cast<int>(t.size()), static_cast<int>(SoundLibrary::kMaxNameLen));
    ui::textRight(c, ui::kFontTiny, fx + fw - 12, fy + fh - 13, cnt, faintOnParch, 1);

    // What will actually happen with this name
    const std::string clean = SoundLibrary::sanitizeName(t);
    SoundLibrary& lib = SoundLibrary::instance();
    std::string hint = "TYPE OR CLICK THE KEYS.   ENTER = OK   ESC = CANCEL";
    uint32_t hintCol = p.muted;
    if (!clean.empty()) {
        const bool bank = modal_.namePurpose == kNameNewBank || modal_.namePurpose == kNameRenameBank;
        const uint32_t except = (modal_.namePurpose == kNameRenameSound || modal_.namePurpose == kNameRenameBank) ? modal_.targetId : 0;
        const std::string finalName = bank ? lib.uniqueBankName(clean, except) : lib.uniqueSoundName(clean, except);
        if (finalName != clean) { hint = "THAT NAME IS TAKEN - IT WILL BE SAVED AS " + finalName; hintCol = p.accent3; }
    }
    ui::textFit(c, ui::kFontLight, x + 22, y + 90, w - 44, hint.c_str(), hintCol);

    // Clickable keys (arcade name entry)
    static const char* kKeyRows[4] = { "1234567890", "QWERTYUIOP", "ASDFGHJKL-", "ZXCVBNM.'&" };
    const int kx0 = x + 42;
    auto keycap = [&](int kx, int ky, int kw, const char* label, int code, int font) {
        const bool hv = hovered(kx, ky, kw, 22);
        const uint32_t f = slot(c, kx, ky, kw, 22, hv ? 2 : 0);
        ui::textCentered(c, font, kx + kw / 2, ky + (22 - ui::capHeight(font)) / 2, label, inkOn(p, f));
        addHit(kx, ky, kw, 22, kHitNameKey, code);
    };
    for (int r = 0; r < 4; ++r) {
        for (int i = 0; kKeyRows[r][i]; ++i) {
            const char ch[2] = { kKeyRows[r][i], 0 };
            keycap(kx0 + i * 40, y + 110 + r * 26, 36, ch, kKeyRows[r][i], ui::kFontTitle);
        }
    }
    keycap(kx0, y + 214, 196, "SPACE", ' ', ui::kFontBody);
    keycap(kx0 + 200, y + 214, 96, "DEL", 8, ui::kFontBody);
    keycap(kx0 + 300, y + 214, 96, "CLEAR", 127, ui::kFontBody);

    divider(c, x + w / 2, y + 248);

    const char* okLabel = modal_.namePurpose == kNameSaveSound ? "SAVE"
                        : modal_.namePurpose == kNameNewBank ? "CREATE" : "RENAME";
    button(c, p, kNameCancelX, kNameBtnY, 120, 30, "CANCEL", false, hovered(kNameCancelX, kNameBtnY, 120, 30));
    button(c, p, kNameOkX, kNameBtnY, 140, 30, okLabel, !clean.empty(), hovered(kNameOkX, kNameBtnY, 140, 30));
    addHit(kNameCancelX, kNameBtnY, 120, 30, kHitModalButton, kBtnCancel);
    addHit(kNameOkX, kNameBtnY, 140, 30, kHitModalButton, kBtnOk);
}

void PixelLabsView::drawConfirmDialog(PixelCanvas& c, const Palette& p) {
    const int x = kConfirmX, y = kConfirmY, w = kConfirmW, h = kConfirmH;
    framePanel(c, x, y, w, h);
    const bool danger = modal_.confirmPurpose != kConfirmUnsaved;
    const ui::SpriteRect& ic = ui::rect(ui::kSprWarning);
    ui::sprite(c, g_skin, ui::kSprWarning, x + 26, y + 18);
    ui::textShadow(c, ui::kFontTitle, x + 26 + ic.w + 10, y + 20, modal_.title.c_str(), danger ? roleColor(p, ui::kRoleRed) : p.accent,
                   p.shadow, 2);
    divider(c, x + w / 2, y + 46);
    ui::textFit(c, ui::kFontBody, x + 26, y + 66, w - 52, modal_.line1.c_str(), p.ink);
    ui::textFit(c, ui::kFontLight, x + 26, y + 82, w - 52, modal_.line2.c_str(), p.muted);
    const int n = static_cast<int>(modal_.buttons.size());
    for (int i = 0; i < n; ++i) {
        const int bx = confirmBtnX(i, n);
        button(c, p, bx, kConfirmBtnY, kConfirmBtnW, 30, dialogButtonLabel(modal_.buttons[i]), i == 0,
               hovered(bx, kConfirmBtnY, kConfirmBtnW, 30));
        addHit(bx, kConfirmBtnY, kConfirmBtnW, 30, kHitModalButton, modal_.buttons[i]);
    }
}

void PixelLabsView::drawBankPicker(PixelCanvas& c, const Palette& p) {
    const int x = kPickX, y = kPickY, w = kPickW, h = kPickH;
    framePanel(c, x, y, w, h);
    ui::textShadow(c, ui::kFontTitle, x + 26, y + 16, "ADD TO BANK", p.accent, p.shadow, 2);
    ui::textFit(c, ui::kFontBody, x + 26, y + 40, w - 52, soundName(modal_.pickerKey).c_str(), p.ink);

    const int nBanks = static_cast<int>(lib_.banks.size());
    const int n = nBanks + 1;
    modal_.pickerScroll = std::clamp(modal_.pickerScroll, 0, std::max(0, n - kPickRows));
    well(c, p, x + 20, kPickRowsTop - 3, w - 40, kPickRows * kPickRowH + 5);
    for (int r = 0; r < kPickRows && modal_.pickerScroll + r < n; ++r) {
        const int idx = modal_.pickerScroll + r;
        const int ry = kPickRowsTop + r * kPickRowH;
        const int rx = x + 24, rw = w - 58, rh = kPickRowH - 2;
        const bool hv = hovered(rx, ry, rw, rh);
        const int ty = ry + (rh - ui::capHeight(ui::kFontBody)) / 2;
        uint32_t face = p.inset;
        if (hv) { ui::hbar(c, g_skin, ui::kSprFlatMid, rx, ry + (rh - kFlatH) / 2, rw, 4); face = faceColor(ui::kSprFlatMid); }
        if (idx == nBanks) {
            ui::text(c, ui::kFontBody, rx + 8, ty, "+ NEW BANK...", accentOn(p, face, hv ? p.accent3 : p.accent2));
            addHit(rx, ry, rw, rh, kHitPickerNew);
            continue;
        }
        const SoundBank& b = lib_.banks[idx];
        const bool in = std::find(b.items.begin(), b.items.end(), modal_.pickerKey) != b.items.end();
        const ui::SpriteRect& rr = ui::rect(ui::kSprRuneGold0);
        ui::sprite(c, g_skin, in ? ui::kSprRuneGold0 : ui::kSprRuneGrey0, rx + 4, ry + (rh - rr.h) / 2);
        char cnt[16];
        std::snprintf(cnt, sizeof(cnt), "%d", static_cast<int>(b.items.size()));
        const int cw = ui::textWidth(ui::kFontBody, cnt);
        ui::text(c, ui::kFontBody, rx + rw - 6 - cw, ty, cnt, mutedOn(p, face));
        ui::textFit(c, ui::kFontBody, rx + 24, ty, rw - 40 - cw, b.name.c_str(), in ? inkOn(p, face) : (hv ? accentOn(p, face, p.accent) : mutedOn(p, face)));
        addHit(rx, ry, rw, rh, kHitPickerBank, static_cast<int>(b.id));
    }
    drawScrollbar(c, p, x + w - 30, kPickRowsTop - 2, kPickRows * kPickRowH + 2, kPickRows, n, modal_.pickerScroll);
    ui::textFit(c, ui::kFontLight, x + 26, kPickRowsTop + kPickRows * kPickRowH + 10, w - 52,
                nBanks ? "CLICK A BANK TO PUT THE SOUND IN OR TAKE IT OUT" : "NO BANKS YET - MAKE ONE WITH + NEW BANK",
                p.muted);
    button(c, p, kPickDoneX, kPickBtnY, 110, 30, "DONE", true, hovered(kPickDoneX, kPickBtnY, 110, 30));
    addHit(kPickDoneX, kPickBtnY, 110, 30, kHitModalButton, kBtnDone);
}

void PixelLabsView::drawSongPicker(PixelCanvas& c, const Palette& p) {
    const int x = kSongX, y = kSongY, w = kSongW, h = kSongH;
    framePanel(c, x, y, w, h);
    ui::textShadow(c, ui::kFontTitle, x + 26, y + 16, "DEMO SONGS", p.accent, p.shadow, 2);
    ui::textRight(c, ui::kFontLight, x + w - 28, y + 22, "THEY PLAY ON THE LOADED SOUND", p.muted);

    const bool playing = telemetry_->demoActive.load(std::memory_order_relaxed) != 0;
    const int playingSong = playing ? telemetry_->demoSong.load(std::memory_order_relaxed) : -1;

    // Group tabs
    for (int g = 0; g < kNumDemoGroups; ++g) {
        const int tx = songTabX(g);
        const bool active = (g == modal_.songGroup);
        const bool hv = hovered(tx, kSongTabY, kSongTabW, 26);
        const uint32_t f = bar(c, tx, kSongTabY, kSongTabW, 26, active, hv, active);
        int ids[kNumDemoSongs];
        char label[32];
        std::snprintf(label, sizeof(label), "%s  %d", demoGroupName(g), demoGroupSongs(g, ids, kNumDemoSongs));
        const int ty = barY(kSongTabY, 26) + (kBarH - ui::capHeight(ui::kFontTitle)) / 2;
        ui::textCentered(c, ui::kFontTitle, tx + kSongTabW / 2, ty, label,
                         active ? inkOn(p, f) : (hv ? accentOn(p, f, p.accent) : mutedOn(p, f)));
        addHit(tx, kSongTabY, kSongTabW, 26, kHitSongGroup, g);
    }

    // Songs in two columns
    int ids[kNumDemoSongs];
    const int n = demoGroupSongs(modal_.songGroup, ids, kNumDemoSongs);
    well(c, p, x + 20, kSongRowsTop - 4, w - 40, kSongRows * kSongRowH + 6);
    for (int i = 0; i < n && i < 2 * kSongRows; ++i) {
        const int col = i / kSongRows, row = i % kSongRows;
        const int rx = songRowX(col), ry = kSongRowsTop + row * kSongRowH;
        const int rw = kSongColW, rh = kSongRowH - 2;
        const bool hv = hovered(rx, ry, rw, rh);
        const bool on = (ids[i] == playingSong);
        uint32_t face = p.inset;
        if (on) { ui::hbar(c, g_skin, ui::kSprFlatGilt, rx, ry + (rh - kFlatH) / 2, rw, 4); face = faceColor(ui::kSprFlatGilt); }
        else if (hv) { ui::hbar(c, g_skin, ui::kSprFlatMid, rx, ry + (rh - kFlatH) / 2, rw, 4); face = faceColor(ui::kSprFlatMid); }
        const int ty = ry + (rh - ui::capHeight(ui::kFontBody)) / 2;
        char num[8];
        std::snprintf(num, sizeof(num), "%02d", i + 1);
        ui::text(c, ui::kFontBody, rx + 8, ty, num, mutedOn(p, face));
        if (on) {
            const bool blink = (frame_ / 10) & 1;
            c.fillRect(rx + rw - 16, ty + 1, 7, 7, blink ? accentOn(p, face, p.accent3) : inkOn(p, face));
        }
        ui::textFit(c, ui::kFontBody, rx + 30, ty, rw - 52, demoSongName(ids[i]),
                    on ? inkOn(p, face) : (hv ? accentOn(p, face, p.accent) : inkOn(p, face)));
        addHit(rx, ry, rw, rh, kHitSongRow, ids[i]);
    }

    ui::textFit(c, ui::kFontLight, x + 26, kSongRowsTop + kSongRows * kSongRowH + 14, w - 52,
                demoGroupHint(modal_.songGroup), p.muted);

    // STOP / DONE
    {
        const bool hv = playing && hovered(kSongStopX, kSongBtnY, 120, 30);
        const uint32_t f = bar(c, kSongStopX, kSongBtnY, 120, 30, false, hv);
        const int ty = barY(kSongBtnY, 30) + (kBarH - ui::capHeight(ui::kFontTitle)) / 2;
        const uint32_t col = playing ? (hv ? accentOn(p, f, p.accent) : inkOn(p, f)) : mutedOn(p, f);
        const int tw = 12 + ui::textWidth(ui::kFontTitle, "STOP");
        const int tx = kSongStopX + (120 - tw) / 2;
        c.fillRect(tx, ty + 1, 8, 8, col);
        ui::text(c, ui::kFontTitle, tx + 12, ty, "STOP", col);
        if (playing) addHit(kSongStopX, kSongBtnY, 120, 30, kHitSongStop);
    }
    button(c, p, kSongDoneX, kSongBtnY, 110, 30, "DONE", true, hovered(kSongDoneX, kSongBtnY, 110, 30));
    addHit(kSongDoneX, kSongBtnY, 110, 30, kHitModalButton, kBtnDone);
}

// =============================================================================
// Input
// =============================================================================

int PixelLabsView::keyAt(int x, int y) const {
    const int top = kKeyboardTop, bottom = kKeyboardBottom;
    if (y < top || y >= bottom) return -1;
    const int nWhite = whiteIndex(kLastKey) + 1;
    if (x < kKeyboardLeft || x >= kKeyboardLeft + nWhite * kWhiteKeyW) return -1;
    if (y < top + 88) {
        for (int n = kFirstKey; n <= kLastKey; ++n) {
            if (!isBlackKey(n)) continue;
            const int kx = kKeyboardLeft + (whiteIndex(n) + 1) * kWhiteKeyW - 8 - 1;
            if (x >= kx && x < kx + 16) return n;
        }
    }
    const int wi = (x - kKeyboardLeft) / kWhiteKeyW;
    for (int n = kFirstKey; n <= kLastKey; ++n) {
        if (!isBlackKey(n) && whiteIndex(n) == wi) return n;
    }
    return -1;
}

// Lower on a key = harder hit, like striking it further from the hinge.
int PixelLabsView::velocityAt(int note, int y) const {
    const int keyH = isBlackKey(note) ? 88 : (kKeyboardBottom - kKeyboardTop);
    const float f = std::clamp(static_cast<float>(y - kKeyboardTop) / static_cast<float>(keyH), 0.0f, 1.0f);
    return std::clamp(static_cast<int>(36.0f + 91.0f * f + 0.5f), 1, 127);
}

void PixelLabsView::handleMouseDown(int x, int y) {
    if (hwnd_) { SetFocus(hwnd_); SetCapture(hwnd_); }
    const Hit* h = hitAt(x, y);
    if (!h || !telemetry_) return;
    const Hit hit = *h; // hits_ may be rebuilt by callbacks
    SoundLibrary& lib = SoundLibrary::instance();
    switch (hit.kind) {
        case kHitSoundRow:   requestLoad(unpackKey(hit.a)); break;
        case kHitPresetPrev: stepPreset(-1); break;
        case kHitPresetNext: stepPreset(+1); break;
        case kHitEraTab:
            if (hit.a == kTabBanks && browserTab() == kTabBanks) openBankId_ = 0; // tab again = back to the list
            telemetry_->eraTab.store(hit.a, std::memory_order_relaxed);
            listScroll_ = 0;
            lastTabSeen_ = -1;
            break;

        // ---- Library ----
        case kHitStar: {
            const SoundKey k = unpackKey(hit.a);
            lib.setFavorite(k, !lib_.isFavorite(k));
            refreshState();
            break;
        }
        case kHitHeaderStar:
            if (cur_.key.kind == kSoundDetached) { showToast("SAVE THE SOUND FIRST, THEN STAR IT"); break; }
            showToast(lib.setFavorite(cur_.key, !lib_.isFavorite(cur_.key)) ? "ADDED TO FAVORITES" : "REMOVED FROM FAVORITES");
            refreshState();
            break;
        case kHitSave:
            if (cur_.key.kind == kSoundUser && lib_.findSound(cur_.key.id)) {
                if (modified_) saveOverwrite();
                else showToast("NO CHANGES TO SAVE");
            } else {
                closeModal();
                openSaveAs(kAfterNothing);
            }
            break;
        case kHitSaveAs:
            closeModal();
            openSaveAs(kAfterNothing);
            break;
        case kHitAddToBank:
            closeModal();
            if (modified_ || cur_.key.kind == kSoundDetached) openSaveAs(kAfterBankPicker);
            else openBankPicker(cur_.key);
            break;
        case kHitSoundRename: {
            const SoundKey k = SoundKey::user(static_cast<uint32_t>(hit.a));
            closeModal();
            openNameDialog(kNameRenameSound, "RENAME SOUND", "NOW CALLED " + soundName(k), soundName(k), k.id);
            break;
        }
        case kHitSoundDelete: {
            const SoundKey k = SoundKey::user(static_cast<uint32_t>(hit.a));
            closeModal();
            modal_.targetId = k.id;
            openConfirm(kConfirmDeleteSound, "DELETE SOUND", "DELETE " + soundName(k) + " FROM MY SOUNDS?",
                        "IT IS ALSO TAKEN OUT OF YOUR FAVORITES AND BANKS.", { kBtnDelete, kBtnCancel });
            break;
        }
        case kHitBankRow:
            openBankId_ = static_cast<uint32_t>(hit.a);
            listScroll_ = 0;
            lastTabSeen_ = -1;
            break;
        case kHitBankBack:
            openBankId_ = 0;
            listScroll_ = 0;
            lastTabSeen_ = -1;
            break;
        case kHitBankNew:
            closeModal();
            openNameDialog(kNameNewBank, "NEW BANK", "GIVE YOUR BANK A NAME", lib.uniqueBankName("MY BANK"), 0);
            break;
        case kHitBankRename: {
            const SoundBank* b = lib_.findBank(static_cast<uint32_t>(hit.a));
            if (!b) break;
            closeModal();
            openNameDialog(kNameRenameBank, "RENAME BANK", "NOW CALLED " + b->name, b->name, b->id);
            break;
        }
        case kHitBankDelete: {
            const SoundBank* b = lib_.findBank(static_cast<uint32_t>(hit.a));
            if (!b) break;
            closeModal();
            modal_.targetId = b->id;
            openConfirm(kConfirmDeleteBank, "DELETE BANK", "DELETE THE BANK " + b->name + "?",
                        "THE SOUNDS IN IT STAY IN YOUR LIBRARY.", { kBtnDelete, kBtnCancel });
            break;
        }
        case kHitBankRemoveItem:
            lib.removeFromBank(openBankId_, unpackKey(hit.a));
            refreshState();
            break;
        case kHitTouchPrev: stepList(kParamTouch, kNumTouchCurves, -1); break;
        case kHitTouchNext: stepList(kParamTouch, kNumTouchCurves, +1); break;

        // ---- Dialogs ----
        case kHitModalBlock: break;
        case kHitModalButton: pressDialogButton(hit.a); break;
        case kHitNameKey: nameInput(hit.a); break;
        case kHitPickerBank: {
            const uint32_t bankId = static_cast<uint32_t>(hit.a);
            if (lib.bankContains(bankId, modal_.pickerKey)) lib.removeFromBank(bankId, modal_.pickerKey);
            else lib.addToBank(bankId, modal_.pickerKey);
            refreshState();
            break;
        }
        case kHitPickerNew: {
            const SoundKey k = modal_.pickerKey;
            modal_ = Modal();
            modal_.backToPicker = true;
            modal_.pickerKey = k;
            openNameDialog(kNameNewBank, "NEW BANK", "IT WILL HOLD " + soundName(k), lib.uniqueBankName("MY BANK"), 0);
            break;
        }
        case kHitMacroBtn:   telemetry_->activeMacro.store(hit.a, std::memory_order_relaxed); break;
        case kHitBigDial: {
            dragKind_ = kHitBigDial;
            dragHit_ = hit;
            dragParam_ = kMacroParamIds[std::clamp(telemetry_->activeMacro.load(), 0, kNumMacros - 1)];
            dragStartY_ = y;
            dragStartVal_ = param(dragParam_);
            break;
        }
        case kHitStepPrev:
        case kHitStepNext:
            for (const auto& s : kSteppers) {
                if (s.param == hit.a) stepList(s.param, s.count, hit.kind == kHitStepPrev ? -1 : 1);
            }
            break;
        case kHitSlider:
            dragKind_ = kHitSlider;
            dragHit_ = hit;
            dragParam_ = hit.a;
            setParam(hit.a, static_cast<float>(x - hit.x) / static_cast<float>(hit.w - 1));
            break;
        case kHitKeyboard: {
            const int k = keyAt(x, y);
            if (k >= 0) {
                dragKind_ = kHitKeyboard;
                mouseNote_ = k;
                mouseVel_ = velocityAt(k, y);
                playGuiNote(k, true, mouseVel_);
            }
            break;
        }
        case kHitDemo: {
            const int cur = telemetry_->demoActive.load(std::memory_order_relaxed);
            telemetry_->demoActive.store(cur ? 0 : 1, std::memory_order_release);
            break;
        }
        case kHitDemoList: openSongPicker(); break;
        case kHitSongGroup: modal_.songGroup = std::clamp(hit.a, 0, kNumDemoGroups - 1); break;
        case kHitSongRow: playDemoSong(hit.a); break;
        case kHitSongStop: telemetry_->demoActive.store(0, std::memory_order_release); break;
        case kHitTheme: {
            const int t = telemetry_->theme.load(std::memory_order_relaxed);
            telemetry_->theme.store((t + 1) % kNumThemes, std::memory_order_relaxed);
            break;
        }
        case kHitZoom: {
            const int s = telemetry_->uiScale.load(std::memory_order_relaxed);
            telemetry_->uiScale.store((s + 1) % kNumUiScales, std::memory_order_relaxed);
            requestResize();
            break;
        }
        case kHitOctDown: kbdOctave_ = std::max(1, kbdOctave_ - 1); break;
        case kHitOctUp:   kbdOctave_ = std::min(7, kbdOctave_ + 1); break;
        default: break;
    }
}

void PixelLabsView::handleMouseMove(int x, int y) {
    hoverX_ = x;
    hoverY_ = y;
    switch (dragKind_) {
        case kHitBigDial:
            setParam(dragParam_, dragStartVal_ + (dragStartY_ - y) / 200.0f);
            break;
        case kHitSlider:
            setParam(dragParam_, static_cast<float>(x - dragHit_.x) / static_cast<float>(dragHit_.w - 1));
            break;
        case kHitKeyboard: {
            const int k = keyAt(x, y);
            if (k != mouseNote_) {
                if (mouseNote_ >= 0) playGuiNote(mouseNote_, false);
                mouseNote_ = k;
                if (k >= 0) { mouseVel_ = velocityAt(k, y); playGuiNote(k, true, mouseVel_); }
            }
            break;
        }
        default: break;
    }
}

void PixelLabsView::handleMouseUp(int, int) {
    if (GetCapture() == hwnd_) ReleaseCapture();
    if (mouseNote_ >= 0) playGuiNote(mouseNote_, false);
    mouseNote_ = -1;
    dragKind_ = kHitNone;
    dragParam_ = -1;
}

void PixelLabsView::handleDoubleClick(int x, int y) {
    const Hit* h = hitAt(x, y);
    if (h && telemetry_ && modal_.kind == kModalNone && (h->kind == kHitSlider || h->kind == kHitBigDial)) {
        const int pid = (h->kind == kHitSlider) ? h->a
                        : kMacroParamIds[std::clamp(telemetry_->activeMacro.load(), 0, kNumMacros - 1)];
        // Back to the stored sound's value (velocity sensitivity is a keyboard setting: its default)
        setParam(pid, pid == kParamVelSens ? kDefaultVelSens : cur_.ref[pid]);
        return;
    }
    handleMouseDown(x, y); // buttons, keys and dialog keycaps still react to fast clicks
}

void PixelLabsView::handleWheel(int x, int y, float steps) {
    if (!telemetry_) return;
    if (modal_.kind != kModalNone) {
        if (modal_.kind == kModalBankPicker) modal_.pickerScroll -= static_cast<int>(steps * 2.0f);
        return;
    }
    const Hit* h = (x >= 0) ? hitAt(x, y) : nullptr;
    const int kind = h ? h->kind : kHitNone;
    const bool inList = x >= kListX && x < kListX + kListW && y >= kRowsTop - 2 && y < kRowsTop + kVisibleRows * kRowH + 2;
    if (inList || kind == kHitPresetList) {
        listScroll_ -= static_cast<int>(steps * 2.0f); // clamped when the list is drawn
        if (listScroll_ < 0) listScroll_ = 0;
        return;
    }
    if (kind == kHitTouchPrev || kind == kHitTouchNext) { stepList(kParamTouch, kNumTouchCurves, steps > 0 ? 1 : -1); return; }
    if (kind == kHitSlider) { setParam(h->a, param(h->a) + steps * 0.02f); return; }
    if (kind == kHitStepPrev || kind == kHitStepNext) {
        for (const auto& s : kSteppers) if (s.param == h->a) stepList(s.param, s.count, steps > 0 ? 1 : -1);
        return;
    }
    if (kind == kHitPresetPrev || kind == kHitPresetNext) { stepPreset(steps > 0 ? -1 : 1); return; }
    if (kind == kHitMacroBtn) { setParam(kMacroParamIds[h->a], param(kMacroParamIds[h->a]) + steps * 0.03f); return; }
    if (kind == kHitKeyboard) return;
    const int pid = kMacroParamIds[std::clamp(telemetry_->activeMacro.load(), 0, kNumMacros - 1)];
    setParam(pid, param(pid) + steps * 0.03f);
}

bool PixelLabsView::handleModalKey(int vk) {
    switch (vk) {
        case VK_ESCAPE:
            pressDialogButton((modal_.kind == kModalBankPicker || modal_.kind == kModalSongPicker) ? kBtnDone : kBtnCancel);
            return true;
        case VK_RETURN:
            if (modal_.kind == kModalName) pressDialogButton(kBtnOk);
            else if (modal_.kind == kModalConfirm && !modal_.buttons.empty()) pressDialogButton(modal_.buttons[0]);
            else pressDialogButton(kBtnDone);
            return true;
        case VK_BACK:   nameInput(8); return true;
        case VK_DELETE: nameInput(127); return true;
        default: return false;
    }
}

// Character for a key press when the host gives us WM_KEYDOWN without a WM_CHAR.
static int charFromVk(int vk) {
    if ((vk >= 'A' && vk <= 'Z') || (vk >= '0' && vk <= '9')) return vk;
    if (vk >= VK_NUMPAD0 && vk <= VK_NUMPAD9) return '0' + (vk - VK_NUMPAD0);
    switch (vk) {
        case VK_SPACE:      return ' ';
        case VK_OEM_MINUS:
        case VK_SUBTRACT:   return '-';
        case VK_OEM_PERIOD:
        case VK_DECIMAL:    return '.';
        case VK_OEM_COMMA:  return ',';
        default:            return 0;
    }
}

bool PixelLabsView::handleKey(int vk, bool down) {
    if (vk < 0 || vk > 255) return false;
    if (modal_.kind != kModalNone) {
        if (down) handleModalKey(vk);
        return true; // never play notes or let host shortcuts fire behind a dialog
    }
    static const struct { int vk; int semis; } kMap[] = {
        { 'A', 0 }, { 'W', 1 }, { 'S', 2 }, { 'E', 3 }, { 'D', 4 }, { 'F', 5 }, { 'T', 6 }, { 'G', 7 },
        { 'Y', 8 }, { 'H', 9 }, { 'U', 10 }, { 'J', 11 }, { 'K', 12 }, { 'O', 13 }, { 'L', 14 },
        { 'P', 15 }, { VK_OEM_1, 16 }
    };
    if (down && vk == 'Z') { kbdOctave_ = std::max(1, kbdOctave_ - 1); return true; }
    if (down && vk == 'X') { kbdOctave_ = std::min(7, kbdOctave_ + 1); return true; }
    if (vk == 'Z' || vk == 'X') return true;
    for (const auto& m : kMap) {
        if (m.vk != vk) continue;
        if (down) {
            if (kbdHeld_[vk]) return true; // auto-repeat
            const int note = std::clamp((kbdOctave_ + 1) * 12 + m.semis, 0, 127);
            kbdHeld_[vk] = note + 1;
            playGuiNote(note, true);
        } else if (kbdHeld_[vk]) {
            playGuiNote(kbdHeld_[vk] - 1, false);
            kbdHeld_[vk] = 0;
        }
        return true;
    }
    return false;
}

LRESULT CALLBACK PixelLabsView::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    PixelLabsView* self = nullptr;
    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = static_cast<PixelLabsView*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    } else {
        self = reinterpret_cast<PixelLabsView*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }
    if (!self) return DefWindowProcW(hwnd, msg, wParam, lParam);

    const int z = self->zoom();
    auto lx = [&](LPARAM lp) { return GET_X_LPARAM(lp) * 100 / z; };
    auto ly = [&](LPARAM lp) { return GET_Y_LPARAM(lp) * 100 / z; };

    switch (msg) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            if (self->memDC_) {
                if (z == 100) {
                    BitBlt(hdc, 0, 0, kBaseWidth, kBaseHeight, self->memDC_, 0, 0, SRCCOPY);
                } else {
                    SetStretchBltMode(hdc, COLORONCOLOR); // nearest neighbour keeps pixels crisp
                    StretchBlt(hdc, 0, 0, kBaseWidth * z / 100, kBaseHeight * z / 100,
                               self->memDC_, 0, 0, kBaseWidth, kBaseHeight, SRCCOPY);
                }
            }
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_ERASEBKGND:
            return 1;
        case WM_TIMER:
            if (wParam == kTimerId) {
                self->renderUI();
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            return 0;
        case WM_LBUTTONDOWN:
            self->handleMouseDown(lx(lParam), ly(lParam));
            return 0;
        case WM_LBUTTONDBLCLK:
            self->handleDoubleClick(lx(lParam), ly(lParam));
            return 0;
        case WM_MOUSEMOVE: {
            TRACKMOUSEEVENT tme = { sizeof(TRACKMOUSEEVENT), TME_LEAVE, hwnd, 0 };
            TrackMouseEvent(&tme);
            self->handleMouseMove(lx(lParam), ly(lParam));
            return 0;
        }
        case WM_MOUSELEAVE:
            if (self->dragKind_ == kHitNone) { self->hoverX_ = -1; self->hoverY_ = -1; }
            return 0;
        case WM_LBUTTONUP:
            self->handleMouseUp(lx(lParam), ly(lParam));
            return 0;
        case WM_CAPTURECHANGED:
            if (reinterpret_cast<HWND>(lParam) != hwnd && self->dragKind_ != kHitNone) self->handleMouseUp(0, 0);
            return 0;
        case WM_MOUSEWHEEL: {
            POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            ScreenToClient(hwnd, &pt);
            self->handleWheel(pt.x * 100 / z, pt.y * 100 / z,
                              static_cast<float>(GET_WHEEL_DELTA_WPARAM(wParam)) / WHEEL_DELTA);
            return 0;
        }
        case WM_GETDLGCODE:
            return DLGC_WANTALLKEYS | DLGC_WANTCHARS;
        case WM_KEYDOWN:
            if (self->modal_.kind != kModalNone) {
                const uint64_t now = GetTickCount64();
                if (now - self->lastHostKeyMs_ <= kKeyEchoMs) return 0; // already delivered through onKeyDown
                self->lastNativeKeyMs_ = now;
                const int vk = static_cast<int>(wParam);
                if (!self->handleModalKey(vk) && self->modal_.kind == kModalName) {
                    // Letters normally arrive as WM_CHAR; type from the key code only if none is queued
                    MSG m;
                    if (!PeekMessageW(&m, hwnd, WM_CHAR, WM_CHAR, PM_NOREMOVE)) {
                        const int ch = charFromVk(vk);
                        if (ch) self->nameInput(ch);
                    }
                }
                return 0;
            }
            if (self->handleKey(static_cast<int>(wParam), true)) return 0;
            break;
        case WM_CHAR:
            if (self->modal_.kind == kModalName) {
                const uint64_t now = GetTickCount64();
                if (now - self->lastHostKeyMs_ <= kKeyEchoMs) return 0;
                self->lastNativeKeyMs_ = now;
                if (wParam >= 32) self->handleChar(static_cast<int>(wParam), false);
                return 0;
            }
            if (self->modal_.kind != kModalNone) return 0;
            break;
        case WM_KEYUP:
            if (self->handleKey(static_cast<int>(wParam), false)) return 0;
            break;
        case WM_KILLFOCUS:
            for (int vk = 0; vk < 256; ++vk) {
                if (self->kbdHeld_[vk]) { self->playGuiNote(self->kbdHeld_[vk] - 1, false); self->kbdHeld_[vk] = 0; }
            }
            return 0;
        default:
            break;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

} // namespace PixelLabs
