#include "PixelLabsView.h"
#include "PixelFont5x7.h"
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
        c.fillPixelBox(x, y, w, h, hover ? p.accent2 : p.accent, hover ? p.accent2 : p.accent, 2);
    } else {
        c.fillRect(x + 2, y + 2, w, h, p.shadow);
        c.fillPixelBox(x, y, w, h, hover ? p.inset : p.panel, hover ? p.accent : p.border, 2);
    }
    const int th = 7 * scale;
    drawPixelTextCentered(c, x + w / 2, y + (h - th) / 2, label, active ? p.bg : p.ink, scale);
}

// 7x7 pixel icons, one byte per row, bit 6 = leftmost pixel
static const uint8_t kIconStar[7]   = { 0x08, 0x1C, 0x7F, 0x3E, 0x1C, 0x36, 0x22 };
static const uint8_t kIconDisk[7]   = { 0x7E, 0x5D, 0x5D, 0x7F, 0x41, 0x41, 0x7F };
static const uint8_t kIconFolder[7] = { 0x70, 0x7F, 0x41, 0x41, 0x41, 0x41, 0x7F };
static const uint8_t kIconPencil[7] = { 0x03, 0x07, 0x0E, 0x1C, 0x38, 0x50, 0x60 };
static const uint8_t kIconCross[7]  = { 0x41, 0x22, 0x14, 0x08, 0x14, 0x22, 0x41 };
static const uint8_t kIconMinus[7]  = { 0x00, 0x00, 0x00, 0x7F, 0x00, 0x00, 0x00 };

static void drawIcon(PixelCanvas& c, int x, int y, const uint8_t rows[7], uint32_t col, int scale = 1) {
    for (int r = 0; r < 7; ++r)
        for (int b = 0; b < 7; ++b)
            if (rows[r] & (1 << (6 - b))) c.fillRect(x + b * scale, y + r * scale, scale, scale, col);
}

static void dialogFrame(PixelCanvas& c, const Palette& p, int x, int y, int w, int h) {
    c.fillRect(x + 5, y + 5, w, h, p.shadow);
    c.fillPixelBox(x, y, w, h, p.panel, p.accent, 2);
    c.fillRect(x + 6, y + 6, w - 12, 1, p.border);
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
    if (k.kind == kSoundFactory) return upper(factoryPreset(static_cast<int>(k.id)).name);
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
    if (modal_.kind == kModalBankPicker) { closeModal(); return; }
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
    if (modal_.kind != kModalNone) drawModal(c, p);
}

void PixelLabsView::drawHeader(PixelCanvas& c, const Palette& p) {
    // Logo with a hard pixel drop shadow
    drawPixelText(c, 18, 12, "PIXEL", p.shadow, 3);
    drawPixelText(c, 16, 10, "PIXEL", p.accent, 3);
    const int lx = 16 + measurePixelText("PIXEL ", 3);
    drawPixelText(c, lx + 2, 12, "LABS", p.shadow, 3);
    drawPixelText(c, lx, 10, "LABS", p.ink, 3);
    drawPixelText(c, 16, 38, "RETRO INSTRUMENT LAB  V" PIXELLABS_VERSION_SHORT, p.muted, 1);

    // Sound display
    const int bx = 236, by = 8, bw = 392, bh = 40;
    panel(c, p, bx, by, bw, bh);
    insetBox(c, p, bx + 32, by + 4, bw - 64, bh - 8);
    const int era = currentEra();

    const bool hPrev = hovered(bx, by, 30, bh), hNext = hovered(bx + bw - 30, by, 30, bh);
    triangle(c, bx + 16, by + bh / 2, 7, -1, hPrev ? p.accent : p.ink);
    triangle(c, bx + bw - 16, by + bh / 2, 7, 1, hNext ? p.accent : p.ink);
    addHit(bx, by, 30, bh, kHitPresetPrev);
    addHit(bx + bw - 30, by, 30, bh, kHitPresetNext);

    // Favourite star for the loaded sound
    {
        const bool canFav = cur_.key.kind != kSoundDetached;
        const bool fav = canFav && lib_.isFavorite(cur_.key);
        const bool hv = hovered(kHeaderStarX - 9, kHeaderStarY - 9, 18, 18);
        const uint32_t col = fav ? p.accent3 : (hv && canFav ? p.accent : p.border);
        drawIcon(c, kHeaderStarX - 7, kHeaderStarY - 7, kIconStar, col, 2);
        addHit(kHeaderStarX - 9, kHeaderStarY - 9, 18, 18, kHitHeaderStar);
    }

    const std::string& name = cur_.name;
    const int nameScale = measurePixelText(name.c_str(), 2) <= bw - 128 ? 2 : 1;
    drawPixelTextCentered(c, bx + bw / 2 + 8, by + (nameScale == 2 ? 9 : 12), name.c_str(), p.ink, nameScale);

    char sub[96];
    if (cur_.key.kind == kSoundFactory)
        std::snprintf(sub, sizeof(sub), "%02u/%02d  %s  /  %s", cur_.key.id + 1, kNumFactoryPresets, kEraNames[era], cur_.category.c_str());
    else
        std::snprintf(sub, sizeof(sub), "%s  %s  /  %s", cur_.key.kind == kSoundUser ? "MY SOUND" : "NOT SAVED",
                      kEraNames[era], cur_.category.c_str());
    const char* edited = modified_ ? "  EDITED" : "";
    const int subW = measurePixelText(sub, 1) + measurePixelText(edited, 1);
    const int sx = bx + bw / 2 + 14 - subW / 2;
    c.fillRect(sx - 12, by + 28, 7, 7, kEraColors[era]);
    drawPixelText(c, sx, by + 28, sub, p.muted, 1);
    if (modified_) drawPixelText(c, sx + measurePixelText(sub, 1), by + 28, edited, p.accent, 1);

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
    static const char* kTabNames[kNumBrowserTabs] = {
        "ALL", "80S CHIP", "90S CHIP", "SYNTH 80S", "MEDIEVAL", "SFX + KIT", "MY SOUNDS", "FAVORITES", "BANKS"
    };
    const int tab = browserTab();
    const int y = kTabY, h = kTabH;
    for (int t = 0; t < kNumBrowserTabs; ++t) {
        const int x = tabX(t), w = tabW(t);
        const bool active = (t == tab);
        const bool hv = hovered(x, y, w, h);
        c.fillRect(x + 2, y + 2, w, h, p.shadow);
        c.fillPixelBox(x, y, w, h, active ? p.accent : (hv ? p.inset : p.panel), active ? p.accent : (hv ? p.accent : p.border), 2);
        const int tw = measurePixelText(kTabNames[t], 1);
        const int tx = x + (w - tw) / 2 + (t > 0 ? 6 : 0);
        if (t >= 1 && t <= kNumEras) c.fillRect(tx - 12, y + 9, 7, 7, kEraColors[t - 1]);
        else if (t == kTabUser) drawIcon(c, tx - 12, y + 9, kIconDisk, active ? p.bg : p.accent3);
        else if (t == kTabFavorites) drawIcon(c, tx - 12, y + 9, kIconStar, active ? p.bg : p.accent3);
        else if (t == kTabBanks) drawIcon(c, tx - 12, y + 9, kIconFolder, active ? p.bg : p.accent3);
        drawPixelText(c, tx, y + 9, kTabNames[t], active ? p.bg : p.ink, 1);
        addHit(x, y, w, h, kHitEraTab, t);
    }
}

void PixelLabsView::drawPresetList(PixelCanvas& c, const Palette& p) {
    const int x = kListX, y = kListY, w = kListW, h = kListH;
    panel(c, p, x, y, w, h);
    addHit(x, y, w, h, kHitPresetList);
    const int tab = browserTab();
    insetBox(c, p, x + 6, kRowsTop - 2, w - 12, kVisibleRows * kRowH + 4);

    if (tab == kTabBanks && openBankId_ == 0) {
        drawBankRows(c, p);
    } else {
        const std::vector<SoundKey> list = browseList();
        char title[64];
        if (tab == kTabBanks) {
            const SoundBank* b = lib_.findBank(openBankId_);
            const bool hv = hovered(x + 4, y + 3, w - 8, 17);
            triangle(c, x + 13, y + 11, 5, -1, hv ? p.accent : p.ink);
            std::snprintf(title, sizeof(title), "%s  %d", b ? b->name.c_str() : "", static_cast<int>(list.size()));
            textFit(c, x + 22, y + 8, w - 32, title, hv ? p.accent : p.ink, 1);
            addHit(x + 4, y + 3, w - 8, 17, kHitBankBack);
        } else {
            const char* label = tab == kTabAll ? "ALL SOUNDS" : tab == kTabUser ? "MY SOUNDS"
                              : tab == kTabFavorites ? "FAVORITES" : kEraNames[tab - 1];
            std::snprintf(title, sizeof(title), "%s  %d", label, static_cast<int>(list.size()));
            drawPixelText(c, x + 10, y + 8, title, p.muted, 1);
            if (static_cast<int>(list.size()) > kVisibleRows)
                drawPixelText(c, x + w - 10 - measurePixelText("WHEEL", 1), y + 8, "WHEEL", p.border, 1);
        }
        drawSoundRows(c, p, list, tab);
    }
    drawActionBar(c, p);
}

static void drawScrollbar(PixelCanvas& c, const Palette& p, int sx, int sy, int sh, int visible, int n, int scroll) {
    const int maxScroll = std::max(0, n - visible);
    if (maxScroll <= 0) return;
    c.fillRect(sx, sy, 3, sh, p.border);
    const int th = std::max(12, sh * visible / n);
    const int ty = sy + (sh - th) * scroll / maxScroll;
    c.fillRect(sx - 1, ty, 5, th, p.accent2);
}

static void drawHintLines(PixelCanvas& c, const Palette& p, int cx, int y, std::initializer_list<const char*> lines) {
    int ly = y;
    for (const char* l : lines) { drawPixelTextCentered(c, cx, ly, l, p.muted, 1); ly += 12; }
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
        const int cx = kRowX + kRowW / 2, cy = kRowsTop + 90;
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
        if (sel) c.fillRect(rx, ry, rw, rh, p.accent);
        else if (hv) c.fillRect(rx, ry, rw, rh, p.panel);
        c.fillRect(rx + 2, ry + 3, 3, rh - 6, kEraColors[soundEra(key)]);
        if (user) {
            drawIcon(c, rx + 10, ry + 6, kIconDisk, sel ? p.bg : p.accent3);
        } else {
            char num[8];
            std::snprintf(num, sizeof(num), "%02u", key.id + 1);
            drawPixelText(c, rx + 9, ry + 6, num, sel ? p.bg : p.muted, 1);
        }
        addHit(rx, ry, rw, rh, kHitSoundRow, packKey(key));

        // Row tools: star always, rename / delete / remove-from-bank while hovered
        const bool fav = lib_.isFavorite(key);
        const bool starHv = hovered(rx + rw - 15, ry, 15, rh);
        const uint32_t starCol = fav ? (sel ? p.ink : p.accent3) : (starHv ? (sel ? p.ink : p.accent) : (sel ? p.inset : p.border));
        drawIcon(c, rx + rw - 11, ry + 6, kIconStar, starCol);
        addHit(rx + rw - 15, ry, 15, rh, kHitStar, packKey(key));

        int tools = 0;
        if (hv || starHv || hovered(rx + rw - 45, ry, 30, rh)) {
            const uint32_t toolCol = sel ? p.bg : p.ink;
            if (tab == kTabUser && user) {
                const bool h1 = hovered(rx + rw - 29, ry, 14, rh), h2 = hovered(rx + rw - 43, ry, 14, rh);
                drawIcon(c, rx + rw - 25, ry + 6, kIconCross, h1 ? p.accent2 : toolCol);
                drawIcon(c, rx + rw - 39, ry + 6, kIconPencil, h2 ? p.accent2 : toolCol);
                addHit(rx + rw - 29, ry, 14, rh, kHitSoundDelete, static_cast<int>(key.id));
                addHit(rx + rw - 43, ry, 14, rh, kHitSoundRename, static_cast<int>(key.id));
                tools = 2;
            } else if (tab == kTabBanks) {
                const bool h1 = hovered(rx + rw - 29, ry, 14, rh);
                drawIcon(c, rx + rw - 25, ry + 6, kIconMinus, h1 ? p.accent2 : toolCol);
                addHit(rx + rw - 29, ry, 14, rh, kHitBankRemoveItem, packKey(key));
                tools = 1;
            }
        }
        textFit(c, rx + 26, ry + 6, rw - 26 - 18 - tools * 14, soundName(key).c_str(), sel ? p.bg : p.ink, 1);
    }
    drawScrollbar(c, p, kListX + kListW - 13, kRowsTop, kVisibleRows * kRowH, kVisibleRows, n, listScroll_);
}

void PixelLabsView::drawBankRows(PixelCanvas& c, const Palette& p) {
    const int x = kListX, y = kListY;
    const int nBanks = static_cast<int>(lib_.banks.size());
    const int n = nBanks + 1; // + NEW BANK
    char title[32];
    std::snprintf(title, sizeof(title), "BANKS  %d", nBanks);
    drawPixelText(c, x + 10, y + 8, title, p.muted, 1);
    if (lastTabSeen_ != -2) { lastTabSeen_ = -2; lastKeySeen_ = { -1, 0 }; }
    listScroll_ = std::clamp(listScroll_, 0, std::max(0, n - kVisibleRows));

    for (int r = 0; r < kVisibleRows && listScroll_ + r < n; ++r) {
        const int idx = listScroll_ + r;
        const int ry = kRowsTop + r * kRowH;
        const int rx = kRowX, rw = kRowW, rh = kRowH - 1;
        const bool hv = hovered(rx, ry, rw, rh);
        if (hv) c.fillRect(rx, ry, rw, rh, p.panel);
        if (idx == nBanks) {
            drawPixelText(c, rx + 9, ry + 6, "+ NEW BANK", hv ? p.accent : p.accent2, 1);
            addHit(rx, ry, rw, rh, kHitBankNew);
            continue;
        }
        const SoundBank& b = lib_.banks[idx];
        drawIcon(c, rx + 10, ry + 6, kIconFolder, p.accent3);
        addHit(rx, ry, rw, rh, kHitBankRow, static_cast<int>(b.id));
        int right = rx + rw - 4;
        if (hv || hovered(rx + rw - 30, ry, 30, rh)) {
            const bool h1 = hovered(rx + rw - 15, ry, 15, rh), h2 = hovered(rx + rw - 29, ry, 14, rh);
            drawIcon(c, rx + rw - 11, ry + 6, kIconCross, h1 ? p.accent2 : p.ink);
            drawIcon(c, rx + rw - 25, ry + 6, kIconPencil, h2 ? p.accent2 : p.ink);
            addHit(rx + rw - 15, ry, 15, rh, kHitBankDelete, static_cast<int>(b.id));
            addHit(rx + rw - 29, ry, 14, rh, kHitBankRename, static_cast<int>(b.id));
            right = rx + rw - 32;
        }
        char cnt[16];
        std::snprintf(cnt, sizeof(cnt), "%d", static_cast<int>(b.items.size()));
        const int cw = measurePixelText(cnt, 1);
        drawPixelText(c, right - cw, ry + 6, cnt, p.muted, 1);
        textFit(c, rx + 26, ry + 6, right - cw - 8 - (rx + 26), b.name.c_str(), p.ink, 1);
    }
    if (nBanks == 0) {
        drawHintLines(c, p, kRowX + kRowW / 2, kRowsTop + 60,
                      { "A BANK IS YOUR OWN SET", "OF SOUNDS. MAKE AS MANY", "AS YOU LIKE AND NAME", "THEM ANYTHING." });
    }
    drawScrollbar(c, p, kListX + kListW - 13, kRowsTop, kVisibleRows * kRowH, kVisibleRows, n, listScroll_);
}

void PixelLabsView::drawActionBar(PixelCanvas& c, const Palette& p) {
    const int y = kActionY, h = kActionH, w = kActionW;
    button(c, p, kSaveBtnX, y, w, h, "SAVE", modified_, hovered(kSaveBtnX, y, w, h));
    button(c, p, kSaveAsBtnX, y, w, h, "SAVE AS", false, hovered(kSaveAsBtnX, y, w, h));
    button(c, p, kBankBtnX, y, w, h, "+ BANK", false, hovered(kBankBtnX, y, w, h));
    addHit(kSaveBtnX, y, w, h, kHitSave);
    addHit(kSaveAsBtnX, y, w, h, kHitSaveAs);
    addHit(kBankBtnX, y, w, h, kHitAddToBank);
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
        // Stored-sound reference tick
        const int rx = sx + static_cast<int>((sw - 1) * cur_.ref[s.param]);
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
        }
    }

    drawPixelText(c, x + 10, y + 12, "NOTES", p.muted, 1);
    int tx = x + 50;
    if (count == 0) {
        if (toastFrames_ > 0) {
            const int tw = std::min(measurePixelText(toast_.c_str(), 1) + 12, notesRight - tx);
            c.fillPixelBox(tx - 4, y + 6, tw, 18, p.accent3, p.accent3, 2);
            textFit(c, tx + 2, y + 12, tw - 10, toast_.c_str(), p.bg, 1);
        } else if (info[0]) {
            textFit(c, tx, y + 12, notesRight - tx, info, p.accent2, 1);
        } else {
            std::string desc;
            if (cur_.key.kind == kSoundFactory) desc = factoryPreset(static_cast<int>(cur_.key.id)).description;
            else if (cur_.key.kind == kSoundUser) desc = "YOUR SOUND, MADE FROM " + upper(factoryPreset(cur_.basePreset).name);
            else desc = "NOT IN YOUR LIBRARY - PRESS SAVE AS TO KEEP IT";
            textFit(c, tx, y + 12, notesRight - tx, desc.c_str(), p.border, 1);
        }
    } else {
        for (int i = 0; i < count && i < 8 && tx < notesRight - 60; ++i) {
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
                tx += measurePixelText(cb, 2) + 20;
            }
        }
        if (info[0]) {
            const int iw = measurePixelText(info, 1);
            if (notesRight - iw > tx + 8) drawPixelText(c, notesRight - iw, y + 12, info, p.accent2, 1);
        }
    }

    // Keyboard touch: curve stepper + velocity sensitivity
    {
        const int ty = y + 6, th = 18;
        const int touch = normToList(param(kParamTouch), kNumTouchCurves);
        insetBox(c, p, kTouchX, ty, kTouchW, th);
        const bool hp = hovered(kTouchX, ty, kTouchW / 2, th), hn = hovered(kTouchX + kTouchW / 2, ty, kTouchW - kTouchW / 2, th);
        triangle(c, kTouchX + 9, ty + 9, 5, -1, hp ? p.accent : p.ink);
        triangle(c, kTouchX + kTouchW - 9, ty + 9, 5, 1, hn ? p.accent : p.ink);
        char buf[32];
        std::snprintf(buf, sizeof(buf), "TOUCH %s", kTouchShort[touch]);
        drawPixelTextCentered(c, kTouchX + kTouchW / 2, ty + 6, buf, p.ink, 1);
        addHit(kTouchX, ty, kTouchW / 2, th, kHitTouchPrev);
        addHit(kTouchX + kTouchW / 2, ty, kTouchW - kTouchW / 2, th, kHitTouchNext);

        const float vs = param(kParamVelSens);
        const bool off = touch == kTouchOff;
        const bool hv = hovered(kVelX, ty, kVelW, th) || (dragKind_ == kHitSlider && dragParam_ == kParamVelSens);
        insetBox(c, p, kVelX, ty, kVelW, th);
        const int fill = static_cast<int>((kVelW - 4) * vs);
        for (int fx = 0; fx < fill; fx += 4) c.fillRect(kVelX + 2 + fx, ty + th - 5, std::min(3, fill - fx), 3, off ? p.border : (hv ? p.accent : p.accent2));
        std::snprintf(buf, sizeof(buf), "VEL SENS %.0f%%", vs * 100.0f);
        drawPixelTextCentered(c, kVelX + kVelW / 2, ty + 4, buf, off ? p.muted : (hv ? p.accent : p.ink), 1);
        addHit(kVelX, ty, kVelW, th, kHitSlider, kParamVelSens);
    }

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
    dialogFrame(c, p, x, y, w, h);
    drawPixelText(c, x + 20, y + 16, modal_.title.c_str(), p.accent, 2);
    textFit(c, x + 20, y + 38, w - 40, modal_.subtitle.c_str(), p.muted, 1);

    // Text field with a blinking block cursor
    const std::string& t = modal_.text;
    insetBox(c, p, x + 20, y + 54, w - 40, 32);
    if (t.empty()) drawPixelText(c, x + 30, y + 63, "TYPE A NAME", p.border, 2);
    else drawPixelText(c, x + 30, y + 63, t.c_str(), p.ink, 2);
    const int cx = x + 30 + (t.empty() ? 0 : measurePixelText(t.c_str(), 2) + 3);
    if ((frame_ / 8) & 1) c.fillRect(cx, y + 61, 10, 17, p.accent);
    char cnt[16];
    std::snprintf(cnt, sizeof(cnt), "%d/%d", static_cast<int>(t.size()), static_cast<int>(SoundLibrary::kMaxNameLen));
    drawPixelText(c, x + w - 28 - measurePixelText(cnt, 1), y + 79 - 7, cnt, p.border, 1);

    // What will actually happen with this name
    const std::string clean = SoundLibrary::sanitizeName(t);
    SoundLibrary& lib = SoundLibrary::instance();
    std::string hint = "TYPE ON YOUR KEYBOARD OR CLICK THE KEYS.  ENTER = OK  ESC = CANCEL";
    uint32_t hintCol = p.muted;
    if (!clean.empty()) {
        const bool bank = modal_.namePurpose == kNameNewBank || modal_.namePurpose == kNameRenameBank;
        const uint32_t except = (modal_.namePurpose == kNameRenameSound || modal_.namePurpose == kNameRenameBank) ? modal_.targetId : 0;
        const std::string finalName = bank ? lib.uniqueBankName(clean, except) : lib.uniqueSoundName(clean, except);
        if (finalName != clean) { hint = "THAT NAME IS TAKEN - IT WILL BE SAVED AS " + finalName; hintCol = p.accent3; }
    }
    textFit(c, x + 20, y + 94, w - 40, hint.c_str(), hintCol, 1);

    // Clickable keys (arcade name entry)
    static const char* kKeyRows[4] = { "1234567890", "QWERTYUIOP", "ASDFGHJKL-", "ZXCVBNM.'&" };
    const int kx0 = x + 42;
    auto keycap = [&](int kx, int ky, int kw, const char* label, int code, int scale) {
        const bool hv = hovered(kx, ky, kw, 22);
        c.fillRect(kx + 2, ky + 2, kw, 22, p.shadow);
        c.fillPixelBox(kx, ky, kw, 22, hv ? p.accent : p.inset, hv ? p.accent : p.border, 2);
        drawPixelTextCentered(c, kx + kw / 2, ky + (22 - 7 * scale) / 2, label, hv ? p.bg : p.ink, scale);
        addHit(kx, ky, kw, 22, kHitNameKey, code);
    };
    for (int r = 0; r < 4; ++r) {
        for (int i = 0; kKeyRows[r][i]; ++i) {
            const char ch[2] = { kKeyRows[r][i], 0 };
            keycap(kx0 + i * 40, y + 110 + r * 26, 36, ch, kKeyRows[r][i], 2);
        }
    }
    keycap(kx0, y + 214, 196, "SPACE", ' ', 1);
    keycap(kx0 + 200, y + 214, 96, "DEL", 8, 1);
    keycap(kx0 + 300, y + 214, 96, "CLEAR", 127, 1);

    const char* okLabel = modal_.namePurpose == kNameSaveSound ? "SAVE"
                        : modal_.namePurpose == kNameNewBank ? "CREATE" : "RENAME";
    button(c, p, kNameCancelX, kNameBtnY, 120, 30, "CANCEL", false, hovered(kNameCancelX, kNameBtnY, 120, 30));
    button(c, p, kNameOkX, kNameBtnY, 140, 30, okLabel, !clean.empty(), hovered(kNameOkX, kNameBtnY, 140, 30));
    addHit(kNameCancelX, kNameBtnY, 120, 30, kHitModalButton, kBtnCancel);
    addHit(kNameOkX, kNameBtnY, 140, 30, kHitModalButton, kBtnOk);
}

void PixelLabsView::drawConfirmDialog(PixelCanvas& c, const Palette& p) {
    const int x = kConfirmX, y = kConfirmY, w = kConfirmW, h = kConfirmH;
    dialogFrame(c, p, x, y, w, h);
    drawPixelText(c, x + 20, y + 16, modal_.title.c_str(), p.accent, 2);
    textFit(c, x + 20, y + 50, w - 40, modal_.line1.c_str(), p.ink, 1);
    textFit(c, x + 20, y + 66, w - 40, modal_.line2.c_str(), p.muted, 1);
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
    dialogFrame(c, p, x, y, w, h);
    drawPixelText(c, x + 20, y + 16, "ADD TO BANK", p.accent, 2);
    textFit(c, x + 20, y + 38, w - 40, soundName(modal_.pickerKey).c_str(), p.ink, 1);

    const int nBanks = static_cast<int>(lib_.banks.size());
    const int n = nBanks + 1;
    modal_.pickerScroll = std::clamp(modal_.pickerScroll, 0, std::max(0, n - kPickRows));
    insetBox(c, p, x + 20, kPickRowsTop - 2, w - 40, kPickRows * kPickRowH + 4);
    for (int r = 0; r < kPickRows && modal_.pickerScroll + r < n; ++r) {
        const int idx = modal_.pickerScroll + r;
        const int ry = kPickRowsTop + r * kPickRowH;
        const int rx = x + 24, rw = w - 54, rh = kPickRowH - 2;
        const bool hv = hovered(rx, ry, rw, rh);
        if (hv) c.fillRect(rx, ry, rw, rh, p.panel);
        if (idx == nBanks) {
            drawPixelText(c, rx + 8, ry + 6, "+ NEW BANK...", hv ? p.accent : p.accent2, 1);
            addHit(rx, ry, rw, rh, kHitPickerNew);
            continue;
        }
        const SoundBank& b = lib_.banks[idx];
        const bool in = std::find(b.items.begin(), b.items.end(), modal_.pickerKey) != b.items.end();
        c.fillRect(rx + 6, ry + 4, 11, 11, hv ? p.accent : p.border);
        c.fillRect(rx + 8, ry + 6, 7, 7, p.inset);
        if (in) c.fillRect(rx + 9, ry + 7, 5, 5, p.accent3);
        char cnt[16];
        std::snprintf(cnt, sizeof(cnt), "%d", static_cast<int>(b.items.size()));
        const int cw = measurePixelText(cnt, 1);
        drawPixelText(c, rx + rw - 6 - cw, ry + 6, cnt, p.muted, 1);
        textFit(c, rx + 24, ry + 6, rw - 40 - cw, b.name.c_str(), in ? p.ink : p.muted, 1);
        addHit(rx, ry, rw, rh, kHitPickerBank, static_cast<int>(b.id));
    }
    drawScrollbar(c, p, x + w - 26, kPickRowsTop, kPickRows * kPickRowH, kPickRows, n, modal_.pickerScroll);
    textFit(c, x + 20, kPickRowsTop + kPickRows * kPickRowH + 10, w - 40,
            nBanks ? "CLICK A BANK TO PUT THE SOUND IN OR TAKE IT OUT" : "NO BANKS YET - MAKE ONE WITH + NEW BANK",
            p.muted, 1);
    button(c, p, kPickDoneX, kPickBtnY, 110, 30, "DONE", true, hovered(kPickDoneX, kPickBtnY, 110, 30));
    addHit(kPickDoneX, kPickBtnY, 110, 30, kHitModalButton, kBtnDone);
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
            pressDialogButton(modal_.kind == kModalBankPicker ? kBtnDone : kBtnCancel);
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
