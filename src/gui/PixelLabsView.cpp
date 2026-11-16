#include "PixelLabsView.h"
#include "PixelFont5x7.h"
#include "../presets/FactoryPresets.h"
#include "../plugin/ParamNames.h"
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

struct Palette {
    const char* name;
    uint32_t bg, panel, inset, border, shadow;
    uint32_t ink, muted, accent, accent2, accent3;
    uint32_t keyWhite, keyWhiteShade, keyWhiteInk, keyBlack, keyBlackInk, keyHit, keyHitInk;
    uint8_t scanAlpha;
};

static const char* kThemeNames[kNumThemes] = { "AUTO", "NES", "DMG", "NEON", "CODEX", "ARCADE", "PAPER" };

static const Palette kPalettes[kNumThemes] = {
    // 0 AUTO placeholder (resolved per era)
    { "AUTO", 0,0,0,0,0, 0,0,0,0,0, 0,0,0,0,0,0,0, 0 },
    // 1 NES - console grey, red and sky
    { "NES",
      0xFF111116, 0xFF1D1D25, 0xFF0A0A0E, 0xFF3D3D4C, 0xFF050507,
      0xFFF4F4F4, 0xFF8E8EA0, 0xFFE8382F, 0xFF3CBCFC, 0xFFFCB040,
      0xFFEDEDED, 0xFFB8B8C2, 0xFF2A2A33, 0xFF1A1A20, 0xFF9A9AAA, 0xFFE8382F, 0xFFFFFFFF, 60 },
    // 2 DMG - four shades of handheld green
    { "DMG",
      0xFF0F380F, 0xFF1C4A1C, 0xFF0A2A0A, 0xFF4E7A1E, 0xFF061F06,
      0xFF9BBC0F, 0xFF6E9420, 0xFFC6E25A, 0xFF8BAC0F, 0xFFE0F08A,
      0xFF9BBC0F, 0xFF6E9420, 0xFF0F380F, 0xFF0F380F, 0xFF8BAC0F, 0xFF306230, 0xFFC6E25A, 70 },
    // 3 NEON - synthwave night
    { "NEON",
      0xFF0D0221, 0xFF1A0B3B, 0xFF07011A, 0xFF46207F, 0xFF04000E,
      0xFFF7EEFF, 0xFFA28BCB, 0xFFFF2E97, 0xFF00F0FF, 0xFFFFD319,
      0xFFEDE6FF, 0xFFB7A6E0, 0xFF2A1250, 0xFF170732, 0xFFB08CFF, 0xFFFF2E97, 0xFFFFFFFF, 70 },
    // 4 CODEX - illuminated manuscript
    { "CODEX",
      0xFF1A120B, 0xFF2C1F13, 0xFF120C07, 0xFF6B4E2A, 0xFF0A0603,
      0xFFF3E6C8, 0xFFB0946A, 0xFFE0AE3C, 0xFFC8362B, 0xFF4F8CC4,
      0xFFEFE1C0, 0xFFC7B48C, 0xFF3A2A16, 0xFF24170C, 0xFFC7A46A, 0xFFE0AE3C, 0xFF1A120B, 50 },
    // 5 ARCADE - black cabinet, maze blue, marquee yellow
    { "ARCADE",
      0xFF000000, 0xFF0B0B18, 0xFF000000, 0xFF2626E0, 0xFF000000,
      0xFFFFFFFF, 0xFF8A8AB0, 0xFFFFE600, 0xFF00E5FF, 0xFFFF4FB0,
      0xFFF2F2F2, 0xFFB0B0C8, 0xFF101018, 0xFF12121E, 0xFF8A8AB0, 0xFFFFE600, 0xFF000000, 80 },
    // 6 PAPER - light studio skin
    { "PAPER",
      0xFFEAE3D2, 0xFFF8F4EA, 0xFFE1D7C2, 0xFF2B2B2B, 0xFFBDB29B,
      0xFF1C1C1C, 0xFF6A6252, 0xFFE0502A, 0xFF2479A3, 0xFF5E8A2C,
      0xFFFFFFFF, 0xFFD8D0BE, 0xFF1C1C1C, 0xFF222222, 0xFFBDB6A6, 0xFFE0502A, 0xFFFFFFFF, 18 }
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
        case kEraChip90:   return 2;
        case kEraSynth:    return 3;
        case kEraMedieval: return 4;
        case kEraSfx:      return 5;
        default:           return 1;
    }
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

static const char* kDrumNames[12] = {
    "KICK", "RIM", "SNARE", "CLAP", "TIGHT SN", "LO TOM", "C.HAT", "MID TOM", "O.HAT", "HI TOM", "CRASH", "COWBELL"
};
static const char* kDrumKeyLabels[12] = {
    "KIK", "RM", "SNR", "CP", "TSN", "LTM", "CH", "MTM", "OH", "HTM", "CR", "BEL"
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

static void panel(PixelCanvas& c, const Palette& p, int x, int y, int w, int h) {
    c.fillRect(x + 3, y + 3, w, h, p.shadow);
    c.fillPixelBox(x, y, w, h, p.panel, p.border, 2);
}

static void insetBox(PixelCanvas& c, const Palette& p, int x, int y, int w, int h) {
    c.fillRect(x, y, w, h, p.inset);
    c.fillRect(x, y, w, 1, p.shadow);
    c.fillRect(x, y, 1, h, p.shadow);
}

// Draws text, shortening it with ".." when it is wider than maxW.
static void textFit(PixelCanvas& c, int x, int y, int maxW, const char* text, uint32_t col, int scale = 1) {
    char buf[96];
    std::snprintf(buf, sizeof(buf), "%s", text ? text : "");
    if (measurePixelText(buf, scale) > maxW) {
        size_t n = std::strlen(buf);
        while (n > 2) {
            buf[n - 1] = '\0';
            --n;
            char tmp[100];
            std::snprintf(tmp, sizeof(tmp), "%s..", buf);
            if (measurePixelText(tmp, scale) <= maxW) { std::snprintf(buf, sizeof(buf), "%s", tmp); break; }
        }
    }
    drawPixelText(c, x, y, buf, col, scale);
}

static void triangle(PixelCanvas& c, int cx, int cy, int size, int dir, uint32_t col) {
    // dir: -1 points left, +1 points right
    for (int i = 0; i < size; ++i) {
        const int h = (dir < 0) ? i * 2 + 1 : (size - i) * 2 - 1; // tip on the pointing side
        c.fillRect(cx - size / 2 + i, cy - h / 2, 1, h, col);
    }
}

static void button(PixelCanvas& c, const Palette& p, int x, int y, int w, int h,
                   const char* label, bool active, bool hover, int scale = 1) {
    if (active) {
        c.fillRect(x + 2, y + 2, w, h, p.shadow);
        c.fillPixelBox(x, y, w, h, p.accent, p.accent, 2);
    } else {
        c.fillRect(x + 2, y + 2, w, h, p.shadow);
        c.fillPixelBox(x, y, w, h, hover ? p.inset : p.panel, hover ? p.accent : p.border, 2);
    }
    const int th = 7 * scale;
    drawPixelTextCentered(c, x + w / 2, y + (h - th) / 2, label, active ? p.bg : p.ink, scale);
}

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

PixelLabsView::PixelLabsView(SharedUITelemetry* telemetry,
                             std::function<void(int paramId, float normVal)> onParamChange,
                             std::function<void(int presetIdx)> onPresetChange)
    : telemetry_(telemetry),
      onParamChange_(std::move(onParamChange)),
      onPresetChange_(std::move(onPresetChange)) {
    hits_.reserve(256);
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

Steinberg::tresult PLUGIN_API PixelLabsView::onKeyDown(Steinberg::char16 key, Steinberg::int16 keyCode, Steinberg::int16) {
    return handleKey(vkFromChar(key, keyCode), true) ? Steinberg::kResultTrue : Steinberg::kResultFalse;
}

Steinberg::tresult PLUGIN_API PixelLabsView::onKeyUp(Steinberg::char16 key, Steinberg::int16 keyCode, Steinberg::int16) {
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
    if (onParamChange_) onParamChange_(id, v);
}

void PixelLabsView::stepList(int paramId, int count, int dir) {
    int idx = normToList(param(paramId), count);
    idx = (idx + dir + count) % count;
    setParam(paramId, listToNorm(idx, count));
}

std::vector<int> PixelLabsView::filteredPresets() const {
    std::vector<int> out;
    const int tab = telemetry_ ? telemetry_->eraTab.load(std::memory_order_relaxed) : 0;
    for (int i = 0; i < kNumFactoryPresets; ++i) {
        if (tab == 0 || factoryPreset(i).era == tab - 1) out.push_back(i);
    }
    return out;
}

void PixelLabsView::stepPreset(int dir) {
    if (!telemetry_ || !onPresetChange_) return;
    const std::vector<int> list = filteredPresets();
    if (list.empty()) return;
    const int cur = telemetry_->currentPreset.load(std::memory_order_relaxed);
    auto it = std::find(list.begin(), list.end(), cur);
    int pos = 0;
    if (it != list.end()) {
        const int n = static_cast<int>(list.size());
        pos = (static_cast<int>(it - list.begin()) + dir + n) % n;
    }
    onPresetChange_(list[pos]);
}

int PixelLabsView::currentEra() const {
    return telemetry_ ? factoryPreset(telemetry_->currentPreset.load(std::memory_order_relaxed)).era : kEraChip80;
}

void PixelLabsView::playGuiNote(int note, bool on) {
    if (!telemetry_ || note < 0 || note > 127) return;
    telemetry_->pushGuiNote(note, on ? 100 : 0);
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
    return hoverX_ >= x && hoverX_ < x + w && hoverY_ >= y && hoverY_ < y + h && dragKind_ == kHitNone;
}

// =============================================================================
// Rendering
// =============================================================================

void PixelLabsView::renderUI() {
    if (!pixelBuffer_ || !telemetry_) return;
    ++frame_;
    hits_.clear();

    PixelCanvas c;
    c.pixels = pixelBuffer_;
    c.width = kBaseWidth;
    c.height = kBaseHeight;

    int theme = std::clamp(telemetry_->theme.load(std::memory_order_relaxed), 0, kNumThemes - 1);
    if (theme == 0) theme = autoThemeForEra(currentEra());
    const Palette& p = kPalettes[theme];

    c.fillRect(0, 0, kBaseWidth, kBaseHeight, p.bg);
    // Faint dot grid on the background
    for (int y = 4; y < kBaseHeight; y += 8)
        for (int x = 4; x < kBaseWidth; x += 8) c.blendPixel(x, y, p.border, 90);

    drawHeader(c, p);
    drawEraTabs(c, p);
    drawPresetList(c, p);
    drawHero(c, p);
    drawSoundPanel(c, p);
    drawStatusStrip(c, p);
    drawKeyboard(c, p);
}

void PixelLabsView::drawHeader(PixelCanvas& c, const Palette& p) {
    // Logo with a hard pixel drop shadow
    drawPixelText(c, 18, 12, "PIXEL", p.shadow, 3);
    drawPixelText(c, 16, 10, "PIXEL", p.accent, 3);
    const int lx = 16 + measurePixelText("PIXEL ", 3);
    drawPixelText(c, lx + 2, 12, "LABS", p.shadow, 3);
    drawPixelText(c, lx, 10, "LABS", p.ink, 3);
    drawPixelText(c, 16, 38, "RETRO INSTRUMENT LAB  V2", p.muted, 1);

    // Preset display
    const int bx = 236, by = 8, bw = 392, bh = 40;
    panel(c, p, bx, by, bw, bh);
    insetBox(c, p, bx + 32, by + 4, bw - 64, bh - 8);
    const int cur = telemetry_->currentPreset.load(std::memory_order_relaxed);
    const FactoryPreset& fp = factoryPreset(cur);

    const bool hPrev = hovered(bx, by, 30, bh), hNext = hovered(bx + bw - 30, by, 30, bh);
    triangle(c, bx + 16, by + bh / 2, 7, -1, hPrev ? p.accent : p.ink);
    triangle(c, bx + bw - 16, by + bh / 2, 7, 1, hNext ? p.accent : p.ink);
    addHit(bx, by, 30, bh, kHitPresetPrev);
    addHit(bx + bw - 30, by, 30, bh, kHitPresetNext);

    char nameBuf[64];
    std::snprintf(nameBuf, sizeof(nameBuf), "%s", fp.name);
    int nameScale = measurePixelText(nameBuf, 2) <= bw - 80 ? 2 : 1;
    drawPixelTextCentered(c, bx + bw / 2, by + 9, nameBuf, p.ink, nameScale);
    char sub[96];
    std::snprintf(sub, sizeof(sub), "%02d/%02d  %s  /  %s", cur + 1, kNumFactoryPresets, kEraNames[fp.era], fp.category);
    drawPixelTextCentered(c, bx + bw / 2 + 6, by + 28, sub, p.muted, 1);
    c.fillRect(bx + bw / 2 + 6 - measurePixelText(sub, 1) / 2 - 12, by + 28, 7, 7, kEraColors[fp.era]);

    // Demo button
    const bool demo = telemetry_->demoActive.load(std::memory_order_relaxed) != 0;
    {
        const int x = 640, y = 10, w = 120, h = 34;
        const bool hv = hovered(x, y, w, h);
        const bool blink = demo && ((frame_ / 10) & 1);
        button(c, p, x, y, w, h, "", demo, hv);
        const uint32_t col = demo ? p.bg : p.ink;
        if (demo) {
            c.fillRect(x + 12, y + 12, 9, 9, blink ? p.accent2 : col);
            drawPixelText(c, x + 28, y + 13, "STOP DEMO", col, 1);
        } else {
            triangle(c, x + 16, y + 17, 6, 1, p.accent);
            drawPixelText(c, x + 28, y + 13, "PLAY DEMO", col, 1);
        }
        addHit(x, y, w, h, kHitDemo);
    }
    // Skin button
    {
        const int x = 768, y = 10, w = 90, h = 34;
        const int theme = telemetry_->theme.load(std::memory_order_relaxed);
        char buf[32];
        std::snprintf(buf, sizeof(buf), "SKIN %s", kThemeNames[std::clamp(theme, 0, kNumThemes - 1)]);
        button(c, p, x, y, w, h, buf, false, hovered(x, y, w, h));
        addHit(x, y, w, h, kHitTheme);
    }
    // Zoom button
    {
        const int x = 866, y = 10, w = 78, h = 34;
        char buf[32];
        std::snprintf(buf, sizeof(buf), "ZOOM %d%%", zoom());
        button(c, p, x, y, w, h, buf, false, hovered(x, y, w, h));
        addHit(x, y, w, h, kHitZoom);
    }
}

void PixelLabsView::drawEraTabs(PixelCanvas& c, const Palette& p) {
    static const char* kTabNames[kNumEras + 1] = { "ALL", "80S CHIP", "90S CHIP", "SYNTH 80S", "MEDIEVAL", "SFX + KIT" };
    const int tab = telemetry_->eraTab.load(std::memory_order_relaxed);
    const int y = 58, h = 24, w = 151, gap = 4;
    for (int t = 0; t <= kNumEras; ++t) {
        const int x = 16 + t * (w + gap);
        const bool active = (t == tab);
        const bool hv = hovered(x, y, w, h);
        c.fillRect(x + 2, y + 2, w, h, p.shadow);
        c.fillPixelBox(x, y, w, h, active ? p.accent : (hv ? p.inset : p.panel), active ? p.accent : (hv ? p.accent : p.border), 2);
        const int tw = measurePixelText(kTabNames[t], 1);
        const int tx = x + (w - tw) / 2 + (t > 0 ? 6 : 0);
        if (t > 0) c.fillRect(tx - 12, y + 9, 7, 7, kEraColors[t - 1]);
        drawPixelText(c, tx, y + 9, kTabNames[t], active ? p.bg : p.ink, 1);
        addHit(x, y, w, h, kHitEraTab, t);
    }
}

void PixelLabsView::drawPresetList(PixelCanvas& c, const Palette& p) {
    const int x = 16, y = 90, w = 240, h = 310;
    panel(c, p, x, y, w, h);
    const std::vector<int> list = filteredPresets();
    const int cur = telemetry_->currentPreset.load(std::memory_order_relaxed);
    const int tab = telemetry_->eraTab.load(std::memory_order_relaxed);

    char title[32];
    std::snprintf(title, sizeof(title), "PRESETS  %d", static_cast<int>(list.size()));
    drawPixelText(c, x + 10, y + 8, title, p.muted, 1);
    drawPixelText(c, x + w - 10 - measurePixelText("WHEEL", 1), y + 8, "WHEEL", p.border, 1);

    const int rowsTop = y + 22, rowH = 19, visible = 14;
    const int n = static_cast<int>(list.size());
    const int maxScroll = std::max(0, n - visible);

    // Keep the current preset in view when it changes or the tab changes
    if (cur != lastPresetSeen_ || tab != lastTabSeen_) {
        auto it = std::find(list.begin(), list.end(), cur);
        if (it != list.end()) {
            const int pos = static_cast<int>(it - list.begin());
            if (pos < listScroll_) listScroll_ = pos;
            if (pos >= listScroll_ + visible) listScroll_ = pos - visible + 1;
        }
        lastPresetSeen_ = cur;
        lastTabSeen_ = tab;
    }
    listScroll_ = std::clamp(listScroll_, 0, maxScroll);

    insetBox(c, p, x + 6, rowsTop - 2, w - 12, visible * rowH + 4);
    addHit(x, y, w, h, kHitPresetList);
    for (int r = 0; r < visible && listScroll_ + r < n; ++r) {
        const int idx = list[listScroll_ + r];
        const FactoryPreset& fp = factoryPreset(idx);
        const int ry = rowsTop + r * rowH;
        const int rx = x + 8, rw = w - 24;
        const bool sel = (idx == cur);
        const bool hv = hovered(rx, ry, rw, rowH - 1);
        if (sel) c.fillRect(rx, ry, rw, rowH - 1, p.accent);
        else if (hv) c.fillRect(rx, ry, rw, rowH - 1, p.panel);
        c.fillRect(rx + 2, ry + 3, 3, rowH - 7, kEraColors[fp.era]);
        char num[8];
        std::snprintf(num, sizeof(num), "%02d", idx + 1);
        drawPixelText(c, rx + 9, ry + 6, num, sel ? p.bg : p.muted, 1);
        textFit(c, rx + 26, ry + 6, rw - 30, fp.name, sel ? p.bg : p.ink, 1);
        addHit(rx, ry, rw, rowH - 1, kHitPresetRow, idx);
    }

    // Scrollbar
    if (maxScroll > 0) {
        const int sx = x + w - 13, sy = rowsTop, sh = visible * rowH;
        c.fillRect(sx, sy, 3, sh, p.border);
        const int th = std::max(12, sh * visible / n);
        const int ty = sy + (sh - th) * listScroll_ / maxScroll;
        c.fillRect(sx - 1, ty, 5, th, p.accent2);
    }
}

void PixelLabsView::drawHero(PixelCanvas& c, const Palette& p) {
    const int x = 266, y = 90, w = 364, h = 310;
    panel(c, p, x, y, w, h);

    // ---- Oscilloscope ----
    const int sx = x + 10, sy = y + 10, sw = w - 20, sh = 62;
    insetBox(c, p, sx, sy, sw, sh);
    for (int gx = sx + 8; gx < sx + sw; gx += 16) c.blendPixel(gx, sy + sh / 2, p.border, 200);
    for (int gy = sy + 6; gy < sy + sh; gy += 8) c.blendPixel(sx + sw / 2, gy, p.border, 200);
    {
        float snap[256];
        const uint32_t wIdx = telemetry_->scopeWriteIdx.load(std::memory_order_relaxed);
        for (int i = 0; i < 256; ++i) snap[i] = telemetry_->scopeSamples[(wIdx + i) & 255];
        int start = 0;
        for (int i = 1; i < 128; ++i) {
            if (snap[i - 1] <= 0.0f && snap[i] > 0.0f) { start = i; break; }
        }
        int prevX = sx, prevY = sy + sh / 2;
        for (int i = 0; i < 128; ++i) {
            const float v = std::clamp(snap[start + i] * 1.6f, -1.0f, 1.0f);
            const int px = sx + 2 + i * (sw - 4) / 127;
            const int py = sy + sh / 2 - static_cast<int>(v * (sh / 2 - 4));
            if (i > 0) c.drawLine(prevX, prevY, px, py, p.accent2, 2);
            prevX = px;
            prevY = py;
        }
    }
    drawPixelText(c, sx + 6, sy + 5, "SCOPE", p.muted, 1);
    {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%.0f BPM", telemetry_->tempoBpm.load(std::memory_order_relaxed));
        drawPixelText(c, sx + sw - 6 - measurePixelText(buf, 1), sy + 5, buf, p.muted, 1);
    }
    scanlines(c, sx, sy, sw, sh, p.scanAlpha);

    // ---- Big LED macro dial ----
    const int macro = std::clamp(telemetry_->activeMacro.load(std::memory_order_relaxed), 0, kNumMacros - 1);
    const int macroParam = kMacroParamIds[macro];
    const float mv = param(macroParam);
    const int cx = x + w / 2, cy = y + 146, r = 56;
    const bool dialHover = hovered(cx - 70, cy - 70, 140, 140) || dragKind_ == kHitBigDial;
    c.fillCircle(cx, cy, r + 8, p.inset, 2);
    c.drawPixelCircle(cx, cy, r + 9, dialHover ? p.accent : p.border, 2, 2);
    const int segs = 27;
    const int lit = static_cast<int>(std::lround(mv * (segs - 1)));
    for (int i = 0; i < segs; ++i) {
        const double ang = (225.0 - i * (270.0 / (segs - 1))) * 3.14159265358979 / 180.0;
        const int px = cx + static_cast<int>(std::lround(std::cos(ang) * (r - 4)));
        const int py = cy - static_cast<int>(std::lround(std::sin(ang) * (r - 4)));
        const bool on = (mv > 0.001f) && i <= lit;
        if (on) blendRect(c, px - 6, py - 6, 13, 13, p.accent, 45);
        c.fillRect(px - 3, py - 3, 7, 7, on ? (i == lit ? p.ink : p.accent) : p.border);
    }
    c.drawPixelCircle(cx, cy, r - 18, p.border, 2, 2);
    char valBuf[32];
    formatParamValue(macroParam, mv, valBuf, sizeof(valBuf));
    const int vScale = measurePixelText(valBuf, 3) <= 70 ? 3 : 2;
    drawPixelTextCentered(c, cx, cy - (7 * vScale) / 2, valBuf, p.ink, vScale);
    scanlines(c, cx - r - 10, cy - r - 10, 2 * r + 20, 2 * r + 20, p.scanAlpha / 3);
    addHit(cx - 70, cy - 70, 140, 140, kHitBigDial);

    char mname[48];
    if (macroParam == kParamReverb)
        std::snprintf(mname, sizeof(mname), "%s / %s", kMacroNames[macro], reverbTypeName(normToList(param(kParamReverbType), kNumReverbTypes)));
    else if (macroParam == kParamDelay)
        std::snprintf(mname, sizeof(mname), "%s / %s", kMacroNames[macro], delayTypeName(normToList(param(kParamDelayType), kNumDelayTypes)));
    else
        std::snprintf(mname, sizeof(mname), "%s", kMacroNames[macro]);
    drawPixelTextCentered(c, cx, cy + r + 14, mname, p.accent, measurePixelText(mname, 2) <= w - 20 ? 2 : 1);

    // ---- Output meters (left of the dial) ----
    auto meter = [&](int mx, float peak, const char* label) {
        const int my = cy - 60, mh = 120, nSeg = 15;
        const float db = 20.0f * std::log10(std::max(peak, 1e-5f));
        const int on = std::clamp(static_cast<int>((db + 48.0f) / 48.0f * nSeg + 0.5f), 0, nSeg);
        for (int i = 0; i < nSeg; ++i) {
            const int segY = my + mh - (i + 1) * 8;
            uint32_t col = p.border;
            if (i < on) col = (i >= nSeg - 2) ? p.accent : (i >= nSeg - 5 ? p.accent3 : p.accent2);
            c.fillRect(mx, segY, 10, 6, col);
        }
        drawPixelTextCentered(c, mx + 5, my + mh + 4, label, p.muted, 1);
    };
    meter(x + 22, telemetry_->peakL.load(std::memory_order_relaxed), "L");
    meter(x + 38, telemetry_->peakR.load(std::memory_order_relaxed), "R");

    // ---- Voice LEDs (right of the dial) ----
    {
        const int vx = x + w - 52, vy = cy - 58;
        const int voices = telemetry_->activeVoices.load(std::memory_order_relaxed);
        drawPixelText(c, vx, vy, "VOICE", p.muted, 1);
        for (int i = 0; i < 16; ++i) {
            const int col = i % 4, row = i / 4;
            c.fillRect(vx + col * 9, vy + 12 + row * 9, 7, 7, i < voices ? p.accent2 : p.border);
        }
        char buf[16];
        std::snprintf(buf, sizeof(buf), "%02d/16", voices);
        drawPixelText(c, vx, vy + 50, buf, p.ink, 1);
        const bool sus = param(kParamSustain) >= 0.5f;
        c.fillRect(vx, vy + 66, 7, 7, sus ? p.accent3 : p.border);
        drawPixelText(c, vx + 10, vy + 66, "PEDAL", sus ? p.ink : p.muted, 1);
        const float bend = (param(kParamPitchBend) - 0.5f) * 4.0f;
        c.fillRect(vx, vy + 82, 40, 3, p.border);
        c.fillRect(vx + 19 + static_cast<int>(bend * 9.0f), vy + 80, 3, 7, p.accent);
        drawPixelText(c, vx, vy + 92, "BEND", p.muted, 1);
    }

    // ---- 12 macro buttons ----
    const int bw = 56, bh = 30, gap = 4;
    const int bx0 = x + (w - (6 * bw + 5 * gap)) / 2;
    for (int m = 0; m < kNumMacros; ++m) {
        const int bx = bx0 + (m % 6) * (bw + gap);
        const int by = y + 232 + (m / 6) * (bh + gap);
        const bool sel = (m == macro);
        const bool hv = hovered(bx, by, bw, bh);
        c.fillRect(bx + 2, by + 2, bw, bh, p.shadow);
        c.fillPixelBox(bx, by, bw, bh, sel ? p.accent : (hv ? p.inset : p.panel), sel ? p.accent : (hv ? p.accent : p.border), 2);
        drawPixelTextCentered(c, bx + bw / 2, by + 7, kMacroShort[m], sel ? p.bg : p.ink, 1);
        const float v = param(kMacroParamIds[m]);
        c.fillRect(bx + 6, by + 20, bw - 12, 4, sel ? p.bg : p.inset);
        c.fillRect(bx + 6, by + 20, static_cast<int>((bw - 12) * v), 4, sel ? p.ink : p.accent2);
        addHit(bx, by, bw, bh, kHitMacroBtn, m);
    }
}

void PixelLabsView::drawSoundPanel(PixelCanvas& c, const Palette& p) {
    const int x = 640, y = 90, w = 304, h = 310;
    panel(c, p, x, y, w, h);
    drawPixelText(c, x + 10, y + 8, "SOUND", p.muted, 1);
    {
        const char* hint = "DBL-CLICK = RESET";
        drawPixelText(c, x + w - 10 - measurePixelText(hint, 1), y + 8, hint, p.border, 1);
    }

    // List steppers
    for (int i = 0; i < 4; ++i) {
        const StepperDef& s = kSteppers[i];
        const int ry = y + 22 + i * 22;
        drawPixelText(c, x + 10, ry + 6, s.label, p.muted, 1);
        const int bx = x + 50, bw = w - 60, bh = 18;
        insetBox(c, p, bx, ry, bw, bh);
        const bool hp = hovered(bx, ry, 20, bh), hn = hovered(bx + bw - 20, ry, 20, bh);
        triangle(c, bx + 10, ry + 9, 5, -1, hp ? p.accent : p.ink);
        triangle(c, bx + bw - 10, ry + 9, 5, 1, hn ? p.accent : p.ink);
        char buf[48];
        formatParamValue(s.param, param(s.param), buf, sizeof(buf));
        const int idx = normToList(param(s.param), s.count);
        char full[64];
        std::snprintf(full, sizeof(full), "%s", buf);
        drawPixelTextCentered(c, bx + bw / 2, ry + 6, full, p.ink, 1);
        char cnt[16];
        std::snprintf(cnt, sizeof(cnt), "%d/%d", idx + 1, s.count);
        drawPixelText(c, bx + 24, ry + 6, cnt, p.border, 1);
        addHit(bx, ry, bw / 2, bh, kHitStepPrev, s.param);
        addHit(bx + bw / 2, ry, bw - bw / 2, bh, kHitStepNext, s.param);
    }

    // 16 sliders in two columns
    const FactoryPreset& fp = factoryPreset(telemetry_->currentPreset.load(std::memory_order_relaxed));
    for (int i = 0; i < 16; ++i) {
        const SliderDef& s = kSliders[i];
        const int col = i / 8, row = i % 8;
        const int sx = x + 10 + col * 146, sy = y + 114 + row * 24, sw = 138;
        const float v = param(s.param);
        const bool active = (dragKind_ == kHitSlider && dragParam_ == s.param);
        const bool hv = hovered(sx, sy, sw, 20) || active;
        drawPixelText(c, sx, sy, s.label, hv ? p.ink : p.muted, 1);
        char buf[32];
        formatParamValue(s.param, v, buf, sizeof(buf));
        drawPixelText(c, sx + sw - measurePixelText(buf, 1), sy, buf, hv ? p.accent : p.ink, 1);
        const int by = sy + 10, bh = 8;
        insetBox(c, p, sx, by, sw, bh);
        const bool bipolar = (s.param == kParamSweep);
        if (bipolar) {
            const int mid = sx + sw / 2;
            const int px = sx + static_cast<int>((sw - 1) * v);
            c.fillRect(std::min(mid, px), by + 1, std::abs(px - mid) + 1, bh - 2, p.accent2);
            c.fillRect(mid, by - 1, 1, bh + 2, p.muted);
        } else {
            // Segmented pixel fill
            const int fill = static_cast<int>((sw - 2) * v);
            for (int fx = 0; fx < fill; fx += 4) c.fillRect(sx + 1 + fx, by + 1, std::min(3, fill - fx), bh - 2, hv ? p.accent : p.accent2);
        }
        // Preset reference tick
        const int rx = sx + static_cast<int>((sw - 1) * fp.values[s.param]);
        c.fillRect(rx, by + bh, 1, 2, p.muted);
        // Handle
        const int hx = sx + static_cast<int>((sw - 3) * v);
        c.fillRect(hx, by - 2, 3, bh + 4, p.ink);
        addHit(sx, sy - 2, sw, 22, kHitSlider, s.param);
    }
}

void PixelLabsView::drawStatusStrip(PixelCanvas& c, const Palette& p) {
    const int x = 16, y = 408, w = 928, h = 30;
    panel(c, p, x, y, w, h);

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

    drawPixelText(c, x + 10, y + 12, "NOTES", p.muted, 1);
    int tx = x + 50;
    if (count == 0) {
        textFit(c, tx, y + 12, 560, factoryPreset(telemetry_->currentPreset.load(std::memory_order_relaxed)).description, p.border, 1);
    } else {
        for (int i = 0; i < count && i < 8; ++i) {
            char nb[16];
            if (drums) std::snprintf(nb, sizeof(nb), "%s", kDrumNames[notes[i] % 12]);
            else std::snprintf(nb, sizeof(nb), "%s%d", noteName(notes[i]), noteOctave(notes[i]));
            drawPixelText(c, tx, y + 8, nb, p.ink, 2);
            tx += measurePixelText(nb, 2) + 10;
        }
        if (total > 8) {
            char more[8];
            std::snprintf(more, sizeof(more), "+%d", total - 8);
            drawPixelText(c, tx, y + 12, more, p.muted, 1);
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
                c.fillPixelBox(tx + 4, y + 5, measurePixelText(cb, 2) + 12, 20, p.accent, p.accent, 2);
                drawPixelText(c, tx + 10, y + 8, cb, p.bg, 2);
            }
        }
    }

    // Hover readout (right side)
    const Hit* hh = (hoverX_ >= 0) ? hitAt(hoverX_, hoverY_) : nullptr;
    if (dragKind_ == kHitSlider || dragKind_ == kHitBigDial) hh = &dragHit_;
    char info[64] = {};
    if (hh) {
        int pid = -1;
        if (hh->kind == kHitSlider || hh->kind == kHitStepPrev || hh->kind == kHitStepNext) pid = hh->a;
        else if (hh->kind == kHitBigDial) pid = kMacroParamIds[std::clamp(telemetry_->activeMacro.load(), 0, kNumMacros - 1)];
        else if (hh->kind == kHitMacroBtn) pid = kMacroParamIds[hh->a];
        if (pid >= 0) {
            char v[32];
            formatParamValue(pid, param(pid), v, sizeof(v));
            const char* label = "";
            for (const auto& s : kSliders) if (s.param == pid) label = s.label;
            for (const auto& s : kSteppers) if (s.param == pid) label = s.label;
            for (int m = 0; m < kNumMacros; ++m) if (kMacroParamIds[m] == pid) label = kMacroNames[m];
            std::snprintf(info, sizeof(info), "%s: %s", label, v);
        } else if (hh->kind == kHitKeyboard) {
            const int k = keyAt(hoverX_, hoverY_);
            if (k >= 0) std::snprintf(info, sizeof(info), "KEY %s%d  (MIDI %d)", noteName(k), noteOctave(k), k);
        }
    }
    if (info[0]) drawPixelText(c, x + 630 - measurePixelText(info, 1), y + 12, info, p.accent2, 1);

    // Computer keyboard octave control
    {
        const int ox = x + w - 170;
        char buf[32];
        std::snprintf(buf, sizeof(buf), "PC KEYS A=C%d", kbdOctave_);
        const bool hd = hovered(ox, y + 6, 18, 18), hu = hovered(ox + 150, y + 6, 18, 18);
        insetBox(c, p, ox, y + 6, 168, 18);
        triangle(c, ox + 9, y + 15, 5, -1, hd ? p.accent : p.ink);
        triangle(c, ox + 159, y + 15, 5, 1, hu ? p.accent : p.ink);
        drawPixelTextCentered(c, ox + 84, y + 12, buf, p.ink, 1);
        addHit(ox, y + 6, 22, 18, kHitOctDown);
        addHit(ox + 146, y + 6, 22, 18, kHitOctUp);
    }
}

void PixelLabsView::drawKeyboard(PixelCanvas& c, const Palette& p) {
    const int top = kKeyboardTop, bottom = kKeyboardBottom;
    const int left = kKeyboardLeft, ww = kWhiteKeyW;
    const int nWhite = whiteIndex(kLastKey) + 1;
    const int whiteH = bottom - top;
    const int blackH = 88, blackW = 16;

    c.fillRect(left - 4, top - 4, nWhite * ww + 8, whiteH + 8, p.shadow);
    c.fillRect(left - 4, top - 4, nWhite * ww + 8, 4, p.border);

    const uint64_t lo = telemetry_->activeNotesLow.load(std::memory_order_relaxed);
    const uint64_t hi = telemetry_->activeNotesHigh.load(std::memory_order_relaxed);
    auto isOn = [&](int n) { return n < 64 ? ((lo >> n) & 1ull) != 0 : ((hi >> (n - 64)) & 1ull) != 0; };
    const bool drums = normToList(param(kParamWaveform), kNumWaveforms) == kWaveDrums;
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
            c.fillRect(kx, bottom - 6, ww - 2, 6, p.keyWhiteShade);
            if (n == hoverKey) blendRect(c, kx, top, ww - 2, whiteH - 6, p.accent, 50);
        } else {
            c.fillRect(kx, top, ww - 2, 3, p.keyHitInk);
        }
        const uint32_t ink = on ? p.keyHitInk : p.keyWhiteInk;
        // Letter name on every white key, octave number on C
        char letter[2] = { noteName(n)[0], 0 };
        drawPixelTextCentered(c, kx + (ww - 2) / 2, bottom - 24, letter, ink, 2);
        if (n % 12 == 0) {
            char oct[4];
            std::snprintf(oct, sizeof(oct), "%d", noteOctave(n));
            drawPixelTextCentered(c, kx + (ww - 2) / 2, bottom - 36, oct, on ? ink : p.accent, 1);
        }
        if (drums) drawPixelTextCentered(c, kx + (ww - 2) / 2, top + blackH + 8, kDrumKeyLabels[n % 12], on ? ink : p.muted, 1);
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
            if (n == hoverKey) blendRect(c, kx, top, blackW, blackH, p.accent, 70);
        }
        const char* label = drums ? kDrumKeyLabels[n % 12] : noteName(n);
        drawPixelTextCentered(c, kx + blackW / 2, top + blackH - 18, label, on ? p.keyHitInk : p.keyBlackInk, 1);
    }
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

void PixelLabsView::handleMouseDown(int x, int y) {
    if (hwnd_) { SetFocus(hwnd_); SetCapture(hwnd_); }
    const Hit* h = hitAt(x, y);
    if (!h || !telemetry_) return;
    const Hit hit = *h; // hits_ may be rebuilt by callbacks
    switch (hit.kind) {
        case kHitPresetRow:  if (onPresetChange_) onPresetChange_(hit.a); break;
        case kHitPresetPrev: stepPreset(-1); break;
        case kHitPresetNext: stepPreset(+1); break;
        case kHitEraTab:
            telemetry_->eraTab.store(hit.a, std::memory_order_relaxed);
            listScroll_ = 0;
            lastTabSeen_ = -1;
            break;
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
                playGuiNote(k, true);
            }
            break;
        }
        case kHitDemo: {
            const int cur = telemetry_->demoActive.load(std::memory_order_relaxed);
            telemetry_->demoActive.store(cur ? 0 : 1, std::memory_order_release);
            break;
        }
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
                if (k >= 0) playGuiNote(k, true);
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
    if (h && telemetry_ && (h->kind == kHitSlider || h->kind == kHitBigDial)) {
        const int pid = (h->kind == kHitSlider) ? h->a
                        : kMacroParamIds[std::clamp(telemetry_->activeMacro.load(), 0, kNumMacros - 1)];
        const FactoryPreset& fp = factoryPreset(telemetry_->currentPreset.load(std::memory_order_relaxed));
        setParam(pid, fp.values[pid]);
        return;
    }
    handleMouseDown(x, y); // buttons and keys still react to fast clicks
}

void PixelLabsView::handleWheel(int x, int y, float steps) {
    if (!telemetry_) return;
    const Hit* h = (x >= 0) ? hitAt(x, y) : nullptr;
    const int kind = h ? h->kind : kHitNone;
    if (kind == kHitPresetRow || kind == kHitPresetList) {
        listScroll_ -= static_cast<int>(steps * 2.0f);
        const int n = static_cast<int>(filteredPresets().size());
        listScroll_ = std::clamp(listScroll_, 0, std::max(0, n - 14));
        return;
    }
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

bool PixelLabsView::handleKey(int vk, bool down) {
    if (vk < 0 || vk > 255) return false;
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
            if (self->handleKey(static_cast<int>(wParam), true)) return 0;
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
