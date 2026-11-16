#pragma once

#include "vst3/vst3_abi.h"
#include "../plugin/PixelLabsIds.h"
#include <windows.h>
#include <atomic>
#include <functional>
#include <vector>

namespace PixelLabs {

struct PixelCanvas;
struct Palette;

class PixelLabsView : public Steinberg::IPlugView {
public:
    // Logical canvas; the window is this times the zoom factor.
    static constexpr int kBaseWidth  = 960;
    static constexpr int kBaseHeight = 600;

    // Layout anchors (logical pixels) - also used by the test host to click things.
    static constexpr int kDemoBtnX = 700, kDemoBtnY = 26;
    static constexpr int kThemeBtnX = 800, kThemeBtnY = 26;
    static constexpr int kKeyboardTop = 446, kKeyboardBottom = 592;
    static constexpr int kKeyboardLeft = 12, kWhiteKeyW = 26;
    static constexpr int kFirstKey = 36, kLastKey = 96; // 61 keys, C2..C7

    static int zoomPercent(int scaleIdx);

    PixelLabsView(SharedUITelemetry* telemetry,
                  std::function<void(int paramId, float normVal)> onParamChange,
                  std::function<void(int presetIdx)> onPresetChange);
    virtual ~PixelLabsView();

    // FUnknown
    Steinberg::tresult PLUGIN_API queryInterface(const Steinberg::TUID _iid, void** obj) override;
    Steinberg::uint32 PLUGIN_API addRef() override;
    Steinberg::uint32 PLUGIN_API release() override;

    // IPlugView
    Steinberg::tresult PLUGIN_API isPlatformTypeSupported(Steinberg::FIDString type) override;
    Steinberg::tresult PLUGIN_API attached(void* parent, Steinberg::FIDString type) override;
    Steinberg::tresult PLUGIN_API removed() override;
    Steinberg::tresult PLUGIN_API onWheel(float distance) override;
    Steinberg::tresult PLUGIN_API onKeyDown(Steinberg::char16 key, Steinberg::int16 keyCode, Steinberg::int16 modifiers) override;
    Steinberg::tresult PLUGIN_API onKeyUp(Steinberg::char16 key, Steinberg::int16 keyCode, Steinberg::int16 modifiers) override;
    Steinberg::tresult PLUGIN_API getSize(Steinberg::ViewRect* size) override;
    Steinberg::tresult PLUGIN_API onSize(Steinberg::ViewRect* newSize) override;
    Steinberg::tresult PLUGIN_API onFocus(Steinberg::TBool state) override;
    Steinberg::tresult PLUGIN_API setFrame(Steinberg::IPlugFrame* frame) override;
    Steinberg::tresult PLUGIN_API canResize() override;
    Steinberg::tresult PLUGIN_API checkSizeConstraint(Steinberg::ViewRect* rect) override;

private:
    enum HitKind {
        kHitNone = 0,
        kHitPresetRow,     // a = preset index
        kHitPresetPrev,
        kHitPresetNext,
        kHitPresetList,    // scroll area
        kHitEraTab,        // a = tab (0 = all)
        kHitMacroBtn,      // a = macro index
        kHitBigDial,
        kHitStepPrev,      // a = param id
        kHitStepNext,      // a = param id
        kHitSlider,        // a = param id
        kHitKeyboard,
        kHitDemo,
        kHitTheme,
        kHitZoom,
        kHitOctDown,
        kHitOctUp
    };
    struct Hit { int x, y, w, h, kind, a; };

    std::atomic<uint32_t> refCount_{1};
    SharedUITelemetry* telemetry_ = nullptr;
    std::function<void(int, float)> onParamChange_;
    std::function<void(int)> onPresetChange_;
    Steinberg::IPlugFrame* plugFrame_ = nullptr;

    HWND hwnd_ = nullptr;
    HBITMAP dibBitmap_ = nullptr;
    HDC memDC_ = nullptr;
    uint32_t* pixelBuffer_ = nullptr;

    std::vector<Hit> hits_;
    int hoverX_ = -1, hoverY_ = -1;
    int frame_ = 0;

    // Drag state
    int dragKind_ = kHitNone;
    int dragParam_ = -1;
    int dragStartX_ = 0, dragStartY_ = 0;
    float dragStartVal_ = 0.0f;
    Hit dragHit_ = {};
    int mouseNote_ = -1;

    // Preset browser
    int listScroll_ = 0;
    int lastPresetSeen_ = -1;
    int lastTabSeen_ = -1;

    // Computer-keyboard playing
    int kbdOctave_ = 4;          // octave of the A key
    int kbdHeld_[256] = {};      // vk -> note + 1 (0 = not held)

    int zoom() const;            // percent
    void createOffscreenBuffer();
    void destroyOffscreenBuffer();
    void renderUI();
    void requestResize();

    // Drawing sections
    void drawHeader(PixelCanvas& c, const Palette& p);
    void drawEraTabs(PixelCanvas& c, const Palette& p);
    void drawPresetList(PixelCanvas& c, const Palette& p);
    void drawHero(PixelCanvas& c, const Palette& p);
    void drawSoundPanel(PixelCanvas& c, const Palette& p);
    void drawStatusStrip(PixelCanvas& c, const Palette& p);
    void drawKeyboard(PixelCanvas& c, const Palette& p);

    void addHit(int x, int y, int w, int h, int kind, int a = 0);
    const Hit* hitAt(int x, int y) const;
    bool hovered(int x, int y, int w, int h) const;

    float param(int id) const;
    void setParam(int id, float v);
    void stepList(int paramId, int count, int dir);
    void stepPreset(int dir);
    std::vector<int> filteredPresets() const;
    int currentEra() const;

    void handleMouseDown(int x, int y);
    void handleMouseMove(int x, int y);
    void handleMouseUp(int x, int y);
    void handleDoubleClick(int x, int y);
    void handleWheel(int x, int y, float steps);
    bool handleKey(int vk, bool down);
    int keyAt(int x, int y) const;
    void playGuiNote(int note, bool on);

    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
};

} // namespace PixelLabs
